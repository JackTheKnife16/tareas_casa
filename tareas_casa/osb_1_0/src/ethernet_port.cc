#include <sst/core/sst_config.h>

#include "ethernet_port.h"

#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new EthernetPort::EthernetPort object
 *
 * @param id
 * @param params
 */
EthernetPort::EthernetPort(ComponentId_t id, Params& params) : Component(id)
{
    // Port number
    port                      = params.find<uint32_t>("port", 0);
    // Port group index
    pg_index                  = params.find<uint32_t>("pg_index", 0);
    // Inter-Packet Gap size
    ipg_size                  = params.find<uint32_t>("ipg_size", 20);
    // Bus width
    bus_width                 = params.find<uint32_t>("bus_width", 128);
    // Port group index
    init_chunk                = params.find<uint32_t>("initial_chunk", 2);
    // Port group index
    non_init_chunk            = params.find<uint32_t>("non_initial_chunk", 1);
    // Port's bandwidth
    port_speed                = params.find<UnitAlgebra>("port_speed", "25Gb/s");
    // Offered load
    offered_load              = params.find<float>("offered_load", 100.00) / 100;
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":EthernetPort[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Ethernet Port --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port: %d\n", port);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port Group Index: %d\n", pg_index);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Bus width: %d\n", bus_width);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Intial Chunk Size: %d\n", init_chunk);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Non-Initial Chunk Size: %d\n", non_init_chunk);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Bandwidth: %s\n", port_speed.toStringBestSI().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Offered Load: %f \n", offered_load);

    // Set request packet true, to start requesting packets
    request_pkt = true;

    // Compute default send delay based on bandwidth and word size (Bus size)
    UnitAlgebra total_bits = UnitAlgebra(std::to_string(bus_width * BITS_IN_BYTE) + "b");
    UnitAlgebra interval   = (total_bits / (port_speed * offered_load)) / UnitAlgebra("1ps");
    default_delay          = interval.getRoundedValue();
    // Also compute the IPG delay used before requesting a new packet
    total_bits             = UnitAlgebra(std::to_string(ipg_size * BITS_IN_BYTE) + "b");
    interval               = (total_bits / (port_speed * offered_load)) / UnitAlgebra("1ps");
    ipg_delay              = interval.getRoundedValue();

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    // Traffic Generator link with handler
    traffic_gen_link = configureLink(
        "traffic_gen", base_tc, new Event::Handler<EthernetPort>(this, &EthernetPort::new_packet_handler));

    // Output link with no handler
    output_link = configureLink("output", base_tc);

    // Sender link with handler
    sender_link = configureSelfLink(
        "sender_link", base_tc, new Event::Handler<EthernetPort>(this, &EthernetPort::sender_routine));

    // Packet request link
    schedule_link = configureSelfLink(
        "schedule_link", base_tc, new Event::Handler<EthernetPort>(this, &EthernetPort::schedule_routine));

    // Check if links are connected
    assert(traffic_gen_link);
    assert(output_link);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");

    // Stats config
    bytes_sent   = registerStatistic<uint32_t>("bytes_sent", "1");
    sent_packets = registerStatistic<uint32_t>("sent_packets", "1");

    // Tell SST to wait until we authorize it to exit
    // TODO: remove this when more components are implemented
    registerAsPrimaryComponent();
    primaryComponentDoNotEndSim();
}

/**
 * @brief Initial stage in SST simulations. It collects the first packet event from the traffic generator.
 *
 * @param phase Current initialization phase.
 */
void
EthernetPort::init(uint32_t phase)
{
    // Receive the packet in phase 1 because it is sent during phase 0 from traffic gen
    if ( phase == 1 ) {
        // Check the first initial event from traffic gen
        Event*       ev  = traffic_gen_link->recvInitData();
        PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
        if ( pkt ) {
            // If it is valid, it'll be pushed
            pkt_queue.push(pkt);
        }
    }
}

/**
 * @brief Setup stage which starts at time 0. It chunks the current packet and
 * schedules a word.
 *
 */
void
EthernetPort::setup()
{
    if ( !pkt_queue.empty() ) {
        // Assing current with the new packet
        current_packet = pkt_queue.front();
        pkt_queue.pop();

        chunk_packet(current_packet);
        schedule_word();
    }
    else {
        request_pkt = false;
    }
}

/**
 * @brief It schedules a new word using the sender link.
 *
 */
void
EthernetPort::schedule_word()
{
    PacketWordEvent* front_word = words_to_send.front();
    words_to_send.pop();
    // If the valid byte count is not equal to the word size,
    // the delay is different
    SST::SimTime_t send_delay = default_delay;
    if ( front_word->valid_byte_count != bus_width ) {
        UnitAlgebra total_bits = UnitAlgebra(std::to_string(front_word->valid_byte_count * BITS_IN_BYTE) + "b");
        UnitAlgebra interval   = (total_bits / (port_speed * offered_load)) / UnitAlgebra("1ps");
        send_delay             = interval.getRoundedValue();
    }

    sender_link->send(send_delay, front_word);
}

/**
 * @brief Takes a full packet event and cuts it in chunks.
 *
 * @param pkt Full Packet Event
 */
void
EthernetPort::chunk_packet(PacketEvent* pkt)
{
    out->verbose(CALL_INFO, DEBUG, 0, "New packet to chunk: pkt_size: %d\n", pkt->pkt_size);
    // Packet bytes counter
    uint32_t bytes_count = pkt->pkt_size;
    // Chunk counter
    uint32_t chunk_count = 0;
    // Counter for words in a chunk, reseted when a chunk is complete
    uint32_t word_count  = 0;
    // Sequence number
    uint32_t seq_num     = 0;

    // Flag to indicate that the end of packet is reached
    bool is_eop = false;

    // While there are bytes to chunk
    while ( bytes_count > 0 ) {
        // Create a word
        PacketWordEvent* word = new PacketWordEvent(pkt);

        // For now the same priority for both
        word->fe_priority = pkt->priority;

        // Set the word sequence number
        word->word_sequence_number = seq_num;

        // Find the current chunk size depending on the chunk count (First may be different)
        uint32_t current_chunk_size;
        if ( chunk_count == 0 ) {
            // Initial chunk so set start of packet
            word->start_of_packet = true;
            current_chunk_size    = init_chunk - 1;
        }
        else {
            // Non-Initial chunk
            current_chunk_size = non_init_chunk - 1;
        }

        // If this is the first word
        if ( word_count == 0 ) {
            // Set first chunk word as true
            word->first_chunk_word = true;

            // If bytes left to check are lower than a chunk, set eop as true
            is_eop = false;
            if ( bytes_count <= (current_chunk_size + 1) * bus_width ) { is_eop = true; }

            // In case the initial chunk word is also the last chunk word
            // increment chunks, and reset word count
            if ( current_chunk_size == 0 ) {
                word->last_chunk_word = true;

                chunk_count++;

                word_count = 0;
            }
            else {
                // Otherwise just count another words in the chunk
                word_count++;
            }
        }
        else if ( word_count == current_chunk_size ) {
            // If the current word count reaches the current cunk size
            // this is the last word chunk
            word->last_chunk_word = true;

            chunk_count++;

            word_count = 0;
        }
        else {
            // Otherwise count a word in the current chunk
            word_count++;
        }

        // If eop is set, set it in the word
        if ( is_eop ) { word->end_of_packet = true; }

        if ( bytes_count <= bus_width ) {
            // If the left bytes are lower that the word size, finish the chunk and word
            word->last_chunk_word  = true;
            word->end_of_packet    = true;
            // Set the valid bit count
            word->valid_byte_count = bytes_count;

            // Reset the bytes coun to finish loop
            bytes_count = 0;
        }
        else {
            // Otherwise, valid bit count is the word size
            word->valid_byte_count = bus_width;
            // Decrement bytes count
            bytes_count -= bus_width;
        }

        // Update word seq number
        seq_num++;

        // Push word to the chunk queue
        words_to_send.push(word);
    }
}

/**
 * @brief Used as handler for the sender routine, if there are not more packet words
 * it calls the request routine with the IPG delay.
 *
 * @param ev Packet Word Event
 */
void
EthernetPort::sender_routine(SST::Event* ev)
{
    // Stat collection
    PacketWordEvent* pkt = dynamic_cast<PacketWordEvent*>(ev);
    out->verbose(
        CALL_INFO, MODERATE, 0, "Sending word: pkt_id=%u sop=%d eop=%d first=%d last=%d seq=%d vbc=%d\n", pkt->pkt_id,
        pkt->start_of_packet, pkt->end_of_packet, pkt->first_chunk_word, pkt->last_chunk_word,
        pkt->word_sequence_number, pkt->valid_byte_count);
    output_link->send(ev);

    if ( !words_to_send.empty() ) { schedule_word(); }
    else {
        // Request if request flag is true
        if ( request_pkt ) {
            PacketRequestEvent* req = new PacketRequestEvent();
            req->node_request       = 0;
            req->port_request       = port;

            traffic_gen_link->send(req);
        }
        // Call the request link with the IPG delay to chunk a new packet and send the words
        schedule_link->send(ipg_delay, NULL);
    }
}

/**
 * @brief Schedule routine that chunks and schedules a new packet if available, since it is called after the IPG delay,
 * it is used to collect stats from the current packet.
 *
 * @param ev NULL (Required by SST)
 */
void
EthernetPort::schedule_routine(SST::Event* ev)
{
    out->verbose(CALL_INFO, DEBUG, 0, "IPG delay is done, the port %d can send more words\n", port);

    // Stats collection
    bytes_sent->addData(current_packet->pkt_size);
    sent_packets->addData(1);

    delete current_packet;
    current_packet = NULL;

    if ( !pkt_queue.empty() ) {
        // Assing current with the new packet
        current_packet = pkt_queue.front();
        pkt_queue.pop();

        chunk_packet(current_packet);
        schedule_word();
    }
    else {
        // TODO: remove when there are more components
        primaryComponentOKToEndSim();
    }
}

/**
 * @brief Handler for packet incoming packets from the Traffic Generator
 *
 * @param ev Event with a packet
 */
void
EthernetPort::new_packet_handler(SST::Event* ev)
{
    // If event is NULL, there are not more pkts to send
    if ( ev == NULL ) {
        request_pkt = false;
        out->verbose(CALL_INFO, MODERATE, 0, "Ethernet Port %d ran out of pkts!\n", port);
    }
    else {
        PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
        if ( pkt ) {
            // Add new packet to the queue
            pkt_queue.push(pkt);
        }
        else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
        }
    }
}

/**
 * @brief Finish stage of SST
 *
 */
void
EthernetPort::finish()
{}
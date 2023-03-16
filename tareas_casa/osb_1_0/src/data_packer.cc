#include <sst/core/sst_config.h>

#include "data_packer.h"

#include "sst/core/event.h"

#include "fds_event.h"

#include <assert.h>
#include <fstream>
#include <functional> // std::bind
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new DataPacker::DataPacker object
 *
 * @param id
 * @param params
 */
DataPacker::DataPacker(ComponentId_t id, Params& params) : Component(id)
{
    // Clock frequency
    UnitAlgebra frequency = params.find<UnitAlgebra>("frequency", "1500 MHz");
    // Buffer Slice index
    buffer_slice_index    = params.find<uint32_t>("buffer_slice_index", 0);
    // Port group index
    port_ingress_slices   = params.find<uint32_t>("port_ingress_slices", 2);
    // Input bus width
    in_bus_width          = params.find<uint32_t>("input_bus_width", 128);
    // Output bus width
    out_bus_width         = params.find<uint32_t>("output_bus_width", 256);
    // FDS hole size
    fds_size              = params.find<uint32_t>("fds_size", 80);
    // Initial Chunk size
    init_chunk            = params.find<uint32_t>("initial_chunk", 2);
    // Non-Initial Chunk size
    non_init_chunk        = params.find<uint32_t>("non_initial_chunk", 1);
    // Frontplane config file
    std::string frontplane_config =
        params.find<std::string>("frontplane_config", "osb_1_0/config/frontplane_config/4x800G.json");
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":DataPacker[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Data packer --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Clock frequency: %s\n", frequency.toStringBestSI().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Buffer Slice index: %d\n", buffer_slice_index);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of Port Ingress Slices: %d\n", port_ingress_slices);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Input bus width: %d\n", in_bus_width);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Output bus width: %d\n", out_bus_width);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> FDS Size: %d\n", fds_size);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Intial Chunk Size: %d\n", init_chunk);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Non-Initial Chunk Size: %d\n", non_init_chunk);

    base_tc = registerTimeBase("1ps", true);

    // Delay routine starts disabled
    delay_routine_running = false;

    // Compute the frequency delay
    UnitAlgebra interval = (UnitAlgebra("1") / frequency) / UnitAlgebra("1ps");
    freq_delay           = interval.getRoundedValue();
    out->verbose(CALL_INFO, MODERATE, 0, "Frequency delay is: %lu ps\n", freq_delay);

    // open JSON file for front plane info
    JSON          frontplane_config_data;
    std::ifstream frontplane_config_file(frontplane_config);
    frontplane_config_file >> frontplane_config_data;

    for ( int i = 0; i < port_ingress_slices; i++ ) {
        // Compute pg index
        uint32_t      pg_index        = i + buffer_slice_index * port_ingress_slices;
        std::string   ethernet_config = frontplane_config_data[pg_index]["ethernet_config"];
        // open JSON file for front plane info
        JSON          eth_config_data;
        std::ifstream eth_config_file(ethernet_config);
        eth_config_file >> eth_config_data;
        uint32_t ports_per_pg = eth_config_data.size();

        for ( int j = 0; j < ports_per_pg; j++ ) {
            if ( eth_config_data[j]["enable"] ) {
                // The port in the config is the pid in the pg
                uint32_t pid         = eth_config_data[j]["port"];
                // The port number in the chip is computed as follows
                uint32_t port_number = pid + pg_index * ports_per_pg;
                out->verbose(
                    CALL_INFO, DEBUG, 0, "Creating a accumulator for port %d which pid is %d and pg index is %d\n",
                    port_number, pid, pg_index);

                // Create function pointer to notify method
                std::function<void(void)> notify = std::bind(&DataPacker::notify_delay_routine, this);

                port_accumulators[port_number] =
                    new PortAccumulator(port_number, out_bus_width, fds_size, init_chunk, non_init_chunk, notify, out);
            }
        }
    }

    // Configure in links
    // Input link with handler
    in_packet_bus_link = configureLink(
        "in_packet_bus", base_tc, new Event::Handler<DataPacker>(this, &DataPacker::packet_word_receiver));

    // Output link with no handler
    out_packet_bus_link = configureLink("out_packet_bus", base_tc);

    // Scheduler routine
    delay_element_lookup_routine_link = configureSelfLink(
        "delay_element_lookup_routine", base_tc,
        new Event::Handler<DataPacker>(this, &DataPacker::delay_element_lookup_routine));

    delay_element_send_routine_link = configureSelfLink(
        "delay_element_send_routine", base_tc,
        new Event::Handler<DataPacker>(this, &DataPacker::delay_element_send_routine));

    // Check if links are connected
    assert(in_packet_bus_link);
    assert(out_packet_bus_link);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");

    // Stats config
    bytes_sent   = registerStatistic<uint32_t>("bytes_sent", "1");
    sent_packets = registerStatistic<uint32_t>("sent_packets", "1");
}

/**
 * @brief Used to notify the delay routine to start.
 *
 */
void
DataPacker::notify_delay_routine()
{
    safe_lock.lock();
    if ( !delay_routine_running ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Enabling delay routine.\n");
        delay_routine_running = true;
        delay_element_lookup_routine_link->send(NULL);
    }
    safe_lock.unlock();
}

/**
 * @brief Disables the delay routine.
 *
 */
void
DataPacker::disable_delay_routine()
{
    out->verbose(CALL_INFO, DEBUG, 0, "Disabling delay routine\n");
    safe_lock.lock();
    delay_routine_running = false;
    safe_lock.unlock();
}

/**
 * @brief Handler for incoming packet words
 *
 * @param ev Packet Word Event
 */
void
DataPacker::packet_word_receiver(SST::Event* ev)
{
    // Cast the event
    PacketWordEvent* pkt_word = dynamic_cast<PacketWordEvent*>(ev);

    if ( pkt_word ) {
        out->verbose(
            CALL_INFO, DEBUG, 0, "Received a new packet word %u, putting it in port accumulator %d\n", pkt_word->pkt_id,
            pkt_word->src_port);
        // Add it into the respective port accumulator
        port_accumulators[pkt_word->src_port]->add_new_word(pkt_word);

        // Release the word, since won't be used anymore
        delete pkt_word;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief First phase of the delay routine which finds a delay routine with a complete word
 *
 * @param ev
 */
void
DataPacker::delay_element_lookup_routine(SST::Event* ev)
{
    // Iterate the Port Accumulators to find one with a complete word
    PortAccumulator* port_with_word = NULL;
    for ( auto& port_accumulator : port_accumulators ) {
        if ( port_accumulator.second->word_ready() ) {
            port_with_word = port_accumulator.second;
            break;
        }
    }

    // Was there a port accumulator with a complete word?
    if ( port_with_word ) {
        PacketWordEvent* pkt_word = port_with_word->pop();

        out->verbose(
            CALL_INFO, DEBUG, 0, "Popped a packet word %u from port accumulator %d\n", pkt_word->pkt_id,
            pkt_word->src_port);
        // Call the next phase after the freq delay
        delay_element_send_routine_link->send(freq_delay, pkt_word);
    }
    else {
        out->fatal(CALL_INFO, -1, "There are not packet words ready to send\n");
    }
}

/**
 * @brief Last phase of delay routine which sends the packet word, and FDS when it is the first word
 *
 * @param ev
 */
void
DataPacker::delay_element_send_routine(SST::Event* ev)
{
    PacketWordEvent* pkt_word = dynamic_cast<PacketWordEvent*>(ev);

    out->verbose(
        CALL_INFO, MODERATE, 0,
        "Sending new word through output packet bus: pkt_id=%u src_port=%d pri=%d sop=%d eop=%d first=%d last=%d "
        " seq=%d vbc=%d\n",
        pkt_word->pkt_id, pkt_word->src_port, pkt_word->priority, pkt_word->start_of_packet, pkt_word->end_of_packet,
        pkt_word->first_chunk_word, pkt_word->last_chunk_word, pkt_word->word_sequence_number,
        pkt_word->valid_byte_count);

    // Stat collection
    uint32_t bytes_to_send = pkt_word->valid_byte_count;
    // Remove the FDS hole bytes at the initial word
    if ( pkt_word->start_of_packet && pkt_word->first_chunk_word ) { bytes_to_send -= fds_size; }
    bytes_sent->addData(bytes_to_send);

    // Sent packets stat is updated when a eop is seen
    if ( pkt_word->end_of_packet && pkt_word->last_chunk_word ) { sent_packets->addData(1); }

    out_packet_bus_link->send(pkt_word);

    // Check if there is another word ready to see if the delay routine keeps running
    bool word_ready = false;
    for ( auto& port_accumulator : port_accumulators ) {
        if ( port_accumulator.second->word_ready() ) {
            word_ready = true;
            break;
        }
    }

    // Must the routine keep running?
    if ( word_ready ) {
        out->verbose(CALL_INFO, DEBUG, 0, "There are transactions to send, delay routine keeps running\n");
        delay_element_lookup_routine_link->send(NULL);
    }
    else {
        out->verbose(CALL_INFO, DEBUG, 0, "No transactions, delay routine is disabled\n");
        disable_delay_routine();
    }
}

/**
 * @brief Finish stage of SST
 *
 */
void
DataPacker::finish()
{}
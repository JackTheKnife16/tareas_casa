#include <sst/core/sst_config.h>

#include "acceptance_checker.h"

#include "sst/core/event.h"

#include "fds_event.h"

#include <assert.h>
#include <fstream>
#include <functional> // std::bind
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new AcceptanceChecker::AcceptanceChecker object
 *
 * @param id
 * @param params
 */
AcceptanceChecker::AcceptanceChecker(ComponentId_t id, Params& params) : Component(id)
{
    // Clock frequency
    UnitAlgebra frequency     = params.find<UnitAlgebra>("frequency", "1500 MHz");
    // Port group index
    pg_index                  = params.find<uint32_t>("pg_index", 0);
    // Input bus width
    bus_width                 = params.find<uint32_t>("bus_width", 256);
    // MTU config
    std::string mtu_path      = params.find<std::string>("mtu_config_file", "osb_1_0/config/mtu_config/mtu_basic.json");
    // Packet data limit
    packet_data_limit         = params.find<uint32_t>("packet_data_limit", 1);
    // FDS limit
    fds_limit                 = params.find<uint32_t>("fds_limit", 1);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":AcceptanceChecker[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Acceptance Checker --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Frequency: %s\n", frequency.toStringBestSI().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port Group Index: %d\n", pg_index);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Bus width: %dB\n", bus_width);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> MTU config: %s\n", mtu_path.c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Packet data limit: %d\n", packet_data_limit);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> FDS limit: %d\n", fds_limit);

    base_tc = registerTimeBase("1ps", true);

    // Create function pointer to notify method
    std::function<void(void)> notify = std::bind(&AcceptanceChecker::notify_checker_routine, this);
    packet_data_buffer               = new ReceiverBuffer("packet_data_buffer", 0, notify, out);
    fds_buffer                       = new ReceiverBuffer("fds_buffer", 0, out);

    // Checker routine starts
    checker_routine_running = false;

    // Compute the frequency delay
    UnitAlgebra interval = (UnitAlgebra("1") / frequency) / UnitAlgebra("1ps");
    freq_delay           = interval.getRoundedValue();
    out->verbose(CALL_INFO, MODERATE, 0, "Frequency delay is: %lu ps\n", freq_delay);

    // open JSON file for MTU info
    JSON          mtu_config_data;
    std::ifstream mtu_config_file(mtu_path);
    mtu_config_file >> mtu_config_data;
    int port_per_pg = mtu_config_data.size();
    // Read mtu info
    for ( int i = 0; i < port_per_pg; i++ ) {
        uint32_t pid                  = mtu_config_data[i]["port"];
        uint32_t port_number          = pid + pg_index * port_per_pg;
        uint32_t mtu_size             = mtu_config_data[i]["mtu"];
        mtu_config[port_number]       = mtu_size;
        // Set all accepted ports as false
        accepted_by_port[port_number] = false;
    }

    // Configure in links
    // With handlers
    in_packet_bus_link = configureLink(
        "in_packet_bus", base_tc,
        new Event::Handler<ReceiverBuffer>(packet_data_buffer, &ReceiverBuffer::new_transaction_handler));
    in_fds_bus_link = configureLink(
        "in_fds_bus", base_tc,
        new Event::Handler<ReceiverBuffer>(fds_buffer, &ReceiverBuffer::new_transaction_handler));
    packet_fifos_link = configureLink(
        "packet_fifos", base_tc,
        new Event::Handler<AcceptanceChecker>(this, &AcceptanceChecker::packet_fifo_update_receiver));
    fds_fifos_link = configureLink(
        "fds_fifos", base_tc,
        new Event::Handler<AcceptanceChecker>(this, &AcceptanceChecker::fds_fifo_update_receiver));

    // Without handlers
    drop_receiver_link  = configureLink("drop_receiver", base_tc);
    out_packet_bus_link = configureLink("out_packet_bus", base_tc);
    out_fds_bus_link    = configureLink("out_fds_bus", base_tc);

    // Self-link
    // Scheduler routine
    checker_routine_link = configureSelfLink(
        "checker_routine", base_tc, new Event::Handler<AcceptanceChecker>(this, &AcceptanceChecker::checker_routine));

    // Check if links are connected
    assert(in_packet_bus_link);
    assert(in_fds_bus_link);
    assert(packet_fifos_link);
    assert(fds_fifos_link);
    assert(drop_receiver_link);
    assert(out_packet_bus_link);
    assert(out_fds_bus_link);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Initial stage in SST simulations. It receives the initial status of the Multi-FIFO.
 *
 * @param phase Current initialization phase.
 */
void
AcceptanceChecker::init(uint32_t phase)
{
    if ( phase == 1 ) {
        // Get packet data multi-fifo initial status
        FIFOStatusEvent* status = dynamic_cast<FIFOStatusEvent*>(packet_fifos_link->recvInitData());
        do {
            out->verbose(
                CALL_INFO, DEBUG, 0, "Setting initial status of packet FIFO(%d, %d): %d entries available\n",
                status->port_idx, status->pri_idx, status->space_available);
            packet_data_space_avail[status->port_idx][status->pri_idx] = status->space_available;
            status = dynamic_cast<FIFOStatusEvent*>(packet_fifos_link->recvInitData());
        } while ( status );

        // Get fds multi-fifo initial status
        status = dynamic_cast<FIFOStatusEvent*>(fds_fifos_link->recvInitData());
        do {
            out->verbose(
                CALL_INFO, DEBUG, 0, "Setting initial status of FDS FIFO(%d, %d): %d entries available\n",
                status->port_idx, status->pri_idx, status->space_available);
            fds_space_avail[status->port_idx][status->pri_idx] = status->space_available;
            status = dynamic_cast<FIFOStatusEvent*>(fds_fifos_link->recvInitData());
        } while ( status );
    }
}

/**
 * @brief Used to notify the checker routine when it must start again.
 *
 */
void
AcceptanceChecker::notify_checker_routine()
{
    safe_lock.lock();
    if ( !checker_routine_running ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Enabling checker routine.\n");
        checker_routine_running = true;
        checker_routine_link->send(freq_delay, NULL);
    }
    safe_lock.unlock();
}

/**
 * @brief Disables the checker routine
 *
 */
void
AcceptanceChecker::disable_checker_routine()
{
    out->verbose(CALL_INFO, DEBUG, 0, "Disabling checker routine\n");
    safe_lock.lock();
    checker_routine_running = false;
    safe_lock.unlock();
}

/**
 * @brief Send a packet word to the output bus.
 *
 * @param pkt_word Packet word to send
 */
void
AcceptanceChecker::push_packet_data(PacketWordEvent* pkt_word)
{
    out->verbose(
        CALL_INFO, DEBUG, 0,
        "Pushing new word to the packet FIFO: pkt_id=%u src_port=%d pri=%d sop=%d eop=%d first=%d last=%d seq=%d "
        "vbc=%d\n",
        pkt_word->pkt_id, pkt_word->src_port, pkt_word->priority, pkt_word->start_of_packet, pkt_word->end_of_packet,
        pkt_word->first_chunk_word, pkt_word->last_chunk_word, pkt_word->word_sequence_number,
        pkt_word->valid_byte_count);

    out_packet_bus_link->send(pkt_word);
}

/**
 * @brief Discards a packet word, when it is the last chunk, the word is sent to the Drop Receiver
 *
 * @param pkt_word Packet word to discard
 */
void
AcceptanceChecker::discard_packet_data(PacketWordEvent* pkt_word)
{
    out->verbose(
        CALL_INFO, DEBUG, 0,
        "Discarding new word: pkt_id=%u src_port=%d pri=%d sop=%d eop=%d first=%d last=%d seq=%d vbc=%d\n",
        pkt_word->pkt_id, pkt_word->src_port, pkt_word->priority, pkt_word->start_of_packet, pkt_word->end_of_packet,
        pkt_word->first_chunk_word, pkt_word->last_chunk_word, pkt_word->word_sequence_number,
        pkt_word->valid_byte_count);

    if ( pkt_word->end_of_packet and pkt_word->last_chunk_word ) {
        drop_receiver_link->send(pkt_word);
        delete pkt_word;
    }
    else {
        delete pkt_word;
    }
}


/**
 * @brief Checker routine that decides if a packet word is accepted or dropped
 *
 * @param ev Null Event
 */
void
AcceptanceChecker::checker_routine(SST::Event* ev)
{
    // Pops word from the packet buffer
    PacketWordEvent* pkt = dynamic_cast<PacketWordEvent*>(packet_data_buffer->pop());

    // Is this the first word of a packet?
    if ( pkt->start_of_packet && pkt->first_chunk_word ) {
        out->verbose(
            CALL_INFO, DEBUG, 0, "New start of packet received: port=%d priority=%d\n", pkt->src_port, pkt->priority);

        uint32_t mtu_entries = ceil(float(mtu_config[pkt->src_port]) / float(bus_width));
        out->verbose(
            CALL_INFO, DEBUG, 0, "Number of entries for an MTU in port %d: %d B which are %d entries in the FIFO.\n",
            pkt->src_port, mtu_config[pkt->src_port], mtu_entries);

        // Space available for both Packet and FDS FIFO
        packet_data_safe_lock.lock();
        uint32_t pkt_fifo_space_available = packet_data_space_avail[pkt->src_port][pkt->priority];
        packet_data_safe_lock.unlock();

        fds_safe_lock.lock();
        uint32_t fds_fifo_space_available = fds_space_avail[pkt->src_port][pkt->priority];
        fds_safe_lock.unlock();

        out->verbose(
            CALL_INFO, DEBUG, 0, "Space available for FIFO(%d, %d): Packet Data: %d entries, FDS: %d entries.\n",
            pkt->src_port, pkt->priority, pkt_fifo_space_available, fds_fifo_space_available);

        // Is there enough space in the FIFOs?
        if ( (mtu_entries * packet_data_limit) <= pkt_fifo_space_available && fds_limit <= fds_fifo_space_available ) {
            out->verbose(
                CALL_INFO, MODERATE, 0, "Packet accepted: port=%d priority=%d.\n", pkt->src_port, pkt->priority);
            accepted_by_port[pkt->src_port] = true;

            FDSEvent* fds = dynamic_cast<FDSEvent*>(fds_buffer->pop());
            out_fds_bus_link->send(fds);

            push_packet_data(pkt);
        }
        else {
            out->verbose(
                CALL_INFO, MODERATE, 0, "Packet rejected: port=%d priority=%d.\n", pkt->src_port, pkt->priority);
            accepted_by_port[pkt->src_port] = false;

            FDSEvent* fds = dynamic_cast<FDSEvent*>(fds_buffer->pop());
            delete fds;

            discard_packet_data(pkt);
        }
    }
    else if ( accepted_by_port[pkt->src_port] ) {
        // If a packet was accepted previously, it'll be accepted
        push_packet_data(pkt);
    }
    else {
        // Otherwise, it'll be dropped
        discard_packet_data(pkt);
    }

    // Must the checker keep running?
    if ( !packet_data_buffer->is_empty() ) { checker_routine_link->send(freq_delay, NULL); }
    else {
        disable_checker_routine();
    }
}

/**
 * @brief Handler for incoming updates from the packet data multi-FIFO
 *
 * @param ev Status Event
 */
void
AcceptanceChecker::packet_fifo_update_receiver(SST::Event* ev)
{
    FIFOStatusEvent* status = dynamic_cast<FIFOStatusEvent*>(ev);
    if ( status ) {
        packet_data_safe_lock.lock();
        out->verbose(
            CALL_INFO, DEBUG, 0, "Updating packet fifo port %d priority %d: %d entries available\n", status->port_idx,
            status->pri_idx, status->space_available);

        packet_data_space_avail[status->port_idx][status->pri_idx] = status->space_available;
        packet_data_safe_lock.unlock();
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for incoming updates from the FDS multi-FIFO
 *
 * @param ev Status Event
 */
void
AcceptanceChecker::fds_fifo_update_receiver(SST::Event* ev)
{
    FIFOStatusEvent* status = dynamic_cast<FIFOStatusEvent*>(ev);
    if ( status ) {
        fds_safe_lock.lock();
        out->verbose(
            CALL_INFO, DEBUG, 0, "Updating FDS fifo port %d priority %d: %d entries available\n", status->port_idx,
            status->pri_idx, status->space_available);

        fds_space_avail[status->port_idx][status->pri_idx] = status->space_available;
        fds_safe_lock.unlock();
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Finish stage of SST
 *
 */
void
AcceptanceChecker::finish()
{}
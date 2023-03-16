#include <sst/core/sst_config.h>

#include "outer_parser.h"

#include "sst/core/event.h"

#include "fds_event.h"
#include "packet_word_event.h"

#include <assert.h>
#include <functional> // std::bind

using namespace SST;

/**
 * @brief Construct a new OuterParser::OuterParser object
 *
 * @param id
 * @param params
 */
OuterParser::OuterParser(ComponentId_t id, Params& params) : Component(id)
{
    // Clock frequency
    UnitAlgebra frequency     = params.find<UnitAlgebra>("frequency", "1500 MHz");
    // Port group index
    pg_index                  = params.find<uint32_t>("pg_index", 0);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":OuterParser[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Outer Parser --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Clock frequency: %s\n", frequency.toStringBestSI().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port Group Index: %d\n", pg_index);

    base_tc = registerTimeBase("1ps", true);

    // Delay routine starts disabled
    delay_element_running = false;

    // Compute the frequency delay
    UnitAlgebra interval = (UnitAlgebra("1") / frequency) / UnitAlgebra("1ps");
    freq_delay           = interval.getRoundedValue();
    out->verbose(CALL_INFO, MODERATE, 0, "Frequency delay is: %lu ps\n", freq_delay);

    // Create function pointer to notify method
    std::function<void(void)> notify = std::bind(&OuterParser::notify_delay_routine, this);

    packet_buffer = new ReceiverBuffer("packet_buffer", 0, notify, out);
    fds_buffer    = new ReceiverBuffer("fds_buffer", 0, out);

    // Configure in links
    // Input link with handler
    in_packet_bus_link = configureLink(
        "in_packet_bus", base_tc,
        new Event::Handler<ReceiverBuffer>(packet_buffer, &ReceiverBuffer::new_transaction_handler));
    // Input fds bus
    in_fds_bus_link = configureLink(
        "in_fds_bus", base_tc,
        new Event::Handler<ReceiverBuffer>(fds_buffer, &ReceiverBuffer::new_transaction_handler));

    // Output packet link with no handler
    out_packet_bus_link = configureLink("out_packet_bus", base_tc);
    // Output fds link
    out_fds_bus_link    = configureLink("out_fds_bus", base_tc);

    // Check if links are connected
    assert(in_packet_bus_link);
    assert(in_fds_bus_link);
    assert(out_packet_bus_link);
    assert(out_fds_bus_link);

    // Delay Element routine
    delay_routine_link =
        configureSelfLink("delay_routine", base_tc, new Event::Handler<OuterParser>(this, &OuterParser::delay_routine));

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief If the delay element is disabled, it is enabled. This method is called by the packet buffer, when
 * they receive a new transaction and the buffer was empty.
 *
 */
void
OuterParser::notify_delay_routine()
{
    safe_lock.lock();
    if ( !delay_element_running ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Enabling delay routine in %lu ps\n", freq_delay);
        delay_element_running = true;
        delay_routine_link->send(freq_delay, NULL);
    }
    safe_lock.unlock();
}

/**
 * @brief It disables the delay element routine
 *
 */
void
OuterParser::disable_delay_routine()
{
    out->verbose(CALL_INFO, DEBUG, 0, "Disabling delay routine\n");
    safe_lock.lock();
    delay_element_running = false;
    safe_lock.unlock();
}


/**
 * @brief Delay Element routine that forwards packet data every clock cycle
 *
 * @param ev Event not used
 */
void
OuterParser::delay_routine(SST::Event* ev)
{
    if ( !packet_buffer->is_empty() ) {
        // Get next transaction
        PacketWordEvent* pkt_word = dynamic_cast<PacketWordEvent*>(packet_buffer->pop());
        out->verbose(
            CALL_INFO, MODERATE, 0,
            "Sending word through output packet bus: pkt_id=%u src_port=%d pri=%d sop=%d eop=%d first=%d last=%d "
            "seq=%d vbc=%d\n",
            pkt_word->pkt_id, pkt_word->src_port, pkt_word->priority, pkt_word->start_of_packet,
            pkt_word->end_of_packet, pkt_word->first_chunk_word, pkt_word->last_chunk_word,
            pkt_word->word_sequence_number, pkt_word->valid_byte_count);

        // Forward transaction
        // If it is the first word, forward the fds as well
        if ( pkt_word->start_of_packet && pkt_word->first_chunk_word ) {
            FDSEvent* fds = dynamic_cast<FDSEvent*>(fds_buffer->pop());

            out->verbose(CALL_INFO, MODERATE, 0, "Sending the FDS through output fds bus: pkt_id=%u\n", fds->pkt_id);
            out_fds_bus_link->send(fds);
        }
        out_packet_bus_link->send(pkt_word);
        // Call the routine again
        delay_routine_link->send(freq_delay, NULL);
    }
    else {
        disable_delay_routine();
    }
}

/**
 * @brief Finish stage of SST
 *
 */
void
OuterParser::finish()
{}
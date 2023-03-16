#include <sst/core/sst_config.h>

#include "receiver.h"

#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new Receiver::Receiver object
 *
 * @param id
 * @param params
 */
Receiver::Receiver(ComponentId_t id, Params& params) : Component(id)
{
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":Receiver[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Sender Component --\n");

    pkt_counter = 0;

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    input  = configureLink("input", base_tc, new Event::Handler<Receiver>(this, &Receiver::handle_new_transaction));
    output = configureLink("output", base_tc);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Handler for packet incoming packets words
 *
 * @param ev Event with a packet word
 */
void
Receiver::handle_new_transaction(SST::Event* ev)
{
    PacketWordEvent* pkt = dynamic_cast<PacketWordEvent*>(ev);

    out->verbose(
        CALL_INFO, MODERATE, 0, "Received a new transaction: sop=%d eop=%d first=%d last=%d vbc=%d\n",
        pkt->start_of_packet, pkt->end_of_packet, pkt->first_chunk_word, pkt->last_chunk_word, pkt->valid_byte_count);

    if ( pkt->end_of_packet && pkt->last_chunk_word ) { output->send(pkt); }
}

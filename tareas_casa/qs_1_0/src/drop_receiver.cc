#include <sst/core/sst_config.h>

#include "drop_receiver.h"

#include "sst/core/event.h"

#include "packet_event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new DropReceiver::DropReceiver object
 *
 * @param id
 * @param params
 */
DropReceiver::DropReceiver(ComponentId_t id, Params& params) : Component(id)
{
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":DropReceiver[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Drop Receiver --\n");

    pkts_drop     = 0;
    tcp_pkts_drop = 0;

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    wred = configureLink("wred", base_tc, new Event::Handler<DropReceiver>(this, &DropReceiver::handle_new_drop));
    out->verbose(CALL_INFO, MODERATE, 0, "wred link was configured\n");

    finisher = configureLink("finisher", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "finisher link was configured\n");

    // Check it is not null
    assert(wred);

    // Create statistics
    dropped_packets = registerStatistic<uint32_t>("dropped_packets", "1");
    destination     = registerStatistic<uint32_t>("destination", "1");
    source          = registerStatistic<uint32_t>("source", "1");
    priority        = registerStatistic<uint32_t>("priority", "1");

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Handler for packet discards
 *
 * @param ev Event with dropped packet
 */
void
DropReceiver::handle_new_drop(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        safe_lock.lock();
        out->verbose(
            CALL_INFO, INFO, 0, "Received a dropped packet with pkt_id = %u and priority = %d\n", pkt->pkt_id,
            pkt->priority);

        // Count pkts
        pkts_drop++;
        // If it is a TCP packet, count it and inform
        if ( pkt->is_tcp ) {
            tcp_pkts_drop++;
            TCPPacketEvent* tcp_pkt = dynamic_cast<TCPPacketEvent*>(ev);
            out->verbose(
                CALL_INFO, INFO, 0,
                "Dropped TCP packet: pkt_id = %u, flag = %d, src_port = %d, dest_port = %d, ack = %d, seq = %d\n",
                tcp_pkt->pkt_id, tcp_pkt->flags, tcp_pkt->src_port, tcp_pkt->dest_tcp_port, tcp_pkt->ack_number,
                tcp_pkt->seq_number);
        }

        // Add statistics
        dropped_packets->addData(1);
        destination->addData(pkt->dest_port);
        source->addData(pkt->src_port);
        priority->addData(pkt->priority);

        out->verbose(CALL_INFO, INFO, 0, "Sending the dropped packet with pkt_id = %u to the finisher.\n", pkt->pkt_id);

        finisher->send(pkt);

        safe_lock.unlock();
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
DropReceiver::finish()
{
    out->verbose(CALL_INFO, INFO, 0, "%d packets were dropped during the simulation\n", pkts_drop);
    out->verbose(CALL_INFO, INFO, 0, "%d of those were TCP packets\n", tcp_pkts_drop);
}

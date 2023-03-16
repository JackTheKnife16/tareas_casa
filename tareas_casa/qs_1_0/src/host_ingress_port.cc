#include <sst/core/sst_config.h>

#include "host_ingress_port.h"

#include "sst/core/event.h"

#include "packet_event.h"

#include <assert.h>
#include <math.h>

using namespace SST;

/**
 * @brief Construct a new HostIngressPort::HostIngressPort object
 *
 * @param id
 * @param params
 */
HostIngressPort::HostIngressPort(ComponentId_t id, Params& params) : Component(id)
{
    // Port number
    port                      = params.find<uint32_t>("port", 0);
    // Node number
    node                      = params.find<uint32_t>("node", 0);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":HostIngressPort[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Host Ingress Port --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Node: %d\n", node);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port: %d\n", port);

    base_tc = registerTimeBase("1ps", true);

    pkts_sent = 0;

    // Configure in links
    external_port = configureLink(
        "external_port", base_tc, new Event::Handler<HostIngressPort>(this, &HostIngressPort::handle_new_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "external_port link was configured\n");

    acceptance_checker = configureLink("acceptance_checker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "acceptance_checker link was configured\n");

    // Check links
    assert(external_port);
    assert(acceptance_checker);

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");

    // Create statistics
    priorities   = registerStatistic<uint32_t>("priorities", "1");
    bytes_sent   = registerStatistic<uint32_t>("bytes_sent", "1");
    sent_packets = registerStatistic<uint32_t>("sent_packets", "1");
}

/**
 * @brief Handler for new packets
 *
 * @param ev Envent with the new packet
 */
void
HostIngressPort::handle_new_pkt(SST::Event* ev)
{
    // If event is NULL, there are not more pkts to send
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        safe_lock.lock();
        out->verbose(
            CALL_INFO, INFO, 0,
            "Received new packet on ingress port: pkt_id=%u, size=%dB, destination_port=%d, priority=%d\n", pkt->pkt_id,
            pkt->pkt_size, pkt->dest_port, pkt->priority);

        // Set the source port
        pkt->src_node = node;
        pkt->src_port = port;
        // Statistics collection
        priorities->addData(pkt->priority);
        sent_packets->addData(1);
        bytes_sent->addData(pkt->pkt_size + IPG_SIZE);

        out->verbose(
            CALL_INFO, INFO, 0,
            "Sending packet to acceptance checker, pkt_id=%u, size=%dB, destination=%d, priority=%d\n", pkt->pkt_id,
            pkt->pkt_size, pkt->dest_port, pkt->priority);

        acceptance_checker->send(pkt);
        pkts_sent++;

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
HostIngressPort::finish()
{
    out->verbose(CALL_INFO, INFO, 0, "Ingress Port %d: %d packets\n", port, pkts_sent);
}

#include <sst/core/sst_config.h>

#include "egress_update.h"

#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new EgressUpdate::EgressUpdate object
 *
 * @param id
 * @param params
 */
EgressUpdate::EgressUpdate(ComponentId_t id, Params& params) : Component(id)
{

    // Number of ports
    num_ports                 = params.find<uint32_t>("num_ports", 52);
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":EgressUpdate[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Egress Update --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Ports: %d ports\n", num_ports);

    base_tc = registerTimeBase("1ps", true);

    // Configure out link
    packet_buffer = configureLink("packet_buffer", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "packet_buffer link was configured\n");

    // Configure in links
    Event::Handler<EgressUpdate>* ports_handler =
        new Event::Handler<EgressUpdate>(this, &EgressUpdate::handlePktUpdate);
    for ( int i = 0; i < num_ports; i++ ) {
        std::string e_port = "egress_port_" + std::to_string(i);
        egress_ports[i]    = configureLink(e_port, base_tc, ports_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "egress_port_%d link was configured\n", i);

        assert(egress_ports[i]);
    }
    // Verify that links are not NULL
    assert(packet_buffer);

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Handles incoming packet to the packet buffer
 *
 * @param ev Event with new packet transmitted
 */
void
EgressUpdate::updatePktBuffer(SST::Event* ev)
{
    packet_buffer->send(ev);
}

/**
 * @brief Handles outcoming packet to the packet buffer
 *
 * @param ev Event with new packet transmitted
 */
void
EgressUpdate::handlePktUpdate(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    out->verbose(
        CALL_INFO, INFO, 0,
        "Packet update event received for packet with pkt_id = %u that is leaving egress_port[%d]. Passing transaction "
        "to packet_buffer.\n",
        pkt->pkt_id, pkt->dest_port);

    safe_lock.lock();
    updatePktBuffer(ev);
    safe_lock.unlock();
}

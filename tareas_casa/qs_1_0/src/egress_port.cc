#include <sst/core/sst_config.h>

#include "egress_port.h"

#include "sst/core/event.h"

#include "packet_request_event.h"

#include <assert.h>
#include <math.h>

using namespace SST;

/**
 * @brief Construct a new EgressPort::EgressPort object
 *
 * @param id
 * @param params
 */
EgressPort::EgressPort(ComponentId_t id, Params& params) : Component(id)
{
    // Number of nodes
    num_nodes                 = params.find<int>("num_nodes", 1);
    // Number of ports
    num_ports                 = params.find<int>("num_ports", 52);
    // Node number
    node                      = params.find<uint32_t>("node", 0);
    // Port number
    port                      = params.find<uint32_t>("port", 0);
    // Port's bandwidth
    port_bw                   = params.find<UnitAlgebra>("port_bw", "5Gb/s");
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":EgressPort[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Egress Port --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Node: %d\n", node);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port: %d\n", port);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Bandwidth: %s\n", port_bw.toString().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of nodes: %d\n", num_nodes);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of ports: %d\n", num_ports);

    base_tc = registerTimeBase("1ps", true);

    sent_pkts = 0;

    // Configure in links
    scheduler = configureLink("scheduler", base_tc, new Event::Handler<EgressPort>(this, &EgressPort::handle_new_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "scheduler link was configured\n");

    output = configureLink("output", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "output link was configured\n");

    packet_buffer = configureLink("packet_buffer", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "packet_buffer link was configured\n");

    // Configure self links
    // if (port == 0 || port == 8 || port == 16 || port == 24 || port == 32 ||
    // port == 40 || port == 48 || port == 51)
    timing_link =
        configureSelfLink("timing_link", base_tc, new Event::Handler<EgressPort>(this, &EgressPort::output_timing));
    out->verbose(CALL_INFO, MODERATE, 0, "the timing_link selflink was configured\n");

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");

    // Create statistics
    priorities       = registerStatistic<uint32_t>("priorities", "1");
    bytes_sent       = registerStatistic<uint32_t>("bytes_sent", "1");
    sent_packets     = registerStatistic<uint32_t>("sent_packets", "1");
    current_priority = registerStatistic<uint32_t>("current_priority", "1");

    // Bytes sent by source port
    bytes_sent_by_port = new Statistic<uint32_t>*[num_ports];
    for ( int i = 0; i < num_ports; i++ ) {
        std::string port_name = "port_" + std::to_string(i);
        bytes_sent_by_port[i] = registerStatistic<uint32_t>("bytes_sent_by_port", port_name);
    }

    // Bytes sent by priority
    bytes_sent_by_priority = new Statistic<uint32_t>*[NUM_PRI];
    for ( int i = 0; i < NUM_PRI; i++ ) {
        std::string pri_name      = "pri_" + std::to_string(i);
        bytes_sent_by_priority[i] = registerStatistic<uint32_t>("bytes_sent_by_priority", pri_name);
    }
}

/**
 * @brief Handler to keep time count after a pkt is sent
 * <REVIEW> will the bandwidht keep if it was not line rate?
 * check SST elements
 *
 * @param ev Event that always will be null
 */
void
EgressPort::output_timing(Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    // Statistics collection
    priorities->addData(pkt->priority);

    uint32_t bytes_to_send = pkt->pkt_size + IPG_SIZE;
    bytes_sent->addData(bytes_to_send);
    bytes_sent_by_port[pkt->src_port]->addData(bytes_to_send);
    bytes_sent_by_priority[pkt->priority]->addData(bytes_to_send);

    sent_packets->addData(1);
    current_priority->addData(pkt->priority);

    sent_pkts++;

    out->verbose(
        CALL_INFO, INFO, 0,
        "Sending packet out of the system: pkt_id = %u, size = %dB, destination node = %d, destination port = %d, "
        "priority = %d\n",
        pkt->pkt_id, pkt->pkt_size, pkt->dest_node, pkt->dest_port, pkt->priority);

    // Request a new packet to the Scheduler
    scheduler->send(NULL);
    out->verbose(CALL_INFO, INFO, 0, "Requesting a new packet to the scheduler for port %d\n", pkt->dest_port);
    // Sending packet
    output->send(pkt);
    packet_buffer->send(pkt->clone());

    out->verbose(
        CALL_INFO, MODERATE, 0,
        "Packet sent: pkt_id = %u, size = %dB, destination node = %d, destination port = %d, priority = %d\n",
        pkt->pkt_id, pkt->pkt_size, pkt->dest_node, pkt->dest_port, pkt->priority);
}

/**
 * @brief Handler for new packets that will send the packet and call the timing
 * link
 *
 * @param ev Event with new packet
 */
void
EgressPort::handle_new_pkt(SST::Event* ev)
{

    if ( ev == NULL ) { scheduler->send(NULL); }
    else {
        PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
        if ( pkt ) {
            // Compute delay to send packet
            UnitAlgebra total_bits = UnitAlgebra(std::to_string((pkt->pkt_size + IPG_SIZE) * BITS_IN_BYTE) + "b");
            UnitAlgebra interval   = (total_bits / port_bw) / UnitAlgebra("1ps");
            send_delay             = interval.getRoundedValue();

            out->verbose(
                CALL_INFO, INFO, 0,
                "New packet received from the scheduler: pkt_id = %u, size = %dB, destination node = %d, destination "
                "port = %d, priority = %d.\n",
                pkt->pkt_id, pkt->pkt_size, pkt->dest_node, pkt->dest_port, pkt->priority);

            // Schedule packet
            timing_link->send(send_delay, pkt);
            out->verbose(CALL_INFO, INFO, 0, "Packet scheduled to be sent with a delay of %d ps.\n", send_delay);
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
EgressPort::finish()
{
    out->verbose(CALL_INFO, INFO, 0, "Packet sent from egress_port_%d: %d packets\n", port, sent_pkts);
}

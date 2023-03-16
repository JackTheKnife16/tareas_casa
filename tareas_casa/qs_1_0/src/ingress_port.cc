#include <sst/core/sst_config.h>

#include "ingress_port.h"

#include "sst/core/event.h"

#include "packet_event.h"
#include "packet_request_event.h"

#include <assert.h>
#include <math.h>

using namespace SST;

/**
 * @brief Construct a new IngressPort::IngressPort object
 *
 * @param id
 * @param params
 */
IngressPort::IngressPort(ComponentId_t id, Params& params) : Component(id)
{
    // Node number
    node                      = params.find<uint32_t>("node", 0);
    // Port number
    port                      = params.find<uint32_t>("port", 0);
    // Port's bandwidth
    port_bw                   = params.find<UnitAlgebra>("port_bw", "5Gb/s");
    // Offered load
    offered_load              = params.find<float>("offered_load", 100.00) / 100;
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":IngressPort[@p:@l]: ";
    std::string log_file = "logs/" + getName() + ".log";
    out                  = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    // Start delay
    std::string start_delay_s = params.find<std::string>("start_delay", "0s");
    UnitAlgebra start_delay_unit(start_delay_s);
    if ( !start_delay_unit.hasUnits("s") ) {
        out->fatal(CALL_INFO, -1, "start_delay must be specified in units of s\n");
    }
    start_delay = (start_delay_unit / UnitAlgebra("1ps")).getRoundedValue();

    request_pkt = true;
    out->verbose(CALL_INFO, MODERATE, 0, "-- Ingress Port --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Node: %d\n", node);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port: %d\n", port);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Bandwidth: %s\n", port_bw.toString().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Start Delay: %lu ps\n", start_delay);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Offered Load: %f \n", offered_load);

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    traffic_generator = configureLink(
        "traffic_generator", base_tc, new Event::Handler<IngressPort>(this, &IngressPort::handle_new_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "traffic_generator link was configured\n");

    acceptance_checker = configureLink("acceptance_checker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "acceptance_checker link was configured\n");

    timing_link =
        configureSelfLink("timing_link", base_tc, new Event::Handler<IngressPort>(this, &IngressPort::output_timing));
    out->verbose(CALL_INFO, MODERATE, 0, "timing_link selflink was configured\n");

    // Check links
    assert(traffic_generator);
    assert(acceptance_checker);
    assert(timing_link);

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");

    // Create statistics
    priorities   = registerStatistic<uint32_t>("priorities", "1");
    bytes_sent   = registerStatistic<uint32_t>("bytes_sent", "1");
    sent_packets = registerStatistic<uint32_t>("sent_packets", "1");
}

/**
 * @brief SST init phase method
 *
 * @param phase Current init phase
 */
void
IngressPort::init(unsigned int phase)
{
    if ( phase == 1 ) {
        for ( int i = 0; i < INIT_PKTS; i++ ) {
            Event*       ev  = traffic_generator->recvInitData();
            PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
            if ( pkt ) {
                // Collect data for first 5 pkts
                priorities->addData(pkt->priority);

                packets_to_send.push(pkt);
                out->verbose(
                    CALL_INFO, INFO, 0,
                    "Received initial packet from traffic_generator and added it to the queue. Queue size: %d, packet "
                    "id: %u, destination port: %d, priority: %d\n",
                    packets_to_send.size(), pkt->pkt_id, pkt->dest_port, pkt->priority);
            }
            else {
                break;
            }
        }
    }
}

/**
 * @brief Set up stage of SST
 *
 */
void
IngressPort::setup()
{
    if ( !packets_to_send.empty() ) {
        // Retreive pkt from list
        PacketEvent* pkt = packets_to_send.front();
        packets_to_send.pop();
        out->verbose(
            CALL_INFO, INFO, 0,
            "Pulling init packet from queue to send it to acceptance_checker, current size of queue: %d, pkt_id: %u\n",
            packets_to_send.size(), pkt->pkt_id);

        UnitAlgebra total_bits = UnitAlgebra(std::to_string((pkt->pkt_size + IPG_SIZE) * BITS_IN_BYTE) + "b");
        UnitAlgebra interval   = (total_bits / (port_bw * offered_load)) / UnitAlgebra("1ps");
        send_delay             = interval.getRoundedValue();

        // Start counting delays
        out->verbose(
            CALL_INFO, MODERATE, 0,
            "Waiting delay time to send init pkt to acceptance_checker, delay time = %d ps, pkt_id = %u\n",
            start_delay + send_delay, pkt->pkt_id);

        timing_link->send(start_delay + send_delay, pkt);

        PacketRequestEvent* req = new PacketRequestEvent();
        req->node_request       = node;
        req->port_request       = port;
        out->verbose(
            CALL_INFO, INFO, 0, "ingress_port[%d][%d] requesting a new pkt to traffic_generator\n", node, port);

        traffic_generator->send(req);
    }
}

/**
 * @brief Handler to keep time count after a pkt is sent
 *
 * @param ev Event that will be always null
 */
void
IngressPort::output_timing(Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    // Collect bytes sent
    bytes_sent->addData(pkt->pkt_size + IPG_SIZE);
    sent_packets->addData(1);

    // Set the source port
    pkt->src_node = node;
    pkt->src_port = port;

    out->verbose(
        CALL_INFO, INFO, 0,
        "Sending %spacket to acceptance_checker: pkt_id = %u, dest node= %d, dest port = %d, size = %d, priority = "
        "%d\n",
        last_pkt.c_str(), pkt->pkt_id, pkt->dest_node, pkt->dest_port, pkt->pkt_size, pkt->priority);
    acceptance_checker->send(pkt);

    if ( !packets_to_send.empty() ) {
        safe_lock.lock();
        PacketEvent* pkt_to_send = packets_to_send.front();
        packets_to_send.pop();

        out->verbose(
            CALL_INFO, MODERATE, 0,
            "Pulling packet from queue to send it to acceptance_checker, current size of queue: %d, pkt_id: %u\n",
            packets_to_send.size(), pkt_to_send->pkt_id);

        safe_lock.unlock();

        UnitAlgebra total_bits = UnitAlgebra(std::to_string((pkt_to_send->pkt_size + IPG_SIZE) * BITS_IN_BYTE) + "b");
        UnitAlgebra interval   = (total_bits / (port_bw * offered_load)) / UnitAlgebra("1ps");
        send_delay             = interval.getRoundedValue();
        out->verbose(
            CALL_INFO, MODERATE, 0,
            "Waiting delay time to send pkt to acceptance_checker, delay time = %d ps, pkt_id = %u\n", send_delay,
            pkt_to_send->pkt_id);

        timing_link->send(send_delay, pkt_to_send);
    }

    // Request pkt to packet generator
    if ( request_pkt ) {
        PacketRequestEvent* req = new PacketRequestEvent();
        req->node_request       = node;
        req->port_request       = port;
        out->verbose(
            CALL_INFO, INFO, 0, "ingress_port[%d][%d] requesting a new pkt to traffic_generator\n", node, port);

        traffic_generator->send(req);
    }
}

/**
 * @brief Handler for new packets
 *
 * @param ev Envent with the new packet
 */
void
IngressPort::handle_new_pkt(SST::Event* ev)
{
    // If event is NULL, there are not more pkts to send
    if ( ev == NULL ) {
        request_pkt = false;
        out->verbose(CALL_INFO, INFO, 0, "No more packets to send, Node %d and Port %d ran out of pkts!\n", node, port);
        last_pkt += "last ";
    }
    else {
        PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
        if ( pkt ) {
            safe_lock.lock();
            // Statistics collection
            priorities->addData(pkt->priority);
            packets_to_send.push(pkt);

            out->verbose(
                CALL_INFO, INFO, 0,
                "Received a new packet from the traffic generator: Packet ID = %u, Packet Size = %d bytes, Destination "
                "Port = %d, Priority = %d\n",
                pkt->pkt_id, pkt->pkt_size, pkt->dest_port, pkt->priority);

            out->verbose(
                CALL_INFO, INFO, 0, "The packet was added to the send queue. The current queue size is %d\n",
                packets_to_send.size());
            safe_lock.unlock();
        }
        else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
        }
    }
}

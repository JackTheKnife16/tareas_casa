#include <sst/core/sst_config.h>

#include "acceptance_checker.h"

#include "sst/core/event.h"
#include "sst/core/rng/mersenne.h"

#include "packet_event.h"
#include "packet_util_event.h"
#include "util_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>
#include <math.h>

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

    num_ports                    = params.find<uint32_t>("num_ports", 52);
    // MA config file
    std::string   ma_config_path = params.find<std::string>("ma_config_file", "qs_1_0/ma_config.json");
    JSON          ma_config;
    std::ifstream ma_config_file(ma_config_path);
    ma_config_file >> ma_config;
    // Map pq --> mg
    int pq_maps = ma_config["Egress_PQ"]["Mapping"].size();
    for ( int i = 0; i < pq_maps; i++ ) {
        int queue    = ma_config["Egress_PQ"]["Mapping"][i]["Queue"];
        int mg_index = ma_config["Egress_PQ"]["Mapping"][i]["MG_parent"];

        pq_mg_map[queue] = mg_index;
    }
    // Memory groups utilization
    int mg_elements = ma_config["Egress_MG"]["Mapping"].size();
    for ( int i = 0; i < mg_elements; i++ ) {
        int mg_index        = ma_config["Egress_MG"]["Mapping"][i]["MG"];
        // Init utilization in 0%
        mg_values[mg_index] = 0;
    }
    // Queue groups utilization
    for ( int i = 0; i < num_ports; i++ ) {
        // Init utilization in 0%
        qg_values[i] = 0;
    }
    // Physical queue utilization --> 52 port and 8 priority queues
    for ( int i = 0; i < num_ports; i++ ) {
        for ( size_t j = 0; j <= HIGHEST_PRI_Q; j++ ) {
            // Init utilization in 0%
            pq_index_t pq_index;
            pq_index.index_s.port       = i;
            pq_index.index_s.priority   = j;
            pq_values[pq_index.index_i] = 0;
        }
    }

    // Verbosity
    const int verbose = params.find<int>("verbose", 0);

    pq_util_value = -1;

    std::stringstream prefix;
    prefix << "@t:" << getName() << ":AcceptanceChecker[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);
    out->verbose(CALL_INFO, MODERATE, 0, "-- Acceptance Checker --\n");
    out->verbose(CALL_INFO, DEBUG, 0, "|--> MA config: %s\n", ma_config_path.c_str());

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    mg_utilization = configureLink(
        "mg_utilization", base_tc, new Event::Handler<AcceptanceChecker>(this, &AcceptanceChecker::handle_update_mg));
    out->verbose(CALL_INFO, MODERATE, 0, "mg_utilization link was configured\n");
    qg_utilization = configureLink(
        "qg_utilization", base_tc, new Event::Handler<AcceptanceChecker>(this, &AcceptanceChecker::handle_update_qg));
    out->verbose(CALL_INFO, MODERATE, 0, "qg_utilization link was configured\n");
    pq_utilization = configureLink(
        "pq_utilization", base_tc, new Event::Handler<AcceptanceChecker>(this, &AcceptanceChecker::handle_update_pq));
    out->verbose(CALL_INFO, MODERATE, 0, "pq_utilization link was configured\n");
    wred = configureLink("wred", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "wred link was configured\n");

    // Configure ingress port links
    Event::Handler<AcceptanceChecker>* ports_handler =
        new Event::Handler<AcceptanceChecker>(this, &AcceptanceChecker::handle_new_pkt);
    for ( int i = 0; i < num_ports; i++ ) {
        std::string e_port = "ingress_port_" + std::to_string(i);
        ingress_ports[i]   = configureLink(e_port, base_tc, ports_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "ingress_port_%d link was configured\n", i);
    }

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}
/**
 * @brief Handler for memory groups updates
 *
 * @param ev Event with MG utilization update
 */
void
AcceptanceChecker::handle_update_mg(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_lock.lock();
        mg_values[util->element_index] = util->utilization;

        out->verbose(CALL_INFO, DEBUG, 0, "Received update event from mg_utilization link.\n");
        out->verbose(CALL_INFO, DEBUG, 0, "Updated mg_values[%d] to %d.\n", util->element_index, util->utilization);

        safe_lock.unlock();
        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for queue groups updates
 *
 * @param ev Event with QG utilization update
 */
void
AcceptanceChecker::handle_update_qg(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_lock.lock();
        qg_values[util->element_index] = util->utilization;

        out->verbose(CALL_INFO, DEBUG, 0, "Received update event from qg_utilization link.\n");
        out->verbose(CALL_INFO, DEBUG, 0, "Updated qg_values[%d] to %d.\n", util->element_index, util->utilization);

        safe_lock.unlock();
        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for physical queue updates
 *
 * @param ev Event with PQ utilization update
 */
void
AcceptanceChecker::handle_update_pq(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_lock.lock();
        pq_values[util->element_index] = util->utilization;
        pq_index_t value_v;
        value_v.index_i = util->element_index;
        out->verbose(CALL_INFO, DEBUG, 0, "Received update event from pq_utilization link.\n");
        out->verbose(
            CALL_INFO, DEBUG, 0, "Updated pq_values[%d,%d] to %d.\n", value_v.index_s.port, value_v.index_s.priority,
            util->utilization);

        safe_lock.unlock();
        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Send a packet event to the WRED link
 *
 * @param pkt Packet Util event
 */
void
AcceptanceChecker::send_packet(PacketUtilEvent* pkt)
{
    out->verbose(
        CALL_INFO, INFO, 0,
        "Sending a PacketUtilEvent through the WRED link. "
        "This event contains the original packet information and additional data "
        "on the mg, qg, and pq utilization metrics. The packet ID is: %u\n",
        pkt->pkt_id);
    wred->send(pkt);
}

/**
 * @brief Handler for new packet from the ingress ports
 *
 * @param ev Event with the packet
 */
void
AcceptanceChecker::handle_new_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        std::string is_tcp = "Is the new packet TCP?";
        if ( pkt->is_tcp ) { is_tcp += " Yes"; }
        else {
            is_tcp += " No";
        }

        // Added verbose output to describe the received packet
        out->verbose(
            CALL_INFO, INFO, 0,
            "A new packet was received through ingress_port_%d link, "
            "packet ID: %u, packet size: %d B\n",
            pkt->src_port, pkt->pkt_id, pkt->pkt_size);

        out->verbose(CALL_INFO, MODERATE, 0, "%s\n", is_tcp.c_str());

        // Extract packet info
        unsigned int pkt_id    = pkt->pkt_id;
        unsigned int pkt_size  = pkt->pkt_size;
        unsigned int src_port  = pkt->src_port;
        unsigned int dest_port = pkt->dest_port;
        unsigned int priority  = pkt->priority;
        // Get indexes for each accoutin element
        pq_index_t   pq;
        pq.index_s.port       = dest_port;
        pq.index_s.priority   = priority;
        unsigned int pq_index = pq.index_i;
        // Map mg_index
        int          mg_index = get_mg_index(pq);
        out->verbose(CALL_INFO, DEBUG, 0, "mg_index = %d, for packet with pkt_id = %u\n", mg_index, pkt_id);

        // Create event with packet and utilization
        PacketUtilEvent* pkt_util;
        // If the event is TCP we need to forward the TCP data
        if ( !pkt->is_tcp ) {
            // Not a TCP packet, using normal Packet Util event
            out->verbose(CALL_INFO, MODERATE, 0, "Creating a new PacketUtilEvent\n");
            pkt_util = new PacketUtilEvent();
        }
        else {
            // Create TCP event to save the TCP data
            out->verbose(CALL_INFO, MODERATE, 0, "Creating a new TCPPacketUtilEvent\n");
            TCPPacketUtilEvent* tcp_pkt_util = new TCPPacketUtilEvent();
            TCPPacketEvent*     tcp_pkt      = dynamic_cast<TCPPacketEvent*>(ev);
            if ( tcp_pkt ) {
                // Set TCP data
                tcp_pkt_util->src_tcp_port  = tcp_pkt->src_tcp_port;
                tcp_pkt_util->dest_tcp_port = tcp_pkt->dest_tcp_port;
                tcp_pkt_util->ack_number    = tcp_pkt->ack_number;
                tcp_pkt_util->seq_number    = tcp_pkt->seq_number;
                tcp_pkt_util->flags         = tcp_pkt->flags;
                tcp_pkt_util->window_size   = tcp_pkt->window_size;
                tcp_pkt_util->src_ip_addr   = tcp_pkt->src_ip_addr;
                tcp_pkt_util->dest_ip_addr  = tcp_pkt->dest_ip_addr;
                tcp_pkt_util->src_mac_addr  = tcp_pkt->src_mac_addr;
                tcp_pkt_util->dest_mac_addr = tcp_pkt->dest_mac_addr;
                tcp_pkt_util->length_type   = tcp_pkt->length_type;
            }
            else {
                out->fatal(CALL_INFO, -1, "Can't cast event into TCPPacketEvent!\n");
            }
            // Set tcp packet util event
            pkt_util = tcp_pkt_util;
        }
        // Set basic packet util data
        pkt_util->pkt_id    = pkt_id;
        pkt_util->pkt_size  = pkt_size;
        pkt_util->src_port  = src_port;
        pkt_util->dest_port = dest_port;
        pkt_util->priority  = priority;
        pkt_util->ip_ecn    = pkt->ip_ecn;
        pkt_util->pq_index  = pq_index;
        pkt_util->qg_index  = dest_port;
        pkt_util->mg_index  = mg_index;

        safe_lock.lock();
        // Assign utilizations
        out->verbose(CALL_INFO, DEBUG, 0, "Assigning MG Utilization to PacketUtilEvent with packet ID = %u\n", pkt_id);
        pkt_util->mg_util = fetch_mg_util(mg_index);

        out->verbose(CALL_INFO, DEBUG, 0, "Assigning QG Utilization to PacketUtilEvent with packet ID = %u\n", pkt_id);
        pkt_util->qg_util = fetch_qg_util(dest_port);

        out->verbose(CALL_INFO, DEBUG, 0, "Assigning PQ Utilization to PacketUtilEvent with packet ID = %u\n", pkt_id);
        pkt_util->pq_util = fetch_pq_util(pq);

        out->verbose(
            CALL_INFO, DEBUG, 0,
            "Prepared to transmit a new PacketUtilEvent through WRED link: "
            "packet ID = %u, size = %dB, destination = %d, priority = %d, "
            "mg = %d, qg = %d, pq = %d\n",
            pkt_util->pkt_id, pkt_util->pkt_size, pkt_util->dest_port, pkt_util->priority, pkt_util->mg_util,
            pkt_util->qg_util, pkt_util->pq_util);
        // Send packet
        send_packet(pkt_util);
        safe_lock.unlock();
        // Release memory
        delete pkt;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Get MG index
 *
 * @param pq PQ index
 * @return int MG index
 */
int
AcceptanceChecker::get_mg_index(pq_index_t pq)
{
    int queue    = pq.index_s.priority;
    int mg_index = pq_mg_map[queue];
    out->verbose(
        CALL_INFO, DEBUG, 0, "Obtaining mg_index for the given PQ index: mg_index = %d, priority queue = %d\n",
        mg_index, queue);

    return mg_index;
}

/**
 * @brief Fetch the MG utilization from the MG map
 *
 * @param mg_index MG index
 * @return int MG utilization
 */
int
AcceptanceChecker::fetch_mg_util(int mg_index)
{
    // Get current utilization
    int mg_utilization = mg_values[mg_index];
    out->verbose(
        CALL_INFO, DEBUG, 0, "Fetching MG utilization for the given mg_index: mg_utilization = %d, mg_index = %d\n",
        mg_utilization, mg_index);

    return mg_utilization;
}

/**
 * @brief Fetch the QG utilization from the QG map
 *
 * @param port QG index which is the port
 * @return int QG utilization
 */
int
AcceptanceChecker::fetch_qg_util(int port)
{
    // Get current utilization
    int qg_utilization = qg_values[port];
    out->verbose(
        CALL_INFO, DEBUG, 0, "Fetching QG utilization for the given port: qg_utilization = %d, port = %d\n",
        qg_utilization, port);

    return qg_utilization;
}

/**
 * @brief Fetch the PQ utilization from the PQ map
 *
 * @param pq PQ index
 * @return int PQ utilization
 */
int
AcceptanceChecker::fetch_pq_util(pq_index_t pq)
{
    // Get current utilization
    int pq_utilization = pq_values[pq.index_i];
    out->verbose(
        CALL_INFO, DEBUG, 0,
        "Fetching PQ utilization for the given port and priority: pq_utilization = %d, port = %d, priority = %d\n",
        pq_utilization, pq.index_s.port, pq.index_s.priority);

    return pq_utilization;
}

#include <sst/core/sst_config.h>

#include "dualq_acceptance_checker.h"

#include "sst/core/event.h"

#include "packet_event.h"
#include "packet_util_event.h"
#include "util_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

DualQAcceptanceChecker::DualQAcceptanceChecker(ComponentId_t id, Params& params) : AcceptanceChecker(id, params)
{
    // L4S queue utilization
    for ( int i = 0; i < num_ports; i++ ) {
        // Init utilization in 0%
        lq_values[i] = 0;
    }

    // Configure lq link
    lq_utilization = configureLink(
        "lq_utilization", base_tc,
        new Event::Handler<DualQAcceptanceChecker>(this, &DualQAcceptanceChecker::handle_update_lq));
    out->verbose(CALL_INFO, MODERATE, 0, "lq_utilization link was configured\n");
    // Configure ingress port links
    Event::Handler<DualQAcceptanceChecker>* ports_handler =
        new Event::Handler<DualQAcceptanceChecker>(this, &DualQAcceptanceChecker::handle_new_pkt);
    for ( int i = 0; i < ingress_ports.size(); i++ ) {
        std::string e_port = "ingress_port_" + std::to_string(i);
        ingress_ports[i]   = configureLink(e_port, base_tc, ports_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "ingress_port_%d link was configured\n", i);
    }
}

void
DualQAcceptanceChecker::handle_update_lq(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        out->verbose(CALL_INFO, INFO, 0, "New LQ update: lq = %d, util = %d\n", util->element_index, util->utilization);
        safe_lock.lock();
        lq_values[util->element_index] = util->utilization;
        safe_lock.unlock();
        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

int
DualQAcceptanceChecker::fetch_lq_util(int port)
{
    // Get current utilization
    int lq_utilization = lq_values[port];
    out->verbose(CALL_INFO, DEBUG, 0, "lq_utilization for port %d: %d\n", port, lq_utilization);

    return lq_utilization;
}

void
DualQAcceptanceChecker::handle_new_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        out->verbose(CALL_INFO, INFO, 0, "New packet is tcp? %d\n", pkt->is_tcp);
        // Extract packet info
        unsigned int pkt_id    = pkt->pkt_id;
        unsigned int pkt_size  = pkt->pkt_size;
        unsigned int src_port  = pkt->src_port;
        unsigned int dest_port = pkt->dest_port;
        unsigned int priority  = pkt->priority;
        // Get indexes for each accoutin element
        pq_index_t   pq;
        pq.index_s.port                = dest_port;
        pq.index_s.priority            = priority;
        unsigned int          pq_index = pq.index_i;
        // Map mg_index
        int                   mg_index = get_mg_index(pq);
        // Create event with packet and utilization
        DualQPacketUtilEvent* pkt_util;
        // If the event is TCP we need to forward the TCP data
        if ( !pkt->is_tcp ) {
            // Not a TCP packet, using normal Packet Util event
            pkt_util = new DualQPacketUtilEvent();
        }
        else {
            // Create TCP event to save the TCP data
            DualQTCPPacketUtilEvent* tcp_pkt_util = new DualQTCPPacketUtilEvent();
            TCPPacketEvent*          tcp_pkt      = dynamic_cast<TCPPacketEvent*>(ev);
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
        pkt_util->lq_index  = dest_port;

        safe_lock.lock();
        // Assign utilizations
        pkt_util->mg_util = fetch_mg_util(mg_index);
        pkt_util->qg_util = fetch_qg_util(dest_port);
        pkt_util->pq_util = fetch_pq_util(pq);
        pkt_util->lq_util = fetch_lq_util(dest_port);

        out->verbose(
            CALL_INFO, 1, 0,
            "Sending pkt util: size = %dB, dest = %d, pri = %d, mg = %d, "
            "qg = %d, pq = %d, pkt_id = %u\n",
            pkt_util->pkt_size, pkt_util->dest_port, pkt_util->priority, pkt_util->mg_util, pkt_util->qg_util,
            pkt_util->pq_util, pkt_util->pkt_id);
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

// Version 2
// ------------------------------------------------------------------------------------------------------------------------------------------

DualQAcceptanceCheckerV2::DualQAcceptanceCheckerV2(ComponentId_t id, Params& params) : AcceptanceChecker(id, params)
{
    // L4S queue utilization
    for ( int i = 0; i < num_ports; i++ ) {
        // Init utilization in 0%
        lq_values[i] = 0;
    }

    // Configure links
    lq_utilization = configureLink(
        "lq_utilization", base_tc,
        new Event::Handler<DualQAcceptanceCheckerV2>(this, &DualQAcceptanceCheckerV2::handle_update_lq));
    out->verbose(CALL_INFO, MODERATE, 0, "lq_utilization link was configured\n");
    l4s_wred = configureLink("l4s_wred", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "l4s_wred link was configured\n");
    // Configure ingress port links
    Event::Handler<DualQAcceptanceCheckerV2>* fifos_handler =
        new Event::Handler<DualQAcceptanceCheckerV2>(this, &DualQAcceptanceCheckerV2::handle_new_pkt);
    Event::Handler<DualQAcceptanceCheckerV2>* l_fifos_handler =
        new Event::Handler<DualQAcceptanceCheckerV2>(this, &DualQAcceptanceCheckerV2::handle_new_l_pkt);
    for ( int i = 0; i < ingress_ports.size(); i++ ) {
        std::string i_port = std::to_string(i);

        ingress_ports[i] = configureLink("c_fifo_" + i_port, base_tc, fifos_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "c_fifo_%d link was configured\n", i);
        l_fifos[i] = configureLink("l_fifo_" + i_port, base_tc, l_fifos_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "l_fifo_%d link was configured\n", i);
    }
}

void
DualQAcceptanceCheckerV2::handle_update_lq(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_lock.lock();
        lq_values[util->element_index] = util->utilization;
        out->verbose(CALL_INFO, DEBUG, 0, "Updated lq_values[%d] = %d\n", util->element_index, util->utilization);
        safe_lock.unlock();
        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

int
DualQAcceptanceCheckerV2::fetch_lq_util(int port)
{
    // Get current utilization
    int lq_utilization = lq_values[port];
    out->verbose(CALL_INFO, DEBUG, 0, "Getting lq_utilization = %d, port = %d\n", lq_utilization, port);

    return lq_utilization;
}

void
DualQAcceptanceCheckerV2::handle_new_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        out->verbose(CALL_INFO, INFO, 0, "New packet is tcp? %d\n", pkt->is_tcp);
        // Extract packet info
        unsigned int pkt_id    = pkt->pkt_id;
        unsigned int pkt_size  = pkt->pkt_size;
        unsigned int src_port  = pkt->src_port;
        unsigned int dest_port = pkt->dest_port;
        unsigned int priority  = pkt->priority;
        // Get indexes for each accoutin element
        pq_index_t   pq;
        pq.index_s.port                = dest_port;
        pq.index_s.priority            = priority;
        unsigned int          pq_index = pq.index_i;
        // Map mg_index
        int                   mg_index = get_mg_index(pq);
        // Create event with packet and utilization
        DualQPacketUtilEvent* pkt_util;
        // If the event is TCP we need to forward the TCP data
        if ( !pkt->is_tcp ) {
            // Not a TCP packet, using normal Packet Util event
            pkt_util = new DualQPacketUtilEvent();
        }
        else {
            // Create TCP event to save the TCP data
            DualQTCPPacketUtilEvent* tcp_pkt_util = new DualQTCPPacketUtilEvent();
            TCPPacketEvent*          tcp_pkt      = dynamic_cast<TCPPacketEvent*>(ev);
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
        pkt_util->mg_util = fetch_mg_util(mg_index);
        pkt_util->qg_util = fetch_qg_util(dest_port);
        pkt_util->pq_util = fetch_pq_util(pq);

        out->verbose(
            CALL_INFO, INFO, 0,
            "Sending Classic pkt util: size = %dB, dest = %d, pri = %d, "
            "mg = %d, qg = %d, pq = %d, pkt_id = %u\n",
            pkt_util->pkt_size, pkt_util->dest_port, pkt_util->priority, pkt_util->mg_util, pkt_util->qg_util,
            pkt_util->pq_util, pkt_util->pkt_id);
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

void
DualQAcceptanceCheckerV2::handle_new_l_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        // Extract packet info
        unsigned int          pkt_id    = pkt->pkt_id;
        unsigned int          pkt_size  = pkt->pkt_size;
        unsigned int          src_port  = pkt->src_port;
        unsigned int          dest_port = pkt->dest_port;
        unsigned int          priority  = pkt->priority;
        DualQPacketUtilEvent* pkt_util;
        // If the event is TCP we need to forward the TCP data
        if ( !pkt->is_tcp ) {
            // Not a TCP packet, using normal Packet Util event
            pkt_util = new DualQPacketUtilEvent();
        }
        else {
            // Create TCP event to save the TCP data
            DualQTCPPacketUtilEvent* tcp_pkt_util = new DualQTCPPacketUtilEvent();
            TCPPacketEvent*          tcp_pkt      = dynamic_cast<TCPPacketEvent*>(ev);
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
        pkt_util->lq_index  = dest_port;

        safe_lock.lock();
        // Assign utilizations
        pkt_util->lq_util = fetch_lq_util(dest_port);

        out->verbose(
            CALL_INFO, INFO, 0,
            "Sending L4S pkt util: size = %dB, dest = %d, pri = %d, lq = "
            "%d, pkt_id = %u\n",
            pkt_util->pkt_size, pkt_util->dest_port, pkt_util->priority, pkt_util->lq_util, pkt_util->pkt_id);
        // Send packet
        l4s_wred->send(pkt_util);
        safe_lock.unlock();
        // Release memory
        delete pkt;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

#include <sst/core/sst_config.h>

#include "dualq_wred.h"

#include "sst/core/event.h"

#include "dualq_coupling_event.h"
#include "packet_event.h"
#include "packet_util_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>
#include <math.h>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new DualQClassicWRED::DualQClassicWRED object
 *
 * @param id
 * @param params
 */
DualQClassicWRED::DualQClassicWRED(ComponentId_t id, Params& params) : WRED(id, params)
{
    k_proportionality = params.find<float>("k_proportionality", 0);
    model_version     = params.find<uint32_t>("model_version", 1);
    // Configure lq link
    coupling          = configureLink("coupling", base_tc);

    p_C = new Statistic<float>*[num_ports];
    for ( int i = 0; i < num_ports; i++ ) {
        std::string port_name("port_");
        port_name = port_name + std::to_string(i);
        p_C[i]    = registerStatistic<float>("p_C", port_name);
    }
}


/**
 * @brief Handler for new DualQPacketUtilEvent
 *
 * @param ev Event with new packet
 */
void
DualQClassicWRED::handle_new_pkt(SST::Event* ev)
{
    DualQPacketUtilEvent* pkt_util = dynamic_cast<DualQPacketUtilEvent*>(ev);
    if ( pkt_util ) {
        // Create packet
        PacketEvent* pkt;
        if ( !pkt_util->is_tcp ) { pkt = new PacketEvent(); }
        else {
            TCPPacketEvent*          tcp_pkt      = new TCPPacketEvent();
            DualQTCPPacketUtilEvent* tcp_pkt_util = dynamic_cast<DualQTCPPacketUtilEvent*>(ev);
            if ( tcp_pkt_util ) {
                // Set TCP data
                tcp_pkt->src_tcp_port  = tcp_pkt_util->src_tcp_port;
                tcp_pkt->dest_tcp_port = tcp_pkt_util->dest_tcp_port;
                tcp_pkt->ack_number    = tcp_pkt_util->ack_number;
                tcp_pkt->seq_number    = tcp_pkt_util->seq_number;
                tcp_pkt->flags         = tcp_pkt_util->flags;
                tcp_pkt->window_size   = tcp_pkt_util->window_size;
                tcp_pkt->src_ip_addr   = tcp_pkt_util->src_ip_addr;
                tcp_pkt->dest_ip_addr  = tcp_pkt_util->dest_ip_addr;
                tcp_pkt->src_mac_addr  = tcp_pkt_util->src_mac_addr;
                tcp_pkt->dest_mac_addr = tcp_pkt_util->dest_mac_addr;
                tcp_pkt->length_type   = tcp_pkt_util->length_type;
            }
            else {
                out->fatal(CALL_INFO, -1, "Can't cast event into TCPPacketEvent!\n");
            }

            pkt = tcp_pkt;
        }
        pkt->pkt_id    = pkt_util->pkt_id;
        pkt->pkt_size  = pkt_util->pkt_size;
        pkt->src_port  = pkt_util->src_port;
        pkt->dest_port = pkt_util->dest_port;
        pkt->priority  = pkt_util->priority;
        pkt->ip_ecn    = pkt_util->ip_ecn;

        out->verbose(
            CALL_INFO, MODERATE, 0,
            "Getting pkt util: id = %u, size = %dB, dest = %d, pri = %d, "
            "mg = %d, qg = %d, pq = %d\n",
            pkt_util->pkt_id, pkt_util->pkt_size, pkt_util->dest_port, pkt_util->priority, pkt_util->mg_util,
            pkt_util->qg_util, pkt_util->pq_util);

        float drop_marking_prob = 0.00;

        safe_lock.lock();

        // Computing p'
        bool               ect = false;
        ecn_wred_profile_t current_ecn_profile;
        if ( is_ecn_capable(pkt) && enable_ecn ) {
            ect                 = true;
            current_ecn_profile = ecn_pq_profiles[pkt->priority];
            drop_marking_prob   = compute_ecn_marking_prob(pkt_util->pq_util, current_ecn_profile);
            // If packet is below drop point
            if ( pkt_util->pq_util >= current_ecn_profile.ecn_drop_point ) { drop_marking_prob = HIGHEST_UTIL; }
        }
        else {
            DualQPacketUtilEvent* dualq_pkt = new DualQPacketUtilEvent();
            dualq_pkt->mg_index             = pkt_util->mg_index;
            dualq_pkt->mg_util              = pkt_util->mg_util;
            dualq_pkt->qg_index             = pkt_util->qg_index;
            dualq_pkt->qg_util              = pkt_util->qg_util;
            dualq_pkt->pq_index             = pkt_util->pq_index;
            dualq_pkt->pq_util              = pkt_util->pq_util;
            drop_marking_prob               = compute_drop_prob(dualq_pkt);

            delete dualq_pkt;
        }

        // Computing p_CL
        float coupling_probability = drop_marking_prob * k_proportionality;
        if ( coupling_probability > HIGHEST_UTIL ) { coupling_probability = HIGHEST_UTIL; }

        // Sending p_CL to L4S WRED
        CouplingEvent* coupling_event        = new CouplingEvent();
        coupling_event->lq_index             = pkt_util->qg_index;
        coupling_event->coupling_probability = coupling_probability;
        coupling->send(coupling_event);

        // Computing p_C
        drop_marking_prob = pow(drop_marking_prob, 2) / HIGHEST_UTIL;

        // Collecting the stats
        p_C[pkt_util->qg_index]->addData(drop_marking_prob);

        bool pkt_accepted;
        // If the packet is ECN capable
        if ( ect ) {
            // If the PQ utilization is lower or equal than the stating point
            // the packet will be accepted, otherwise, check if it is marked or
            // dropped
            if ( pkt_util->pq_util <= current_ecn_profile.wred_profile.starting_point ) {
                out->verbose(CALL_INFO, INFO, 0, "Packet is accepted directly, pkt_id = %u\n", pkt_util->pkt_id);
                pkt_accepted = true;
            }
            else {
                out->verbose(CALL_INFO, INFO, 0, "Checking if packet will be marked or dropped\n");
                // Check if the packet is accepted, marked (in this case, it is accepted
                // as well), or dropped
                pkt_accepted = ecn_check(pkt, pkt_util->pq_util, drop_marking_prob, current_ecn_profile.ecn_drop_point);
            }
        }
        else {
            out->verbose(CALL_INFO, MODERATE, 0, "Drop probability: %f\n", drop_marking_prob);
            // Check if pkt is accepted
            pkt_accepted = accept_pkt(drop_marking_prob);
        }

        if ( pkt_accepted ) {
            out->verbose(CALL_INFO, INFO, 0, "Packet Accepted ipecn = %d, pkt_id = %u\n", pkt->ip_ecn, pkt->pkt_id);
            // Sending to port fifos
            port_fifos[pkt->dest_port]->send(pkt);
            // If model version is 1, we must update the bytes tracker and packet
            // buffer
            if ( model_version == 1 ) {
                // Sending to bytes tracker
                bytes_tracker->send(pkt->clone());
                // Sending to packet buffer
                packet_buffer->send(pkt->clone());
            }
        }
        else {
            out->verbose(CALL_INFO, INFO, 0, "Packet Dropped, pkt_id = %u\n", pkt->pkt_id);
            // Packet dropped
            drop_receiver->send(pkt);
            // It is the DualQ v2, then we must send NULL
            // And update bytes tracker and packet buffer
            if ( model_version == 2 ) {
                port_fifos[pkt->dest_port]->send(NULL);
                // Sending to bytes tracker
                bytes_tracker->send(pkt->clone());
                // Sending to packet buffer
                packet_buffer->send(pkt->clone());
            }
        }

        safe_lock.unlock();
        // Release memory
        delete pkt_util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

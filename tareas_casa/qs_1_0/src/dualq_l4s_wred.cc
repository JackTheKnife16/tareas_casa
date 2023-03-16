#include <sst/core/sst_config.h>

#include "dualq_l4s_wred.h"

#include "sst/core/event.h"
#include "sst/core/rng/mersenne.h"

#include "dualq_coupling_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>
#include <math.h>

using JSON = nlohmann::json;
using namespace SST;

using namespace SST;

/**
 * @brief Construct a new L4SWRED::L4SWRED object
 *
 * @param id
 * @param params
 */
L4SWRED::L4SWRED(ComponentId_t id, Params& params) : Component(id)
{
    num_ports                    = params.find<uint32_t>("num_ports", 52);
    model_version                = params.find<uint32_t>("model_version", 1);
    // MA config file
    std::string   ma_config_path = params.find<std::string>("ma_config_file", "qs_1_0/ma_config.json");
    JSON          ma_config;
    std::ifstream ma_config_file(ma_config_path);
    ma_config_file >> ma_config;
    // LQ map to index WRED profile
    int lq_indexes = ma_config["Egress_LQ"]["WRED"].size();
    for ( int i = 0; i < lq_indexes; i++ ) {
        std::string speed = ma_config["Egress_LQ"]["WRED"][i]["LQ"];

        int wred_index       = ma_config["Egress_LQ"]["WRED"][i]["WRED_index"];
        lq_wred_index[speed] = wred_index;
    }
    // LQ WRED profiles by priorities
    int lq_profiles_table_size = ma_config["Egress_LQ_WRED_tables"].size();
    for ( int i = 0; i < lq_profiles_table_size; i++ ) {
        for ( int j = 0; j < NUM_PRI; j++ ) {
            ecn_wred_profile_t lq_wred_profile;
            lq_wred_profile.wred_profile.starting_point =
                ma_config["Egress_LQ_WRED_tables"][std::to_string(i)][std::to_string(j)]["Start_point"];

            float exponent = ma_config["Egress_LQ_WRED_tables"][std::to_string(i)][std::to_string(j)]["Exponent"];
            float mantissa = ma_config["Egress_LQ_WRED_tables"][std::to_string(i)][std::to_string(j)]["Mantissa"];
            lq_wred_profile.wred_profile.slope = exponent + (mantissa / 16);

            lq_wred_profile.ecn_drop_point =
                ma_config["Egress_LQ_WRED_tables"][std::to_string(i)][std::to_string(j)]["ecn_drop_point"];
            lq_profiles[i][j] = lq_wred_profile;
        }
    }
    // Port Config file
    std::string   port_config_path = params.find<std::string>("port_config_file", "qs_1_0/port_config.json");
    JSON          port_config_json;
    std::ifstream port_config_file(port_config_path);
    port_config_file >> port_config_json;
    // Port config initialization
    unsigned short port_available = port_config_json.size();
    for ( unsigned short i = 0; i < port_available; i++ ) {
        // Get port speed
        std::string speed = port_config_json[i]["bw"];
        // Add port with speed to map
        port_config[i]    = speed;
    }
    // Seed for RNG
    const uint32_t seed       = (uint32_t)params.find<int64_t>("seed", 1);
    rng                       = new SST::RNG::MersenneRNG(seed);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":L4SWRED[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- L4SWRED --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> MA config: %s\n", ma_config_path.c_str());

    base_tc = registerTimeBase("1ps", true);

    // Init coupling probability table
    for ( int i = 0; i < num_ports; i++ ) {
        coupling_probability[i] = 0.00;
    }

    // Configure in links
    acceptance_checker =
        configureLink("acceptance_checker", base_tc, new Event::Handler<L4SWRED>(this, &L4SWRED::handle_new_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "acceptance_checker link was configured\n");

    coupling = configureLink("coupling", base_tc, new Event::Handler<L4SWRED>(this, &L4SWRED::handle_new_coupling));
    out->verbose(CALL_INFO, MODERATE, 0, "coupling link was configured\n");

    drop_receiver = configureLink("drop_receiver", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "drop_receiver link was configured\n");

    packet_buffer = configureLink("packet_buffer", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "packet_buffer link was configured\n");

    bytes_tracker = configureLink("bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "bytes_tracker link was configured\n");

    // Configure inggress port links
    for ( int i = 0; i < num_ports; i++ ) {
        std::string port_f = "port_fifo_" + std::to_string(i);
        port_fifos[i]      = configureLink(port_f, base_tc);
    }

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");

    p_CL = new Statistic<float>*[num_ports];
    for ( int i = 0; i < num_ports; i++ ) {
        std::string port_name("port_");
        port_name = port_name + std::to_string(i);
        p_CL[i]   = registerStatistic<float>("p_CL", port_name);
    }

    p_L = new Statistic<float>*[num_ports];
    for ( int i = 0; i < num_ports; i++ ) {
        std::string port_name("port_");
        port_name = port_name + std::to_string(i);
        p_L[i]    = registerStatistic<float>("p_L", port_name);
    }

    ce_packets         = registerStatistic<uint32_t>("ce_packets", "1");
    ce_packets_by_dest = registerStatistic<uint32_t>("ce_packets_by_dest", "1");
    ce_packets_by_src  = registerStatistic<uint32_t>("ce_packets_by_src", "1");
}

/**
 * @brief Check if a packet has the ECN Capable Transport
 *
 * @param pkt Packet to check
 * @return true Packet is ECT
 * @return false Packet isn't ECT
 */
bool
L4SWRED::is_ecn_capable(PacketEvent* pkt)
{
    bool ect = false;
    if ( pkt->ip_ecn == 0x1 || pkt->ip_ecn == 0x2 ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Packet is ECN Capable Transport, pkt_id = %u\n", pkt->pkt_id);
        ect = true;
    }
    else {
        out->verbose(CALL_INFO, DEBUG, 0, "The packet does not have ECN Capable Transport, pkt_id = %u\n", pkt->pkt_id);
    }

    return ect;
}

/**
 * @brief Drop Probably Calculation:
 * If (CU < SP ) Then PB = 0
 * Else PB  = (CU – SP) * Slope.  (cap at 127 if result is over)
 *
 * @param profile
 * @param util
 * @return float
 */
float
L4SWRED::drop_calculation(wred_profile_t profile, int util)
{
    float drop_prob;
    if ( float(util) < profile.starting_point ) { drop_prob = 0.00; }
    else {
        drop_prob = (float(util) - profile.starting_point) * profile.slope;

        if ( drop_prob > HIGHEST_UTIL ) { drop_prob = HIGHEST_UTIL; }
    }

    out->verbose(
        CALL_INFO, DEBUG, 0,
        "Calculating drop probability for utilization %d with profile starting point %d and slope %f.\n", util,
        profile.starting_point, profile.slope);


    return drop_prob;
}

/**
 * @brief Computes the marking probability using the ECN WRED profile
 *
 * @param util Current PQ utilization
 * @param ecn_pq_profiles ECN WRED profile to use
 * @return float Marking probability
 */
float
L4SWRED::compute_ecn_marking_prob(int util, ecn_wred_profile_t ecn_pq_profiles)
{
    float ecn_mark_prob = drop_calculation(ecn_pq_profiles.wred_profile, util);
    out->verbose(CALL_INFO, DEBUG, 0, "Computing ECN marking probability for utilization %d\n", util);
    out->verbose(CALL_INFO, DEBUG, 0, "ECN marking probability calculated: %f\n", ecn_mark_prob);
    return ecn_mark_prob;
}

void
L4SWRED::handle_new_pkt(SST::Event* ev)
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
            CALL_INFO, INFO, 0,
            "Getting pkt util: size = %dB, dest = %d, pri = %d, lq = %d, "
            "pkt_id = %u\n",
            pkt_util->pkt_size, pkt_util->dest_port, pkt_util->priority, pkt_util->lq_util, pkt_util->pkt_id);

        safe_lock.lock();
        // REVIEW: What to do when a packet arrives and it is not ECT
        bool pkt_accepted;

        // Get port speed
        std::string speed = port_config[pkt->dest_port];

        int                wred_index          = lq_wred_index[speed];
        // Get the ECN profile for the packet priority
        ecn_wred_profile_t current_ecn_profile = lq_profiles[wred_index][pkt->priority];

        // If the PQ utilization is lower or equal than the stating point
        // the packet will be accepted, otherwise, check if it is marked or dropped
        float marking_probability;
        if ( pkt_util->lq_util <= current_ecn_profile.wred_profile.starting_point ) {
            out->verbose(CALL_INFO, MODERATE, 0, "Packet is accepted directly, pkt_id = %u\n", pkt->pkt_id);
            pkt_accepted        = true;
            marking_probability = 0;
        }
        else {
            out->verbose(
                CALL_INFO, MODERATE, 0, "Checking if packet will be marked or dropped, pkt_id = %u\n", pkt->pkt_id);
            // Compute marking prob using the ecn profile
            marking_probability = compute_ecn_marking_prob(pkt_util->lq_util, current_ecn_profile);
        }

        // Coupling comparisson
        if ( coupling_probability[pkt_util->lq_index] > marking_probability ) {
            marking_probability = coupling_probability[pkt_util->lq_index];
        }

        // p_L stat collection
        p_L[pkt_util->lq_index]->addData(marking_probability);

        // Check if the packet is accepted, marked (in this case, it is accepted as
        // well), or dropped
        pkt_accepted = ecn_check(pkt, pkt_util->lq_util, marking_probability, current_ecn_profile.ecn_drop_point);

        if ( pkt_accepted ) {
            out->verbose(CALL_INFO, MODERATE, 0, "Packet Accepted ipecn = %d, pkt_id = %u\n", pkt->ip_ecn, pkt->pkt_id);
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
            out->verbose(CALL_INFO, MODERATE, 0, "Packet Dropped, pkt_id = %u\n", pkt->pkt_id);
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

/**
 * @brief Handler for new coupling updates from Classic WRED
 *
 * @param ev Coupling event
 */
void
L4SWRED::handle_new_coupling(SST::Event* ev)
{
    // Send to egress port
    CouplingEvent* coup = dynamic_cast<CouplingEvent*>(ev);
    if ( coup ) {
        out->verbose(
            CALL_INFO, MODERATE, 0, "New coupling probability update: LQ index = %d, probability = %f\n",
            coup->lq_index, coup->coupling_probability);

        safe_lock.lock();

        coupling_probability[coup->lq_index] = coup->coupling_probability;

        // Collect p_CL stat
        p_CL[coup->lq_index]->addData(coup->coupling_probability);

        safe_lock.unlock();

        delete coup;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Determines if a packet is just accepted, marked and accepted, or
 * dropped
 *
 * @param pkt Packet to mark, in that case.
 * @param util Current PQ utilization
 * @param marking_prob Marking probability
 * @param ecn_drop_point ECN dropping point
 * @return true If the packet was accepted
 * @return false If the packet was dropped
 */
bool
L4SWRED::ecn_check(PacketEvent* pkt, int util, float marking_prob, int ecn_drop_point)
{
    out->verbose(
        CALL_INFO, 1, 0, "Checking ECN: util: %d, mark_prob: %f, ecn_drop_pt: %d\n", util, marking_prob,
        ecn_drop_point);
    bool pkt_accepted = false;
    if ( util < ecn_drop_point ) {
        // Get nombre between 0-127
        float accept_probability = rng->generateNextUInt32() % HIGHEST_UTIL;
        if ( accept_probability < marking_prob ) {
            congestion_experienced(pkt);
            ce_packets->addData(1);
            ce_packets_by_dest->addData(pkt->dest_port);
            ce_packets_by_src->addData(pkt->src_port);
            out->verbose(CALL_INFO, INFO, 0, "Packet was marked: ip_ecn = %d, pkt_id = %u\n", pkt->ip_ecn, pkt->pkt_id);
        }
        pkt_accepted = true;
    }
    out->verbose(CALL_INFO, INFO, 0, "Was the packet accepted? %d\n", pkt_accepted);

    return pkt_accepted;
}

/**
 * @brief Set the IP ECN to 0x3 which mean ECN_CE
 *
 * @param pkt Packet to mark as ECN_CE
 */
void
L4SWRED::congestion_experienced(PacketEvent* pkt)
{
    pkt->ip_ecn = (pkt->ip_ecn & 0xfc) | 3;
    out->verbose(CALL_INFO, DEBUG, 0, "Marked packet with pkt_id = %u as ECN_CE\n", pkt->pkt_id);
}

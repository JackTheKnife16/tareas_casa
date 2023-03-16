#include <sst/core/sst_config.h>

#include "wred.h"

#include "sst/core/event.h"
#include "sst/core/rng/mersenne.h"

#include "packet_event.h"
#include "packet_util_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>
#include <math.h>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new WRED::WRED object
 *
 * @param id
 * @param params
 */
WRED::WRED(ComponentId_t id, Params& params) : Component(id)
{
    num_ports                    = params.find<uint32_t>("num_ports", 52);
    // MA config file
    std::string   ma_config_path = params.find<std::string>("ma_config_file", "qs_1_0/ma_config.json");
    JSON          ma_config;
    std::ifstream ma_config_file(ma_config_path);
    ma_config_file >> ma_config;
    // MG WRED profile
    int mg_profiles = ma_config["Egress_MG"]["WRED"].size();
    for ( int i = 0; i < mg_profiles; i++ ) {
        int mg_index = ma_config["Egress_MG"]["WRED"][i]["MG"];

        wred_profile_t mg_wred_profile;
        mg_wred_profile.starting_point = ma_config["Egress_MG"]["WRED"][i]["Start_point"];

        float exponent        = ma_config["Egress_MG"]["WRED"][i]["Exponent"];
        float mantissa        = ma_config["Egress_MG"]["WRED"][i]["Mantissa"];
        mg_wred_profile.slope = exponent + (mantissa / 16);

        wred_mg[mg_index] = mg_wred_profile;
    }
    // PQ WRED utilization
    int pq_profiles = ma_config["Egress_PQ"]["WRED"].size();
    for ( int i = 0; i < pq_profiles; i++ ) {
        int queue = ma_config["Egress_PQ"]["WRED"][i]["Queue"];

        wred_profile_t pq_wred_profile;
        pq_wred_profile.starting_point = ma_config["Egress_PQ"]["WRED"][i]["Start_point"];

        float exponent        = ma_config["Egress_PQ"]["WRED"][i]["Exponent"];
        float mantissa        = ma_config["Egress_PQ"]["WRED"][i]["Mantissa"];
        pq_wred_profile.slope = exponent + (mantissa / 16);

        wred_pq[queue] = pq_wred_profile;
    }
    // QG map to index WRED profile
    int qg_indexes = ma_config["Egress_QG"]["WRED"].size();
    for ( int i = 0; i < qg_indexes; i++ ) {
        std::string speed = ma_config["Egress_QG"]["WRED"][i]["QG"];

        int wred_index       = ma_config["Egress_QG"]["WRED"][i]["WRED_index"];
        qg_wred_index[speed] = wred_index;
    }
    // QG WRED profiles by priorities
    int qg_profiles_table_size = ma_config["Egress_QG_WRED_tables"].size();
    for ( int i = 0; i < qg_profiles_table_size; i++ ) {
        for ( int j = 0; j < NUM_PRI; j++ ) {
            wred_profile_t qg_wred_profile;
            qg_wred_profile.starting_point =
                ma_config["Egress_QG_WRED_tables"][std::to_string(i)][std::to_string(j)]["wred_start"];

            float exponent = ma_config["Egress_QG_WRED_tables"][std::to_string(i)][std::to_string(j)]["exponent"];
            float mantissa = ma_config["Egress_QG_WRED_tables"][std::to_string(i)][std::to_string(j)]["mantissa"];
            qg_wred_profile.slope = exponent + (mantissa / 16);

            qg_profiles[i][j] = qg_wred_profile;
        }
    }
    // Enable ECN
    enable_ecn = params.find<bool>("enable_ecn", 0);
    if ( enable_ecn ) {
        // ECN config file
        std::string   ecn_config_path = params.find<std::string>("ecn_config_file", "qs_1_0/ecn_config.json");
        JSON          ecn_config;
        std::ifstream ecn_config_file(ecn_config_path);
        ecn_config_file >> ecn_config;
        // ECN PQ WRED initialization
        int ecn_pq_prof_size = ecn_config.size();
        for ( int i = 0; i < ecn_pq_prof_size; i++ ) {
            std::string        priority_string = std::to_string(i);
            ecn_wred_profile_t ecn_pq_wred_profile;
            ecn_pq_wred_profile.wred_profile.starting_point = ecn_config[priority_string]["Start_point"];

            float exponent                         = ecn_config[priority_string]["Exponent"];
            float mantissa                         = ecn_config[priority_string]["Mantissa"];
            ecn_pq_wred_profile.wred_profile.slope = exponent + (mantissa / 16);

            ecn_pq_wred_profile.ecn_drop_point = ecn_config[priority_string]["ecn_drop_point"];

            ecn_pq_profiles[i] = ecn_pq_wred_profile;
        }
    }
    // Port Config file
    std::string port_config_path =
        params.find<std::string>("port_config_file", "qs_1_0/config/front_plane_config/48x2.5G4x10G.json");
    JSON          port_config_json;
    std::ifstream port_config_file(port_config_path);
    port_config_file >> port_config_json;
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
    prefix << "@t:" << getName() << ":WRED[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- WRED --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> MA config: %s\n", ma_config_path.c_str());

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    acceptance_checker =
        configureLink("acceptance_checker", base_tc, new Event::Handler<WRED>(this, &WRED::handle_new_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "acceptance_checker link was configured\n");

    drop_receiver = configureLink("drop_receiver", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "drop_receiver link was configured\n");

    // The following two- links are optional, they depend on the model so we must
    // check them before sending an event
    packet_buffer = configureLink("packet_buffer", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "packet_buffer link was configured\n");

    bytes_tracker = configureLink("bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "bytes_tracker link was configured\n");

    // Configure inggress port links
    for ( int i = 0; i < num_ports; i++ ) {
        std::string port_f = "port_fifo_" + std::to_string(i);
        port_fifos[i]      = configureLink(port_f, base_tc);
        out->verbose(CALL_INFO, MODERATE, 0, "port_fifo_%d link was configured\n", i);

        assert(port_fifos[i]);
    }

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");

    // Stats init
    ce_packets         = registerStatistic<uint32_t>("ce_packets", "1");
    ce_packets_by_dest = registerStatistic<uint32_t>("ce_packets_by_dest", "1");
    ce_packets_by_src  = registerStatistic<uint32_t>("ce_packets_by_src", "1");
}

/**
 * @brief Handler for packet requests
 *
 * @param ev
 */
void
WRED::handle_new_pkt(SST::Event* ev)
{
    PacketUtilEvent* pkt_util = dynamic_cast<PacketUtilEvent*>(ev);
    if ( pkt_util ) {
        // Create packet
        PacketEvent* pkt;

        if ( !pkt_util->is_tcp ) {
            pkt = new PacketEvent();
            out->verbose(
                CALL_INFO, INFO, 0,
                "A packet_util_event has arrived through acceptance_checker "
                "link, pkt_id = %u\n",
                pkt_util->pkt_id);
        }
        else {
            TCPPacketEvent*     tcp_pkt      = new TCPPacketEvent();
            TCPPacketUtilEvent* tcp_pkt_util = dynamic_cast<TCPPacketUtilEvent*>(ev);
            out->verbose(
                CALL_INFO, INFO, 0,
                "A tcp packet_util_event has arrived through "
                "acceptance_checker link, pkt_id = %u\n",
                tcp_pkt_util->pkt_id);

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
            CALL_INFO, DEBUG, 0,
            "Some information about pkt: id = %u, size = %dB, dest = %d, "
            "pri = %d, mg = %d, qg = %d, pq = %d\n",
            pkt_util->pkt_id, pkt_util->pkt_size, pkt_util->dest_port, pkt_util->priority, pkt_util->mg_util,
            pkt_util->qg_util, pkt_util->pq_util);

        bool pkt_accepted;

        safe_lock.lock();
        // Check if the packet is ECN capable
        if ( is_ecn_capable(pkt) && enable_ecn ) {
            out->verbose(
                CALL_INFO, MODERATE, 0, "New Packet ECT: id = %u, dest = %d, pri = %d, pq_util = %d\n", pkt->pkt_id,
                pkt->dest_port, pkt->priority, pkt_util->pq_util);
            // Get the ECN profile for the packet priority
            ecn_wred_profile_t current_ecn_profile = ecn_pq_profiles[pkt->priority];

            // If the PQ utilization is lower or equal than the stating point
            // the packet will be accepted, otherwise, check if it is marked or
            // dropped
            if ( pkt_util->pq_util <= current_ecn_profile.wred_profile.starting_point ) {
                out->verbose(CALL_INFO, MODERATE, 0, "Packet is accepted directly, pkt_id = %u\n", pkt_util->pkt_id);
                pkt_accepted = true;
            }
            else {
                out->verbose(CALL_INFO, MODERATE, 0, "Checking if packet will be marked or dropped\n");
                // Compute marking prob using the ecn profile
                float marking_probability = compute_ecn_marking_prob(pkt_util->pq_util, current_ecn_profile);
                out->verbose(
                    CALL_INFO, MODERATE, 0, "Marking probablity = %f, pkt id = %u\n", marking_probability, pkt->pkt_id);
                // Check if the packet is accepted, marked (in this case, it is accepted
                // as well), or dropped
                out->verbose(CALL_INFO, MODERATE, 0, "Checking if packet with pkt_id = %u is accepted\n", pkt->pkt_id);
                pkt_accepted =
                    ecn_check(pkt, pkt_util->pq_util, marking_probability, current_ecn_profile.ecn_drop_point);
            }
        }
        else {
            // Get probability
            out->verbose(
                CALL_INFO, MODERATE, 0, "Requesting the drop probability for packet with pkt_id = %u\n", pkt->pkt_id);
            float drop_probability = compute_drop_prob(pkt_util);
            // Check if pkt is accepted
            out->verbose(
                CALL_INFO, MODERATE, 0, "Is packet with pkt_id = %u and drop probability = %f accepted?\n", pkt->pkt_id,
                drop_probability);
            pkt_accepted = accept_pkt(drop_probability);
        }

        if ( pkt_accepted ) {
            out->verbose(
                CALL_INFO, MODERATE, 0,
                "Packet with pkt_id = %u and priority = %d, was Accepted, "
                "ipecn = %d\n",
                pkt->pkt_id, pkt->priority, pkt->ip_ecn);
            // Sending to port fifos
            out->verbose(
                CALL_INFO, INFO, 0, "Sending packet with pkt_id = %u to port_fifo[%d]\n", pkt->pkt_id, pkt->dest_port);
            port_fifos[pkt->dest_port]->send(pkt);

            // Check if the bytes_tracker and packet_buffer links are configured
            if ( bytes_tracker && packet_buffer ) {
                // Sending to bytes tracker
                bytes_tracker->send(pkt->clone());
                out->verbose(
                    CALL_INFO, INFO, 0,
                    "Sending a pkt clone of packet with pkt_id = %u, to "
                    "bytes_tracker\n",
                    pkt->pkt_id);
                // Sending to packet buffer
                packet_buffer->send(pkt->clone());
                out->verbose(
                    CALL_INFO, INFO, 0,
                    "Sending a pkt clone of packet with pkt_id = %u, to "
                    "packet_buffer\n",
                    pkt->pkt_id);
            }
        }
        else {
            out->verbose(
                CALL_INFO, INFO, 0,
                "Sending the packet with pkt_id = %u and priority = %d, to "
                "drop_receiver, the packet was dropped\n",
                pkt->pkt_id, pkt->priority);
            drop_receiver->send(pkt);
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
 * @brief Check if a packet has the ECN Capable Transport
 *
 * @param pkt Packet to check
 * @return true Packet is ECT
 * @return false Packet isn't ECT
 */
bool
WRED::is_ecn_capable(PacketEvent* pkt)
{
    bool ect = false;
    if ( pkt->ip_ecn == 0x1 || pkt->ip_ecn == 0x2 ) { ect = true; }

    if ( ect ) { out->verbose(CALL_INFO, DEBUG, 0, "pkt %u is ECN Capable Transport\n", pkt->pkt_id); }
    else {
        out->verbose(CALL_INFO, DEBUG, 0, "pkt %u isn't ECN Capable Transport\n", pkt->pkt_id);
    }
    return ect;
}

/**
 * @brief Computes the marking probability using the ECN WRED profile
 *
 * @param util Current PQ utilization
 * @param ecn_pq_profiles ECN WRED profile to use
 * @return float Marking probability
 */
float
WRED::compute_ecn_marking_prob(int util, ecn_wred_profile_t ecn_pq_profiles)
{

    float ecn_mark_prob = drop_calculation(ecn_pq_profiles.wred_profile, util);
    out->verbose(CALL_INFO, DEBUG, 0, "Computed ECN marking probability: %.2f\n", ecn_mark_prob);

    return ecn_mark_prob;
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
WRED::ecn_check(PacketEvent* pkt, int util, float marking_prob, int ecn_drop_point)
{
    out->verbose(
        CALL_INFO, INFO, 0, "Checking ECN: util: %d mark_prob: %f ecn_drop_pt: %d\n", util, marking_prob,
        ecn_drop_point);
    bool pkt_accepted = false;
    if ( util < ecn_drop_point ) {
        // Get number between 0-127
        float accept_probability = rng->generateNextUInt32() % HIGHEST_UTIL;
        if ( accept_probability < marking_prob ) {
            congestion_experienced(pkt);
            ce_packets->addData(1);
            ce_packets_by_dest->addData(pkt->dest_port);
            ce_packets_by_src->addData(pkt->src_port);
            out->verbose(CALL_INFO, DEBUG, 0, "Packet was marked: ip_ecn = %d\n", pkt->ip_ecn);
        }
        pkt_accepted = true;
    }
    out->verbose(CALL_INFO, DEBUG, 0, "Was the packet accepted? %d\n", pkt_accepted);

    return pkt_accepted;
}

/**
 * @brief Set the IP ECN to 0x3 which mean ECN_CE
 *
 * @param pkt Packet to mark as ECN_CE
 */
void
WRED::congestion_experienced(PacketEvent* pkt)
{
    pkt->ip_ecn = (pkt->ip_ecn & 0xfc) | 3;
}

/**
 * @brief Computes all drop probabilities and chooses the greatest of the three
 * accountin elements
 *
 * @param pkt_util Packet Utilization event
 * @return float Chosen drop probability
 */
float
WRED::compute_drop_prob(PacketUtilEvent* pkt_util)
{
    // Compute probabilities for each accounting element
    out->verbose(CALL_INFO, DEBUG, 0, "Computing drop probability for pkt %u\n", pkt_util->pkt_id);
    out->verbose(CALL_INFO, DEBUG, 0, "Computing MG drop probability for pkt %u\n", pkt_util->pkt_id);
    float mg_prob = compute_mg_drop_prob(pkt_util->mg_util, pkt_util->mg_index);
    out->verbose(CALL_INFO, DEBUG, 0, "MG drop probability for pkt %u is %f\n", pkt_util->pkt_id, mg_prob);

    pq_index_t pq_index;
    pq_index.index_i = pkt_util->pq_index;

    out->verbose(CALL_INFO, DEBUG, 0, "Computing QG drop probability for pkt %u\n", pkt_util->pkt_id);
    float qg_prob = compute_qg_drop_prob(pkt_util->qg_util, pq_index);
    out->verbose(CALL_INFO, DEBUG, 0, "QG drop probability for pkt %u is %f\n", pkt_util->pkt_id, qg_prob);

    out->verbose(CALL_INFO, DEBUG, 0, "Computing PQ drop probability for pkt %u\n", pkt_util->pkt_id);
    float pq_prob = compute_pq_drop_prob(pkt_util->pq_util, pq_index);
    out->verbose(CALL_INFO, DEBUG, 0, "PQ drop probability for pkt %u is %f\n", pkt_util->pkt_id, pq_prob);

    // Choose highest probability
    float highest_prob = mg_prob;
    if ( qg_prob > highest_prob ) {
        highest_prob = qg_prob;
        out->verbose(
            CALL_INFO, DEBUG, 0, "The QG probability %f is higher than MG probability %f for pkt %u\n", qg_prob,
            mg_prob, pkt_util->pkt_id);
    }
    if ( pq_prob > highest_prob ) {
        highest_prob = pq_prob;
        out->verbose(
            CALL_INFO, DEBUG, 0, "The PQ probability %f is higher than QG probability %f for pkt %u\n", pq_prob,
            qg_prob, pkt_util->pkt_id);
    }

    out->verbose(
        CALL_INFO, DEBUG, 0, "The highest probability between MG, QG and PQ for pkt %u is: %f\n", pkt_util->pkt_id,
        highest_prob);

    out->verbose(CALL_INFO, DEBUG, 0, "Value of drop probability for pkt %u is: %f\n", pkt_util->pkt_id, highest_prob);
    return highest_prob;
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
WRED::drop_calculation(wred_profile_t profile, int util)
{
    float drop_prob;
    if ( float(util) < profile.starting_point ) { drop_prob = 0.00; }
    else {
        drop_prob = (float(util) - profile.starting_point) * profile.slope;

        if ( drop_prob > HIGHEST_UTIL ) drop_prob = HIGHEST_UTIL;
    }
    out->verbose(CALL_INFO, DEBUG, 0, "calculate drop probability = %f\n", drop_prob);
    return drop_prob;
}

float
WRED::compute_mg_drop_prob(int util, int mg_index)
{
    wred_profile_t profile      = wred_mg[mg_index];
    float          mg_drop_prob = drop_calculation(profile, util);

    out->verbose(CALL_INFO, DEBUG, 0, "mg drop probability = %f\n", mg_drop_prob);
    return mg_drop_prob;
}

float
WRED::compute_qg_drop_prob(int util, pq_index_t pq)
{
    // Get port speed
    std::string speed = port_config[pq.index_s.port];

    int            wred_index   = qg_wred_index[speed];
    wred_profile_t profile      = qg_profiles[wred_index][pq.index_s.priority];
    float          qg_drop_prob = drop_calculation(profile, util);

    out->verbose(CALL_INFO, DEBUG, 0, "qg drop probability = %f\n", qg_drop_prob);
    return qg_drop_prob;
}
float
WRED::compute_pq_drop_prob(int util, pq_index_t pq)
{
    wred_profile_t profile      = wred_pq[pq.index_s.priority];
    float          pq_drop_prob = drop_calculation(profile, util);

    out->verbose(CALL_INFO, DEBUG, 0, "pq drop probability = %f\n", pq_drop_prob);
    return pq_drop_prob;
}

bool
WRED::accept_pkt(float drop_probability)
{
    // Get nombre between 0-127
    float accept_probability = rng->generateNextUInt32() % HIGHEST_UTIL;

    out->verbose(
        CALL_INFO, MODERATE, 0,
        "Generated accept probability of pkt to compare with drop "
        "probability: accept = %f, drop = %f\n",
        accept_probability, round(drop_probability));

    bool accepted = false;
    if ( accept_probability >= round(drop_probability) ) {
        accepted = true;
        out->verbose(CALL_INFO, MODERATE, 0, "pkt is accepted because accept_probability >= drop_probability\n");
    }
    else {
        out->verbose(CALL_INFO, MODERATE, 0, "pkt is not accepted because accept_probability < drop_probability\n");
    }

    return accepted;
}

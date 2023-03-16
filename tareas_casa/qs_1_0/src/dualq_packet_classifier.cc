#include <sst/core/sst_config.h>

#include "dualq_packet_classifier.h"

#include "sst/core/event.h"

#include "pq_index.h"
#include "util_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new DualQPacketClassifier::DualQPacketClassifier object
 *
 * @param id
 * @param params
 */
DualQPacketClassifier::DualQPacketClassifier(ComponentId_t id, Params& params) : Component(id)
{
    // Number of ports
    num_ports      = params.find<int>("num_ports", 52);
    // Utilization threshold
    util_threshold = params.find<int>("util_threshold", 124);

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
        int mg_index       = ma_config["Egress_MG"]["Mapping"][i]["MG"];
        // Init utilization in 0%
        mg_table[mg_index] = 0;
    }
    // Queue groups utilization
    for ( int i = 0; i < num_ports; i++ ) {
        // Init utilization in 0%
        qg_table[i] = 0;
    }
    // Physical queue utilization --> 52 port and 8 priority queues
    for ( int i = 0; i < num_ports; i++ ) {
        for ( size_t j = 0; j <= HIGHEST_PRI_Q; j++ ) {
            // Init utilization in 0%
            pq_index_t pq_index;
            pq_index.index_s.port      = i;
            pq_index.index_s.priority  = j;
            pq_table[pq_index.index_i] = 0;
        }
    }
    // L4S queue utilization
    for ( int i = 0; i < num_ports; i++ ) {
        // Init utilization in 0%
        lq_table[i] = 0;
    }

    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":DualQPacketClassifier[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Packet Classifier --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of ports: %d\n", num_ports);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Utilization Threshold: %d\n", util_threshold);

    base_tc = registerTimeBase("1ps", true);

    // Configure ingress port links
    Event::Handler<DualQPacketClassifier>* ports_handler =
        new Event::Handler<DualQPacketClassifier>(this, &DualQPacketClassifier::handle_new_packet);
    for ( int i = 0; i < num_ports; i++ ) {
        // Port Index
        std::string i_port = std::to_string(i);

        // Ingress port link
        ingress_ports[i] = configureLink("in_" + i_port, base_tc, ports_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "in_%d link was configured\n", i);
        assert(ingress_ports[i]);

        // L FIFO link
        l_fifos[i] = configureLink("l_fifo_" + i_port, base_tc);
        out->verbose(CALL_INFO, MODERATE, 0, "l_fifo_%d link was configured\n", i);
        assert(l_fifos[i]);

        // FIFOs link
        fifos[i] = configureLink("fifo_" + i_port, base_tc);
        out->verbose(CALL_INFO, MODERATE, 0, "fifo_%d link was configured\n", i);
        assert(fifos[i]);
    }

    // Configure in links
    packet_buffer = configureLink("packet_buffer", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "packet_buffer link was configured\n");

    bytes_tracker = configureLink("bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "bytes_tracker link was configured\n");

    l_bytes_tracker = configureLink("l_bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "l_bytes_tracker link was configured\n");

    drop_receiver = configureLink("drop_receiver", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "drop_receiver link was configured\n");

    mg_util = configureLink(
        "mg_util", base_tc, new Event::Handler<DualQPacketClassifier>(this, &DualQPacketClassifier::handle_update_mg));
    out->verbose(CALL_INFO, MODERATE, 0, "mg_util link was configured\n");

    qg_util = configureLink(
        "qg_util", base_tc, new Event::Handler<DualQPacketClassifier>(this, &DualQPacketClassifier::handle_update_qg));
    out->verbose(CALL_INFO, MODERATE, 0, "qg_util link was configured\n");

    pq_util = configureLink(
        "pq_util", base_tc, new Event::Handler<DualQPacketClassifier>(this, &DualQPacketClassifier::handle_update_pq));
    out->verbose(CALL_INFO, MODERATE, 0, "pq_util link was configured\n");

    lq_util = configureLink(
        "lq_util", base_tc, new Event::Handler<DualQPacketClassifier>(this, &DualQPacketClassifier::handle_update_lq));
    out->verbose(CALL_INFO, MODERATE, 0, "lq_util link was configured\n");

    // Check if they are not null
    assert(packet_buffer);
    assert(bytes_tracker);
    assert(drop_receiver);
    assert(mg_util);
    assert(qg_util);
    assert(pq_util);
    assert(lq_util);

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Handler for new packets from acceptance checker
 *
 * @param ev Event with packet utilization
 */
void
DualQPacketClassifier::handle_new_packet(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        out->verbose(
            CALL_INFO, INFO, 0, "New packet with ECN 0x%X from source port = %d, pkt_id = %u\n", pkt->ip_ecn,
            pkt->src_port, pkt->pkt_id);
        if ( pkt->ip_ecn % 2 == 1 ) { send_to_l_queue(pkt); }
        else {
            send_to_c_queue(pkt);
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for MG updates
 *
 * @param ev Util Event to parse
 */
void
DualQPacketClassifier::handle_update_mg(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_lock.lock();
        mg_table[util->element_index] = util->utilization;
        safe_lock.unlock();

        out->verbose(
            CALL_INFO, DEBUG, 0, "Received Memory Group utilization update: element_index=%d, utilization=%d\n",
            util->element_index, util->utilization);

        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for QG updates
 *
 * @param ev Util Event to parse
 */
void
DualQPacketClassifier::handle_update_qg(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_lock.lock();
        qg_table[util->element_index] = util->utilization;
        safe_lock.unlock();

        out->verbose(
            CALL_INFO, DEBUG, 0, "Received Queue Group utilization update: element_index=%d, utilization=%d\n",
            util->element_index, util->utilization);
        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for PQ updates
 *
 * @param ev Util Event to parse
 */
void
DualQPacketClassifier::handle_update_pq(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_lock.lock();
        pq_table[util->element_index] = util->utilization;
        safe_lock.unlock();

        out->verbose(
            CALL_INFO, DEBUG, 0, "Updated pq_table with utilization value %d at index %d\n", util->utilization,
            util->element_index);

        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for LQ updates
 *
 * @param ev Util Event to parse
 */
void
DualQPacketClassifier::handle_update_lq(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_lock.lock();
        lq_table[util->element_index] = util->utilization;
        safe_lock.unlock();

        out->verbose(
            CALL_INFO, DEBUG, 0, "Updated lq_table with utilization value %d at index %d\n", util->utilization,
            util->element_index);
        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Get utilization of elements related to the packet
 *
 * @param dest_port
 * @param priority
 * @return int
 */
int
DualQPacketClassifier::get_classic_utilization(uint32_t dest_port, uint32_t priority)
{
    int mg_index = pq_mg_map[priority];
    int util     = mg_table[mg_index];

    if ( util < qg_table[dest_port] ) { util = qg_table[dest_port]; }

    pq_index_t pq_index;
    pq_index.index_s.port     = dest_port;
    pq_index.index_s.priority = priority;
    if ( util < pq_table[pq_index.index_i] ) { util = pq_table[pq_index.index_i]; }
    out->verbose(
        CALL_INFO, DEBUG, 0, "Getting utilization for packet with destination port: %d and priority: %d\n", dest_port,
        priority);

    return util;
}

/**
 * @brief Get LQ utilization from a destination port
 *
 * @param dest_port
 * @return int
 */
int
DualQPacketClassifier::get_l4s_utilization(uint32_t dest_port)
{
    out->verbose(CALL_INFO, DEBUG, 0, "Getting utilization for L4S packet with destination port: %d\n", dest_port);
    return lq_table[dest_port];
}

/**
 * @brief Forward the packet to the L AQM
 *
 * @param packet packet util event to forward
 */
void
DualQPacketClassifier::send_to_l_queue(PacketEvent* packet)
{
    out->verbose(
        CALL_INFO, INFO, 0, "L4S check util = %d, pkt_id = %u\n", get_l4s_utilization(packet->dest_port),
        packet->pkt_id);
    if ( get_l4s_utilization(packet->dest_port) < util_threshold ) {
        out->verbose(
            CALL_INFO, INFO, 0, "Forwarding packet to L4S Port FIFO %d, pkt_id = %u\n", packet->dest_port,
            packet->pkt_id);
        l_fifos[packet->dest_port]->send(packet);
        packet_buffer->send(packet->clone());
        l_bytes_tracker->send(packet->clone());
    }
    else {
        out->verbose(
            CALL_INFO, INFO, 0,
            "Dropping packet from L4S because util=%d >= threshold=%d, "
            "pkt_id = %d\n",
            get_l4s_utilization(packet->dest_port), util_threshold, packet->pkt_id);
        drop_receiver->send(packet);
    }
}

/**
 * @brief Forward the packet to the C AQM
 *
 * @param packet packet util event to forward
 */
void
DualQPacketClassifier::send_to_c_queue(PacketEvent* packet)
{
    out->verbose(
        CALL_INFO, INFO, 0, "Classic check util = %d, pkt_id = %d\n",
        get_classic_utilization(packet->dest_port, packet->priority), packet->pkt_id);
    if ( get_classic_utilization(packet->dest_port, packet->priority) < util_threshold ) {
        out->verbose(
            CALL_INFO, INFO, 0, "Forwarding packet to Classic Port FIFO %d, pkt_id = %u\n", packet->dest_port,
            packet->pkt_id);
        fifos[packet->dest_port]->send(packet);
        packet_buffer->send(packet->clone());
        bytes_tracker->send(packet->clone());
    }
    else {
        out->verbose(
            CALL_INFO, INFO, 0,
            "Dropping packet from Classic because util=%d >= "
            "threshold=%d, pkt_id = %u\n",
            get_classic_utilization(packet->dest_port, packet->priority), util_threshold, packet->pkt_id);
        drop_receiver->send(packet);
    }
}

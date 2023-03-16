#include <sst/core/sst_config.h>

#include "bytes_tracker.h"

#include "sst/core/event.h"

#include "bytes_use_event.h"
#include "packet_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>
#include <math.h>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new BytesTracker::BytesTracker object
 *
 * @param id
 * @param params
 */
BytesTracker::BytesTracker(ComponentId_t id, Params& params) : Component(id)
{

    // Number of ports
    num_ports = params.find<uint32_t>("num_ports", 52);

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
        int mg_index            = ma_config["Egress_MG"]["Mapping"][i]["MG"];
        // Init utilization in 0%
        mg_util_bytes[mg_index] = 0;
    }
    // Queue groups utilization
    for ( int i = 0; i < num_ports; i++ ) {
        // Init utilization in 0%
        qg_util_bytes[i] = 0;
    }
    // Physical queue utilization --> 52 port and 8 priority queues
    for ( int i = 0; i < num_ports; i++ ) {
        for ( size_t j = 0; j < PRI_QUEUES; j++ ) {
            // Init utilization in 0%
            pq_index_t pq_index;
            pq_index.index_s.port           = i;
            pq_index.index_s.priority       = j;
            pq_util_bytes[pq_index.index_i] = 0;
        }
    }

    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":BytesTracker[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Bytes Tracker --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> MA config: %s\n", ma_config_path.c_str());

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    mg_bytes = configureLink("mg_bytes", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "mg_bytes link was configured\n");
    qg_bytes = configureLink("qg_bytes", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "qg_bytes link was configured\n");
    pq_bytes = configureLink("pq_bytes", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "pq_bytes link was configured\n");
    wred = configureLink("wred", base_tc, new Event::Handler<BytesTracker>(this, &BytesTracker::handle_accepted_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "wred link was configured\n");
    // Check links
    assert(mg_bytes);
    assert(qg_bytes);
    assert(pq_bytes);
    assert(wred);
    // Configure ingress port links
    Event::Handler<BytesTracker>* ports_handler =
        new Event::Handler<BytesTracker>(this, &BytesTracker::handle_transmitted_pkt);
    for ( int i = 0; i < num_ports; i++ ) {
        std::string e_port = "port_scheduler_" + std::to_string(i);
        port_schedulers[i] = configureLink(e_port, base_tc, ports_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "port_scheduler_%d link was configured\n", i);
        assert(port_schedulers[i]);
    }

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Update the bytes in use of the elements related to the given packet
 *
 * @param pkt New packet
 * @param receive Was this packet queued or transmitted?
 */
void
BytesTracker::update_bytes_in_use(PacketEvent* pkt, bool receive)
{
    // Get pq_index and mg_index
    pq_index_t pq_index;
    pq_index.index_s.port     = pkt->dest_port;
    pq_index.index_s.priority = pkt->priority;
    out->verbose(
        CALL_INFO, DEBUG, 0, "Getting mg_index for packet with destination port %d and priority %d\n",
        pq_index.index_s.port, pq_index.index_s.priority);

    int         mg_index    = get_mg_index(pq_index);
    std::string str_control = "";

    out->verbose(CALL_INFO, DEBUG, 0, "Updating MG, QG, and PQ utilization bytes\n");

    // Update MG
    unsigned int mg_byte_util = update_mg_util_bytes(pkt, mg_index, receive);
    // Update QG
    unsigned int qg_byte_util = update_qg_util_bytes(pkt, pkt->dest_port, receive);
    // Update PQ
    unsigned int pq_byte_util = update_pq_util_bytes(pkt, pq_index, receive);

    if ( receive )
        str_control += "received pkt (adding)";
    else
        str_control += "transmitted pkt (subtracting)";

    out->verbose(
        CALL_INFO, DEBUG, 0, "Bytes in use %s: mg[%d] = %d B, qg[%d] = %d B, pq[%d][%d] = %d B\n", str_control.c_str(),
        mg_index, mg_byte_util, pkt->dest_port, qg_byte_util, pq_index.index_s.port, pq_index.index_s.priority,
        pq_byte_util);

    // Update accouting elements
    out->verbose(CALL_INFO, INFO, 0, "Updating accounting elements and sending bytes in use to bytes updaters.\n");
    update_accounting_elements(mg_index, pkt->dest_port, pq_index, mg_byte_util, qg_byte_util, pq_byte_util);
}

/**
 * @brief Update the bytes utilization of the MG related to the given packet
 *
 * @param pkt New Packet
 * @param mg_index MG index
 * @param receive Queued or transmitted
 * @return unsigned int Current MG bytes in use
 */
unsigned int
BytesTracker::update_mg_util_bytes(PacketEvent* pkt, int mg_index, bool receive)
{
    // Check if it is a new packet
    if ( receive ) {
        mg_util_bytes[mg_index] += pkt->pkt_size;
        out->verbose(CALL_INFO, DEBUG, 0, "Received new packet with ID %u, mg_index %d.\n", pkt->pkt_id, mg_index);
        out->verbose(
            CALL_INFO, DEBUG, 0, "Packet size of %d bytes added to mg_util_bytes[%d].\n", pkt->pkt_size, mg_index);
        out->verbose(CALL_INFO, DEBUG, 0, "mg_util_bytes[%d] is now %d bytes.\n", mg_index, mg_util_bytes[mg_index]);
    }
    else {
        mg_util_bytes[mg_index] -= pkt->pkt_size;
        out->verbose(CALL_INFO, DEBUG, 0, "Transmitted packet with ID %u, mg_index %d.\n", pkt->pkt_id, mg_index);
        out->verbose(
            CALL_INFO, DEBUG, 0, "Packet size of %d bytes subtracted from mg_util_bytes[%d].\n", pkt->pkt_size,
            mg_index);
        out->verbose(CALL_INFO, DEBUG, 0, "mg_util_bytes[%d] is now %d bytes.\n", mg_index, mg_util_bytes[mg_index]);
    }

    return mg_util_bytes[mg_index];
}


/**
 * @brief Update the bytes utilization of the QG related to the given packet
 *
 * @param pkt New Packet
 * @param qg_index QG index
 * @param receive Queued or transmitted
 * @return unsigned int Current QG bytes in use
 */
unsigned int
BytesTracker::update_qg_util_bytes(PacketEvent* pkt, int qg_index, bool receive)
{
    // Check if it is a new packet
    if ( receive ) {
        qg_util_bytes[qg_index] += pkt->pkt_size;
        out->verbose(CALL_INFO, DEBUG, 0, "Received new packet with ID %u for qg_index %d.\n", pkt->pkt_id, qg_index);
        out->verbose(
            CALL_INFO, DEBUG, 0, "Added packet size of %d bytes to qg_util_bytes[%d].\n", pkt->pkt_size, qg_index);
        out->verbose(CALL_INFO, DEBUG, 0, "qg_util_bytes[%d] is now %d bytes.\n", qg_index, qg_util_bytes[qg_index]);
    }
    else {
        qg_util_bytes[qg_index] -= pkt->pkt_size;
        out->verbose(CALL_INFO, DEBUG, 0, "Transmitted packet with ID %u for qg_index %d.\n", pkt->pkt_id, qg_index);
        out->verbose(
            CALL_INFO, DEBUG, 0, "Subtracted packet size of %d bytes from qg_util_bytes[%d].\n", pkt->pkt_size,
            qg_index);
        out->verbose(CALL_INFO, DEBUG, 0, "qg_util_bytes[%d] is now %d bytes.\n", qg_index, qg_util_bytes[qg_index]);
    }

    return qg_util_bytes[qg_index];
}

/**
 * @brief Update the bytes utilization of the PQ related to the given packet
 *
 * @param pkt New Packet
 * @param pq_index PQ index
 * @param receive Queued or transmitted
 * @return unsigned int Current PQ bytes in use
 */
unsigned int
BytesTracker::update_pq_util_bytes(PacketEvent* pkt, pq_index_t pq_index, bool receive)
{
    // Check if it is a new packet
    if ( receive ) {
        pq_util_bytes[pq_index.index_i] += pkt->pkt_size;
        out->verbose(
            CALL_INFO, DEBUG, 0, "Received packet with ID %u, destination port %d and priority %d.\n", pkt->pkt_id,
            pq_index.index_s.port, pq_index.index_s.priority);
        out->verbose(
            CALL_INFO, DEBUG, 0, "Packet size of %d bytes added to pq_util_bytes[%d][%d].\n", pkt->pkt_size,
            pq_index.index_s.port, pq_index.index_s.priority);
        out->verbose(
            CALL_INFO, DEBUG, 0, "pq_util_bytes[%d][%d] is now %d bytes.\n", pq_index.index_s.port,
            pq_index.index_s.priority, pq_util_bytes[pq_index.index_i]);
    }
    else {
        pq_util_bytes[pq_index.index_i] -= pkt->pkt_size;
        out->verbose(
            CALL_INFO, DEBUG, 0, "Transmitted packet with ID %u, destination port %d and priority %d.\n", pkt->pkt_id,
            pq_index.index_s.port, pq_index.index_s.priority);
        out->verbose(
            CALL_INFO, DEBUG, 0, "Packet size of %d bytes subtracted from pq_util_bytes[%d][%d].\n", pkt->pkt_size,
            pq_index.index_s.port, pq_index.index_s.priority);
        out->verbose(
            CALL_INFO, DEBUG, 0, "pq_util_bytes[%d][%d] is now %d bytes.\n", pq_index.index_s.port,
            pq_index.index_s.priority, pq_util_bytes[pq_index.index_i]);
    }

    return pq_util_bytes[pq_index.index_i];
}

/**
 * @brief Handler for accepted packets
 *
 * @param ev Event with new packet
 */
void
BytesTracker::handle_accepted_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        safe_lock.lock();
        out->verbose(
            CALL_INFO, INFO, 0,
            "New packet received from WRED: pkt id = %u, size = %dB, "
            "source = %d, destination = %d, priority = %d\n",
            pkt->pkt_id, pkt->pkt_size, pkt->src_port, pkt->dest_port, pkt->priority);

        out->verbose(CALL_INFO, MODERATE, 0, "Updating bytes in use (adding %dB)\n", pkt->pkt_size);
        update_bytes_in_use(pkt, true);
        safe_lock.unlock();

        // Release memory
        delete pkt;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for transmitted packets
 *
 * @param ev Event with transmitted packet
 */
void
BytesTracker::handle_transmitted_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        safe_lock.lock();
        out->verbose(
            CALL_INFO, INFO, 0,
            "New packet transmitted by scheduler[%d][%d]: pkt id = %u, "
            "size = %dB, destination = %d, priority = %d\n",
            pkt->dest_node, pkt->dest_port, pkt->pkt_id, pkt->pkt_size, pkt->dest_port, pkt->priority);

        out->verbose(CALL_INFO, MODERATE, 0, "Updating bytes in use (subtracting %dB)\n", pkt->pkt_size);
        update_bytes_in_use(pkt, false);
        safe_lock.unlock();

        // Release memory
        delete pkt;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Send updates to the accouting elements that have just beed
 * received/transmitted
 *
 * @param mg_index MG index
 * @param qg_index QG index
 * @param pq_index PQ index
 * @param mg_bytes_in_use MG current bytes in use
 * @param qg_bytes_in_use QG current bytes in use
 * @param pq_bytes_in_use PQ current bytes in use
 */
void
BytesTracker::update_accounting_elements(
    int mg_index, int qg_index, pq_index_t pq_index, unsigned int mg_bytes_in_use, unsigned int qg_bytes_in_use,
    unsigned int pq_bytes_in_use)
{
    // MG
    BytesUseEvent* mg_update = new BytesUseEvent();
    mg_update->bytes_in_use  = mg_bytes_in_use;
    mg_update->element_index = mg_index;
    out->verbose(
        CALL_INFO, DEBUG, 0, "Sending mg_bytes_in_use = %d B and mg_index = %d, to mg_bytes_updater\n", mg_bytes_in_use,
        mg_index);
    mg_bytes->send(mg_update);
    // QG
    BytesUseEvent* qg_update = new BytesUseEvent();
    qg_update->bytes_in_use  = qg_bytes_in_use;
    qg_update->element_index = qg_index;
    out->verbose(
        CALL_INFO, DEBUG, 0, "Sending qg_bytes_in_use = %d B and qg_index = %d, to qg_bytes_updater\n", qg_bytes_in_use,
        qg_index);
    qg_bytes->send(qg_update);
    // PQ
    BytesUseEvent* pq_update = new BytesUseEvent();
    pq_update->bytes_in_use  = pq_bytes_in_use;
    pq_update->element_index = pq_index.index_i;
    out->verbose(
        CALL_INFO, DEBUG, 0,
        "Sending pq_bytes_in_use = %d B and pq_index = [%d][%d], to "
        "pq_bytes_updater\n",
        pq_bytes_in_use, pq_index.index_s.port, pq_index.index_s.priority);
    pq_bytes->send(pq_update);
}

/**
 * @brief Get MG index
 *
 * @param pq PQ index
 * @return int MG index
 */
int
BytesTracker::get_mg_index(pq_index_t pq)
{
    int queue    = pq.index_s.priority;
    int mg_index = pq_mg_map[queue];

    out->verbose(CALL_INFO, DEBUG, 0, "mg_index = %d\n", mg_index);
    return mg_index;
}

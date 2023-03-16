#include <sst/core/sst_config.h>

#include "dualq_bytes_tracker.h"

#include "sst/core/event.h"

#include "bytes_use_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

DualQBytesTracker::DualQBytesTracker(ComponentId_t id, Params& params) : BytesTracker(id, params)
{
    // L4S Queue utilization
    for ( int i = 0; i < num_ports; i++ ) {
        // Init utilization in 0%
        lq_util_bytes[i] = 0;
    }

    lq_bytes = configureLink("lq_bytes", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "lq_bytes link was configured\n");

    l4s_wred = configureLink(
        "l4s_wred", base_tc, new Event::Handler<DualQBytesTracker>(this, &DualQBytesTracker::handle_l_accepted_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "l4s_wred link was configured\n");

    assert(lq_bytes);
    assert(l4s_wred);
    // Configure ingress port links
    Event::Handler<DualQBytesTracker>* ports_handler =
        new Event::Handler<DualQBytesTracker>(this, &DualQBytesTracker::handle_l_transmitted_pkt);
    for ( int i = 0; i < num_ports; i++ ) {
        std::string s_port     = "l_port_scheduler_" + std::to_string(i);
        l4s_port_schedulers[i] = configureLink(s_port, base_tc, ports_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "l_port_scheduler_%d link was configured\n", i);
        assert(l4s_port_schedulers[i]);
    }

    wred_v2 = configureLink(
        "wred_v2", base_tc, new Event::Handler<DualQBytesTracker>(this, &DualQBytesTracker::handle_transmitted_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "wred_v2 link was configured\n");

    l4s_wred_v2 = configureLink(
        "l4s_wred_v2", base_tc,
        new Event::Handler<DualQBytesTracker>(this, &DualQBytesTracker::handle_l_transmitted_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "l4s_wred_v2 link was configured\n");
}

unsigned int
DualQBytesTracker::update_lq_util_bytes(PacketEvent* pkt, int lq_index, bool receive)
{
    // Check if it is a new packet
    if ( receive ) {
        lq_util_bytes[lq_index] += pkt->pkt_size;
        out->verbose(
            CALL_INFO, DEBUG, 0, "Adding pkt_size to lq_util_bytes, size = %d, id = %u\n", pkt->pkt_size, pkt->pkt_id);
        out->verbose(
            CALL_INFO, DEBUG, 0, "lq util bytes for lq index = %d is: %d\n", lq_util_bytes[lq_index], lq_index);
    }
    else {
        lq_util_bytes[lq_index] -= pkt->pkt_size;
        out->verbose(
            CALL_INFO, DEBUG, 0, "Subtracting pkt_size to lq_util_bytes, size = %d, id = %u\n", pkt->pkt_size,
            pkt->pkt_id);
        out->verbose(
            CALL_INFO, DEBUG, 0, "lq util bytes for lq index = %d is: %d\n", lq_util_bytes[lq_index], lq_index);
    }

    return lq_util_bytes[lq_index];
}

void
DualQBytesTracker::handle_l_accepted_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        safe_lock.lock();
        // Update LQ
        unsigned int   lq_byte_util = update_lq_util_bytes(pkt, pkt->dest_port, true);
        // Send new bytes in use to LQ
        BytesUseEvent* lq_update    = new BytesUseEvent();
        lq_update->bytes_in_use     = lq_byte_util;
        lq_update->element_index    = pkt->dest_port;
        lq_bytes->send(lq_update);

        out->verbose(
            CALL_INFO, INFO, 0, "L4S Bytes in use (Receiving): lq=(%d, %d B), pkt_id = %u\n", pkt->dest_port,
            lq_byte_util, pkt->pkt_id);
        safe_lock.unlock();

        // Release memory
        delete pkt;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

void
DualQBytesTracker::handle_l_transmitted_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        safe_lock.lock();
        // Update LQ
        unsigned int   lq_byte_util = update_lq_util_bytes(pkt, pkt->dest_port, false);
        // Send new bytes in use to LQ
        BytesUseEvent* lq_update    = new BytesUseEvent();
        lq_update->bytes_in_use     = lq_byte_util;
        lq_update->element_index    = pkt->dest_port;
        lq_bytes->send(lq_update);
        out->verbose(
            CALL_INFO, 1, 0, "L4S Bytes in use (Transmitting): lq=(%d, %d B), pkt_id = %u\n", pkt->dest_port,
            lq_byte_util, pkt->pkt_id);
        safe_lock.unlock();

        // Release memory
        delete pkt;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

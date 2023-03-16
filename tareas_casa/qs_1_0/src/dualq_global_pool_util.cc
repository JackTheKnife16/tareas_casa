#include <sst/core/sst_config.h>

#include "dualq_global_pool_util.h"

#include "sst/core/event.h"

#include "util_event.h"

#include "space_event.h"
#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

DualQGlobalPoolUtil::DualQGlobalPoolUtil(ComponentId_t id, Params& params) : GlobalPoolUtil(id, params)
{
    // Configure packet buffer link with current implementation
    packet_buffer = configureLink(
        "packet_buffer", base_tc,
        new Event::Handler<DualQGlobalPoolUtil>(this, &DualQGlobalPoolUtil::handle_pkt_buffer_update));
    out->verbose(CALL_INFO, MODERATE, 0, "packet_buffer link was configured\n");

    // L4S Queue links
    for ( int i = 0; i < num_ports; i++ ) {
        std::string lq_id = "l4s_queue_" + std::to_string(i);
        l4s_queues[i]     = configureLink(lq_id, base_tc);
        out->verbose(CALL_INFO, MODERATE, 0, "l4s_queue_%d link was configured\n", i);

        assert(l4s_queues[i]);
    }
}

void
DualQGlobalPoolUtil::update_lqs(unsigned int utilization)
{
    UtilEvent* util     = new UtilEvent();
    util->utilization   = utilization;
    // element_index is not use in this case
    util->element_index = 0;
    for ( int i = 0; i < num_ports; i++ ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Updating LQ in index %d, utilization = %d\n", i, utilization);
        l4s_queues[i]->send(util->clone());
    }
    delete util;
}

void
DualQGlobalPoolUtil::handle_pkt_buffer_update(SST::Event* ev)
{
    SpaceEvent* space = dynamic_cast<SpaceEvent*>(ev);
    if ( space ) {
        safe_lock.lock();
        out->verbose(CALL_INFO, INFO, 0, "Received new SpaceEvent: available space = %u\n", space->block_avail);
        // Compute global pool utilization
        unsigned int utilization = float(1 - (float(space->block_avail) / float(buffer_size))) * max_utilization;
        // Update children
        out->verbose(
            CALL_INFO, DEBUG, 0,
            "Updating Memory Groups, Queue Groups and Logical Groups with new utilization value.\n");
        update_mgs(utilization);
        update_qgs(utilization);
        update_lqs(utilization);
        safe_lock.unlock();

        // Release
        delete space;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

#include <sst/core/sst_config.h>

#include "global_pool_util.h"

#include "sst/core/event.h"

#include "util_event.h"

#include "space_event.h"
#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new GlobalPoolUtil::GlobalPoolUtil object
 *
 * @param id
 * @param params
 */
GlobalPoolUtil::GlobalPoolUtil(ComponentId_t id, Params& params) : Component(id)
{
    num_ports                    = params.find<unsigned int>("num_ports", 52);
    max_utilization              = params.find<unsigned int>("max_utilization", 127);
    buffer_size                  = params.find<unsigned int>("buffer_size", 65536);
    // MA config file
    std::string   ma_config_path = params.find<std::string>("ma_config_file", "qs_1_0/ma_config.json");
    JSON          ma_config;
    std::ifstream ma_config_file(ma_config_path);
    ma_config_file >> ma_config;
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":GlobalPoolUtil[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Global Pool Util --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> MA config: %s\n", ma_config_path.c_str());

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    packet_buffer = configureLink(
        "packet_buffer", base_tc, new Event::Handler<GlobalPoolUtil>(this, &GlobalPoolUtil::handle_pkt_buffer_update));
    out->verbose(CALL_INFO, MODERATE, 0, "packet_buffer link was configured\n");

    // Check it is not null
    assert(packet_buffer);
    // Memory groups links
    int mg_elements = ma_config["Egress_MG"]["Mapping"].size();
    for ( int i = 0; i < mg_elements; i++ ) {
        int         mg_index = ma_config["Egress_MG"]["Mapping"][i]["MG"];
        std::string mg_id    = "memory_group_" + std::to_string(mg_index);
        memory_groups[i]     = configureLink(mg_id, base_tc);
        out->verbose(CALL_INFO, MODERATE, 0, "memory_group_%d link was configured\n", i);
        assert(memory_groups[i]);
    }
    // Queue groups links
    for ( int i = 0; i < num_ports; i++ ) {
        std::string qg_id = "queue_group_" + std::to_string(i);
        queue_groups[i]   = configureLink(qg_id, base_tc);
        out->verbose(CALL_INFO, MODERATE, 0, "queue_group_%d link was configured\n", i);
        assert(queue_groups[i]);
    }

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Updates the MG elements
 *
 * @param utilization Current utilization of the Global Pool
 */
void
GlobalPoolUtil::update_mgs(unsigned int utilization)
{
    UtilEvent* util     = new UtilEvent();
    util->utilization   = utilization;
    // element_index is not use in this case
    util->element_index = 0;
    for ( int i = 0; i < MEM_GROUPS; i++ ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Updating MG in index %d, utilization = %u\n", i, utilization);
        memory_groups[i]->send(util->clone());
    }
    out->verbose(CALL_INFO, MODERATE, 0, "All memory groups were updated with current utilization\n");
    delete util;
}

/**
 * @brief Updates the QG elements
 *
 * @param utilization Current utilization of the Global Pool
 */
void
GlobalPoolUtil::update_qgs(unsigned int utilization)
{
    UtilEvent* util   = new UtilEvent();
    util->utilization = utilization;
    out->verbose(CALL_INFO, DEBUG, 0, "Current utilization of Global Pool: %d\n", utilization);
    // element_index is not use in this case
    util->element_index = 0;
    for ( int i = 0; i < num_ports; i++ ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Updating QG in index %d, utilization = %u\n", i, utilization);
        queue_groups[i]->send(util->clone());
    }
    out->verbose(CALL_INFO, MODERATE, 0, "All queue groups were updated with current utilization\n");
    delete util;
}

/**
 * @brief Handler for packet buffer updates
 *
 * @param ev Event with packet buffer available blocks
 */
void
GlobalPoolUtil::handle_pkt_buffer_update(SST::Event* ev)
{
    SpaceEvent* space = dynamic_cast<SpaceEvent*>(ev);
    if ( space ) {

        safe_lock.lock();
        out->verbose(CALL_INFO, INFO, 0, "Receiving information about available blocks of packet_buffer\n");
        // COmpute global pool utilization
        unsigned int utilization = float(1 - (float(space->block_avail) / float(buffer_size))) * max_utilization;
        out->verbose(
            CALL_INFO, DEBUG, 0, "Global pool utilization: %d blocks, equivalent to utilization = %d\n",
            space->block_avail, utilization);
        // Update children
        update_mgs(utilization);
        update_qgs(utilization);
        safe_lock.unlock();

        // Release
        delete space;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

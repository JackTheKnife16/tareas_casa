#include <sst/core/sst_config.h>

#include "bytes_updater.h"

#include "sst/core/event.h"

#include "bytes_use_event.h"
#include "pq_index.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new BytesUpdater::BytesUpdater object
 *
 * @param id
 * @param params
 */
BytesUpdater::BytesUpdater(ComponentId_t id, Params& params) : Component(id)
{
    num_ports                    = params.find<uint32_t>("num_ports", 52);
    // Type of element
    type                         = params.find<int>("type", 1);
    // MA config file
    std::string   ma_config_path = params.find<std::string>("ma_config_file", "qs_1_0/ma_config.json");
    JSON          ma_config;
    std::ifstream ma_config_file(ma_config_path);
    ma_config_file >> ma_config;
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":BytesUpdater[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Bytes Updater --\n");

    comp_name = "";

    // Creating element connectors subcomponents
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("accounting_elements");
    if ( lists ) {
        switch ( type ) {
        case 1: // MG
        {
            // Memory groups
            comp_name            = comp_name + "mg_";
            int mg_elements_size = ma_config["Egress_MG"]["Mapping"].size();
            for ( int i = 0; i < mg_elements_size; i++ ) {
                int mg_index = ma_config["Egress_MG"]["Mapping"][i]["MG"];

                create_connector(mg_index, mg_index, lists);
            }
            break;
        }
        case 2: // QG
        {
            // Queue Groups
            comp_name = comp_name + "qg_";
            for ( unsigned int i = 0; i < num_ports; i++ ) {
                if ( lists->isPopulated(i) ) { create_connector(i, i, lists); }
                else {
                    out->verbose(CALL_INFO, MODERATE, 0, "Accounting element %d - %d was not provided!\n", type, i);
                }
            }
            break;
        }
        case 3: // PQ
        {
            // Physical queues
            comp_name = comp_name + "pq_";
            for ( int i = 0; i < num_ports; i++ ) {
                for ( int j = 0; j < PRI_QUEUES; j++ ) {
                    pq_index_t pq_index;
                    pq_index.index_s.port     = i;
                    pq_index.index_s.priority = j;

                    int index_subcomponent = i * PRI_QUEUES + j;

                    create_connector(index_subcomponent, pq_index.index_i, lists);
                }
            }
            break;
        }
        default:
            out->fatal(CALL_INFO, -1, "Accounting element type unknown!\n");
            break;
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "Should provide element connector subcomponents!\n");
    }

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    bytes_tracker = configureLink(
        "bytes_tracker", base_tc, new Event::Handler<BytesUpdater>(this, &BytesUpdater::handle_new_update));
    out->verbose(CALL_INFO, MODERATE, 0, "bytes_tracker link was configured\n");

    // Check it is not null
    assert(bytes_tracker);

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Creates a connector subcomponent and puts it the respective index
 *
 * @param index Subcomponent index
 * @param map_index Accounting element index
 * @param lists List of subcomponents
 */
void
BytesUpdater::create_connector(int index, int map_index, SubComponentSlotInfo* lists)
{
    accouting_elements[map_index] = lists->create<ElementConnector>(index, ComponentInfo::SHARE_STATS);

    out->verbose(
        CALL_INFO, MODERATE, 0, "Created a new subcomponent 'element_connector' to communicate with %s%d.\n",
        comp_name.c_str(), index);

    accouting_elements[map_index]->set_accounting_element_link(NULL);
    assert(accouting_elements[map_index]);
}

/**
 * @brief Handler for new bytes in use events
 *
 * @param ev Event with current bytes in use
 */
void
BytesUpdater::handle_new_update(SST::Event* ev)
{
    BytesUseEvent* bytes_in_use = dynamic_cast<BytesUseEvent*>(ev);
    if ( bytes_in_use ) {
        out->verbose(
            CALL_INFO, DEBUG, 0, "Received BytesUseEvent with bytes in use = %dB.\n", bytes_in_use->bytes_in_use);
        // Send the bytes in use to the given elements
        accouting_elements[bytes_in_use->element_index]->send_event(bytes_in_use);
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

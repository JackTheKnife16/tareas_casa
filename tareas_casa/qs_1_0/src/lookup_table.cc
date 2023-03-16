#include <sst/core/sst_config.h>

#include "lookup_table.h"

#include "sst/core/event.h"

#include "packet_event.h"
#include "pq_index.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;
/**
 * @brief Construct a new LookUpController::LookUpController object
 *
 * @param id
 * @param params
 */
LookUpController::LookUpController(ComponentId_t id, Params& params) : Component(id)
{
    num_ports                 = params.find<uint32_t>("num_ports", 52);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":LookUpController[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- LookUp Controller --\n");

    base_tc = registerTimeBase("1ps", true);

    // Configure in subcomponents
    set_mg_elements();
    set_qg_elements();
    set_pq_elements();

    set_lookup_tables();

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Sets the MG elements connectors that will be requesting in the look up
 * table
 *
 */
void
LookUpController::set_mg_elements()
{
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("mg_elements");
    if ( lists ) {
        for ( int i = 0; i <= lists->getMaxPopulatedSlotNumber(); i++ ) {
            if ( lists->isPopulated(i) ) {
                ElementConnector* element = lists->create<ElementConnector>(i, ComponentInfo::SHARE_NONE);
                element->set_accounting_element_link(
                    new Event::Handler<LookUpController>(this, &LookUpController::handle_new_lookup));

                mg_elements[i] = element;
                assert(mg_elements[i]);

                out->verbose(CALL_INFO, MODERATE, 0, "mg_elements[%d] link was configured\n", i);
            }
        }
    }
    else {
        out->verbose(CALL_INFO, 1, 0, "No MG elements found for this controller\n");
    }
}

/**
 * @brief Sets the QG elements connectors that will be requesting in the look up
 * table
 *
 */
void
LookUpController::set_qg_elements()
{
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("qg_elements");
    if ( lists ) {
        for ( int i = 0; i < num_ports; i++ ) {
            ElementConnector* element = lists->create<ElementConnector>(i, ComponentInfo::SHARE_NONE);
            element->set_accounting_element_link(
                new Event::Handler<LookUpController>(this, &LookUpController::handle_new_lookup));
            qg_elements.push_back(element);
            assert(element);

            out->verbose(CALL_INFO, MODERATE, 0, "qg_elements[%d] link was configured\n", i);
        }
    }
    else {
        out->verbose(CALL_INFO, MODERATE, 0, "No QG elements found for this controller\n");
    }
}

/**
 * @brief Sets the PQ elements connectors that will be requesting in the look up
 * table
 *
 */
void
LookUpController::set_pq_elements()
{
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("pq_elements");
    if ( lists ) {
        for ( int i = 0; i < num_ports; i++ ) {
            for ( int j = 0; j < PRI_QUEUES; j++ ) {
                pq_index_t pq_index;
                pq_index.index_s.port     = i;
                pq_index.index_s.priority = j;

                int index_subcomponent = i * PRI_QUEUES + j;

                ElementConnector* element =
                    lists->create<ElementConnector>(index_subcomponent, ComponentInfo::SHARE_NONE);
                element->set_accounting_element_link(
                    new Event::Handler<LookUpController>(this, &LookUpController::handle_new_lookup));

                pq_elements[pq_index.index_i] = element;
                assert(pq_elements[pq_index.index_i]);

                out->verbose(CALL_INFO, MODERATE, 0, "pq_elements[%d] link was configured\n", pq_index.index_i);
            }
        }
    }
    else {
        out->verbose(CALL_INFO, 1, 0, "No QG elements found for this controller\n");
    }
}

/**
 * @brief Sets the lookup table subcomponents as the user defines
 *
 */
void
LookUpController::set_lookup_tables()
{
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("look_up_table");
    if ( lists ) {
        for ( int i = 0; i <= lists->getMaxPopulatedSlotNumber(); i++ ) {
            if ( lists->isPopulated(i) ) {
                LookUpTable* lut = lists->create<LookUpTable>(i, ComponentInfo::SHARE_NONE);
                LUTs.push_back(lut);

                out->verbose(CALL_INFO, MODERATE, 0, "Lookup table %d configured\n", i);
                assert(lut);
            }
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "No lookup tables for this controller\n");
    }
}

/**
 * @brief Sends the response to the accounting element that requested the lookup
 *
 * @param type Type of element (1: MG, 2: QG, 3: PQ)
 * @param element_index Accounitng element index
 * @param res Response event
 */
void
LookUpController::send_response(int type, unsigned int element_index, LookUpResponseEvent* res)
{
    // Send response to correct element
    switch ( type ) {
    case 1:
        mg_elements[element_index]->send_event(res);
        out->verbose(CALL_INFO, DEBUG, 0, "Sending lookup response through mg_elements[%d] link\n", element_index);
        break;
    case 2:
        qg_elements.at(element_index)->send_event(res);
        out->verbose(CALL_INFO, DEBUG, 0, "Sending lookup response through qg_elements[%d] link\n", element_index);
        break;
    case 3:
        pq_elements[element_index]->send_event(res);
        out->verbose(CALL_INFO, DEBUG, 0, "Sending lookup response through pq_elements[%d] link\n", element_index);
        break;
    default:
        out->fatal(CALL_INFO, -1, "Error! Undefined type of element!\n");
        break;
    }
}

/**
 * @brief Handler for lookup requests
 *
 * @param ev Event with the Lookup request
 */
void
LookUpController::handle_new_lookup(SST::Event* ev)
{
    LookUpRequestEvent* req = dynamic_cast<LookUpRequestEvent*>(ev);
    if ( req ) {
        // Make request
        int          bias          = req->bias;
        int          type          = req->type;
        unsigned int element_index = req->index;
        unsigned int addr          = req->lookup_address;

        out->verbose(
            CALL_INFO, DEBUG, 0, "Received LookUp request: type: %d, index: %d, bias: %d, addr: 0x%0X\n", type,
            element_index, bias, addr);

        // Get utilization from LUT[bias] at addr
        unsigned int         util = LUTs.at(bias)->lookup(addr);
        LookUpResponseEvent* res  = new LookUpResponseEvent();
        res->result               = util;

        // Send the response
        send_response(type, element_index, res);

        // Release memory
        delete req;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Construct a new LookUpTable::LookUpTable object
 *
 * @param id
 * @param params
 */
LookUpTable::LookUpTable(ComponentId_t id, Params& params) : SubComponent(id)
{
    // LookUp table values
    std::string   lookup_config_path = params.find<std::string>("table_file", "qs_1_0/lookup.json");
    JSON          lookup_table_values;
    std::ifstream lookup_table_file(lookup_config_path);
    lookup_table_file >> lookup_table_values;
    // Init memory
    memory = new unsigned int[LUT_ENTRIES];
    for ( int i = 0; i < LUT_ENTRIES; i++ ) {
        memory[i] = lookup_table_values[i];
    }

    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":LookUpTable[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- LookUp Table --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Data file: %s\n", lookup_config_path.c_str());

    base_tc = registerTimeBase("1ps", true);
}

/**
 * @brief Performs the look up operation qith a given address
 *
 * @param address Look up addresss
 * @return unsigned int Look up response
 */
unsigned int
LookUpTable::lookup(unsigned int address)
{
    out->verbose(CALL_INFO, DEBUG, 0, "Performing look up operation with address: 0x%0X\n", address);

    unsigned int response = memory[address];

    out->verbose(CALL_INFO, DEBUG, 0, "Look up response: %u\n", response);

    return response;
}

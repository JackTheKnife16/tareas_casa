#include <sst/core/sst_config.h>

#include "dualq_lookup_controller.h"

#include "sst/core/event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

DualQLookUpController::DualQLookUpController(ComponentId_t id, Params& params) : LookUpController(id, params)
{
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("lq_elements");
    if ( lists ) {
        for ( int i = 0; i < num_ports; i++ ) {
            ElementConnector* element = lists->create<ElementConnector>(i, ComponentInfo::SHARE_NONE);
            element->set_accounting_element_link(
                new Event::Handler<DualQLookUpController>(this, &DualQLookUpController::handle_new_lookup));
            out->verbose(CALL_INFO, MODERATE, 0, "lq_elements[%d] link was configured\n", i);
            lq_elements.push_back(element);
            assert(element);
        }
    }
    else {
        out->verbose(CALL_INFO, MODERATE, 0, "No QG elements found for this controller\n");
    }
}

void
DualQLookUpController::send_response(int type, unsigned int element_index, LookUpResponseEvent* res)
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
    case 4:
        lq_elements.at(element_index)->send_event(res);
        out->verbose(CALL_INFO, DEBUG, 0, "Sending lookup response through lq_elements[%d] link\n", element_index);
        break;
    default:
        out->fatal(CALL_INFO, -1, "Error! Undefined type of element!\n");
        break;
    }
}

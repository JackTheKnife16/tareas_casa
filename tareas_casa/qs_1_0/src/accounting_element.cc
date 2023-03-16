#include <sst/core/sst_config.h>

#include "accounting_element.h"

#include "sst/core/event.h"

#include "bytes_use_event.h"
#include "lookup_event.h"
#include "pq_index.h"
#include "util_event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>
#include <math.h> /* pow */

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new AccountingElement::AccountingElement object
 *
 * @param id
 * @param params
 */
AccountingElement::AccountingElement(ComponentId_t id, Params& params) : Component(id)
{
    // Element values
    max_bytes = params.find<unsigned int>("max_bytes", 250000);
    bias      = params.find<unsigned int>("bias", 0);

    // Identifying the accounting element
    type                  = params.find<int>("type", 1);
    index                 = params.find<unsigned int>("index", 0);
    unsigned int priority = params.find<unsigned int>("priority", 0);
    // If the element is PQ, set the correct index
    if ( type == 3 ) {
        pq_index_t pq_index;
        pq_index.index_s.port     = index;
        pq_index.index_s.priority = priority;

        index = pq_index.index_i;
    }

    // Parents translation config file
    std::string parent_translation_config_path =
        params.find<std::string>("parent_translation_config", "qs_1_0/parent_translation.json");
    JSON          parent_translation_config;
    std::ifstream parent_translation_file(parent_translation_config_path);
    parent_translation_file >> parent_translation_config;
    for ( int i = 0; i < BITS_UTIL; i++ ) {
        parent_translation[i] = parent_translation_config[i];
    }

    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":AccountingElement[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Accounting Element --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Max Bytes: %d B\n", max_bytes);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Bias: %d\n", bias);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Type: %d\n", type);
    if ( type != 3 )
        out->verbose(CALL_INFO, MODERATE, 0, "|--> Index: %d\n", index);
    else
        out->verbose(CALL_INFO, MODERATE, 0, "|--> Index: %d, Priority: %d\n", index, priority);

    // Initial util values
    parents_utilization = 0;
    bytes_in_use        = 0;
    current_utilization = 0;

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    parent_utilization = configureLink(
        "parent_utilization", base_tc,
        new Event::Handler<AccountingElement>(this, &AccountingElement::handle_parent_update));
    if ( type != 3 ) out->verbose(CALL_INFO, MODERATE, 0, "parent_utilization link was configured\n");

    bytes_tracker = configureLink(
        "bytes_tracker", base_tc,
        new Event::Handler<AccountingElement>(this, &AccountingElement::handle_bytes_tracker_update));
    out->verbose(CALL_INFO, MODERATE, 0, "bytes_tracker link was configured\n");

    lookup_table = configureLink(
        "lookup_table", base_tc, new Event::Handler<AccountingElement>(this, &AccountingElement::handle_lookup_result));
    out->verbose(CALL_INFO, MODERATE, 0, "lookup_table link was configured\n");

    child_utilization = configureLink("child_utilization", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "child_utilization link was configured\n");

    // Check it is not null
    assert(parent_utilization);
    assert(bytes_tracker);
    assert(lookup_table);
    assert(child_utilization);

    // Create statistics
    bytes_util  = registerStatistic<uint32_t>("bytes_util", "1");
    parent_util = registerStatistic<uint32_t>("parent_util", "1");
    utilization = registerStatistic<uint32_t>("utilization", "1");

    // Set children subcomponents
    set_children();

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Create children subcomponents
 *
 */
void
AccountingElement::set_children()
{
    // Get user define subcomponents
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("children");
    if ( lists ) {
        // Iterate over the given subcomponets and create them
        for ( int i = 0; i <= lists->getMaxPopulatedSlotNumber(); i++ ) {
            if ( lists->isPopulated(i) ) {
                out->verbose(
                    CALL_INFO, MODERATE, 0,
                    "Creating a subcomponent, element_connector, to communicate %s with pq_%d\n", getName().c_str(), i);
                ElementConnector* element = lists->create<ElementConnector>(i, ComponentInfo::SHARE_NONE);
                element->set_accounting_element_link(NULL);
                children.push_back(element);
                assert(element);
            }
        }
    }
    else {
        out->verbose(CALL_INFO, DEBUG, 0, "No MG elements found for this controller\n");
    }
}

/**
 * @brief Handler for parent utilization update
 *
 * @param ev Event with the parent update
 */
void
AccountingElement::handle_parent_update(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    if ( util ) {
        safe_parent_lock.lock();
        // Update parent's utilization
        parents_utilization = util->utilization;

        // Parent utilization stats
        parent_util->addData(parents_utilization);

        out->verbose(
            CALL_INFO, DEBUG, 0,
            "Receiving parent utilization through parent_utilization link, parent utilization: %u, element index = "
            "%d\n",
            parents_utilization, util->element_index);

        // Request utilization in LookUp
        lookup_utilization();
        safe_parent_lock.unlock();

        // Release memory
        delete util;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for bytes in use updates
 *
 * @param ev Event with bytes tracker updates
 */
void
AccountingElement::handle_bytes_tracker_update(SST::Event* ev)
{
    BytesUseEvent* bytes_update = dynamic_cast<BytesUseEvent*>(ev);
    if ( bytes_update ) {

        out->verbose(
            CALL_INFO, DEBUG, 0, "Receiving current bytes in use through bytes_tracker link: %dB\n",
            bytes_update->bytes_in_use);

        safe_bytes_lock.lock();
        // Update bytes in use
        if ( bytes_update->bytes_in_use > max_bytes ) {
            // bytes_in_use = max_bytes;
            out->verbose(
                CALL_INFO, DEBUG, 0, "Bytes_in_use is higher than max_bytes! --> in_use = %dB, max_bytes = %dB\n",
                bytes_update->bytes_in_use, max_bytes);
        }
        else
            bytes_in_use = bytes_update->bytes_in_use;

        // Bytes utilization stats
        bytes_util->addData(int((float(bytes_in_use) / float(max_bytes)) * MAX_UTIL_VALUE));

        safe_bytes_lock.unlock();

        // Release memory
        delete bytes_update;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Makes a lookup request by creating the addres from the child and
 * parent utilization
 *
 */
void
AccountingElement::lookup_utilization()
{
    LookUpRequestEvent* util_req = new LookUpRequestEvent();
    util_req->bias               = bias;

    // Compute child utilization
    safe_bytes_lock.lock();
    unsigned int child_utilization = int((float(bytes_in_use) / float(max_bytes)) * MAX_UTIL_VALUE);
    safe_bytes_lock.unlock();

    unsigned int actual_parent_utilization = parent_translation[parents_utilization];

    // Compute and apply bits masks
    unsigned int mask_parent = int(pow(2, bits_parent)) - 1;
    unsigned int mask_child  = int(pow(2, bits_child)) - 1;
    actual_parent_utilization &= mask_parent;
    child_utilization &= mask_child;

    // Create lookup address
    child_utilization        = child_utilization << bits_parent;
    unsigned int address     = child_utilization | actual_parent_utilization;
    util_req->lookup_address = address;

    out->verbose(
        CALL_INFO, DEBUG, 0, "Child_utilization = %d, address = %d, type = %d, index = %d\n", child_utilization,
        address, type, index);

    // Set index and type
    util_req->type  = type;
    util_req->index = index;

    // Request utilization
    lookup_table->send(util_req);
}

/**
 * @brief Handler for lookup result
 *
 * @param ev Event with new utilization
 */
void
AccountingElement::handle_lookup_result(SST::Event* ev)
{
    LookUpResponseEvent* util_result = dynamic_cast<LookUpResponseEvent*>(ev);
    if ( util_result ) {
        safe_util_lock.lock();
        /// Update bytes in use
        current_utilization = util_result->result;

        // Child utilization stats
        utilization->addData(current_utilization);

        out->verbose(CALL_INFO, DEBUG, 0, "Current element utilization (type = %d): %d\n", type, current_utilization);
        // Update child
        UtilEvent* util     = new UtilEvent();
        util->utilization   = current_utilization;
        util->element_index = index;

        // Update current utilization
        child_utilization->send(util);

        // Update children
        for ( int i = 0; i < children.size(); i++ )
            children.at(i)->send_event(util->clone());

        safe_util_lock.unlock();

        // Release memory
        delete util_result;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

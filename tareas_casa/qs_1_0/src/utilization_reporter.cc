#include <sst/core/sst_config.h>

#include "utilization_reporter.h"

#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new UtilizationReporter::UtilizationReporter object
 *
 * @param id
 * @param params
 */
UtilizationReporter::UtilizationReporter(ComponentId_t id, Params& params) : Component(id)
{
    // Number of ports
    num_ports                 = params.find<uint32_t>("num_ports", 52);
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":UtilizationReporter[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Utilization Reporter --\n");

    base_tc = registerTimeBase("1ps", true);

    // Configure out link
    acceptance_checker = configureLink("acceptance_checker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "acceptance_checker link was configured\n");

    // Verify that links are not NULL
    assert(acceptance_checker);

    // Setting subcomponents
    set_accounting_elements();

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Sets the accountin elements subcomponents that will send updates
 *
 */
void
UtilizationReporter::set_accounting_elements()
{
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("accounting_elements");
    if ( lists ) {
        for ( int i = 0; i <= lists->getMaxPopulatedSlotNumber(); i++ ) {
            if ( lists->isPopulated(i) ) {
                ElementConnector* element = lists->create<ElementConnector>(i, ComponentInfo::SHARE_NONE);
                out->verbose(
                    CALL_INFO, MODERATE, 0,
                    "Creating an 'element_connector' subcomponent to "
                    "communicate with '%s_%d' from '%s'\n",
                    getName().substr(0, 2).c_str(), i, getName().c_str());

                element->set_accounting_element_link(
                    new Event::Handler<UtilizationReporter>(this, &UtilizationReporter::handle_new_update));
                accounting_elements.push_back(element);
                assert(element);
            }
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "There are nom accounting elements in this component\n");
    }
}

/**
 * @brief Handles outcoming packet to the packet buffer
 *
 * @param ev Event with the utilization
 */
void
UtilizationReporter::handle_new_update(SST::Event* ev)
{
    UtilEvent* util = dynamic_cast<UtilEvent*>(ev);
    safe_lock.lock();
    acceptance_checker->send(ev);

    out->verbose(
        CALL_INFO, DEBUG, 0,
        "Sending event that contains utilization, through acceptance_checker "
        "link, element_index = %d, utilization = %u\n",
        util->element_index, util->utilization);
    safe_lock.unlock();
}

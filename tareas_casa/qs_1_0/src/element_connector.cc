#include <sst/core/sst_config.h>

#include "element_connector.h"

#include "sst/core/event.h"

#include "packet_event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new ElementConnector::ElementConnector object
 *
 * @param id
 * @param params
 */
ElementConnector::ElementConnector(ComponentId_t id, Params& params) : SubComponent(id)
{
    index                     = params.find<std::string>("index", "0");
    type                      = params.find<std::string>("type", "MG");
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":ElementConnector[@p:@l]: ";
    // std::string log_file = "logs/" + getName() + ".log";
    // out = new Output(prefix.str(), verbose, 0, SST::Output::FILE, log_file);
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, DEBUG, 0, "-- Element Connector --\n");
    out->verbose(CALL_INFO, DEBUG, 0, "|--> Type: %s\n", type.c_str());
    out->verbose(CALL_INFO, DEBUG, 0, "|--> Index: %s\n", index.c_str());

    base_tc = registerTimeBase("1ps", true);
}

/**
 * @brief Sets the accounting elements link and the handle event for that link
 *
 * @param handle_event Event handler for the link, if NULL it is ignore
 */
void
ElementConnector::set_accounting_element_link(SST::Event::HandlerBase* handle_event)
{
    if ( handle_event == NULL )
        accounting_element = configureLink("accounting_element", base_tc);
    else
        accounting_element = configureLink("accounting_element", base_tc, handle_event);

    out->verbose(CALL_INFO, DEBUG, 0, "A new accounting_element link was created\n");

    assert(accounting_element);
}

/**
 * @brief Sends a event to the accounting element link
 *
 * @param ev Event to send
 */
void
ElementConnector::send_event(SST::Event* ev)
{
    out->verbose(CALL_INFO, DEBUG, 0, "Sending new event through accounting_element link!\n");
    accounting_element->send(ev);
}

#include <sst/core/sst_config.h>

#include "event_forwarder.h"

#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new EventForwarder::EventForwarder object
 *
 * @param id
 * @param params
 */
EventForwarder::EventForwarder(ComponentId_t id, Params& params) : Component(id)
{

    // Number of ports
    num_inputs                = params.find<uint32_t>("num_inputs", 52);
    num_outputs               = params.find<uint32_t>("num_outputs", 1);
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":EventForwarder[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Event Forwarder --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Inputs: %d\n", num_inputs);

    base_tc = registerTimeBase("1ps", true);

    // Configure out links
    for ( int i = 0; i < num_outputs; i++ ) {
        std::string output_id = "output_" + std::to_string(i);
        output_components.push_back(configureLink(output_id, base_tc));
        out->verbose(CALL_INFO, MODERATE, 0, "output_%d link was configured\n", i);
        assert(output_components[i]);
    }
    // Configure in links
    Event::Handler<EventForwarder>* inputs_handler =
        new Event::Handler<EventForwarder>(this, &EventForwarder::handle_new_event);
    for ( int i = 0; i < num_inputs; i++ ) {
        std::string input_id = "input_" + std::to_string(i);
        input_components[i]  = configureLink(input_id, base_tc, inputs_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "input_%d link was configured\n", i);
        assert(input_components[i]);
    }

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Handles a new incoming event and forwards it to all output components
 *
 * @param ev The incoming event
 */
void
EventForwarder::handle_new_event(SST::Event* ev)
{
    for ( int i = 0; i < num_outputs; i++ ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Forwarding event to output component %d\n", i);
        output_components[i]->send(ev->clone());
    }

    delete ev;
}

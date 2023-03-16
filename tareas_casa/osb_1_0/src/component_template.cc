#include <sst/core/sst_config.h>

#include "component_template.h"

#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new ComponentTemplate::ComponentTemplate object
 *
 * @param id
 * @param params
 */
ComponentTemplate::ComponentTemplate(ComponentId_t id, Params& params) : Component(id)
{
    // Port number
    port                      = params.find<uint32_t>("port", 0);
    // Port group index
    pg_index                  = params.find<uint32_t>("pg_index", 0);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":ComponentTemplate[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Component template --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port: %d\n", port);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port Group Index: %d\n", pg_index);

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    // Input link with handler
    input_link = configureLink(
        "traffic_gen", base_tc, new Event::Handler<ComponentTemplate>(this, &ComponentTemplate::input_handler));

    // Output link with no handler
    output_link = configureLink("output", base_tc);

    // Check if links are connected
    assert(input_link);
    assert(output_link);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Initial stage in SST simulations.
 *
 * @param phase Current initialization phase.
 */
void
ComponentTemplate::init(uint32_t phase)
{}

/**
 * @brief Setup stage which starts at time 0.
 *
 */
void
ComponentTemplate::setup()
{}


/**
 * @brief Handler for input events
 *
 * @param ev Event
 */
void
ComponentTemplate::input_handler(SST::Event* ev)
{}

/**
 * @brief Finish stage of SST
 *
 */
void
ComponentTemplate::finish()
{}
#include <sst/core/sst_config.h>

#include "final_slice.h"

#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new FinalSlice::FinalSlice object
 *
 * @param id
 * @param params
 */
FinalSlice::FinalSlice(ComponentId_t id, Params& params) : Component(id)
{
    // Clock frequency
    UnitAlgebra frequency     = params.find<UnitAlgebra>("frequency", "1500 MHz");
    // Number of Buffer Slices
    buffer_slices             = params.find<uint32_t>("buffer_slices", 2);
    // Number of Port Group Interfaces
    port_group_interfaces = params.find<uint32_t>("port_group_interfaces", 4);
    // Port per Buffer Slice
    ports_per_buffer_slice          = params.find<uint32_t>("ports_per_buffer_slice", 32);
    // Number of port per Port Group Interface
    ports_per_pg                = params.find<uint32_t>("ports_per_pg", 16);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":CentralArbiter[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Central Arbiter --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Frequency: %s\n", frequency.toStringBestSI().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Buffer Slices: %d\n", buffer_slices);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port Group Interfaces: %d\n", port_group_interfaces);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Ports per Buffer Slice: %d\n", ports_per_buffer_slice);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Ports per PG: %d\n", ports_per_pg);

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    // Input link with handler
    fds_input_link = configureLink(
        "fds_input", base_tc, new Event::Handler<FinalSlice>(this, &FinalSlice::fds_receiver));

    // Output link with no handler
    output_link = configureLink("output", base_tc);

    // Check if links are connected
    //assert(input_link);
    //assert(output_link);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}


/**
 * @brief Handler for input events
 *
 * @param ev Event
 */
void
FinalSlice::fds_receiver(SST::Event* ev)
{
    FDSEvent* fds = dynamic_cast<FDSEvent*>(ev);
    uint32_t port = fds->get_src_port();
    buffer_slice_idx = (port - (port % ports_per_buffer_slice)) / ports_per_buffer_slice;
    std::cout << "FREDDY port = "<< port << " buffer_slice_idx = " << buffer_slice_idx << std::endl;
}

/**
 * @brief Finish stage of SST
 *
 */
void
FinalSlice::finish()
{}
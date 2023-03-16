#include <sst/core/sst_config.h>

#include "buffer_slice_mux.h"

#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new BufferSliceMux::BufferSliceMux object
 *
 * @param id
 * @param params
 */
BufferSliceMux::BufferSliceMux(ComponentId_t id, Params& params) : Component(id)
{
    // Number of buffer slices
    buffer_slices             = params.find<uint32_t>("buffer_slices", 2);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":BufferSliceMux[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Buffer Slice Mux --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of Buffer Slices: %d\n", buffer_slices);

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    for ( int i = 0; i < buffer_slices; i++ ) {
        SST::Link* fds_in = configureLink(
            "fds_in_" + std::to_string(i), base_tc,
            new Event::Handler<BufferSliceMux>(this, &BufferSliceMux::transaction_receiver));
        fds_in_links.push_back(fds_in);
        assert(fds_in);
    }

    // Output link with no handler
    fds_out_link = configureLink("fds_out", base_tc);

    // Check if links are connected
    // TODO: check this when everything is connected
    assert(fds_out_link);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Sends the given FDS to the Lookup Pipeline
 *
 * @param fds FDS Event to send
 */
void
BufferSliceMux::server(FDSEvent* fds)
{
    out->verbose(
        CALL_INFO, MODERATE, 0, "Sending FDS to the FWD Lookup Pipeline: pkt_id=%u src_port=%d pri=%d\n", fds->pkt_id,
        fds->src_port, fds->priority);
    // TODO: Uncomment this link
    fds_out_link->send(fds);
}

/**
 * @brief Handler that receives the incoming FDS Events and calls the server method
 *
 * @param ev FDS Event
 */
void
BufferSliceMux::transaction_receiver(SST::Event* ev)
{
    FDSEvent* fds = dynamic_cast<FDSEvent*>(ev);
    if ( fds ) {
        safe_lock.lock();
        server(fds);
        safe_lock.unlock();
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Finish stage of SST
 *
 */
void
BufferSliceMux::finish()
{}
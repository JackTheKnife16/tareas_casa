#ifndef BUFFER_SLICE_MUX_H
#define BUFFER_SLICE_MUX_H

#include "fds_event.h"
#include "model_global.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>
#include <vector> // std::vector

/**
 * @brief Buffer Slice Mux component receives and forwards the granted FDS Events
 *
 */
class BufferSliceMux : public SST::Component
{

public:
    BufferSliceMux(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the BufferSliceMux object
     *
     */
    ~BufferSliceMux() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        BufferSliceMux, "OSB_1_0", "BufferSliceMux", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Receives packets, converts them into chunks, and sends them at the configured rate",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({ "buffer_slices", "Number of Buffer Slices in the OSB.", "2" }, { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "fds_in_%(buffer_slices)d", "Links to the input Buffer Slices", { "OSB_1_0.FDSEvent" } },
        { "fds_out", "Link to FWD Lookup Pipeline component", { "OSB_1_0.FDSEvent" } })

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void finish();

private:
    BufferSliceMux();                      // for serialization only
    BufferSliceMux(const BufferSliceMux&); // do not implement
    void operator=(const BufferSliceMux&); // do not implement

    // Control
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Attributes
    uint32_t buffer_slices;

    // Links
    std::vector<SST::Link*> fds_in_links;
    SST::Link*              fds_out_link;

    // Method
    void server(FDSEvent* fds);

    // Handlers
    void transaction_receiver(SST::Event* ev);
};


#endif
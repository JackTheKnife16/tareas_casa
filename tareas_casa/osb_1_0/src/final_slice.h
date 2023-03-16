#ifndef FINAL_SLICE_H
#define FINAL_SLICE_H

#include "model_global.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#include "fds_event.h"

/**
 * @brief Ethernet Port component that receives packets from the Traffic Generator and forwards them to the
 * Port Group Interface in chunks at the speed configured.
 *
 */
class FinalSlice : public SST::Component
{

public:
    FinalSlice(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~FinalSlice() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        FinalSlice, "OSB_1_0", "FinalSlice", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Receives packets, converts them into chunks, and sends them at the configured rate",
        COMPONENT_CATEGORY_NETWORK)

SST_ELI_DOCUMENT_PARAMS(
        { "frequency", "Clock frequency used as the delay to send every packet word.", "1500 MHz" },
        { "buffer_slices", "Number of BUffer Slcies in the OSB.", "2" },
        { "port_group_interfaces", "Number of Port Group Interfaces in the OSB", "4"},
        { "ports_per_buffer_slice", "Number of ports that a Buffer Slice has.", "32" },
        { "ports_per_pg", "Number of port per Port Group Interface.", "16" }, 
        { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "fds_input", "Link to the input component", { "OSB_1_0.FDSEvent" } },
        { "output", "Link to output component", { "OSB_1_0.PacketEvent" } })

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void finish();

private:
    FinalSlice();                         // for serialization only
    FinalSlice(const FinalSlice&); // do not implement
    void operator=(const FinalSlice&);    // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    SST::SimTime_t freq_delay;
    uint32_t       buffer_slices;
    uint32_t port_group_interfaces;
    uint32_t       ports_per_buffer_slice;
    uint32_t       ports_per_pg;
    uint32_t buffer_slice_idx;

    SST::Link* fds_input_link;
    SST::Link* output_link;

    // Handlers
    void fds_receiver(SST::Event* ev);
};


#endif
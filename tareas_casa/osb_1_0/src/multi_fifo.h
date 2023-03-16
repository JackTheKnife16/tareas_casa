#ifndef MULTI_FIFO_H
#define MULTI_FIFO_H

#include "logical_fifo.h"
#include "model_global.h"
#include "queue_element.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

/**
 * @brief Multi FIFO component that receives transactions and store them in a queue. The number of queues depend on the
 * ports and priorities.
 *
 */
class MultiFIFO : public SST::Component
{

public:
    MultiFIFO(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Multi FIFO object
     *
     */
    ~MultiFIFO() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        MultiFIFO, "OSB_1_0", "MultiFIFO", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Stores transaction in the ports FIFOs, and forwards them on request", COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "ports", "Number of ports.", "16" }, { "priorities", "Number of priorities.", "4" },
        { "port_ingress_slices", "Number of Port Ingress Slices connected to the Buffer Slice.", "2" },
        { "buffer_slice_index", "Buffer Slice index.", "0" }, { "width", "Size of entry in bytes.", "128" },
        { "packet_window_size", "Number of words representing the packet window need by the Lookup Pipeline.", "4" },
        { "pop_size", " Number of entries that a single pop must send.", "1" },
        { "depth_config", "Path to file with depth configuration for each FIFO.",
          "osb_1_0/config/multi_fifo/packet_fifo_config.json" },
        { "frontplane_config", "Frontplane configuration.", "osb_1_0/config/frontplane_config/4x800G.json" },
        { "req_enable", "Flag that indicates if the multi-FIFO sends IC and NIC requests.", "0" },
        { "overflow", "Indicates if the queues can overflow and increase their limits.", "0" }, { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "input_%(port_ingress_slices)d", "Link to the input component", { "OSB_1_0.PacketWordEvent" } },
        { "fifo_req", "Link to component that requets packets", { "OSB_1_0.FIFORequestEvent" } },
        { "output", "Link to the output component", { "OSB_1_0.PacketWordEvent" } },
        { "ic_req", "Link to the IC Req in Central Arbiter component", { "OSB_1_0.ArbiterRequestEvent" } },
        { "nic_req", "Link to the NIC Req port in Central Arbiter component", { "OSB_1_0.ArbiterRequestEvent" } },
        { "output", "Link to the output component", { "OSB_1_0.PacketWordEvent" } },
        { "fifo_update_%(port_ingress_slices)d", "Link to component that receives updates", { "OSB_1_0.FIFOStatusEvent" } })

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void init(uint32_t phase);

    void finish();

private:
    MultiFIFO();                      // for serialization only
    MultiFIFO(const MultiFIFO&);      // do not implement
    void operator=(const MultiFIFO&); // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    uint32_t                         ports;
    uint32_t                         priorities;
    uint32_t                         port_ingress_slices;
    uint32_t                         buffer_slice_index;
    uint32_t                         width;
    uint32_t                         packet_window_size;
    uint32_t                         pop_size;
    // REVISIT: If the priority is used, this is the map to change
    std::map<uint32_t, LogicalFIFO*> logical_fifos;
    bool                             req_enable;
    bool                             overflow;

    // It indicates if a req has been done from a port.
    // It is set to false when the head changes
    std::map<uint32_t, bool> reqs;


    std::map<uint32_t, SST::Link*> port_slice_links;
    std::map<uint32_t, SST::Link*> fifo_update_links;
    SST::Link*                     ic_req_link;
    SST::Link*                     nic_req_link;
    SST::Link*                     fifo_req_link;
    SST::Link*                     output_link;

    // Handlers
    void entry_receiver(SST::Event* ev);
    void server(SST::Event* ev);

    void updater(LogicalFIFO* fifo_to_update);
    void requester(LogicalFIFO* fifo_to_check);
};


#endif
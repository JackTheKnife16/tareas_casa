#ifndef BUFFER_PORT_MUX_H
#define BUFFER_PORT_MUX_H

#include "model_global.h"
#include "packet_word_event.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

/**
 * @brief Buffer Port Mux component forwards the packet data and FDS downstream when it receives a IC or NIC Grant Event
 *
 */
class BufferPortMux : public SST::Component
{

public:
    BufferPortMux(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~BufferPortMux() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        BufferPortMux, "OSB_1_0", "BufferPortMux", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "On IC or NIC Grant Events it forwards the packet data and FDS downstream", COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "buffer_slice_index", "Buffer slice number which the component belongs.", "0" },
        { "fifo_width", "Size of FIFO width.", "128" },
        { "pop_size", "Number of words that a FIFO sends when it pops packet data.", "2" }, { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "pkt_req", "Link to the Packet FIFO req port", { "OSB_1_0.FIFORequestEvent" } },
        { "pkt_in", "Link to the Packet FIFO output port", { "OSB_1_0.PacketWordEvent" } },
        { "fds_req", "Link to the FDS FIFO req port", { "OSB_1_0.FIFORequestEvent" } },
        { "fds_in", "Link to the FDS FIFO output port", { "OSB_1_0.FDSEvent" } },
        { "ic_gnt", "Link to the IC Grant Arbiter port", { "OSB_1_0.ArbiterGrantEvent" } },
        { "nic_gnt", "Link to the NIC Grant Arbiter port", { "OSB_1_0.ArbiterGrantEvent" } },
        { "pkt_out", "Link to the Data Packer component", { "OSB_1_0.PacketWordEvent" } },
        { "fds_out", "Link to Buffer Slice Mux component", { "OSB_1_0.FDSEvent" } })

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void finish();

private:
    BufferPortMux();                      // for serialization only
    BufferPortMux(const BufferPortMux&);  // do not implement
    void operator=(const BufferPortMux&); // do not implement

    // Control
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Attributes
    uint32_t buffer_slice_index;
    uint32_t fifo_width;
    uint32_t pop_size;

    // Word build
    uint32_t                     packet_word_count;
    PacketWordEvent*             pkt_word_in_progress;
    std::map<uint32_t, uint32_t> word_seq_num_count;

    // Links
    SST::Link* pkt_req_link;
    SST::Link* pkt_in_link;
    SST::Link* fds_req_link;
    SST::Link* fds_in_link;
    SST::Link* ic_gnt_link;
    SST::Link* nic_gnt_link;
    SST::Link* pkt_out_link;
    SST::Link* fds_out_link;

    // Methods
    void fds_ic_requestor(uint32_t port, uint32_t priority);
    void packet_data_requestor(uint32_t port, uint32_t priority);

    // Handlers
    void ic_grant_receiver(SST::Event* ev);
    void nic_grant_receiver(SST::Event* ev);
    void packet_data_sender(SST::Event* ev);
    void fds_ic_sender(SST::Event* ev);
};


#endif
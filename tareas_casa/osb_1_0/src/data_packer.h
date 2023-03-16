#ifndef DATA_PACKER_T
#define DATA_PACKER_T

#include "model_global.h"
#include "port_accumulator.h"
#include "receiver_buffer.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

/**
 * @brief Data Packer component changes the bus width by accumulating packet data to create a new word. It also add an
 * FDS hole to the new word.
 *
 */
class DataPacker : public SST::Component
{

public:
    DataPacker(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~DataPacker() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        DataPacker, "OSB_1_0", "DataPacker", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Changes the bus width by accumulating packet data to create a new word", COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "frequency", "Clock frequency used as the delay to send every packet word.", "1500 MHz" },
        { "buffer_slice_index", "Port group index.", "0" },
        { "port_ingress_slices", "Number of Port Ingress Slices connected to the Buffer Slice.", "2" },
        { "input_bus_width", "Size of input bus.", "128B" }, { "output_bus_width", "Size of output bus.", "256B" },
        { "fds_size", "Size of the FDS hole.", "80B" },
        { "initial_chunk", "Number of chunks that represent the initial chunk", "2" },
        { "non_initial_chunk", "Number of chunks that represent the non-initial chunk", "1" },
        { "frontplane_config", "Frontplane configuration.", "osb_1_0/config/frontplane_config/4x800G.json" },
        { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "in_packet_bus", "Link to the input component", { "OSB_1_0.PacketWordEvent" } },
        { "out_packet_bus", "Link to output component", { "OSB_1_0.PacketWordEvent" } })

    SST_ELI_DOCUMENT_STATISTICS(
        { "bytes_sent", "Bytes sent through the simulation", "Bytes", 1 },
        { "sent_packets", "Packets sent through the simulation", "units", 1 })

    void finish();

private:
    DataPacker();                      // for serialization only
    DataPacker(const DataPacker&);     // do not implement
    void operator=(const DataPacker&); // do not implement

    // Statistics
    Statistic<uint32_t>* bytes_sent;
    Statistic<uint32_t>* sent_packets;
    // Control
    std::mutex           safe_lock;
    SST::Output*         out;
    SST::TimeConverter*  base_tc;

    // Attributes
    SST::SimTime_t freq_delay;
    uint32_t       buffer_slice_index;
    uint32_t       port_ingress_slices;
    uint32_t       in_bus_width;
    uint32_t       out_bus_width;
    uint32_t       fds_size;
    uint32_t       init_chunk;
    uint32_t       non_init_chunk;

    // Elements
    std::map<uint32_t, PortAccumulator*> port_accumulators;

    // Delay routine
    SST::Link* delay_element_lookup_routine_link;
    SST::Link* delay_element_send_routine_link;
    bool       delay_routine_running;

    // Links
    SST::Link* in_packet_bus_link;
    SST::Link* out_packet_bus_link;

    // Delay routine
    void notify_delay_routine();
    void disable_delay_routine();

    // Handlers
    void packet_word_receiver(SST::Event* ev);
    void delay_element_lookup_routine(SST::Event* ev);
    void delay_element_send_routine(SST::Event* ev);
};


#endif
#ifndef PORT_GROUP
#define PORT_GROUP

#include "model_global.h"
#include "packet_word_event.h"
#include "receiver_buffer.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>
#include <vector> // std::vector

/**
 * @brief Port Group component that receives packet words from the Ethernet Ports, store them
 * and sends one word every frequency delay using round robin among the ports.
 *
 */
class PortGroup : public SST::Component
{

public:
    PortGroup(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~PortGroup() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        PortGroup, "OSB_1_0", "PortGroup", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Receives packet words from the Ethernet Ports and forwards them to the Classification Pipeline",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "frequency", "Clock frequency used as delay to send packet data.", "1500 MHz" },
        { "pg_index", "Port group index.", "0" }, { "num_ports", "Number of ports per port group.", "16" },
        { "ethernet_config", "Ethernet Interface configuration.", "osb_1_0/config/1x400G.json" },
        { "bus_width", "Data bus width size used as word size for packet data. (Bytes)", "128 B" },
        { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "port_%(num_ports)d", "Links to the Ethernet Port component", { "OSB_1_0.PacketWordEvent" } },
        { "packet_bus", "Link to the Packet Classification Pipeline", { "OSB_1_0.PacketWordEvent" } },
        { "fds_bus", "Link to the Packet Classification Pipeline", { "OSB_1_0.FDSEvent" } })

    SST_ELI_DOCUMENT_STATISTICS(
        { "bytes_sent", "Bytes sent through the simulation", "Bytes", 1 },
        { "sent_packets", "Packets sent through the simulation", "units", 1 })

    void finish();

private:
    PortGroup();                      // for serialization only
    PortGroup(const PortGroup&);      // do not implement
    void operator=(const PortGroup&); // do not implement

    // Statistics
    Statistic<uint32_t>* bytes_sent;
    Statistic<uint32_t>* sent_packets;

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    SST::SimTime_t               freq_delay;
    uint32_t                     pg_index;
    uint32_t                     num_ports;
    uint32_t                     bus_width;
    std::vector<ReceiverBuffer*> port_buffers;

    bool scheduler_running;
    int  queue_idx;
    bool chunk_sent_complete;

    std::vector<SST::Link*> port_links;
    SST::Link*              packet_bus_link;
    SST::Link*              fds_bus_link;
    SST::Link*              scheduler_routine_link;

    void notify_scheduler_routine();
    void disable_sheduler_routine();
    bool packet_data_in_buffers();

    // Handlers
    void scheduler_routine(SST::Event* ev);
};


#endif
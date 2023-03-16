#ifndef COMPONENT_TEMPLATE_H
#define COMPONENT_TEMPLATE_H

#include "fifo_events.h"
#include "model_global.h"
#include "packet_word_event.h"
#include "receiver_buffer.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

/**
 * @brief Acceptance Checker component that decides if a packet is accepted or dropped
 *
 */
class AcceptanceChecker : public SST::Component
{

public:
    AcceptanceChecker(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~AcceptanceChecker() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        AcceptanceChecker, "OSB_1_0", "AcceptanceChecker", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Receives packets, converts them into chunks, and sends them at the configured rate",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "frequency", "Clock frequency used as the delay to send every packet word.", "1500 MHz" },
        { "pg_index", "Port group index.", "0" }, { "bus_width", "Packet data bus width.", "256B" },
        { "mtu_config_file", "Path to the MTU configuration file.", "0" },
        { "packet_data_limit", "Number of MTU that a packet FIFO must have to accept a packet.", "1" },
        { "fds_limit", "Number of FDS entries that a FDS FIFO must have to accept a packet.", "1" },
        { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "in_packet_bus", "Link to the input packet data component", { "OSB_1_0.PacketWordEvent" } },
        { "in_fds_bus", "Link to the input FDS component", { "OSB_1_0.FDSEvent" } },
        { "drop_receiver", "Link to the Drop Receiver component", { "OSB_1_0.PacketWordEvent" } },
        { "packet_fifos", "Link to the Packet FIFO update port", { "OSB_1_0.FIFOStatusEvent" } },
        { "fds_fifos", "Link to the FDS FIFO update port", { "OSB_1_0.FIFOStatusEvent" } },
        { "out_packet_bus", "Link to packet data Multi-FIFO component", { "OSB_1_0.PacketWordEvent" } },
        { "out_fds_bus", "Link to FDS Multi-FIFO component", { "OSB_1_0.FDSEvent" } })

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void init(uint32_t phase);

    void finish();

private:
    AcceptanceChecker();                         // for serialization only
    AcceptanceChecker(const AcceptanceChecker&); // do not implement
    void operator=(const AcceptanceChecker&);    // do not implement

    // Control
    std::mutex          safe_lock;
    std::mutex          packet_data_safe_lock;
    std::mutex          fds_safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Attributes
    SST::SimTime_t               freq_delay;
    uint32_t                     pg_index;
    uint32_t                     bus_width;
    std::map<uint32_t, uint32_t> mtu_config;
    uint32_t                     packet_data_limit;
    uint32_t                     fds_limit;

    // Buffers
    ReceiverBuffer* packet_data_buffer;
    ReceiverBuffer* fds_buffer;
    bool            checker_routine_running;

    // Maps
    std::map<uint32_t, std::map<uint32_t, uint32_t>> packet_data_space_avail;
    std::map<uint32_t, std::map<uint32_t, uint32_t>> fds_space_avail;
    std::map<uint32_t, bool>                         accepted_by_port;

    // Links
    SST::Link* in_packet_bus_link;
    SST::Link* in_fds_bus_link;
    SST::Link* drop_receiver_link;
    SST::Link* packet_fifos_link;
    SST::Link* fds_fifos_link;
    SST::Link* out_packet_bus_link;
    SST::Link* out_fds_bus_link;
    // Self-links
    SST::Link* checker_routine_link;

    void notify_checker_routine();
    void disable_checker_routine();

    void push_packet_data(PacketWordEvent* pkt_word);
    void discard_packet_data(PacketWordEvent* pkt_word);

    // Handlers
    void checker_routine(SST::Event* ev);
    void packet_fifo_update_receiver(SST::Event* ev);
    void fds_fifo_update_receiver(SST::Event* ev);
};


#endif
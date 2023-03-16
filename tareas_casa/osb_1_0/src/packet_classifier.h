#ifndef PACKET_CLASSIFIER
#define PACKET_CLASSIFIER

#include "model_global.h"
#include "receiver_buffer.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

/**
 * @brief Ethernet Port component that receives packets from the Traffic Generator and forwards them to the
 * Port Group Interface in chunks at the speed configured.
 *
 */
class PacketClassifier : public SST::Component
{

public:
    PacketClassifier(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~PacketClassifier() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        PacketClassifier, "OSB_1_0", "PacketClassifier", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Receives packet words, waits a delay representing the slice lookups, and forwards the packet data",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "frequency", "Clock frequency used as the delay to send every packet word.", "1500 MHz" },
        { "pg_index", "Port group index.", "0" }, { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "in_packet_bus", "Link to the Port Group Interface", { "OSB_1_0.PacketWordEvent" } },
        { "in_fds_bus", "Link to the Port Group Interface", { "OSB_1_0.FDSEvent" } },
        { "out_packet_bus", "Link to Packet Classifier component", { "OSB_1_0.PacketWordEvent" } },
        { "out_fds_bus", "Link to Packet Classifier component", { "OSB_1_0.FDSEvent" }})

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void finish();

private:
    PacketClassifier();                        // for serialization only
    PacketClassifier(const PacketClassifier&); // do not implement
    void operator=(const PacketClassifier&);   // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    SST::SimTime_t freq_delay;
    uint32_t       pg_index;

    ReceiverBuffer* packet_buffer;
    ReceiverBuffer* fds_buffer;
    bool            delay_element_running;

    SST::Link* in_packet_bus_link;
    SST::Link* in_fds_bus_link;
    SST::Link* out_packet_bus_link;
    SST::Link* out_fds_bus_link;
    SST::Link* delay_routine_link;

    void notify_delay_routine();
    void disable_delay_routine();

    // Handlers
    void delay_routine(SST::Event* ev);
};


#endif
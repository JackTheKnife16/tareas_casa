#ifndef ETHERNET_PORT_H
#define ETHERNET_PORT_H

#include "model_global.h"
#include "packet_event.h"
#include "packet_request_event.h"
#include "packet_word_event.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

// Number of bit in a byte
#define BITS_IN_BYTE 8

/**
 * @brief Ethernet Port component that receives packets from the Traffic Generator and forwards them to the
 * Port Group Interface in chunks at the speed configured.
 *
 */
class EthernetPort : public SST::Component
{

public:
    EthernetPort(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~EthernetPort() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        EthernetPort, "OSB_1_0", "EthernetPort", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Receives packets, converts them into chunks, and sends them at the configured rate",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "port", "Port number.", "0" }, { "pg_index", "Port group index.", "0" },
        { "port_speed", "Port's speed.", "400Gb/s" }, { "offered_load", "Port's offered load", "100" },
        { "bus_width", "Data bus width size used as word size for packet data. (Bytes)", "128 B" },
        { "initial_chunk", "Number of chunks that represent the initial chunk", "2" },
        { "non_initial_chunk", "Number of chunks that represent the non-initial chunk", "1" },
        { "ipg_size", "Inter-Packet Gap size", "20" }, { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "traffic_gen", "Link to the Traffic Generator component", { "OSB_1_0.PacketEvent" } },
        { "output", "Link to Port Group Interface component", { "OSB_1_0.PacketWordEvent" } })

    SST_ELI_DOCUMENT_STATISTICS(
        { "bytes_sent", "Bytes sent through the simulation", "Bytes", 1 },
        { "sent_packets", "Packets sent through the simulation", "units", 1 })

    void init(uint32_t phase);

    void setup();

    void finish();

private:
    EthernetPort();                      // for serialization only
    EthernetPort(const EthernetPort&);   // do not implement
    void operator=(const EthernetPort&); // do not implement

    // Statistics
    Statistic<uint32_t>* bytes_sent;
    Statistic<uint32_t>* sent_packets;

    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Config attributes
    float            offered_load;
    uint32_t         port;
    uint32_t         pg_index;
    SST::UnitAlgebra port_speed;
    uint32_t         bus_width;
    uint32_t         init_chunk;
    uint32_t         non_init_chunk;
    uint32_t         ipg_size;

    // Fixed delays
    SST::SimTime_t default_delay;
    SST::SimTime_t ipg_delay;

    bool                         request_pkt;
    PacketEvent*                 current_packet;
    std::queue<PacketEvent*>     pkt_queue;
    std::queue<PacketWordEvent*> words_to_send;

    // Self-Links
    SST::Link* sender_link;
    SST::Link* schedule_link;
    // External links
    SST::Link* traffic_gen_link;
    SST::Link* output_link;

    // Methods
    void chunk_packet(PacketEvent* pkt);
    void schedule_word();

    // Handlers
    void new_packet_handler(SST::Event* ev);
    void sender_routine(SST::Event* ev);
    void schedule_routine(SST::Event* ev);
};


#endif
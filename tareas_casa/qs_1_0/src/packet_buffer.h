#ifndef PACKET_BUFFER_T
#define PACKET_BUFFER_T

#include "model_global.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define BLOCK_SIZE 256

#define FDS_SIZE 64

class PacketBuffer : public SST::Component
{
public:
    PacketBuffer(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Packet Buffer object
     *
     */
    ~PacketBuffer() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(PacketBuffer, "QS_1_0", "PacketBuffer",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Simple Packet Buffer model",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
      {"initial_blocks",
       "Intial and max amount of blocks for the Packet Buffer", "65536"},
      {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"acceptance_checker",
                          "Link that receives add packet transactions",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"buffer_manager",
                          "Link to the buffer manager for space updates",
                          {"QS_1_0.SpaceEvent"}},
                         {"egress_ports",
                          "Link that receives pull packet transactions",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"blocks_available",
       "Packet buffer blocks_available through the simulation", "Blocks", 1})

    void setup();

    void finish();

private:
    PacketBuffer();                      // for serialization only
    PacketBuffer(const PacketBuffer&);   // do not implement
    void operator=(const PacketBuffer&); // do not implement

    Statistic<uint32_t>* blocks_available;

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    int initial_blocks;
    int current_avail_blocks;
    int blocks_in_use; // Stats
    int num_pkts;      // Stats

    bool receive_pkt(unsigned int pkt_size);
    bool transmit_pkt(unsigned int pkt_size);
    void update_space();

    void handleReceive(SST::Event* ev);
    void handleTransmit(SST::Event* ev);

    SST::Link* acceptance_checker;
    SST::Link* buffer_manager;
    SST::Link* egress_ports;
};

#endif
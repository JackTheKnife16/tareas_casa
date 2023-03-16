#ifndef HOST_EGRESS_PORT_T
#define HOST_EGRESS_PORT_T

#include "model_global.h"
#include "packet_event.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define BITS_IN_BYTE 8
#define IPG_SIZE     20
#define NUM_PRI      8

class HostEgressPort : public SST::Component
{

public:
    HostEgressPort(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Egress Port object
     *
     */
    ~HostEgressPort() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(HostEgressPort, "QS_1_0", "HostEgressPort",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Send traffic to a given rate",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"port", "Port number. [0, 51]", "0"},
                          {"node", "Node number. [0, 10]", "0"},
                          {"port_bw", "Port's bandwith. [5Gb/s, 25Gb/s]",
                           "5Gb/s"},
                          {"num_nodes", "Number of nodes in the model", "1"},
                          {"num_ports", "Number of ports in the model", "52"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"phys_port", "Link to port component", {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"finisher", "Link to finisher", {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"scheduler", "Link to Scheduler", {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"packet_buffer",
       "Link to packet buffer",
       {"TRAFFIC_GEN_1_0.PacketEvent"}}, )

    SST_ELI_DOCUMENT_STATISTICS(
      {"priorities", "Priority of packets sent", "units", 1},
      {"bytes_sent", "Bytes sent through the simulation", "Bytes", 1},
      {"sent_packets", "Packets sent through the simulation", "units", 1},
      {"current_priority", "Current priority being scheduled", "units", 1},
      {"bytes_sent_by_port", "Bytes sent by source port through the simulation",
       "Bytes", 1},
      {"bytes_sent_by_priority",
       "Bytes sent by priority through the simulation", "Bytes", 1})

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    void finish();

private:
    HostEgressPort();                      // for serialization only
    HostEgressPort(const HostEgressPort&); // do not implement
    void operator=(const HostEgressPort&); // do not implement

    Statistic<uint32_t>*  priorities;
    Statistic<uint32_t>*  bytes_sent;
    Statistic<uint32_t>*  sent_packets;
    Statistic<uint32_t>*  current_priority;
    Statistic<uint32_t>** bytes_sent_by_port;
    Statistic<uint32_t>** bytes_sent_by_priority;

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    unsigned int     node;
    unsigned int     port;
    SST::UnitAlgebra port_bw;
    unsigned int     sent_pkts;
    int              num_nodes;
    int              num_ports;

    void request_packet();
    void send_packet(PacketEvent* pkt);

    void handle_new_pkt(SST::Event* ev);
    void output_timing(SST::Event* ev);

    SST::Link* timing_link;
    SST::Link* phys_port;
    SST::Link* finisher;
    SST::Link* packet_buffer;
    SST::Link* scheduler;
};

#endif
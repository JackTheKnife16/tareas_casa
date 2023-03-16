#ifndef EGRESS_PORT_T
#define EGRESS_PORT_T

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

class EgressPort : public SST::Component
{

public:
    EgressPort(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Egress Port object
     *
     */
    ~EgressPort() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(EgressPort, "QS_1_0", "EgressPort",
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
      {"scheduler", "Link to Scheduler", {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"packet_buffer",
       "Link to packet buffer",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"output", "Link to model output", {"TRAFFIC_GEN_1_0.PacketEvent"}})

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
    EgressPort();                      // for serialization only
    EgressPort(const EgressPort&);     // do not implement
    void operator=(const EgressPort&); // do not implement

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
    int              num_ports;
    int              num_nodes;

    unsigned int sent_pkts;

    SST::SimTime_t send_delay;

    void request_packet();
    void send_packet(PacketEvent* pkt);

    void handle_new_pkt(SST::Event* ev);
    void output_timing(SST::Event* ev);

    SST::Link* timing_link;
    SST::Link* output;
    SST::Link* packet_buffer;
    SST::Link* scheduler;
};

#endif
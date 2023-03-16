#ifndef INGRESS_PORT_T
#define INGRESS_PORT_T

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

#define INIT_PKTS 1
class IngressPort : public SST::Component
{

public:
    IngressPort(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Ingress Port object
     *
     */
    ~IngressPort() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(IngressPort, "QS_1_0", "IngressPort",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Send traffic to a given rate",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"node", "Node number. [0, 10]", "0"},
                          {"port", "Port number. [0, 51]", "0"},
                          {"port_bw", "Port's bandwith. [5Gb/s, 25Gb/s]",
                           "5Gb/s"},
                          {"offered_load", "Port's offered load. [0-100]",
                           "100"},
                          {"start_delay", "Time when the traffic starts", "0s"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"traffic_generator",
                          "Link to packet generator",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"acceptance_checker",
                          "Link to acceptance checker",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"priorities", "Priority of packets sent", "units", 1},
      {"bytes_sent", "Bytes sent through the simulation", "Bytes", 1},
      {"sent_packets", "Packets sent through the simulation", "units", 1})

    void setup();

    void init(unsigned int phase);

    /**
     * @brief Finish stage of SST
     *
     */
    void finish() {};

private:
    IngressPort();                      // for serialization only
    IngressPort(const IngressPort&);    // do not implement
    void operator=(const IngressPort&); // do not implement

    Statistic<uint32_t>* priorities;
    Statistic<uint32_t>* bytes_sent;
    Statistic<uint32_t>* sent_packets;

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    float            offered_load;
    unsigned int     port;
    unsigned int     node;
    std::string      last_pkt = ""; // to modify a string in verbose.
    SST::UnitAlgebra port_bw;

    SST::SimTime_t send_delay;
    SST::SimTime_t start_delay;
    bool           request_pkt;

    // Packets generated from packet_gen component
    // [[pkt__0, pkt_0_1, ...]]
    std::queue<PacketEvent*> packets_to_send;

    void request_packet();
    void send_packet(PacketEvent* pkt);

    void handle_new_pkt(SST::Event* ev);
    void output_timing(SST::Event* ev);

    SST::Link* timing_link;
    SST::Link* acceptance_checker;
    SST::Link* traffic_generator;
};

#endif
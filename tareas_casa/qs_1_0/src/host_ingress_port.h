#ifndef HOST_INGRESS_PORT_T
#define HOST_INGRESS_PORT_T

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

#define MIN_PKT_SIZE 64
#define MAX_PKT_SIZE 9216

#define INIT_PKTS 5

class HostIngressPort : public SST::Component
{

public:
    HostIngressPort(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Ingress Port object
     *
     */
    ~HostIngressPort() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(HostIngressPort, "QS_1_0", "HostIngressPort",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Send traffic to a given rate",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"node", "Node number. [0, 10]", "0"},
                          {"port", "Port number. [0, 51]", "0"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"external_port",
                          "Link to port component",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"acceptance_checker",
                          "Link to acceptance checker",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"priorities", "Priority of packets sent", "units", 1},
      {"bytes_sent", "Bytes sent through the simulation", "units", 1},
      {"sent_packets", "Packets sent through the simulation", "units", 1})

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    void finish();

private:
    HostIngressPort();                       // for serialization only
    HostIngressPort(const HostIngressPort&); // do not implement
    void operator=(const HostIngressPort&);  // do not implement

    Statistic<uint32_t>* priorities;
    Statistic<uint32_t>* bytes_sent;
    Statistic<uint32_t>* sent_packets;

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    unsigned int port;
    unsigned int node;
    unsigned int pkts_sent;

    void handle_new_pkt(SST::Event* ev);

    SST::Link* acceptance_checker;
    SST::Link* external_port;
};

#endif
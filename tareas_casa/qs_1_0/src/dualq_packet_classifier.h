#ifndef PACKET_CLASSIFIER_H
#define PACKET_CLASSIFIER_H

#include "model_global.h"
#include "packet_event.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define HIGHEST_PRI_Q 7

class DualQPacketClassifier : public SST::Component
{

public:
    DualQPacketClassifier(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQPacketClassifier() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      DualQPacketClassifier, "QS_1_0", "DualQPacketClassifier",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Classifies packets for L and C queues and decides if a packet is "
      "dropped depending on the utilization",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
      {"num_ports", "number of ports", "52"},
      {"util_threshold",
       "Utilization threshold where packets are dropped. [1-127]", "124"},
      {"ma_config_file", "Path to Memory Accounting configuration file",
       "qs_1_0/ma_config.json"},
      {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"in_%(num_ports)d", "Links to ingress ports", {"QS_1_0.PackeEvent"}},
      {"fifo_%(num_ports)d", "Links to classic FIFOs", {"QS_1_0.PackeEvent"}},
      {"l_fifo_%(num_ports)d", "Link to L4S FIFO", {"QS_1_0.PackeEvent"}},
      {"packet_buffer", "Link to packet buffer", {"QS_1_0.PacketBufferEvent"}},
      {"bytes_tracker",
       "Link to bytes tracker",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"l_bytes_tracker",
       "Link to L4S bytes tracker",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"drop_receiver",
       "Link to drop receiver",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"mg_util", "Link to mg utilization updater", {"QS_1_0.UtilEvent"}},
      {"qg_util", "Link to qg utilization updater", {"QS_1_0.UtilEvent"}},
      {"pq_util", "Link to pq utilization updater", {"QS_1_0.UtilEvent"}},
      {"lq_util", "Link to lq utilization updater", {"QS_1_0.UtilEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

private:
    DualQPacketClassifier();                             // for serialization only
    DualQPacketClassifier(const DualQPacketClassifier&); // do not implement
    void operator=(const DualQPacketClassifier&);        // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    int num_ports;
    int util_threshold;

    // Map to get mg index from pq index
    std::map<int, int>      pq_mg_map;
    // Map with current utilizations
    std::map<uint32_t, int> mg_table;
    std::map<uint32_t, int> qg_table;
    std::map<uint32_t, int> pq_table;
    std::map<uint32_t, int> lq_table;

    std::map<uint32_t, SST::Link*> ingress_ports;
    std::map<uint32_t, SST::Link*> l_fifos;
    std::map<uint32_t, SST::Link*> fifos;

    SST::Link* packet_buffer;
    SST::Link* bytes_tracker;
    SST::Link* l_bytes_tracker;
    SST::Link* drop_receiver;

    SST::Link* mg_util;
    SST::Link* qg_util;
    SST::Link* pq_util;
    SST::Link* lq_util;

    int get_classic_utilization(uint32_t dest_port, uint32_t priority);
    int get_l4s_utilization(uint32_t dest_port);

    void handle_new_packet(SST::Event* ev);
    void handle_update_mg(SST::Event* ev);
    void handle_update_qg(SST::Event* ev);
    void handle_update_pq(SST::Event* ev);
    void handle_update_lq(SST::Event* ev);

    void send_to_l_queue(PacketEvent* packet);
    void send_to_c_queue(PacketEvent* packet);
};

#endif
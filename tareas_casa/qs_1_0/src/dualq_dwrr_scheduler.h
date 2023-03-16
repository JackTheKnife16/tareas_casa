#ifndef DUALQ_SCTRICT_PRI_SCHEDULER_H
#define DUALQ_SCTRICT_PRI_SCHEDULER_H

#include "scheduler_dwrr.h"

class DualQDWRRScheduler : public SchedulerDWRR
{

public:
    DualQDWRRScheduler(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQDWRRScheduler() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(DualQDWRRScheduler, "QS_1_0", "DualQDWRRScheduler",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Scheduler for the two queues using strict "
                             "priority. FOR DUALQ COUPLED AQM",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_queues", "Number of queues in Port FIFO", "8"},
                          {"highest_priority",
                           "Queue number with the highest priority", "7"},
                          {"dwrr_config_path",
                           "Queue number with the highest priority",
                           "qs_1_0/config/scheduler/dwrr_config_dualq.json"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"egress_port",
                          "Link to egress port",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"port_fifo",
                          "Link to classic port fifos that send updates",
                          {"QS_1_0.QueueUpdateEvent"}},
                         {"l_port_fifo",
                          "Link to L4S port fifo that sends updates",
                          {"QS_1_0.QueueUpdateEvent"}},
                         {"bytes_tracker",
                          "Link to bytes tracker",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_bytes_tracker",
                          "Link to bytes tracker",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"priority_queue_%(num_queues - 1)d",
                          "Link to classic priorirty queues subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_priority_queue",
                          "Link to classic priorirty queues subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"front_size_priority_queue_%(num_queues - 1)d",
                          "Link to classic priority queues size interface",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_front_size_priority_queue",
                          "Link to L4S priority queues size interface",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"queue_serviced", "Current queue being serviced", "unit", 1},
      {"source_serviced", "Current source port being serviced", "unit", 1}, )

private:
    DualQDWRRScheduler();                          // for serialization only
    DualQDWRRScheduler(const DualQDWRRScheduler&); // do not implement
    void operator=(const DualQDWRRScheduler&);     // do not implement

    std::mutex front_size_lock;

    bool serving_l_queue;

    SST::Link* l_port_fifo;
    SST::Link* l_bytes_tracker;

    void request_pkt(int queue_pri) override;

    void handle_update_l_fifo(SST::Event* ev);
    void handle_update_fifos(SST::Event* ev) override;
    void handle_receive_pkt(SST::Event* ev) override;
    void handle_receive_pkt_size(SST::Event* ev) override;
    void handle_l_receive_pkt_size(SST::Event* ev);
};

class DualQDWRRSchedulerV2 : public SchedulerDWRR
{

public:
    DualQDWRRSchedulerV2(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQDWRRSchedulerV2() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(DualQDWRRSchedulerV2, "QS_1_0",
                             "DualQDWRRSchedulerV2",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Scheduler for the two queues using strict "
                             "priority. FOR DUALQ COUPLED AQM",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_queues", "Number of queues in Port FIFO", "8"},
                          {"highest_priority",
                           "Queue number with the highest priority", "7"},
                          {"dwrr_config_path",
                           "Queue number with the highest priority",
                           "qs_1_0/config/scheduler/dwrr_config_dualq.json"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"egress_port",
                          "Link to egress port",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"port_fifo",
                          "Link to classic port fifos that send updates",
                          {"QS_1_0.QueueUpdateEvent"}},
                         {"l_port_fifo",
                          "Link to L4S port fifo that sends updates",
                          {"QS_1_0.QueueUpdateEvent"}},
                         {"bytes_tracker",
                          "Link to bytes tracker",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_bytes_tracker",
                          "Link to bytes tracker",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"priority_queue_%(num_queues - 1)d",
                          "Link to classic priorirty queues subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_priority_queue",
                          "Link to classic priorirty queues subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"front_size_priority_queue_%(num_queues - 1)d",
                          "Link to classic priority queues size interface",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_front_size_priority_queue",
                          "Link to L4S priority queues size interface",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_packet_in",
                          "Link to L4S WRED component",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"packet_in",
                          "Link to Classic WRED component",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"queue_serviced", "Current queue being serviced", "unit", 1},
      {"source_serviced", "Current source port being serviced", "unit", 1}, )

private:
    DualQDWRRSchedulerV2();                            // for serialization only
    DualQDWRRSchedulerV2(const DualQDWRRSchedulerV2&); // do not implement
    void operator=(const DualQDWRRSchedulerV2&);       // do not implement

    std::mutex front_size_lock;

    bool serving_l_queue;
    int  current_queue;

    SST::Link* l_port_fifo;
    SST::Link* l_bytes_tracker;

    SST::Link* l_packet_in;
    SST::Link* packet_in;

    void request_pkt(int queue_pri) override;

    void handle_update_l_fifo(SST::Event* ev);
    void handle_update_fifos(SST::Event* ev) override;
    void handle_receive_pkt(SST::Event* ev) override;
    void handle_receive_pkt_size(SST::Event* ev) override;
    void handle_l_receive_pkt_size(SST::Event* ev);
};

#endif
#ifndef DUALQ_SCTRICT_PRI_SCHEDULER_H
#define DUALQ_SCTRICT_PRI_SCHEDULER_H

#include "scheduler.h"

class DualQStrictPriScheduler : public Scheduler
{

public:
    DualQStrictPriScheduler(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQStrictPriScheduler() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(DualQStrictPriScheduler, "QS_1_0",
                             "DualQStrictPriScheduler",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Scheduler for the two queues using strict "
                             "priority. FOR DUALQ COUPLED AQM",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_queues", "Number of queues in Port FIFO", "8"},
                          {"highest_priority",
                           "Queue number with the highest priority", "7"},
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
                         {"priority_queue_%(num_queues)d",
                          "Link to classic priorirty queues subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_priority_queue",
                          "Link to L4S queue subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"queue_serviced", "Current queue being serviced", "unit", 1},
      {"source_serviced", "Current source port being serviced", "unit", 1}, )

private:
    DualQStrictPriScheduler();                               // for serialization only
    DualQStrictPriScheduler(const DualQStrictPriScheduler&); // do not implement
    void operator=(const DualQStrictPriScheduler&);          // do not implement

    bool l_queue_not_empty;
    bool serving_l_queue;

    SST::Link* l_port_fifo;
    SST::Link* l_priority_queue;
    SST::Link* l_bytes_tracker;

    int  schedule_next_pkt() override;
    void request_pkt(int queue_pri) override;

    void handle_update_l_fifo(SST::Event* ev);
    void handle_receive_pkt(SST::Event* ev) override;
};

class DualQStrictPriSchedulerV2 : public Scheduler
{

public:
    DualQStrictPriSchedulerV2(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQStrictPriSchedulerV2() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(DualQStrictPriSchedulerV2, "QS_1_0",
                             "DualQStrictPriSchedulerV2",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Scheduler for the two queues using strict "
                             "priority. FOR DUALQ COUPLED AQM",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_queues", "Number of queues in Port FIFO", "8"},
                          {"highest_priority",
                           "Queue number with the highest priority", "7"},
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
                         {"priority_queue_%(num_queues)d",
                          "Link to classic priorirty queues subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"l_priority_queue",
                          "Link to L4S queue subcomponent",
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
    DualQStrictPriSchedulerV2();                                 // for serialization only
    DualQStrictPriSchedulerV2(const DualQStrictPriSchedulerV2&); // do not implement
    void operator=(const DualQStrictPriSchedulerV2&);            // do not implement

    bool l_queue_not_empty;
    bool serving_l_queue;

    SST::Link* l_port_fifo;
    SST::Link* l_priority_queue;
    SST::Link* l_bytes_tracker;

    SST::Link* l_packet_in;
    SST::Link* packet_in;

    int  schedule_next_pkt() override;
    void request_pkt(int queue_pri) override;

    void handle_update_l_fifo(SST::Event* ev);
    void handle_receive_pkt(SST::Event* ev) override;
};

#endif
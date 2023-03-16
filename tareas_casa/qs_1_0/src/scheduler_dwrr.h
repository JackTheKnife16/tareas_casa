#ifndef SCHEDULER_DWRR_H
#define SCHEDULER_DWRR_H

#include "model_global.h"
#include "packet_event.h"
#include "queue_update_event.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define INACTIVE 0
#define ACTIVE   1
#define WAIT     2

class SchedulerDWRR : public SST::Component
{

public:
    SchedulerDWRR(SST::ComponentId_t id, SST::Params& params);
    SchedulerDWRR(SST::ComponentId_t id);
    /**
     * @brief Destroy the SchedulerDWRR object
     *
     */
    ~SchedulerDWRR() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(SchedulerDWRR, "QS_1_0", "SchedulerDWRR",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Checks for not empty queue to schedule packets",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_queues", "Number of queues in Port FIFO", "8"},
                          {"highest_priority",
                           "Queue number with the highest priority", "7"},
                          {"dwrr_config_path",
                           "Queue number with the highest priority",
                           "qs_1_0/config/scheduler/dwrr_config_basic.json"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"egress_port",
                          "Link to egress port",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"port_fifo",
                          "Link to port fifo that sends updates",
                          {"QS_1_0.QueueUpdateEvent"}},
                         {"bytes_tracker",
                          "Link to bytes tracker",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"priority_queue_%(num_queues)d",
                          "Link to priorirty queue 0 subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"front_size_priority_queue_%(num_queues)d",
                          "Link to priorirty queue 0 subcomponent",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"queue_serviced", "Current queue being serviced", "unit", 1},
      {"source_serviced", "Current source port being serviced", "unit", 1}, )

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    // void init(unsigned int phase);

    /**
     * @brief Finish stage of SST
     *
     */
    void finish() {};

protected:
    SchedulerDWRR();                      // for serialization only
    SchedulerDWRR(const SchedulerDWRR&);  // do not implement
    void operator=(const SchedulerDWRR&); // do not implement

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Serviced stats
    Statistic<uint32_t>* queue_serviced;
    Statistic<uint32_t>* source_serviced;

    std::map<int, bool> queues_not_empty;

    std::map<int, int> weights;
    std::map<int, int> credits;
    std::map<int, int> activity;
    bool               all_queues_empty;
    bool               send_egress;
    int                num_queues;
    int                highest_priority;

    std::map<int, SST::Link*> queues;
    std::map<int, SST::Link*> front_size;
    SST::Link*                port_fifo;
    SST::Link*                egress_port;
    SST::Link*                bytes_tracker;

    int          update_credits();
    virtual void request_pkt(int queue_pri);
    void         request_pkt_size(int queue_pri);

    virtual void handle_receive_pkt(SST::Event* ev);
    virtual void handle_receive_pkt_size(SST::Event* ev);
    virtual void handle_update_fifos(SST::Event* ev);
    void         handle_egress_port_ready(SST::Event* ev);
};

#endif

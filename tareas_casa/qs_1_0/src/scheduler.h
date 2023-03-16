#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "model_global.h"
#include "packet_event.h"
#include "queue_update_event.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

class Scheduler : public SST::Component
{

public:
    Scheduler(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Scheduler object
     *
     */
    ~Scheduler() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(Scheduler, "QS_1_0", "Scheduler",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Checks for not empty queue to schedule packets",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_queues", "Number of queues in Port FIFO", "8"},
                          {"highest_priority",
                           "Queue number with the highest priority", "7"},
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
    Scheduler();                      // for serialization only
    Scheduler(const Scheduler&);      // do not implement
    void operator=(const Scheduler&); // do not implement

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Serviced stats
    Statistic<uint32_t>* queue_serviced;
    Statistic<uint32_t>* source_serviced;

    std::map<int, bool> queues_not_empty;
    bool                send_to_egress;
    int                 num_queues;
    int                 highest_priority;

    std::map<int, SST::Link*> queues;
    SST::Link*                port_fifo;
    SST::Link*                egress_port;
    SST::Link*                bytes_tracker;

    virtual int  schedule_next_pkt();
    virtual void request_pkt(int queue_pri);

    virtual void handle_receive_pkt(SST::Event* ev);
    void         handle_update_fifos(SST::Event* ev);
    void         handle_egress_port_ready(SST::Event* ev);
};

#endif
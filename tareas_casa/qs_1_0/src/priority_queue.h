#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "model_global.h"
#include "packet_event.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/subcomponent.h>
#include <sst/core/timeConverter.h>

class PriorityQueue : public SST::SubComponent
{

public:
    PriorityQueue(SST::ComponentId_t id, SST::Params& params);
    PriorityQueue(SST::ComponentId_t id, std::string unnamed_sub);
    /**
     * @brief Destroy the Priority Queue object
     *
     */
    ~PriorityQueue() {}

    SST_ELI_REGISTER_SUBCOMPONENT_API(PriorityQueue)

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(PriorityQueue, "QS_1_0",
                                        "PriorityQueue",
                                        SST_ELI_ELEMENT_VERSION(1, 0, 0),
                                        "Priority queue subcomponent",
                                        PriorityQueue)

    SST_ELI_DOCUMENT_PARAMS({"port", "Port number", "0"},
                          {"priority", "Queue priority", "0"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"scheduler",
                          "Link to scheduler",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"scheduler_sizes",
                          "Link to scheduler to send size of pkt",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS({"num_packets", "Number of packets in the queue",
                               "Units", 1},
                              {"num_bytes", "Number of bytes in the queue",
                               "Bytes", 1})

    void enqueue_pkt(PacketEvent* pkt);
    bool is_empty();
    int  size();

protected:
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Stats
    Statistic<uint32_t>* num_packets;
    Statistic<uint32_t>* num_bytes;

    int port;
    int priority;

    std::queue<PacketEvent*> pkt_queue;

    SST::Link* scheduler;
    SST::Link* scheduler_size;

    virtual void handle_pkt_request(SST::Event* ev);
    void         handle_pkt_size_request(SST::Event* ev);
};

#endif
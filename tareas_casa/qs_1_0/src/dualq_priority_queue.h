#ifndef DUALQ_PRIORITY_QUEUE_H
#define DUALQ_PRIORITY_QUEUE_H

#include "priority_queue.h"

class DualQPriorityQueue : public PriorityQueue
{

public:
    DualQPriorityQueue(SST::ComponentId_t id, SST::Params& params);
    DualQPriorityQueue(SST::ComponentId_t id, std::string unnamed_sub);
    /**
     * @brief Destroy the Priority Queue object
     *
     */
    ~DualQPriorityQueue() {}

    SST_ELI_REGISTER_SUBCOMPONENT_API(DualQPriorityQueue)

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(DualQPriorityQueue, "QS_1_0",
                                        "DualQPriorityQueue",
                                        SST_ELI_ELEMENT_VERSION(1, 0, 0),
                                        "Priority queue subcomponent",
                                        DualQPriorityQueue)

    SST_ELI_DOCUMENT_PARAMS({"port", "Port number", "0"},
                          {"priority", "Queue priority", "0"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"scheduler", "Link to scheduler", {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"scheduler_sizes",
       "Link to scheduler to send size of pkt",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"output", "Link to packet output", {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS({"num_packets", "Number of packets in the queue",
                               "Units", 1},
                              {"num_bytes", "Number of bytes in the queue",
                               "Bytes", 1})

private:
    SST::Link* output;

    void handle_pkt_request(SST::Event* ev) override;
};

#endif
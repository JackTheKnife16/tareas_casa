#ifndef PORT_FIFOS_H
#define PORT_FIFOS_H

#include "model_global.h"
#include "packet_event.h"
#include "priority_queue.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

class PortFIFOs : public SST::Component
{

public:
    PortFIFOs(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Port FIFOs object
     *
     */
    ~PortFIFOs() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(PortFIFOs, "QS_1_0", "PortFIFOs",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Fifo that enqueues pkt depending on the priority",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"port", "Port number", "0"},
                          {"num_queues", "Number of queues", "8"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"wred",
                          "Link to wred component",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"packet_selector",
                          "Link to packet selector in scheduler",
                          {"QS_1_0.QueueUpdateEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS({"priority_queues",
                                       "Priority queues for priority.",
                                       "QS_1_0.PriorityQueue"})

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    // void init(unsigned int phase);

    void finish();

private:
    PortFIFOs();                      // for serialization only
    PortFIFOs(const PortFIFOs&);      // do not implement
    void operator=(const PortFIFOs&); // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    int port;
    int num_queues;

    std::map<int, PriorityQueue*> queues;

    SST::Link* wred;
    SST::Link* packet_selector;

    void create_queues();
    void update_queues_status();

    void handle_accepted_pkt(SST::Event* ev);
    void handle_buffer_update(SST::Event* ev);
};

#endif
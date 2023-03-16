#ifndef DROP_RECEIVER_H
#define DROP_RECEIVER_H

#include "model_global.h"
#include "packet_event.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

class DropReceiver : public SST::Component
{

public:
    DropReceiver(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DropReceiver() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(DropReceiver, "QS_1_0", "DropReceiver",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Receives dropped packets",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"wred",
                          "Link to wred component",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"finisher",
                          "Link to finisher component",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"dropped_packets", "Dropped packets", "unit", 1},
      {"destination", "Destination for each dropped packet", "unit", 1},
      {"source", "Source for each dropped packet", "unit", 1},
      {"priority", "Priority for each dropped packet", "unit", 1})

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    // void init(unsigned int phase);

    void finish();

private:
    DropReceiver();                      // for serialization only
    DropReceiver(const DropReceiver&);   // do not implement
    void operator=(const DropReceiver&); // do not implement

    // Stats
    Statistic<uint32_t>* dropped_packets;
    Statistic<uint32_t>* destination;
    Statistic<uint32_t>* source;
    Statistic<uint32_t>* priority;

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    int pkts_drop;
    int tcp_pkts_drop;

    SST::Link* wred;
    SST::Link* finisher;

    void handle_new_drop(SST::Event* ev);
};

#endif
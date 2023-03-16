#ifndef EGRESS_UPDATE_T
#define EGRESS_UPDATE_T

#include "model_global.h"
#include "packet_event.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>
#include <vector> // std::vector

class EventForwarder : public SST::Component
{

public:
    EventForwarder(SST::ComponentId_t id, SST::Params& params);

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(EventForwarder, "QS_1_0", "EventForwarder",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Sends all packet transmit transactions",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_inputs", "Number of inputs to the component",
                           "52"},
                          {"num_outputs", "Number of outputs to the component",
                           "1"},
                          {"verbose", "Sets the verbosity of output", "0"})

    SST_ELI_DOCUMENT_PORTS({"input_%(num_inputs)d",
                          "Links to input components",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"output_%(num_inputs)d",
                          "Link to output component(s)",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS() // FIXME: create stats

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    /**
     * @brief Finish stage of SST
     *
     */
    void finish() {};

private:
    EventForwarder();                      // for serialization only
    EventForwarder(const EventForwarder&); // do not implement
    void operator=(const EventForwarder&); // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    void handle_new_event(SST::Event* ev);

    std::map<uint32_t, SST::Link*> input_components;
    std::vector<SST::Link*>        output_components;

    int num_inputs;
    int num_outputs;
};

#endif

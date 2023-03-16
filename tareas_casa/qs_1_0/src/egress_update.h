#ifndef EGRESS_UPDATE_T
#define EGRESS_UPDATE_T

#include "model_global.h"
#include "packet_event.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

class EgressUpdate : public SST::Component
{

public:
    EgressUpdate(SST::ComponentId_t id, SST::Params& params);

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(EgressUpdate, "QS_1_0", "EgressUpdate",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Sends all packet transmit transactions",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_ports", "number of egress ports", "52"},
                          {"verbose", "Sets the verbosity of output", "0"})

    SST_ELI_DOCUMENT_PORTS({"egress_port_%(num_ports)d",
                          "Links to egress ports",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}},
                         {"packet_buffer",
                          "Link that pull packet from packet buffer",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

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
    EgressUpdate();                      // for serialization only
    EgressUpdate(const EgressUpdate&);   // do not implement
    void operator=(const EgressUpdate&); // do not implement

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    void updatePktBuffer(SST::Event* ev);
    void handlePktUpdate(SST::Event* ev);

    std::map<uint32_t, SST::Link*> egress_ports;
    SST::Link*                     packet_buffer;

    int num_ports;
};

#endif

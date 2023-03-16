#ifndef DUALQ_LOOKUP_CONTROLLER_H
#define DUALQ_LOOKUP_CONTROLLER_H

#include "global_pool_util.h"

class DualQGlobalPoolUtil : public GlobalPoolUtil
{

public:
    DualQGlobalPoolUtil(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQGlobalPoolUtil() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      DualQGlobalPoolUtil, "QS_1_0", "DualQGlobalPoolUtil",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Get packets from all ingress ports, retreive utilization and send them "
      "to WRED. FOR DUALQ COUPLED AQM",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_ports", "number of ports", "52"},
                          {"max_utilization",
                           "Represents 100%% utilization in the global util",
                           "127"},
                          {"buffer_size", "Size of buffer in blocks", "65536"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"packet_buffer",
       "Link to packet buffer component",
       {"QS_1_0.SpaceEvent"}},
      {"memory_group_4", "Link to memory group 4", {"QS_1_0.UtilEvent"}},
      {"memory_group_5", "Link to memory group 5", {"QS_1_0.UtilEvent"}},
      {"memory_group_6", "Link to memory group 6", {"QS_1_0.UtilEvent"}},
      {"memory_group_7", "Link to memory group 7", {"QS_1_0.UtilEvent"}},
      {"queue_group_%(num_ports)d",
       "Link to queue groups",
       {"QS_1_0.UtilEvent"}},
      {"l4s_queue_%(num_ports)d", "Link to L4S queues", {"QS_1_0.UtilEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

private:
    DualQGlobalPoolUtil();                           // for serialization only
    DualQGlobalPoolUtil(const DualQGlobalPoolUtil&); // do not implement
    void operator=(const DualQGlobalPoolUtil&);      // do not implement

    std::map<uint32_t, SST::Link*> l4s_queues;

    void update_lqs(unsigned int utilization);
    void handle_pkt_buffer_update(SST::Event* ev) override;
};

#endif
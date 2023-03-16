#ifndef GLOBAL_POOL_UTIL_H
#define GLOBAL_POOL_UTIL_H

#include "model_global.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define MEM_GROUPS 4

class GlobalPoolUtil : public SST::Component
{

public:
    GlobalPoolUtil(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Global Pool Util object
     *
     */
    ~GlobalPoolUtil() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      GlobalPoolUtil, "QS_1_0", "GlobalPoolUtil",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Sends global pool utilization to accounting elements",
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
       "Link to queue group 0",
       {"QS_1_0.UtilEvent"}}, )

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
    GlobalPoolUtil();                      // for serialization only
    GlobalPoolUtil(const GlobalPoolUtil&); // do not implement
    void operator=(const GlobalPoolUtil&); // do not implement

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    unsigned int max_utilization;
    unsigned int buffer_size;
    int          num_ports;

    SST::Link*                         packet_buffer;
    std::array<SST::Link*, MEM_GROUPS> memory_groups;
    std::map<uint32_t, SST::Link*>     queue_groups;

    void         update_mgs(unsigned int utilization);
    void         update_qgs(unsigned int utilization);
    virtual void handle_pkt_buffer_update(SST::Event* ev);
};

#endif

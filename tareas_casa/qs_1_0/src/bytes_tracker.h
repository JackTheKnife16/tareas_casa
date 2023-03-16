#ifndef BYTES_TRACKER_H
#define BYTES_TRACKER_H

#include "model_global.h"
#include "packet_event.h"
#include "pq_index.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define MEM_GROUPS 4
#define PRI_QUEUES 8

class BytesTracker : public SST::Component
{

public:
    BytesTracker(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Bytes Tracker object
     *
     */
    ~BytesTracker() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      BytesTracker, "QS_1_0", "BytesTracker", SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Keeps track of every byte each accouting element is using",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"ma_config_file",
                           "Path to Memory Accounting configuration file",
                           "qs_1_0/ma_config.json"},
                          {"num_ports", "number of scheduler ports", "52"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"mg_bytes",
                          "Link to mg utilization updater",
                          {"QS_1_0.UtilReportEvent"}},
                         {"qg_bytes",
                          "Link to qg utilization updater",
                          {"QS_1_0.UtilReportEvent"}},
                         {"pq_bytes",
                          "Link to pq utilization updater",
                          {"QS_1_0.UtilReportEvent"}},
                         {"wred",
                          "Link to WRED component",
                          {"QS_1_0.PacketUtilEvent"}},
                         {"port_scheduler_%(num_ports)d",
                          "Links to scheduler ports",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

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
    BytesTracker();                      // for serialization only
    BytesTracker(const BytesTracker&);   // do not implement
    void operator=(const BytesTracker&); // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Map to get mg index from pq index
    std::map<int, int>                   pq_mg_map;
    // Map with current utilizations
    std::map<int, unsigned int>          mg_util_bytes;
    std::map<int, unsigned int>          qg_util_bytes;
    std::map<unsigned int, unsigned int> pq_util_bytes;

    // Links
    SST::Link* mg_bytes;
    SST::Link* qg_bytes;
    SST::Link* pq_bytes;
    SST::Link* wred;

    int                            num_ports;
    std::map<uint32_t, SST::Link*> port_schedulers;

    void handle_accepted_pkt(SST::Event* ev);
    void handle_transmitted_pkt(SST::Event* ev);

    unsigned int update_mg_util_bytes(PacketEvent* pkt, int mg_index, bool receive);
    unsigned int update_qg_util_bytes(PacketEvent* pkt, int qg_index, bool receive);
    unsigned int update_pq_util_bytes(PacketEvent* pkt, pq_index_t pq_index, bool receive);

    int get_mg_index(pq_index_t pq);

    virtual void update_bytes_in_use(PacketEvent* pkt, bool receive);
    void         update_accounting_elements(
                int mg_index, int qg_index, pq_index_t pq_index, unsigned int mg_bytes_in_use, unsigned int qg_bytes_in_use,
                unsigned int pq_bytes_in_use);
};

#endif

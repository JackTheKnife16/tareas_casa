#ifndef ACCEPTANCE_CHECKER_H
#define ACCEPTANCE_CHECKER_H

#include "model_global.h"
#include "packet_util_event.h"
#include "pq_index.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define HIGHEST_PRI_Q 7

class AcceptanceChecker : public SST::Component
{

public:
    AcceptanceChecker(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~AcceptanceChecker() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(AcceptanceChecker, "QS_1_0", "AcceptanceChecker",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Get packets from all ingress ports, retreive "
                             "utilization and send them to WRED",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"ma_config_file",
                           "Path to Memory Accounting configuration file",
                           "qs_1_0/ma_config.json"},
                          {"num_ports", "number of ports", "52"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"mg_utilization",
                          "Link to mg utilization updater",
                          {"QS_1_0.UtilReportEvent"}},
                         {"qg_utilization",
                          "Link to qg utilization updater",
                          {"QS_1_0.UtilReportEvent"}},
                         {"pq_utilization",
                          "Link to pq utilization updater",
                          {"QS_1_0.UtilReportEvent"}},
                         {"wred",
                          "Link to WRED component",
                          {"QS_1_0.PacketUtilEvent"}},
                         {"ingress_port_%(num_ports)d",
                          "Links to all ingress port",
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
    AcceptanceChecker();                         // for serialization only
    AcceptanceChecker(const AcceptanceChecker&); // do not implement
    void operator=(const AcceptanceChecker&);    // do not implement

    unsigned int num_ports;
    int          pq_util_value;

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    // Map to get mg index from pq index
    std::map<int, int>          pq_mg_map;
    // Map with current utilizations
    std::map<int, int>          mg_values;
    std::map<int, int>          qg_values;
    std::map<unsigned int, int> pq_values;

    SST::Link* mg_utilization;
    SST::Link* qg_utilization;
    SST::Link* pq_utilization;
    SST::Link* wred;

    std::map<uint32_t, SST::Link*> ingress_ports;

    void         handle_update_mg(SST::Event* ev);
    void         handle_update_qg(SST::Event* ev);
    void         handle_update_pq(SST::Event* ev);
    virtual void handle_new_pkt(SST::Event* ev);

    int get_mg_index(pq_index_t pq);

    int fetch_mg_util(int mg_index);
    int fetch_qg_util(int port);
    int fetch_pq_util(pq_index_t pq);

    void send_packet(PacketUtilEvent* pkt);
};

#endif

#ifndef DUALQ_ACCEPTANCE_CHECKER_H
#define DUALQ_ACCEPTANCE_CHECKER_H

#include "acceptance_checker.h"

class DualQAcceptanceChecker : public AcceptanceChecker
{

public:
    DualQAcceptanceChecker(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQAcceptanceChecker() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      DualQAcceptanceChecker, "QS_1_0", "DualQAcceptanceChecker",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Get packets from all ingress ports, retreive utilization and send them "
      "to WRED. FOR DUALQ COUPLED AQM",
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
                         {"lq_utilization",
                          "Link to lq utilization updater",
                          {"QS_1_0.UtilReportEvent"}},
                         {"wred",
                          "Link to WRED component",
                          {"QS_1_0.PacketUtilEvent"}},
                         {"ingress_port_%(num_ports)d",
                          "Links to all ingress port",
                          {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

private:
    DualQAcceptanceChecker();                              // for serialization only
    DualQAcceptanceChecker(const DualQAcceptanceChecker&); // do not implement
    void operator=(const DualQAcceptanceChecker&);         // do not implement

    std::map<int, int> lq_values;

    SST::Link* lq_utilization;

    void handle_update_lq(SST::Event* ev);

    int fetch_lq_util(int port);

    void handle_new_pkt(SST::Event* ev) override;
};

class DualQAcceptanceCheckerV2 : public AcceptanceChecker
{

public:
    DualQAcceptanceCheckerV2(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQAcceptanceCheckerV2() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      DualQAcceptanceCheckerV2, "QS_1_0", "DualQAcceptanceCheckerV2",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Get packets from all ingress ports, retreive utilization and send them "
      "to WRED. FOR DUALQ COUPLED AQM",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"ma_config_file",
                           "Path to Memory Accounting configuration file",
                           "qs_1_0/ma_config.json"},
                          {"num_ports", "number of ports", "52"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"mg_utilization",
       "Link to mg utilization updater",
       {"QS_1_0.UtilReportEvent"}},
      {"qg_utilization",
       "Link to qg utilization updater",
       {"QS_1_0.UtilReportEvent"}},
      {"pq_utilization",
       "Link to pq utilization updater",
       {"QS_1_0.UtilReportEvent"}},
      {"lq_utilization",
       "Link to lq utilization updater",
       {"QS_1_0.UtilReportEvent"}},
      {"wred", "Link to WRED component", {"QS_1_0.PacketUtilEvent"}},
      {"l4s_wred", "Link to L4S WRED component", {"QS_1_0.PacketUtilEvent"}},
      {"c_fifo_%(num_ports)d",
       "Links to all the classic FIFOs",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"l_fifo_%(num_ports)d",
       "Links to all L4S FIFOs",
       {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

private:
    DualQAcceptanceCheckerV2();                                // for serialization only
    DualQAcceptanceCheckerV2(const DualQAcceptanceCheckerV2&); // do not implement
    void operator=(const DualQAcceptanceCheckerV2&);           // do not implement

    std::map<int, int> lq_values;

    SST::Link* l4s_wred;
    SST::Link* lq_utilization;

    std::map<uint32_t, SST::Link*> l_fifos;

    void handle_update_lq(SST::Event* ev);

    int fetch_lq_util(int port);

    void handle_new_pkt(SST::Event* ev) override;
    void handle_new_l_pkt(SST::Event* ev);
};

#endif
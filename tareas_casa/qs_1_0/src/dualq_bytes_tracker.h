#ifndef DUALQ_LOOKUP_CONTROLLER_H
#define DUALQ_LOOKUP_CONTROLLER_H

#include "bytes_tracker.h"

class DualQBytesTracker : public BytesTracker
{

public:
    DualQBytesTracker(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQBytesTracker() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      DualQBytesTracker, "QS_1_0", "DualQBytesTracker",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Get packets from all ingress ports, retreive utilization and send them "
      "to WRED. FOR DUALQ COUPLED AQM",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"ma_config_file",
                           "Path to Memory Accounting configuration file",
                           "qs_1_0/ma_config.json"},
                          {"num_ports", "number of scheduler ports", "52"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"mg_bytes",
       "Link to mg utilization updater",
       {"QS_1_0.UtilReportEvent"}},
      {"qg_bytes",
       "Link to qg utilization updater",
       {"QS_1_0.UtilReportEvent"}},
      {"pq_bytes",
       "Link to pq utilization updater",
       {"QS_1_0.UtilReportEvent"}},
      {"lq_bytes",
       "Link to pq utilization updater",
       {"QS_1_0.UtilReportEvent"}},
      {"wred", "Link to WRED component", {"QS_1_0.PacketUtilEvent"}},
      {"l4s_wred", "Link to WRED component", {"QS_1_0.PacketUtilEvent"}},
      {"l_port_scheduler_%(num_ports)d",
       "Link to WRED component",
       {"QS_1_0.PacketUtilEvent"}},
      {"port_scheduler_%(num_ports)d",
       "Links to scheduler ports",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"wred_v2",
       "Link to WRED component, DualQ v2 needs another link for dropped "
       "packets",
       {"QS_1_0.PacketUtilEvent"}},
      {"l4s_wred_v2",
       "Link to WRED component, DualQ v2 needs another link for dropped "
       "packets",
       {"QS_1_0.PacketUtilEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

private:
    DualQBytesTracker();                         // for serialization only
    DualQBytesTracker(const DualQBytesTracker&); // do not implement
    void operator=(const DualQBytesTracker&);    // do not implement

    std::map<int, unsigned int> lq_util_bytes;

    SST::Link*                     lq_bytes;
    SST::Link*                     l4s_wred;
    std::map<uint32_t, SST::Link*> l4s_port_schedulers;
    // Extra WRED link for DualQ Coupled AQM v2
    SST::Link*                     wred_v2;
    SST::Link*                     l4s_wred_v2;

    unsigned int update_lq_util_bytes(PacketEvent* pkt, int lq_index, bool receive);
    void         handle_l_accepted_pkt(SST::Event* ev);
    void         handle_l_transmitted_pkt(SST::Event* ev);
};

#endif
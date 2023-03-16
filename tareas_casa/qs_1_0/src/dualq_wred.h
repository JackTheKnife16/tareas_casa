#ifndef DUALQ_WRED_H
#define DUALQ_WRED_H

#include "wred.h"

class DualQClassicWRED : public WRED
{

public:
    DualQClassicWRED(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQClassicWRED() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      DualQClassicWRED, "QS_1_0", "DualQClassicWRED",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Classic WRED component. Computes drop probability and chooses to drop "
      "or accept. FOR DUALQ COUPLED AQM",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
      {"k_proportionality",
       "k is the constant of proportionality, which is termed the coupling "
       "factor",
       "2"},
      {"ma_config_file",
       "Path to Memory Accounting configuration file, which contain the WRED "
       "profiles",
       "qs_1_0/ma_config.json"},
      {"enable_ecn",
       "Enables Explicit Congestion Notification for ECN capable packets", "0"},
      {"ecn_config_file", "Path to JSON file with the ECN WRED profiles",
       "qs_1_0/ecn_config.json"},
      {"port_config_file", "Path to JSON file with port configuration",
       "qs_1_0/port_config.json"},
      {"traffic_config_file", "Path to file with traffic configuration",
       "qs_1_0/config/traffic_config/traffic_config_phase_one.json"},
      {"num_ports", "number of ports", "52"},
      {"seed", "Seed for random number generator", "1"},
      {"model_version", "DualQ Coupled AQM Model Version", "1"},
      {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"acceptance_checker",
       "Link to acceptance checker",
       {"QS_1_0.PacketUtilEvent"}},
      {"packet_buffer", "Link to packet buffer", {"QS_1_0.PacketBufferEvent"}},
      {"bytes_tracker",
       "Link to bytes tracker",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"drop_receiver",
       "Link to drop receiver",
       {"TRAFFIC_GEN_1_0.PacketEvent"}},
      {"coupling", "Link to L4S WRED coupling", {"QS_1_0.CouplingEvent"}},
      {"port_fifo_%(num_ports)d",
       "Links to all port fifo",
       {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"p_C", "Classic Probability", "units", 1},
      {"ce_packets", "Number of Congestion Experienced packets", "units", 1},
      {"ce_packets_by_dest",
       "Number of Congestion Experienced packets by destiantion port", "units",
       1},
      {"ce_packets_by_src",
       "Number of Congestion Experienced packets by source port", "units", 1})

private:
    DualQClassicWRED();                        // for serialization only
    DualQClassicWRED(const DualQClassicWRED&); // do not implement
    void operator=(const DualQClassicWRED&);   // do not implement

    Statistic<float>** p_C;

    float k_proportionality;
    int   model_version;

    SST::Link* coupling;

    void update_coupling_prob(float p_prime);

    void handle_new_pkt(SST::Event* ev) override;
};

#endif
#ifndef DUALQ_L4S_WRED_H
#define DUALQ_L4S_WRED_H

#include "model_global.h"
#include "packet_event.h"
#include "packet_util_event.h"
#include "wred_profile.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/rng/sstrng.h>
#include <sst/core/timeConverter.h>

#define HIGHEST_UTIL 127
#define NUM_PRI      8

class L4SWRED : public SST::Component
{

public:
    L4SWRED(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~L4SWRED() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(L4SWRED, "QS_1_0", "L4SWRED",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Applies ECN WRED ", COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"ma_config_file",
                           "Path to Memory Accounting configuration file, "
                           "which contain the WRED profiles",
                           "qs_1_0/ma_config.json"},
                          {"port_config_file",
                           "Path to JSON file with port configuration",
                           "qs_1_0/port_config.json"},
                          {"num_ports", "number of ports", "52"},
                          {"seed", "Seed for random number generator", "1"},
                          {"model_version", "DualQ Coupled AQM Model Version",
                           "1"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"coupling", "Link to classic WRED couping", {"QS_1_0.CouplingEvent"}},
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
      {"port_fifo_%(num_ports)d",
       "Links to all port fifo",
       {"TRAFFIC_GEN_1_0.PacketEvent"}})

    SST_ELI_DOCUMENT_STATISTICS(
      {"p_CL", "Coupling Probability", "units", 1},
      {"p_L", "L4S Probability", "units", 1},
      {"ce_packets", "Number of Congestion Experienced packets", "units", 1},
      {"ce_packets_by_dest",
       "Number of Congestion Experienced packets by destiantion port", "units",
       1},
      {"ce_packets_by_src",
       "Number of Congestion Experienced packets by source port", "units", 1})

private:
    L4SWRED();                      // for serialization only
    L4SWRED(const L4SWRED&);        // do not implement
    void operator=(const L4SWRED&); // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    Statistic<float>**   p_CL;
    Statistic<float>**   p_L;
    Statistic<uint32_t>* ce_packets;
    Statistic<uint32_t>* ce_packets_by_dest;
    Statistic<uint32_t>* ce_packets_by_src;

    std::map<int, float>                             coupling_probability;
    SST::RNG::SSTRandom*                             rng;
    int                                              num_ports;
    int                                              model_version;
    std::map<std::string, int>                       lq_wred_index;
    std::map<int, std::map<int, ecn_wred_profile_t>> lq_profiles;
    std::map<unsigned short, std::string>            port_config;

    SST::Link*                     acceptance_checker;
    SST::Link*                     coupling;
    SST::Link*                     packet_buffer;
    SST::Link*                     bytes_tracker;
    SST::Link*                     drop_receiver;
    std::map<uint32_t, SST::Link*> port_fifos;

    bool  is_ecn_capable(PacketEvent* pkt);
    float compute_ecn_marking_prob(int util, ecn_wred_profile_t ecn_pq_profiles);
    bool  ecn_check(PacketEvent* pkt, int util, float marking_prob, int ecn_drop_point);
    void  congestion_experienced(PacketEvent* pkt);
    float drop_calculation(wred_profile_t profile, int util);

    void handle_new_pkt(SST::Event* ev);
    void handle_new_coupling(SST::Event* ev);
};

#endif
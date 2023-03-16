#ifndef WRED_H
#define WRED_H

#include "model_global.h"
#include "packet_event.h"
#include "packet_util_event.h"
#include "wred_profile.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/rng/sstrng.h>
#include <sst/core/timeConverter.h>

#define HIGHEST_UTIL 127
#define NUM_PRI      8

class WRED : public SST::Component
{

public:
    WRED(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the WRED object
     *
     */
    ~WRED() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      WRED, "QS_1_0", "WRED", SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Computes drop probability and chooses to drop or accept",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
      {"ma_config_file",
       "Path to Memory Accounting configuration file, which contain the WRED "
       "profiles",
       "qs_1_0/ma_config.json"},
      {"enable_ecn",
       "Enables Explicit Congestion Notification for ECN capable packets", "0"},
      {"ecn_config_file", "Path to JSON file with the ECN WRED profiles",
       "qs_1_0/ecn_config.json"},
      {"port_config_file", "Path to file with port configuration",
       "port_config/ports_config.json"},
      {"num_ports", "number of ports", "52"},
      {"seed", "Seed for random number generator", "1"}, {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"acceptance_checker",
                          "Link to acceptance checker",
                          {"QS_1_0.PacketUtilEvent"}},
                         {"packet_buffer",
                          "Link to packet buffer",
                          {"QS_1_0.PacketBufferEvent"}},
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
      {"ce_packets", "Number of Congestion Experienced packets", "units", 1},
      {"ce_packets_by_dest",
       "Number of Congestion Experienced packets by destiantion port", "units",
       1},
      {"ce_packets_by_src",
       "Number of Congestion Experienced packets by source port", "units", 1})

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    // void init(unsigned int phase);

    void finish() {};

protected:
    WRED();                      // for serialization only
    WRED(const WRED&);           // do not implement
    void operator=(const WRED&); // do not implement

    std::mutex           safe_lock;
    SST::Output*         out;
    SST::TimeConverter*  base_tc;
    SST::RNG::SSTRandom* rng;
    std::string          port_bw;

    Statistic<uint32_t>* ce_packets;
    Statistic<uint32_t>* ce_packets_by_dest;
    Statistic<uint32_t>* ce_packets_by_src;

    // Map to get WRED profiles
    std::map<int, wred_profile_t> wred_mg;

    std::map<std::string, int> qg_wred_index;

    std::map<int, std::map<int, wred_profile_t>> qg_profiles;

    std::map<int, wred_profile_t> wred_pq;

    std::map<unsigned short, std::string> port_config;

    // ECN
    bool                              enable_ecn;
    std::map<int, ecn_wred_profile_t> ecn_pq_profiles;

    SST::Link* acceptance_checker;
    SST::Link* packet_buffer;
    SST::Link* bytes_tracker;
    SST::Link* drop_receiver;

    std::map<uint32_t, SST::Link*> port_fifos;

    int num_ports;

    virtual void handle_new_pkt(SST::Event* ev);

    void update_bytes_tracker(PacketEvent* pkt);
    void update_pkt_buffer(PacketEvent* pkt);

    bool  is_ecn_capable(PacketEvent* pkt);
    float compute_ecn_marking_prob(int util, ecn_wred_profile_t ecn_pq_profiles);
    bool  ecn_check(PacketEvent* pkt, int util, float marking_prob, int ecn_drop_point);
    void  congestion_experienced(PacketEvent* pkt);

    void send_packet(PacketEvent* pkt);
    void drop_packet(PacketEvent* pkt);

    float compute_drop_prob(PacketUtilEvent* pkt_util);
    float compute_mg_drop_prob(int util, int mg_index);
    float compute_qg_drop_prob(int util, pq_index_t pq);
    float compute_pq_drop_prob(int util, pq_index_t pq);
    float drop_calculation(wred_profile_t profile, int util);

    bool accept_pkt(float drop_probability);
};

#endif

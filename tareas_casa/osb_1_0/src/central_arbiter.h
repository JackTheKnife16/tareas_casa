#ifndef COMPONENT_TEMPLATE_H
#define COMPONENT_TEMPLATE_H

#include "arbitration_result.h"
#include "model_global.h"
#include "nic_arbitration.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>
#include <vector> // std::vector

/**
 * @brief Central Arbiter component is the single brain that know all, accepts port requests from each slice, decides
 * who gets access to the FEP Pipeline, and generates Arbiter Grant Events
 *
 */
class CentralArbiter : public SST::Component
{

public:
    CentralArbiter(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~CentralArbiter() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        CentralArbiter, "OSB_1_0", "CentralArbiter", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Accepts port requests from each slice, decides who gets access to the FEP Pipeline, and generates grants",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "frequency", "Clock frequency used as the delay to send every packet word.", "1500 MHz" },
        { "buffer_slices", "Number of BUffer Slcies in the OSB.", "2" },
        { "ports_per_buffer", "Number of ports that a Buffer Slice has.", "32" },
        { "priorities", "Number priorities supported.", "4" }, { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "ic_req_%(buffer_slices)d", "Links to the IC req ports", { "OSB_1_0.ArbiterRequestEvent" } },
        { "nic_req_%(buffer_slices)d", "Links to the NIC req ports", { "OSB_1_0.ArbiterRequestEvent" } },
        { "ic_gnt_%(buffer_slices)d", "Links to the NIC grant ports", { "OSB_1_0.ArbiterGrantEvent" } },
        { "nic_gnt_%(buffer_slices)d", "Links to the NIC grant ports", { "OSB_1_0.ArbiterGrantEvent" } })

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void finish();

private:
    CentralArbiter();                      // for serialization only
    CentralArbiter(const CentralArbiter&); // do not implement
    void operator=(const CentralArbiter&); // do not implement

    std::mutex          safe_lock;
    std::mutex          ic_safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    SST::SimTime_t freq_delay;
    uint32_t       buffer_slices;
    uint32_t       ports_per_buffer;
    uint32_t       priorities;

    // IC requests: first key is the buffer slice and second the port
    std::map<uint32_t, std::map<uint32_t, bool>> ic_requests;
    // IC choices: first key is buffer_slice, returns the port chosen
    std::map<uint32_t, int>                      ic_choices;
    // Chosen IC buffer slice
    int                                          chosen_buffer_slice;

    // NIC arbitration elements
    std::vector<NICArbitration*> non_initial_chunk_arbitration;

    // Links
    std::vector<SST::Link*> ic_req_links;
    std::vector<SST::Link*> nic_req_links;
    std::vector<SST::Link*> ic_grant_links;
    std::vector<SST::Link*> nic_grant_links;

    // Arbitration Conflict Resolution routine
    bool       arbitration_running;
    SST::Link* arbitration_conflict_resolution_link;
    void       notify_arbitration_routine();
    void       disable_arbitration_routine();

    // Methods
    arbitration_result_t initial_chunk_arbitration();
    bool                 ic_reqs_available();

    // Handlers
    void ic_request_receiver(SST::Event* ev);
    void arbitration_conflict_resolution(SST::Event* ev);
};


#endif
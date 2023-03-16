#ifndef LOOKUP_PIPELINE_T
#define LOOKUP_PIPELINE_T

#include "fds_event.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>


struct FDSTime
{
    FDSEvent*      fds;
    SST::SimTime_t t_fds;
};

class LookupPipeline : public SST::Component
{

public:
    LookupPipeline(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Egress Port object
     *
     */
    ~LookupPipeline() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        LookupPipeline,
        "OSB_1_0",
        "LookupPipeline",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Send traffic to a given rate",
        COMPONENT_CATEGORY_NETWORK
    )

    SST_ELI_DOCUMENT_PARAMS(
        { "frequency", "Clock frequency used as the delay to send every packet word.", "1500 MHz" },
        { "num_cycles", "number of cycles pipeline", "3"},
        { "verbose", "", "0"}
    )

    SST_ELI_DOCUMENT_PORTS(
        {"out_fds_bus", "Link to Final Slice component", {"OSB_1_0.FDSEvent"} },
        {"in_fds_bus", "Link to Buffer Slice Mux component", {"OSB_1_0.FDSEvent"} }
    )

    SST_ELI_DOCUMENT_STATISTICS(
    )

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    void finish();

private:
    LookupPipeline();                      // for serialization only
    LookupPipeline(const LookupPipeline&); // do not implement
    void operator=(const LookupPipeline&); // do not implement


    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    unsigned int num_cycles;

    SST::SimTime_t freq_delay;
    SST::SimTime_t send_delay;

    std::queue<FDSTime*> fds_time_queue;
    bool                 sender_activated;


    void sender_routine(SST::Event* ev);
    void fds_receiver(SST::Event* ev);
    void delay_routine();

    SST::Link* sender_link;
    SST::Link* out_fds_bus_link;
    SST::Link* in_fds_bus_link;
};


#endif

#ifndef UTILIZATION_REPORTER_T
#define UTILIZATION_REPORTER_T

#include "element_connector.h"
#include "util_event.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>
#include <vector> // std::vector

class UtilizationReporter : public SST::Component
{

public:
    UtilizationReporter(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Utilization Reporter object
     *
     */
    ~UtilizationReporter() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      UtilizationReporter, "QS_1_0", "UtilizationReporter",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Send the utilization updates to the acceptance checker",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_ports", "number of ports", "52"},
                          {"verbose", "Sets the verbosity of output", "0"})

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"accounting_elements", "Accounting elements that will send updates.",
       "QS_1_0.ElementConnector"})

    SST_ELI_DOCUMENT_PORTS({"acceptance_checker",
                          "Link to acceptance checker component",
                          {"QS_1_0.UtilEvent"}})

    SST_ELI_DOCUMENT_STATISTICS() // FIXME: create stats

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    /**
     * @brief Finish stage of SST
     *
     */
    void finish() {};

private:
    UtilizationReporter();                           // for serialization only
    UtilizationReporter(const UtilizationReporter&); // do not implement
    void operator=(const UtilizationReporter&);      // do not implement

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    void handle_new_update(SST::Event* ev);
    void set_accounting_elements();

    std::vector<ElementConnector*> accounting_elements;
    SST::Link*                     acceptance_checker;
    unsigned int                   num_ports;
};

#endif

#ifndef DUALQ_LOOKUP_CONTROLLER_H
#define DUALQ_LOOKUP_CONTROLLER_H

#include "lookup_event.h"
#include "lookup_table.h"

class DualQLookUpController : public LookUpController
{

public:
    DualQLookUpController(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~DualQLookUpController() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      DualQLookUpController, "QS_1_0", "DualQLookUpController",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Get packets from all ingress ports, retreive utilization and send them "
      "to WRED. FOR DUALQ COUPLED AQM",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"num_ports", "Number of ports", "52"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"mg_elements",
       "MG elements that request to this controller. [If applies]",
       "QS_1_0.ElementConnector"},
      {"qg_elements",
       "QG elements that request to this controller. [If applies]",
       "QS_1_0.ElementConnector"},
      {"pq_elements",
       "PQ elements that request to this controller. [If applies]",
       "QS_1_0.ElementConnector"},
      {"lq_elements",
       "LQ elements that request to this controller. [If applies]",
       "QS_1_0.ElementConnector"},
      {"look_up_table", "Tables used by the controller", "QS_1_0.LookUpTable"})

    SST_ELI_DOCUMENT_STATISTICS()

private:
    DualQLookUpController();                             // for serialization only
    DualQLookUpController(const DualQLookUpController&); // do not implement
    void operator=(const DualQLookUpController&);        // do not implement

    std::vector<ElementConnector*> lq_elements;

    void send_response(int type, unsigned int element_index, LookUpResponseEvent* res) override;
};

#endif
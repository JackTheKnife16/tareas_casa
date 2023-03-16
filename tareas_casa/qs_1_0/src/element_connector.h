#ifndef ELEMENT_CONNECTOR_H
#define ELEMENT_CONNECTOR_H

#include "bytes_use_event.h"
#include "model_global.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/subcomponent.h>
#include <sst/core/timeConverter.h>

class ElementConnector : public SST::SubComponent
{

public:
    ElementConnector(SST::ComponentId_t id, SST::Params& params);
    ElementConnector(SST::ComponentId_t id, std::string unnamed_sub);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~ElementConnector() {}

    SST_ELI_REGISTER_SUBCOMPONENT_API(ElementConnector)

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(ElementConnector, "QS_1_0",
                                        "ElementConnector",
                                        SST_ELI_ELEMENT_VERSION(1, 0, 0),
                                        "Connector to the accouting elements",
                                        ElementConnector)

    SST_ELI_DOCUMENT_PARAMS({"index", "Index to the accounting element", "\"0\""},
                          {"type", "Type of accounting element. [MG, QG, PQ]",
                           "MG"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"accounting_element",
                          "Link to the respective accounting element",
                          {"QS_1_0.BytesUseEvent"}})

    SST_ELI_DOCUMENT_STATISTICS() // FIXME: create stats

    void set_accounting_element_link(SST::Event::HandlerBase* functor);
    void send_event(SST::Event* ev);

private:
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    std::string index;
    std::string type;

    SST::Link* accounting_element;
};

#endif
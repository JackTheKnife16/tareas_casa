#ifndef COMPONENT_TEMPLATE_H
#define COMPONENT_TEMPLATE_H

#include "model_global.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

/**
 * @brief Ethernet Port component that receives packets from the Traffic Generator and forwards them to the
 * Port Group Interface in chunks at the speed configured.
 *
 */
class ComponentTemplate : public SST::Component
{

public:
    ComponentTemplate(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~ComponentTemplate() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        ComponentTemplate, "OSB_1_0", "ComponentTemplate", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Receives packets, converts them into chunks, and sends them at the configured rate",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "port", "Port number.", "0" }, { "pg_index", "Port group index.", "0" }, { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "input", "Link to the input component", { "OSB_1_0.PacketEvent" } },
        { "output", "Link to output component", { "OSB_1_0.PacketEvent" } })

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void init(uint32_t phase);

    void setup();

    void finish();

private:
    ComponentTemplate();                         // for serialization only
    ComponentTemplate(const ComponentTemplate&); // do not implement
    void operator=(const ComponentTemplate&);    // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    uint32_t port;
    uint32_t pg_index;

    SST::Link* input_link;
    SST::Link* output_link;

    // Handlers
    void input_handler(SST::Event* ev);
};


#endif
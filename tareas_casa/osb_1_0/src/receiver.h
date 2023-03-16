#ifndef DROP_RECEIVER_H
#define DROP_RECEIVER_H

#include "model_global.h"
#include "packet_word_event.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

/**
 * @brief Receiver component that receives packet word events and sends them out when
 * the last chunk is received.
 *
 */
class Receiver : public SST::Component
{

public:
    Receiver(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~Receiver() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        Receiver, "OSB_1_0", "Receiver", SST_ELI_ELEMENT_VERSION(1, 0, 0), "Receives transactions",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({ "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "input", "Link to sender component", { "OSB_1_0.PacketEvent" } },
        { "output", "Link to receiver component", { "OSB_1_0.PacketEvent" } })

    SST_ELI_DOCUMENT_STATISTICS()

private:
    Receiver();                      // for serialization only
    Receiver(const Receiver&);       // do not implement
    void operator=(const Receiver&); // do not implement

    SST::Output*        out;
    SST::TimeConverter* base_tc;

    uint32_t pkt_counter;

    SST::Link* input;
    SST::Link* output;

    void handle_new_transaction(SST::Event* ev);
};


#endif
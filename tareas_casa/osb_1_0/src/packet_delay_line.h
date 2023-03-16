#ifndef PACKET_DELAY_LINE_H
#define PACKET_DELAY_LINE_H

#include "model_global.h"

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#include "packet_word_event.h"
#include "fifo_events.h"

/**
 * @brief Ethernet Port component that receives packets from the Traffic Generator and forwards them to the
 * Port Group Interface in chunks at the speed configured.
 *
 */
class PacketDelayLine : public SST::Component
{

public:
    PacketDelayLine(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Component Template object
     *
     */
    ~PacketDelayLine() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        PacketDelayLine, "OSB_1_0", "PacketDelayLine", SST_ELI_ELEMENT_VERSION(1, 0, 0),
        "Receives packets, converts them into chunks, and sends them at the configured rate",
        COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
        { "idx", "Index of component.", "0" }, { "verbose", "", "0" })

    SST_ELI_DOCUMENT_PORTS(
        { "input1", "Link to the input component", { "OSB_1_0.PacketWordEvent" } },
        { "input2", "Link to the input component", {} },
        { "output1", "Link to output component", { "OSB_1_0.PacketWordEvent" } },
        { "output2", "Link to output component", { "OSB_1_0.FIFOStatusEvent" } })

    // Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    void init(uint32_t phase){};

    void setup(){ };

    void finish();

private:
    PacketDelayLine();                         // for serialization only
    PacketDelayLine(const PacketDelayLine&); // do not implement
    void operator=(const PacketDelayLine&);    // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    uint32_t idx;
    std::queue<PacketWordEvent*> queue_pkt;
    bool empty_queue_flag;

    SST::Link* input_link1;
    SST::Link* input_link2;
    SST::Link* output_link1;
    SST::Link* output_link2;

    // Handlers
    void input_handler1(SST::Event* ev);
    void input_handler2(SST::Event* ev);
    void send_pkt();
    void send_status();
};


#endif
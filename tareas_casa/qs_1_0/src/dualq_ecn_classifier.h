#ifndef ECN_CLASSIFIER_H
#define ECN_CLASSIFIER_H

#include "model_global.h"
#include "packet_util_event.h"

#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

// ECN Codepoints
#define NO_ECT 0x00
#define ECT_0  0x02
#define ECT_1  0x01
#define CE     0x03

class ECNClassifier : public SST::Component
{

public:
    ECNClassifier(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Acceptance Checker object
     *
     */
    ~ECNClassifier() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(ECNClassifier, "QS_1_0", "ECNClassifier",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Classifies packets for L and C queues",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS({"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS(
      {"in", "Link to acceptance checker", {"QS_1_0.DualQPacketUtilEvent"}},
      {"l_check", "Link to L4S AQM", {"QS_1_0.DualPacketUtilEvent"}},
      {"c_check", "Link to Classic AQM", {"QS_1_0.DualPacketUtilEvent"}})

    SST_ELI_DOCUMENT_STATISTICS()

private:
    ECNClassifier();                      // for serialization only
    ECNClassifier(const ECNClassifier&);  // do not implement
    void operator=(const ECNClassifier&); // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    SST::Link* in;
    SST::Link* l_check;
    SST::Link* c_check;

    void handle_new_packet(SST::Event* ev);
    void send_to_l_queue(DualQPacketUtilEvent* packet);
    void send_to_c_queue(DualQPacketUtilEvent* packet);
};

#endif
#ifndef ACCOUNTING_ELEMENT_H
#define ACCOUNTING_ELEMENT_H

#include "element_connector.h"
#include "model_global.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define MAX_UTIL_VALUE 127
#define BITS_UTIL      128

class AccountingElement : public SST::Component
{

public:
    AccountingElement(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Accounting Element object
     *
     */
    ~AccountingElement() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(AccountingElement, "QS_1_0", "AccountingElement",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Accounting element in MA",
                             COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
      {"type", "Type of accounting element. [1:MG, 2:QG, 3:PQ]", "1"},
      {"index", "Accounting element index", "0"},
      {"priority",
       "Priority of accounting element. [Used only for PQ elements]", "0"},
      {"max_bytes", "Max amount of bytes", "250000"},
      {"bias", "Bias value", "0"},
      {"parent_translation_config", "File with parents translation map",
       "qs_1_0/parent_translation.json"},
      {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"parent_utilization",
                          "Link to parent's utilization",
                          {"QS_1_0.UtilEvent"}},
                         {"bytes_tracker",
                          "Link to bytes tracker component",
                          {"QS_1_0.BytesUseEvent"}},
                         {"lookup_table",
                          "Link to look up table component",
                          {"QS_1_0.LookUpRequestEvent"}},
                         {"child_utilization",
                          "Link to output's utilization",
                          {"QS_1_0.UtilEvent"}})

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS({"children", "Element's children.",
                                       "QS_1_0.ElementConnector"})

    SST_ELI_DOCUMENT_STATISTICS(
      {"bytes_util", "Bytes utilization", "Percentage [0-127]", 1},
      {"parent_util", "Parent's utilization", "Percentage [0-127]", 1},
      {"utilization", "Element's utilization", "Percentage [0-127]", 1})

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    // void init(unsigned int phase);

    /**
     * @brief Finish stage of SST
     *
     */
    void finish() {};

private:
    AccountingElement();                         // for serialization only
    AccountingElement(const AccountingElement&); // do not implement
    void operator=(const AccountingElement&);    // do not implement

    // Stats
    Statistic<uint32_t>* bytes_util;
    Statistic<uint32_t>* parent_util;
    Statistic<uint32_t>* utilization;

    std::mutex          safe_parent_lock;
    std::mutex          safe_bytes_lock;
    std::mutex          safe_util_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    const int bits_parent = 5;
    const int bits_child  = 7;

    int          type;
    unsigned int index;

    unsigned int                         parents_utilization;
    unsigned int                         bytes_in_use;
    unsigned int                         max_bytes;
    unsigned int                         bias;
    unsigned int                         current_utilization;
    std::map<unsigned int, unsigned int> parent_translation;

    SST::Link* parent_utilization;
    SST::Link* bytes_tracker;
    SST::Link* lookup_table;
    SST::Link* child_utilization;

    std::vector<ElementConnector*> children;

    void handle_parent_update(SST::Event* ev);
    void handle_bytes_tracker_update(SST::Event* ev);
    void handle_lookup_result(SST::Event* ev);

    void         set_children();
    void         lookup_utilization();
    unsigned int address_tanslation();
};

#endif
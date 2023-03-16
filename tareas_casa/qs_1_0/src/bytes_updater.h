#ifndef BYTES_UPDATER_T
#define BYTES_UPDATER_T

#include "element_connector.h"
#include "model_global.h"
#include "packet_event.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/timeConverter.h>

#define PRI_QUEUES 8

class BytesUpdater : public SST::Component
{

public:
    BytesUpdater(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the Bytes Updater object
     *
     */
    ~BytesUpdater() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
      BytesUpdater, "QS_1_0", "BytesUpdater", SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Delivers bytes updates to the accounting elements",
      COMPONENT_CATEGORY_NETWORK)

    SST_ELI_DOCUMENT_PARAMS(
      {"num_ports", "Number of ports", "52"},
      {"ma_config_file", "Path to Memory Accounting configuration file",
       "qs_1_0/ma_config.json"},
      {"type", "Type of accounting element. [1:MG, 2:QG, 2:PQ]", "1"},
      {"verbose", "", "0"})

    SST_ELI_DOCUMENT_PORTS({"bytes_tracker",
                          "Link to Bytes Tracker component",
                          {"QS_1_0.BytesUseEvent"}})

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"accounting_elements",
       "Accounting elements that this updater has. [Same as ma_config_file]",
       "QS_1_0.ElementConnector"})

    SST_ELI_DOCUMENT_STATISTICS()

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
    BytesUpdater();                      // for serialization only
    BytesUpdater(const BytesUpdater&);   // do not implement
    void operator=(const BytesUpdater&); // do not implement

    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    int         type;
    int         num_ports;
    std::string comp_name;

    SST::Link*                       bytes_tracker;
    std::map<int, ElementConnector*> accouting_elements;
    void                             create_connector(int index, int map_index, SST::SubComponentSlotInfo* lists);

    void handle_new_update(SST::Event* ev);
};

#endif

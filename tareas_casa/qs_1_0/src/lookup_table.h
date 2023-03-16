#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include "element_connector.h"
#include "lookup_event.h"
#include "model_global.h"
#include "packet_event.h"

#include <map>   // std::map
#include <mutex> // std::mutex
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/output.h>
#include <sst/core/subcomponent.h>
#include <sst/core/timeConverter.h>
#include <vector> // std::vector

#define LUT_ENTRIES 4096
#define PRI_QUEUES  8

class LookUpTable : public SST::SubComponent
{

public:
    LookUpTable(SST::ComponentId_t id, SST::Params& params);
    LookUpTable(SST::ComponentId_t id, std::string unnamed_sub);
    /**
     * @brief Destroy the LookUp Table object
     *
     */
    ~LookUpTable() {}

    SST_ELI_REGISTER_SUBCOMPONENT_API(LookUpTable)

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
      LookUpTable, "QS_1_0", "LookUpTable", SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Simple lookup table accesible by accounting elements", LookUpTable)

    SST_ELI_DOCUMENT_PARAMS({"num_ports", "number of egress ports", "52"},
                          {"table_file", "File with data",
                           "qs_1_0/lookup.json"},
                          {"verbose", "", "0"})

    SST_ELI_DOCUMENT_STATISTICS()

    unsigned int lookup(unsigned int address);

private:
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    unsigned int* memory;
};

class LookUpController : public SST::Component
{

public:
    LookUpController(SST::ComponentId_t id, SST::Params& params);
    /**
     * @brief Destroy the LookUp Controller object
     *
     */
    ~LookUpController() {}

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(LookUpController, "QS_1_0", "LookUpController",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "Receives dropped packets",
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
      {"look_up_table", "Tables used by the controller", "QS_1_0.LookUpTable"})

    SST_ELI_DOCUMENT_STATISTICS()

    /**
     * @brief Set up stage of SST
     *
     */
    void setup() {};

    // void init(unsigned int phase);

    void finish() {};
    void handle_new_lookup(SST::Event* ev);

protected:
    LookUpController();                        // for serialization only
    LookUpController(const LookUpController&); // do not implement
    void operator=(const LookUpController&);   // do not implement

    // <REVIEW> do SST handles race conditions?
    std::mutex          safe_lock;
    SST::Output*        out;
    SST::TimeConverter* base_tc;

    std::map<unsigned int, ElementConnector*> mg_elements;
    std::vector<ElementConnector*>            qg_elements;
    std::map<unsigned int, ElementConnector*> pq_elements;

    std::vector<LookUpTable*> LUTs;
    int                       num_ports;

    void set_mg_elements();
    void set_qg_elements();
    void set_pq_elements();
    void set_lookup_tables();

    virtual void send_response(int type, unsigned int element_index, LookUpResponseEvent* res);
};

#endif

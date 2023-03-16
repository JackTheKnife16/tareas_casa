#include <sst/core/sst_config.h>

#include "multi_fifo.h"

#include "sst/core/event.h"

#include "arbiter_events.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new MultiFIFO::MultiFIFO object
 *
 * @param id
 * @param params
 */
MultiFIFO::MultiFIFO(ComponentId_t id, Params& params) : Component(id)
{
    // Number of ports
    ports               = params.find<uint32_t>("ports", 32);
    // Number of priorities
    priorities          = params.find<uint32_t>("priorities", 4);
    // Number of Port Ingress Slices
    port_ingress_slices = params.find<uint32_t>("port_ingress_slices", 2);
    // Port group index
    buffer_slice_index  = params.find<uint32_t>("buffer_slice_index", 0);
    // Entry width
    width               = params.find<uint32_t>("width", 256);
    // Packet window size
    packet_window_size  = params.find<uint32_t>("packet_window_size", 4);
    // Pop size
    pop_size            = params.find<uint32_t>("pop_size", 1);
    // Depth config file
    std::string depth_config =
        params.find<std::string>("depth_config", "osb_1_0/config/fifo_config/packet_fifo_config.json");
    // Ethernet config file
    std::string frontplane_config =
        params.find<std::string>("frontplane_config", "osb_1_0/config/frontplane_config/4x800G.json");
    // Request enable
    req_enable                = params.find<bool>("req_enable", false);
    // Overflow flag
    overflow                  = params.find<bool>("overflow", false);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":MultiFIFO[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Multi-FIFO --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Ports: %d\n", ports);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Priorities: %d\n", priorities);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port Ingress Slices: %d\n", port_ingress_slices);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Buffer Slice index: %d\n", buffer_slice_index);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Entry width: %d\n", width);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Packet Window size: %d\n", packet_window_size);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Pop size: %d\n", pop_size);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Depth config: %s\n", depth_config.c_str());

    base_tc = registerTimeBase("1ps", true);

    // open JSON file for front plane info
    JSON          frontplane_config_data;
    std::ifstream frontplane_config_file(frontplane_config);
    frontplane_config_file >> frontplane_config_data;

    JSON          depth_config_data;
    std::ifstream depth_config_file(depth_config);
    depth_config_file >> depth_config_data;
    // Iterate port ingress slices connected to set everything related to that
    for ( int i = 0; i < port_ingress_slices; i++ ) {
        // First: Create the FIFO and limits for the ports connected to each Port Ingress Slice

        // Compute PG index
        uint32_t pg_index = buffer_slice_index * port_ingress_slices + i;

        // Open Ethernet file based on the port group
        std::string   ethernet_config = frontplane_config_data[pg_index]["ethernet_config"];
        JSON          eth_config_data;
        std::ifstream eth_config_file(ethernet_config);
        eth_config_file >> eth_config_data;

        uint32_t ports_per_pg = eth_config_data.size();
        for ( int j = 0; j < ports_per_pg; j++ ) {
            if ( eth_config_data[j]["enable"] ) {
                // The port in the config is the pid in the pg
                uint32_t pid         = eth_config_data[j]["port"];
                // The port number in the chip is computed as follows
                uint32_t port_number = pid + pg_index * ports_per_pg;
                out->verbose(
                    CALL_INFO, DEBUG, 0, "Creating a logical fifo for port %d which pid is %d and pg index is %d\n",
                    port_number, pid, pg_index);

                // Get limits depending on the speed
                std::string port_speed = eth_config_data[j]["bw"];
                JSON        limits     = depth_config_data[port_speed];

                // REVISIT: add more fifos if priorities will be part of this mfifo
                // for ( int j = 0; j < priorities; j++ ) {
                // Create logical fifos with the limits
                uint32_t fifo_limit = limits["3"]["limit"];
                logical_fifos[port_number] =
                    new LogicalFIFO(port_number, 3, pg_index, fifo_limit, packet_window_size, pop_size, overflow, out);

                out->verbose(CALL_INFO, DEBUG, 0, "Limit for port %d: %d entries\n", port_number, fifo_limit);

                reqs[port_number] = false;
            }
        }

        // Second: Connect the input port to the Port Ingress Slice
        SST::Link* input_connection_link = configureLink(
            "input_" + std::to_string(i), base_tc, new Event::Handler<MultiFIFO>(this, &MultiFIFO::entry_receiver));
        // Check if links are connected
        assert(input_connection_link);
        port_slice_links[pg_index] = input_connection_link;

        // Third: Connect the FIFO update link to the Port Ingress Slice
        SST::Link* update_connection_link = configureLink(
            "fifo_update_" + std::to_string(i), base_tc,
            new Event::Handler<MultiFIFO>(this, &MultiFIFO::entry_receiver));
        assert(update_connection_link);
        fifo_update_links[pg_index] = update_connection_link;
    }

    // Configure links
    ic_req_link   = configureLink("ic_req", base_tc);
    nic_req_link  = configureLink("nic_req", base_tc);
    fifo_req_link = configureLink("fifo_req", base_tc, new Event::Handler<MultiFIFO>(this, &MultiFIFO::server));
    output_link   = configureLink("output", base_tc);

    // Check if links are connected
    assert(fifo_req_link);
    assert(output_link);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Initial stage in SST simulations. Sends the initial status of the connected Multi-FIFO.
 *
 * @param phase Current initialization phase.
 */
void
MultiFIFO::init(uint32_t phase)
{
    if ( phase == 0 ) {
        for ( auto& logical_fifo : logical_fifos ) {
            FIFOStatusEvent* status = logical_fifo.second->get_update();
            fifo_update_links[status->pg_idx]->sendInitData(status);
        }
    }
}

/**
 * @brief Handler for incoming transactions
 *
 * @param ev Transaction
 */
void
MultiFIFO::entry_receiver(SST::Event* ev)
{
    QueueElement* trans = dynamic_cast<QueueElement*>(ev);
    if ( trans ) {
        safe_lock.lock();
        out->verbose(
            CALL_INFO, MODERATE, 0,
            "New transaction received: src_port=%d pri=%d sop=%d eop=%d first=%d last=%d seq=%d\n",
            trans->get_src_port(), trans->get_priority(), trans->get_sop(), trans->get_eop(), trans->get_first(),
            trans->get_last(), trans->get_seq_num());

        LogicalFIFO* dest_fifo = logical_fifos[trans->get_src_port()];

        dest_fifo->push_transaction(trans);

        updater(dest_fifo);
        requester(dest_fifo);

        safe_lock.unlock();
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for incoming pop requests
 *
 * @param ev Request
 */
void
MultiFIFO::server(SST::Event* ev)
{
    FIFORequestEvent* req = dynamic_cast<FIFORequestEvent*>(ev);
    if ( req ) {
        safe_lock.lock();
        LogicalFIFO* req_fifo = logical_fifos[req->port];

        bool eop_found = false;
        for ( int i = 0; i < pop_size; i++ ) {
            QueueElement* trans = NULL;
            if ( !eop_found ) {
                trans = req_fifo->pop();
                out->verbose(CALL_INFO, DEBUG, 0, "Transaction popped %p\n", trans);
                if ( trans ) {
                    if ( trans->get_eop() && trans->get_last() ) {
                        // EOP found... send NULL events after this transaction
                        eop_found = true;
                    }
                }
            }

            output_link->send(trans);
        }

        // When the head changes, the reqs associated is set false to maybe make another req
        reqs[req->port] = false;

        updater(req_fifo);
        requester(req_fifo);

        safe_lock.unlock();
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Receives a FIFO to update to the components connected
 *
 * @param fifo_to_update FIFO to update
 */
void
MultiFIFO::updater(LogicalFIFO* fifo_to_update)
{
    FIFOStatusEvent* status = fifo_to_update->get_update();

    out->verbose(
        CALL_INFO, DEBUG, 0, "Sending an update to acceptance checker: port=%d pri=%d pg=%d space_avail=%d\n",
        status->port_idx, status->pri_idx, status->pg_idx, status->space_available);

    fifo_update_links[status->pg_idx]->send(status);
}

/**
 * @brief Checks if the given logical FIFO can generate a IC or NIC request. (Or nothing)
 *
 * @param fifo_to_check FIFO to check
 */
void
MultiFIFO::requester(LogicalFIFO* fifo_to_check)
{
    if ( req_enable && !reqs[fifo_to_check->get_port()] ) {
        // Create a req only if the fifo is not empty
        if ( !fifo_to_check->is_empty() ) {
            if ( fifo_to_check->get_head_sop() ) {
                // If the head is a SOP, the packet window has to be complete
                if ( fifo_to_check->get_full_packet_window() ) {
                    // Create a IC Req
                    ArbiterRequestEvent* req = new ArbiterRequestEvent();
                    // Set initial chunk because it is a IC Request
                    req->initial_chunk       = true;
                    req->port_idx            = fifo_to_check->get_port();
                    req->pri_idx             = fifo_to_check->get_priority();
                    req->pg_idx              = fifo_to_check->get_pg_index();
                    req->buffer_slice_idx    = buffer_slice_index;

                    out->verbose(
                        CALL_INFO, MODERATE, 0, "Sending a new IC Request: port=%d pri=%d pg=%d buffer_slice=%d\n",
                        req->port_idx, req->pri_idx, req->pg_idx, req->buffer_slice_idx);

                    // Send to IC req link
                    ic_req_link->send(req);
                    reqs[req->port_idx] = true;
                }
            }
            else {
                if ( fifo_to_check->get_pop_ready() ) {
                    // If the head is not a SOP, then a NIC Request must be created
                    ArbiterRequestEvent* req = new ArbiterRequestEvent();
                    // Set initial chunk because it is a IC Request
                    req->initial_chunk       = false;
                    req->port_idx            = fifo_to_check->get_port();
                    req->pri_idx             = fifo_to_check->get_priority();
                    req->pg_idx              = fifo_to_check->get_pg_index();
                    req->buffer_slice_idx    = buffer_slice_index;

                    out->verbose(
                        CALL_INFO, MODERATE, 0, "Sending a new NIC Request: port=%d pri=%d pg=%d buffer_slice=%d\n",
                        req->port_idx, req->pri_idx, req->pg_idx, req->buffer_slice_idx);

                    // Send to NIC req link
                    nic_req_link->send(req);
                    reqs[req->port_idx] = true;
                }
            }
        }
    }
}

/**
 * @brief Finish stage of SST
 *
 */
void
MultiFIFO::finish()
{
    if ( overflow ) {
        out->verbose(CALL_INFO, INFO, 0, "New limit report:\n");
        for ( auto& logical_fifo : logical_fifos ) {
            out->verbose(
                CALL_INFO, MODERATE, 0, "FIFO(%d): %d entries of %d B\n", logical_fifo.first,
                logical_fifo.second->get_limit(), width);
        }
    }
}
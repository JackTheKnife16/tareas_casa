#include <sst/core/sst_config.h>

#include "central_arbiter.h"

#include "sst/core/event.h"

#include "arbiter_events.h"

#include <assert.h>
#include <functional> // std::bind

using namespace SST;

/**
 * @brief Construct a new CentralArbiter::CentralArbiter object
 *
 * @param id
 * @param params
 */
CentralArbiter::CentralArbiter(ComponentId_t id, Params& params) : Component(id)
{
    // Clock frequency
    UnitAlgebra frequency     = params.find<UnitAlgebra>("frequency", "1500 MHz");
    // Number of Buffer Slices
    buffer_slices             = params.find<uint32_t>("buffer_slices", 2);
    // Port per Buffer Slice
    ports_per_buffer          = params.find<uint32_t>("ports_per_buffer", 32);
    // Pop transaction size
    priorities                = params.find<uint32_t>("priorities", 4);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":CentralArbiter[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Central Arbiter --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Frequency: %s\n", frequency.toStringBestSI().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Buffer Slices: %d\n", buffer_slices);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Ports per Buffer Slice: %d\n", ports_per_buffer);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Priorities: %d\n", priorities);

    base_tc = registerTimeBase("1ps", true);

    // First none buffer is chosen
    chosen_buffer_slice = -1;

    // Arbitration routine starts disabled
    arbitration_running = false;

    // Compute the frequency delay
    UnitAlgebra interval = (UnitAlgebra("1") / frequency) / UnitAlgebra("1ps");
    freq_delay           = interval.getRoundedValue();
    out->verbose(CALL_INFO, MODERATE, 0, "Frequency delay is: %lu ps\n", freq_delay);

    // Set up Buffer Slices
    for ( int i = 0; i < buffer_slices; i++ ) {
        // Initialize maps
        // First choice is always 1 before the first port
        ic_choices[i] = -1 + i * ports_per_buffer;
        // IC reqs init
        for ( int j = 0; j < ports_per_buffer; j++ ) {
            // No requests at the beggining
            ic_requests[i][j + i * ports_per_buffer] = false;
        }

        // Create function pointer to notify method
        std::function<void(void)> notify = std::bind(&CentralArbiter::notify_arbitration_routine, this);
        // Create NIC Arbitration elements
        NICArbitration* nic_arb = new NICArbitration(buffer_slices, ports_per_buffer, priorities, i, notify, out);
        non_initial_chunk_arbitration.push_back(nic_arb);

        // Configure in links
        // IC Request Receiver
        std::string buffer_slice_idx = std::to_string(i);
        SST::Link*  ic_rcv_link      = configureLink(
            "ic_req_" + buffer_slice_idx, base_tc,
            new Event::Handler<CentralArbiter>(this, &CentralArbiter::ic_request_receiver));
        ic_req_links.push_back(ic_rcv_link);
        assert(ic_rcv_link);
        // NIC Receiver
        SST::Link* nic_rcv_link = configureLink(
            "nic_req_" + buffer_slice_idx, base_tc,
            new Event::Handler<NICArbitration>(nic_arb, &NICArbitration::nic_request_receiver));
        nic_req_links.push_back(nic_rcv_link);
        assert(nic_rcv_link);

        SST::Link* ic_gnt_link = configureLink("ic_gnt_" + buffer_slice_idx, base_tc);
        ic_grant_links.push_back(ic_gnt_link);
        assert(ic_gnt_link);

        SST::Link* nic_gnt_link = configureLink("nic_gnt_" + buffer_slice_idx, base_tc);
        nic_grant_links.push_back(nic_gnt_link);
        assert(nic_gnt_link);
    }

    // Arbitration conflict resolution routine
    arbitration_conflict_resolution_link = configureSelfLink(
        "arbitration_conflict_resolution", base_tc,
        new Event::Handler<CentralArbiter>(this, &CentralArbiter::arbitration_conflict_resolution));

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Check if there is any IC Req available
 *
 * @return true There is at least one IC Req
 * @return false There aren't any IC Reqs
 */
bool
CentralArbiter::ic_reqs_available()
{
    bool ret = false;
    // Check each Buffer Slice
    for ( auto& buffer_slice_reqs : ic_requests ) {
        for ( auto& port_req : buffer_slice_reqs.second ) {
            if ( port_req.second ) {
                // If there is at least one, finish it
                ret = true;
                break;
            }
        }

        if ( ret ) {
            // Finish both loops
            break;
        }
    }

    return ret;
}

/**
 * @brief It runs the arbitration routine if it is disabled
 *
 */
void
CentralArbiter::notify_arbitration_routine()
{
    safe_lock.lock();
    if ( !arbitration_running ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Enabling arbitration routine in %lu ps\n", freq_delay);
        arbitration_running = true;
        arbitration_conflict_resolution_link->send(freq_delay, NULL);
    }
    safe_lock.unlock();
}

/**
 * @brief It disable the arbitration routine
 *
 */
void
CentralArbiter::disable_arbitration_routine()
{
    out->verbose(CALL_INFO, DEBUG, 0, "Disabling arbitration routine\n");
    safe_lock.lock();
    arbitration_running = false;
    safe_lock.unlock();
}

/**
 * @brief It is the IC Arbitration algorithm that choses a port and buffer slice from the available requests
 *
 * @return arbitration_result_t Arbitration Result
 */
arbitration_result_t
CentralArbiter::initial_chunk_arbitration()
{
    ic_safe_lock.lock();
    int  chosen_port                 = -1;
    bool search                      = true;
    int  search_count                = 0;
    int  initial_chosen_buffer_slice = chosen_buffer_slice;

    // Init the result
    arbitration_result_t result;
    result.found              = false;
    result.port               = 0;
    result.priority           = 0;
    result.buffer_slice_index = 0;
    do {
        // Update search count
        search_count++;
        // Pick a new buffer slice
        chosen_buffer_slice++;
        if ( chosen_buffer_slice >= buffer_slices ) { chosen_buffer_slice = 0; }
        // Get maps with requests
        bool                     found_ic        = false;
        std::map<uint32_t, bool> buffer_requests = ic_requests[chosen_buffer_slice];
        // Check for requests
        for ( int i = 0; i < ports_per_buffer; i++ ) {
            int port_number = i + chosen_buffer_slice * ports_per_buffer;
            if ( buffer_requests[port_number] ) {
                found_ic = true;
                break;
            }
        }

        if ( found_ic ) {
            do {
                int* current_port = &ic_choices[chosen_buffer_slice];
                (*current_port)++;
                // If the chosen port is higher than the limits of the buffer slice, reset to initial
                uint32_t buffer_slice_offset = chosen_buffer_slice * ports_per_buffer;
                if ( *current_port >= (ports_per_buffer + buffer_slice_offset) ) {
                    *current_port = buffer_slice_offset;
                }
                if ( buffer_requests[*current_port] ) {
                    out->verbose(
                        CALL_INFO, DEBUG, 0, "Chosen port for IC Grant: buffer_slice=%d port=%d\n", chosen_buffer_slice,
                        *current_port);
                    search                    = false;
                    // Fill result struct
                    result.found              = true;
                    result.port               = *current_port;
                    // Return priority if needed
                    result.priority           = 3;
                    result.buffer_slice_index = chosen_buffer_slice;
                }
            } while ( search );
        }
        // If nothing is found, the finish the loop
        if ( search_count == buffer_slices && search ) {
            out->verbose(CALL_INFO, MODERATE, 0, "There are not IC Requests.\n");
            chosen_buffer_slice = initial_chosen_buffer_slice;
            search              = false;
        }
    } while ( search );
    ic_safe_lock.unlock();

    return result;
}

/**
 * @brief Receives the incoming Arbiter Requests events and creates a new IC Req in the map
 *
 * @param ev Arbiter Request Event
 */
void
CentralArbiter::ic_request_receiver(SST::Event* ev)
{
    ArbiterRequestEvent* req = dynamic_cast<ArbiterRequestEvent*>(ev);

    if ( req ) {
        bool notify = false;
        if ( !ic_reqs_available() ) { notify = true; }
        out->verbose(
            CALL_INFO, MODERATE, 0, "New IC Request received: buffer_slice=%d port=%d pri=%d.\n", req->buffer_slice_idx,
            req->port_idx, req->pri_idx);
        // Set true the corresondent ic request in the map
        ic_safe_lock.lock();
        ic_requests[req->buffer_slice_idx][req->port_idx] = true;
        ic_safe_lock.unlock();

        if ( notify ) {
            // Notify that a new transaction is received
            notify_arbitration_routine();
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Arbitration Conflic Resolution routine that call the arbitration methods and check if the chosen ports have
 * conficts between the IC and NIC Requests because both are mutually exclusive for each Buffer Slice
 *
 * @param ev NULL event
 */
void
CentralArbiter::arbitration_conflict_resolution(SST::Event* ev)
{
    // Check arbitration for IC Grant
    arbitration_result_t ic_chosen = initial_chunk_arbitration();
    out->verbose(
        CALL_INFO, MODERATE, 0, "IC Grant choice: buffer_slice=%d port=%d.\n", chosen_buffer_slice, ic_chosen.port);

    // Check arbitration for NIC Grant
    std::map<uint32_t, arbitration_result_t> nic_chosen;
    uint32_t                                 buffer_idx = 0;
    for ( auto nic_arb : non_initial_chunk_arbitration ) {
        nic_chosen[buffer_idx] = nic_arb->non_initial_chunk_arbitration();
        nic_arb->set_conflict(false);

        out->verbose(
            CALL_INFO, MODERATE, 0, "NIC Grant choice for Buffer Slice %d: port=%d.\n", buffer_idx,
            nic_chosen[buffer_idx].port);
        buffer_idx++;
    }

    // Is there a conflict with the IC Grant
    if ( nic_chosen[chosen_buffer_slice].found && ic_chosen.found ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Found conflict in Buffer Slice %d.\n", chosen_buffer_slice);
        // Set the conflict flag
        non_initial_chunk_arbitration.at(chosen_buffer_slice)->set_conflict(true);
    }

    // Is there a chosen port for IC Grant?
    if ( ic_chosen.found ) {
        // Create the IC grant event
        ArbiterGrantEvent* ic_grant = new ArbiterGrantEvent();
        ic_grant->initial_chunk     = true;
        ic_grant->port_idx          = ic_chosen.port;
        // TODO: if we add the priority, we should set the actual priority
        ic_grant->pri_idx           = 3;
        ic_grant->buffer_slice_idx  = chosen_buffer_slice;

        out->verbose(
            CALL_INFO, INFO, 0, "Sending IC Grant: buffer_slice=%d port=%d\n", chosen_buffer_slice, ic_chosen.port);
        ic_grant_links.at(chosen_buffer_slice)->send(ic_grant);

        // Clean the IC Req
        ic_safe_lock.lock();
        ic_requests[chosen_buffer_slice][ic_chosen.port] = false;
        ic_safe_lock.unlock();
    }

    // Send the NIC requests from the NIC Arbitration elements
    buffer_idx = 0;
    for ( auto nic_arb : non_initial_chunk_arbitration ) {
        if ( !nic_arb->get_conflict() && nic_chosen[buffer_idx].found ) {
            // Create the NIC grant event
            ArbiterGrantEvent* nic_grant = new ArbiterGrantEvent();
            nic_grant->initial_chunk     = false;
            nic_grant->port_idx          = nic_chosen[buffer_idx].port;
            // TODO: if we add the priority, we should set the actual priority
            nic_grant->pri_idx           = 3;
            nic_grant->buffer_slice_idx  = buffer_idx;

            out->verbose(
                CALL_INFO, INFO, 0, "Sending NIC Grant: buffer_slice=%d port=%d\n", buffer_idx, nic_grant->port_idx);
            nic_grant_links.at(buffer_idx)->send(nic_grant);
            // Clean the NIC Req
            nic_arb->clean_nic_req(nic_grant->port_idx);
        }
        buffer_idx++;
    }

    // Are there still NIC Reqs available?
    bool nic_reqs_available = false;
    for ( auto nic_arb : non_initial_chunk_arbitration ) {
        if ( nic_arb->nic_reqs_available() ) {
            nic_reqs_available = true;
            break;
        }
    }

    // Are there transaction?
    if ( ic_reqs_available() || nic_reqs_available ) {
        // Keep running
        out->verbose(CALL_INFO, DEBUG, 0, "IC or NIC reqs available, arbitration routine keeps running.\n");
        arbitration_conflict_resolution_link->send(freq_delay, NULL);
    }
    else {
        // Disable the routine
        disable_arbitration_routine();
    }
}

/**
 * @brief Finish stage of SST
 *
 */
void
CentralArbiter::finish()
{}
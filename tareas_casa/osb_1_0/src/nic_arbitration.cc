#include <sst/core/sst_config.h>

#include "nic_arbitration.h"

#include "arbiter_events.h"

/**
 * @brief Construct a new NICArbitration::NICArbitration object
 *
 * @param _buffer_slices Number of Buffer Slices
 * @param _ports_per_buffer Number of ports per buffer slice
 * @param _priorities Number of priorities
 * @param _buffer_slice_idx Buffer Slice index
 * @param _notify Pointer to notify function
 * @param _out Output element
 */
NICArbitration::NICArbitration(
    uint32_t _buffer_slices, uint32_t _ports_per_buffer, uint32_t _priorities, uint32_t _buffer_slice_idx,
    std::function<void(void)> _notify, SST::Output* _out)
{
    buffer_slices    = _buffer_slices;
    ports_per_buffer = _ports_per_buffer;
    priorities       = _priorities;
    buffer_slice_idx = _buffer_slice_idx;
    notify_function  = _notify;
    out              = _out;

    conflict    = false;
    chosen_port = -1 + buffer_slice_idx * ports_per_buffer;
    for ( int i = 0; i < ports_per_buffer; i++ ) {
        // Use correct index depending on the Buffer Slice
        nic_requests[i + buffer_slice_idx * ports_per_buffer] = false;
    }
}

/**
 * @brief Receives the incoming NIC Reqs for this Buffer Slice
 *
 * @param ev Arbiter Request Event
 */
void
NICArbitration::nic_request_receiver(SST::Event* ev)
{
    ArbiterRequestEvent* req = dynamic_cast<ArbiterRequestEvent*>(ev);

    if ( req ) {
        bool notify = false;
        if ( !nic_reqs_available() ) { notify = true; }
        // Set true the corresondent ic request in the map
        out->verbose(
            CALL_INFO, MODERATE, 0, "NIC_ARB(%d) New NIC Request received: port=%d pri=%d.\n", buffer_slice_idx,
            req->port_idx, req->pri_idx);
        safe_lock.lock();
        nic_requests[req->port_idx] = true;
        safe_lock.unlock();

        if ( notify ) {
            // Notify that a new transaction is received
            notify_function();
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief It is the arbitration algorithm for NIC Reqs
 *
 * @return arbitration_result_t Result of arbitration
 */
arbitration_result_t
NICArbitration::non_initial_chunk_arbitration()
{
    // Init the result
    arbitration_result_t result;
    result.found              = false;
    result.port               = 0;
    result.priority           = 0;
    result.buffer_slice_index = 0;
    if ( !conflict ) {
        out->verbose(CALL_INFO, DEBUG, 0, "NIC_ARB(%d) Arbitrating for a new NIC Grant.\n", buffer_slice_idx);
        safe_lock.lock();
        bool search              = true;
        int  search_count        = 0;
        int  initial_chosen_port = chosen_port;

        do {
            // Update search count
            search_count++;
            // Pick a new buffer slice
            chosen_port++;
            uint32_t buffer_slice_offset = buffer_slice_idx * ports_per_buffer;
            if ( chosen_port >= (ports_per_buffer + buffer_slice_offset) ) { chosen_port = buffer_slice_offset; }

            // Is there a NIC Requ for this port?
            if ( nic_requests[chosen_port] ) {
                out->verbose(
                    CALL_INFO, MODERATE, 0, "NIC_ARB(%d) Arbitration won by: port=%d.\n", buffer_slice_idx,
                    chosen_port);
                search                    = false;
                // Fill result
                result.found              = true;
                result.port               = chosen_port;
                // Put priority if needed
                result.priority           = 3;
                result.buffer_slice_index = buffer_slice_idx;
            }

            if ( search_count == ports_per_buffer && search ) {
                // Nothing found, finish the loop
                out->verbose(CALL_INFO, MODERATE, 0, "NIC_ARB(%d) There are not NIC Requests.\n", buffer_slice_idx);
                chosen_port = initial_chosen_port;
                search      = false;
            }
        } while ( search );
        safe_lock.unlock();
    }

    return result;
}

/**
 * @brief Check if there are any NIC Reqs available.
 *
 * @return true There is at leat one NIC Req
 * @return false There are not NIC Reqs
 */
bool
NICArbitration::nic_reqs_available()
{
    bool ret = false;
    for ( auto& request : nic_requests ) {
        if ( request.second ) {
            ret = true;
            break;
        }
    }

    return ret;
}

/**
 * @brief Clear the NIC Req flag of a given port
 *
 * @param port Port to clear
 */
void
NICArbitration::clean_nic_req(uint32_t port)
{
    safe_lock.lock();
    nic_requests[port] = false;
    safe_lock.unlock();
}

/**
 * @brief Conflict flag getter
 *
 * @return true There are conflics with the chosen port
 * @return false There are not conflicts
 */
bool
NICArbitration::get_conflict()
{
    return conflict;
}

/**
 * @brief Conflict flag setter
 *
 * @param value New value for conflic
 */
void
NICArbitration::set_conflict(bool value)
{
    conflict = value;
}

#include "logical_fifo.h"

/**
 * @brief Construct a new Receiver Buffer:: Receiver Buffer object
 *
 * @param _port Port number
 * @param _priority Priority number
 * @param _init_limit Initial limit
 * @param _overflow Overflow flag
 */
LogicalFIFO::LogicalFIFO(
    uint32_t _port, uint32_t _priority, uint32_t _pg_index, uint32_t _init_limit, uint32_t _packet_window_size,
    uint32_t _pop_size, bool _overflow, SST::Output* _out)
{
    port               = _port;
    priority           = _priority;
    pg_index           = _pg_index;
    initial_limit      = _init_limit;
    packet_window_size = _packet_window_size;
    limit              = initial_limit;
    space_avail        = limit;
    pop_size           = _pop_size;
    overflow           = _overflow;
    out                = _out;

    head_sop           = false;
    full_packet_window = false;
    pop_ready          = false;
    usage              = 0;
}

/**
 * @brief Adds a new transaction to the FIFO, and checks the head status
 *
 * @param trans
 * @return true
 * @return false
 */
void
LogicalFIFO::push_transaction(QueueElement* trans)
{
    safe_lock.lock();
    // Check if there is space available
    if ( space_avail != 0 ) {
        // Push transaction
        fifo.push_back(trans);
        usage++;


        // Is overflow active
        if ( overflow ) {
            if ( usage > limit ) { limit = usage; }
        }
        else {
            space_avail = limit - usage;
        }
        out->verbose(
            CALL_INFO, DEBUG, 0, "FIFO(%d, %d): New transaction pushed space_avail=%d\n", port, priority, space_avail);
        // Check the head to get a status
        check_head();
    }
    else {
        out->fatal(
            CALL_INFO, -1, "FIFO(%d, %d): Pushing transaction and there is not space available.\n", port, priority);
    }

    safe_lock.unlock();
}

/**
 * @brief Pops words from the FIFO and checks the head status
 *
 * @return QueueElement* Element popped
 */
QueueElement*
LogicalFIFO::pop()
{
    safe_lock.lock();
    QueueElement* trans = NULL;
    if ( !fifo.empty() ) {
        // Get front of vector
        trans = fifo.front();
        // Delete that front
        fifo.erase(fifo.begin());

        // Update space available
        usage--;
        space_avail = limit - usage;

        out->verbose(
            CALL_INFO, DEBUG, 0, "FIFO(%d, %d): New transaction popped space_avail=%d\n", port, priority, space_avail);

        check_head();
    }
    else {
        out->fatal(CALL_INFO, -1, "FIFO(%d, %d): Popping a transaction from an empty FIFO.\n", port, priority);
    }
    safe_lock.unlock();

    return trans;
}

/**
 * @brief Head SOP flag getter
 *
 * @return true FIFO's head is SOP.
 * @return false FIFO's head isn't SOP.
 */
bool
LogicalFIFO::get_head_sop()
{
    safe_lock.lock();
    bool ret = head_sop;
    safe_lock.unlock();

    return ret;
}

/**
 * @brief Full packet window flag getter
 *
 * @return true The packet window is complete.
 * @return false The packet window is not complete.
 */
bool
LogicalFIFO::get_full_packet_window()
{
    safe_lock.lock();
    bool ret = full_packet_window;
    safe_lock.unlock();

    return ret;
}

/**
 * @brief Pop ready flag getter
 *
 * @return true This FIFO is ready to NIC Req
 * @return false This FIFO can't have a NIC Req
 */
bool
LogicalFIFO::get_pop_ready()
{
    safe_lock.lock();
    bool ret = pop_ready;
    safe_lock.unlock();

    return ret;
}

/**
 * @brief Space available getter
 *
 * @return uint32_t Number of entries available
 */
uint32_t
LogicalFIFO::get_space_avail()
{
    safe_lock.lock();
    uint32_t ret = space_avail;
    safe_lock.unlock();

    return ret;
}

/**
 * @brief Port getter
 *
 * @return uint32_t Port number
 */
uint32_t
LogicalFIFO::get_port()
{
    return port;
}

/**
 * @brief Priority getter
 *
 * @return uint32_t Priority
 */
uint32_t
LogicalFIFO::get_priority()
{
    return priority;
}

/**
 * @brief PG index getter
 *
 * @return uint32_t PG index
 */
uint32_t
LogicalFIFO::get_pg_index()
{
    return pg_index;
}

/**
 * @brief Limit getter
 *
 * @return uint32_t Limit
 */
uint32_t
LogicalFIFO::get_limit()
{
    safe_lock.lock();
    uint32_t ret = limit;
    safe_lock.unlock();

    return ret;
}

/**
 * @brief Creates an Status event with the FIFO status
 *
 * @return FIFOStatusEvent* Status Event
 */
FIFOStatusEvent*
LogicalFIFO::get_update()
{
    FIFOStatusEvent* status = new FIFOStatusEvent();
    safe_lock.lock();

    status->port_idx           = port;
    status->pri_idx            = priority;
    status->pg_idx             = pg_index;
    status->space_available    = space_avail;
    status->empty              = fifo.empty();
    status->head_sop           = head_sop;
    status->full_packet_window = full_packet_window;

    safe_lock.unlock();

    return status;
}

/**
 * @brief Check if the FIFO is empty
 *
 * @return true FIFO is empty
 * @return false FIFO is not empty
 */
bool
LogicalFIFO::is_empty()
{
    safe_lock.lock();
    bool ret = fifo.empty();
    safe_lock.unlock();

    return ret;
}

/**
 * @brief Checks the head of the FIFO to update the values
 *
 */
void
LogicalFIFO::check_head()
{
    // If the fifo is empty, all is false
    if ( fifo.empty() ) {
        head_sop           = false;
        full_packet_window = false;

        return;
    }
    // Get front
    QueueElement* head = fifo.front();

    // Is the head a start of packet?
    head_sop = head->get_sop();
    if ( head_sop ) {
        // It is not ready for NIC grant pop
        pop_ready = false;
        // Is there enough packet data to complete the window
        if ( fifo.size() >= packet_window_size ) { full_packet_window = true; }
        else {
            // Set as false
            full_packet_window = false;
            // Maybe one of the available entries has a EOP
            for ( int i = 0; i < fifo.size(); i++ ) {
                QueueElement* trans = fifo.at(i);
                if ( trans->get_eop() && trans->get_last() ) {
                    // The FULL packet is sent to the FE Lookup Pipeline
                    full_packet_window = true;
                    break;
                }
            }
        }
    }
    else {
        // If the head is not eop, the full init chunk fals is set false
        full_packet_window = false;
        if ( fifo.size() >= pop_size ) { pop_ready = true; }
        else {
            // Set as false
            pop_ready = false;
            // Maybe one of the available entries has a EOP
            for ( int i = 0; i < fifo.size(); i++ ) {
                QueueElement* trans = fifo.at(i);
                if ( trans->get_eop() && trans->get_last() ) {
                    // If there is one EOP, it is ready to pop
                    pop_ready = true;
                    break;
                }
            }
        }
    }
}

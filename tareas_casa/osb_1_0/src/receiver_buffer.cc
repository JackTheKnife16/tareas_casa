#include "receiver_buffer.h"

/**
 * @brief Construct a new Receiver Buffer:: Receiver Buffer object
 *
 * @param _index index of the element
 */
ReceiverBuffer::ReceiverBuffer(std::string _name, uint32_t _index, SST::Output* _out)
{
    receiver_name = _name;
    index         = _index;
    out           = _out;
}

/**
 * @brief Construct a new Receiver Buffer:: Receiver Buffer object
 *
 * @param _index index of the element
 * @param _notify Notify function that could activate a routine due to new transactions.
 */
ReceiverBuffer::ReceiverBuffer(std::string _name, uint32_t _index, std::function<void(void)> _notify, SST::Output* _out)
{
    receiver_name   = _name;
    index           = _index;
    notify_function = _notify;
    out             = _out;
}

/**
 * @brief Handler that receives transaction from a link and store them in the transaction
 * queue. Also, it notifies a routine (If configured) that there are new transactions.
 *
 * @param ev Transaction to store.
 */
void
ReceiverBuffer::new_transaction_handler(SST::Event* ev)
{
    out->verbose(CALL_INFO, DEBUG, 0, "(%s) Received new transaction in buffer\n", receiver_name.c_str(), index);
    safe_lock.lock();
    // Check if there are transactions in the queue to determine if the notify function is called
    bool notify = transaction_queue.empty();

    // Push the transaction
    transaction_queue.push(ev);
    safe_lock.unlock();

    // Notify if possible
    if ( notify ) {
        if ( notify_function ) { notify_function(); }
    }
}

/**
 * @brief Checks if the buffer is empty
 *
 * @return true There are transactions in the buffer.
 * @return false There are not transactions in the buffer.
 */
bool
ReceiverBuffer::is_empty()
{
    safe_lock.lock();
    bool empty = transaction_queue.empty();
    safe_lock.unlock();

    return empty;
}

/**
 * @brief Index getter
 *
 * @return uint32_t index number
 */
uint32_t
ReceiverBuffer::get_index()
{
    return index;
}

/**
 * @brief Pops a transaction from the buffer
 *
 * @return SST::Event* Event that was popped
 */
SST::Event*
ReceiverBuffer::pop()
{
    SST::Event* trans = NULL;
    safe_lock.lock();
    if ( !transaction_queue.empty() ) {
        trans = transaction_queue.front();
        transaction_queue.pop();
    }
    safe_lock.unlock();

    return trans;
}
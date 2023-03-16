#include <sst/core/sst_config.h>

#include "port_accumulator.h"

/**
 * @brief Construct a new Receiver Buffer:: Receiver Buffer object
 *
 * @param _index index of the element
 */
PortAccumulator::PortAccumulator(
    uint32_t _port_number, uint32_t _out_bus_width, uint32_t _fds_size, uint32_t _init_chunk, uint32_t _non_init_chunk,
    std::function<void(void)> _notify, SST::Output* _out)
{
    port_number     = _port_number;
    out_bus_width   = _out_bus_width;
    fds_size        = _fds_size;
    notify_function = _notify;
    out             = _out;

    pkt_build_in_progress = false;
    byte_count            = 0;
    chunk_count           = 0;
    word_count            = 0;
    seq_num               = 0;
    init_chunk            = _init_chunk;
    non_init_chunk        = _non_init_chunk;
}

/**
 * @brief Adds a new word to the port accumulartor and depending on the accumulated bytes, it creates a new word. When a
 * new word is created, it notifies the delay element routine.
 *
 * @param pkt_word
 */
void
PortAccumulator::add_new_word(PacketWordEvent* pkt_word)
{
    out->verbose(CALL_INFO, DEBUG, 0, "(Port Accumulator %d) Received a new word.\n", port_number);
    bool notify = false;
    safe_lock.lock();

    // The the byte count is 0, it is stating again
    if ( byte_count == 0 && !pkt_build_in_progress ) {
        pkt_build_in_progress = true;
        out->verbose(
            CALL_INFO, DEBUG, 0, "(Port Accumulator %d) Starting a new word with initial size of %dB\n", port_number,
            fds_size);
        // Adding the FDS hole
        byte_count = fds_size;

        // Store word attributes
        current_pkt_id   = pkt_word->pkt_id;
        current_src_port = pkt_word->src_port;
        current_pkt_size = pkt_word->pkt_size;
        current_priority = pkt_word->priority;

        // Start the seq count
        seq_num = 0;
    }

    // Update byte count
    byte_count += pkt_word->valid_byte_count;

    // Is the port accumulator able to create a new word?
    bool is_last_word = pkt_word->end_of_packet && pkt_word->last_chunk_word;
    if ( byte_count >= out_bus_width || is_last_word ) {
        // Notify when there is a new word and the queue is empty
        notify = words_queue.empty();

        out->verbose(
            CALL_INFO, DEBUG, 0, "(Port Accumulator %d) There are enough bytes to create a word.\n", port_number);
        do {
            create_new_word(is_last_word && byte_count <= out_bus_width);
        } while ( byte_count != 0 && is_last_word );

        // Create word
    }
    safe_lock.unlock();

    // Notify if possible
    if ( notify ) { notify_function(); }
}

/**
 * @brief Check if there is at least one word ready to be sent
 *
 * @return true There is at least one word ready.
 * @return false No words ready.
 */
bool
PortAccumulator::word_ready()
{
    safe_lock.lock();
    bool words_to_send = !words_queue.empty();
    safe_lock.unlock();

    return words_to_send;
}

/**
 * @brief Pops a packet word from the word queue
 *
 * @return PacketWordEvent* First transaction in the queue
 */
PacketWordEvent*
PortAccumulator::pop()
{
    PacketWordEvent* pkt_word = NULL;
    safe_lock.lock();
    if ( !words_queue.empty() ) {
        pkt_word = words_queue.front();
        words_queue.pop();
    }
    safe_lock.unlock();

    return pkt_word;
}

/**
 * @brief Creates a new word based on the chunk configuration.
 *
 */
void
PortAccumulator::create_new_word(bool is_eop)
{
    // Create word event with the stored packet attributes
    PacketWordEvent* pkt_word = new PacketWordEvent();
    pkt_word->pkt_id          = current_pkt_id;
    pkt_word->src_node        = 0;
    pkt_word->src_port        = current_src_port;
    pkt_word->pkt_size        = current_pkt_size;
    pkt_word->priority        = current_priority;

    // Set sequence number
    pkt_word->word_sequence_number = seq_num;

    // Set the current chunk size
    uint32_t current_chunk_size = non_init_chunk - 1;
    if ( chunk_count == 0 ) {
        // Initial chunk so set start of packet
        pkt_word->start_of_packet = true;
        current_chunk_size        = init_chunk - 1;
    }

    // Is this the first word of a chunk?
    if ( word_count == 0 ) {
        pkt_word->first_chunk_word = true;

        // Check if the current chunk size is zero to finish the chunk
        if ( current_chunk_size == 0 ) {
            pkt_word->last_chunk_word = true;

            chunk_count++;

            word_count = 0;
        }
        else {
            word_count++;
        }
    }
    // Is this the last word of a chunk?
    else if ( word_count == current_chunk_size ) {
        pkt_word->last_chunk_word = true;

        chunk_count++;

        word_count = 0;
    }
    else {
        word_count++;
    }

    // If eop is set, set it in the word
    if ( is_eop ) {
        // Set eop attributes to the word
        pkt_word->end_of_packet    = true;
        pkt_word->last_chunk_word  = true;
        pkt_word->end_of_packet    = true;
        pkt_word->valid_byte_count = byte_count;

        // Reset attributes used to create a word
        byte_count  = 0;
        word_count  = 0;
        chunk_count = 0;

        // Set transmission flag to false
        pkt_build_in_progress = false;
    }
    else {
        // It's not an end of packet
        pkt_word->valid_byte_count = out_bus_width;
        byte_count -= out_bus_width;
    }

    // Update sequence count
    seq_num++;

    // Push the word to the queue
    words_queue.push(pkt_word);
}

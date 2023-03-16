#include <sst/core/sst_config.h>

#include "buffer_port_mux.h"

#include "sst/core/event.h"

#include "arbiter_events.h"
#include "fds_event.h"
#include "fifo_events.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new BufferPortMux::BufferPortMux object
 *
 * @param id
 * @param params
 */
BufferPortMux::BufferPortMux(ComponentId_t id, Params& params) : Component(id)
{
    // Buffer Slice index
    buffer_slice_index        = params.find<uint32_t>("buffer_slice_index", 0);
    // FIFO Width
    fifo_width                = params.find<uint32_t>("fifo_width", 128);
    // Pop transaction size
    pop_size                  = params.find<uint32_t>("pop_size", 128);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":BufferPortMux[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Buffer Port Mux --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Buffer Slice index: %d\n", buffer_slice_index);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> FIFO width: %d B\n", fifo_width);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> FIFO pop size: %d words\n", pop_size);

    base_tc = registerTimeBase("1ps", true);

    // Counter for popped packet words
    packet_word_count = 0;

    // Configure in links
    pkt_in_link =
        configureLink("pkt_in", base_tc, new Event::Handler<BufferPortMux>(this, &BufferPortMux::packet_data_sender));
    fds_in_link =
        configureLink("fds_in", base_tc, new Event::Handler<BufferPortMux>(this, &BufferPortMux::fds_ic_sender));
    ic_gnt_link =
        configureLink("ic_gnt", base_tc, new Event::Handler<BufferPortMux>(this, &BufferPortMux::ic_grant_receiver));
    nic_gnt_link =
        configureLink("nic_gnt", base_tc, new Event::Handler<BufferPortMux>(this, &BufferPortMux::nic_grant_receiver));

    pkt_req_link = configureLink("pkt_req", base_tc);
    fds_req_link = configureLink("fds_req", base_tc);
    pkt_out_link = configureLink("pkt_out", base_tc);
    fds_out_link = configureLink("fds_out", base_tc);

    // Check if links are connected
    assert(pkt_in_link);
    assert(fds_in_link);
    assert(ic_gnt_link);
    assert(nic_gnt_link);
    assert(pkt_req_link);
    assert(fds_req_link);
    assert(pkt_out_link);
    // TODO: Do the check when it is complete
    // assert(fds_out_link);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Makes a request to the FDS FIFO matching the given port and priority.
 *
 * @param port Port number
 * @param priority Priority number
 */
void
BufferPortMux::fds_ic_requestor(uint32_t port, uint32_t priority)
{
    out->verbose(CALL_INFO, DEBUG, 0, "Sending request to FDS Multi-FIFO: port=%d priority=%d\n", port, priority);
    FIFORequestEvent* req = new FIFORequestEvent();
    req->port             = port;
    req->priority         = priority;

    fds_req_link->send(req);
}

/**
 * @brief Makes a request to the Packet Data FIFO matching the given port and priority. It also resets the packet word
 * cound and the packet word in progress
 *
 * @param port Port number
 * @param priority Priority number
 */
void
BufferPortMux::packet_data_requestor(uint32_t port, uint32_t priority)
{
    out->verbose(CALL_INFO, DEBUG, 0, "Sending request to packet Multi-FIFO: port=%d priority=%d\n", port, priority);
    FIFORequestEvent* req = new FIFORequestEvent();
    req->port             = port;
    req->priority         = priority;

    safe_lock.lock();
    packet_word_count    = 0;
    pkt_word_in_progress = new PacketWordEvent();
    safe_lock.unlock();

    pkt_req_link->send(req);
}

/**
 * @brief Receives the Initial Chunk Arbiter Grant Events and calls the Packet Data and IC + FDS Requestors to request
 * the data to send.
 *
 * @param ev Arbiter Grant Event
 */
void
BufferPortMux::ic_grant_receiver(SST::Event* ev)
{
    ArbiterGrantEvent* gnt = dynamic_cast<ArbiterGrantEvent*>(ev);
    if ( gnt ) {
        if ( gnt->initial_chunk ) {
            out->verbose(
                CALL_INFO, DEBUG, 0, "An Initial Chunk Grant is received for: port=%d pri=%d\n", gnt->port_idx,
                gnt->pri_idx);
            // Call the requestor to FDS
            fds_ic_requestor(gnt->port_idx, gnt->pri_idx);

            // Call the requestor for packet data
            packet_data_requestor(gnt->port_idx, gnt->pri_idx);
        }
        else {
            out->fatal(CALL_INFO, -1, "Received a Non-Initial Chunk grant to the IC receiver!\n");
        }

        delete gnt;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Receives the Non-Initial Chunk Arbiter Grant Events and calls the Packet Data to request
 * the data to send.
 *
 * @param ev Arbiter Grant Event
 */
void
BufferPortMux::nic_grant_receiver(SST::Event* ev)
{
    ArbiterGrantEvent* gnt = dynamic_cast<ArbiterGrantEvent*>(ev);
    if ( gnt ) {
        if ( !gnt->initial_chunk ) {
            out->verbose(
                CALL_INFO, DEBUG, 0, "An Non-Initial Chunk Grant is received for: port=%d pri=%d\n", gnt->port_idx,
                gnt->pri_idx);
            // Call the requestor for packet data
            packet_data_requestor(gnt->port_idx, gnt->pri_idx);
        }
        else {
            out->fatal(CALL_INFO, -1, "Received a Initial Chunk grant to the NIC receiver!\n");
        }

        delete gnt;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Receives the requested Packet Words,  build a new word, and forwards it to the Data Packer.
 *
 * @param ev Packet Word Event
 */
void
BufferPortMux::packet_data_sender(SST::Event* ev)
{
    safe_lock.lock();
    packet_word_count++;
    out->verbose(CALL_INFO, DEBUG, 0, "Packet word number %d is recieved.\n", packet_word_count);

    if ( ev != NULL ) {
        PacketWordEvent* pkt_w = dynamic_cast<PacketWordEvent*>(ev);
        if ( pkt_w ) {
            out->verbose(
                CALL_INFO, DEBUG, 0,
                "Packet word received: pkt_id=%u src_port=%d pri=%d sop=%d eop=%d first=%d last=%d vbc=%d\n",
                pkt_w->pkt_id, pkt_w->src_port, pkt_w->priority, pkt_w->start_of_packet, pkt_w->end_of_packet,
                pkt_w->first_chunk_word, pkt_w->last_chunk_word, pkt_w->valid_byte_count);
            if ( packet_word_count == 1 ) {
                // First word, then initialize the word to send
                // Basic attributes
                pkt_word_in_progress->pkt_id           = pkt_w->pkt_id;
                pkt_word_in_progress->src_port         = pkt_w->src_port;
                pkt_word_in_progress->priority         = pkt_w->priority;
                pkt_word_in_progress->pkt_size         = pkt_w->pkt_size;
                // Word attributes
                pkt_word_in_progress->start_of_packet  = pkt_w->start_of_packet;
                pkt_word_in_progress->end_of_packet    = pkt_w->end_of_packet;
                pkt_word_in_progress->first_chunk_word = pkt_w->first_chunk_word;
                pkt_word_in_progress->last_chunk_word  = pkt_w->last_chunk_word;
                pkt_word_in_progress->valid_byte_count = pkt_w->valid_byte_count;
            }
            else {
                // Update word attributes
                pkt_word_in_progress->start_of_packet |= pkt_w->start_of_packet;
                pkt_word_in_progress->end_of_packet |= pkt_w->end_of_packet;
                pkt_word_in_progress->first_chunk_word |= pkt_w->first_chunk_word;
                pkt_word_in_progress->last_chunk_word |= pkt_w->last_chunk_word;
                pkt_word_in_progress->valid_byte_count += pkt_w->valid_byte_count;
            }
        }
        else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
        }
    }
    else {
        // Ignore all NULL events
        out->verbose(CALL_INFO, DEBUG, 0, "Ignoring received NULL event.\n");
    }

    // Did the FIFO send all the entries?
    if ( packet_word_count == pop_size ) {
        // Set the Word Sequence number before sending
        if ( pkt_word_in_progress->start_of_packet && pkt_word_in_progress->first_chunk_word ) {
            // Reset the sequence counter if it is the SOP
            word_seq_num_count[pkt_word_in_progress->src_port] = 0;
        }
        pkt_word_in_progress->word_sequence_number = word_seq_num_count[pkt_word_in_progress->src_port];
        word_seq_num_count[pkt_word_in_progress->src_port]++;

        // Received all words, send the built word
        out->verbose(
            CALL_INFO, MODERATE, 0,
            "Sending word to Data Packer: pkt_id=%u src_port=%d pri=%d sop=%d eop=%d first=%d last=%d "
            "seq=%d vbc=%d\n",
            pkt_word_in_progress->pkt_id, pkt_word_in_progress->src_port, pkt_word_in_progress->priority,
            pkt_word_in_progress->start_of_packet, pkt_word_in_progress->end_of_packet,
            pkt_word_in_progress->first_chunk_word, pkt_word_in_progress->last_chunk_word,
            pkt_word_in_progress->word_sequence_number, pkt_word_in_progress->valid_byte_count);
        pkt_out_link->send(pkt_word_in_progress);
    }
    safe_lock.unlock();
}

/**
 * @brief Receives the requested FDS event and forwards it to to the Buffer Slice Mux
 *
 * @param ev FDS Event
 */
void
BufferPortMux::fds_ic_sender(SST::Event* ev)
{
    FDSEvent* fds = dynamic_cast<FDSEvent*>(ev);
    if ( fds ) {
        out->verbose(CALL_INFO, MODERATE, 0, "Sending the FDS to Buffer Slice Mux: pkt_id=%u\n", fds->pkt_id);

        fds_out_link->send(fds);
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}


/**
 * @brief Finish stage of SST
 *
 */
void
BufferPortMux::finish()
{}
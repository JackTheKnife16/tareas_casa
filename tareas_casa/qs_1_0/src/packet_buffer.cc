#include <sst/core/sst_config.h>

#include "packet_buffer.h"

#include "sst/core/event.h"

#include "packet_event.h"

#include "space_event.h"
#include <assert.h>
#include <math.h>

using namespace SST;

/**
 * @brief Construct a new PacketBuffer::PacketBuffer object
 *
 * @param id
 * @param params
 */
PacketBuffer::PacketBuffer(ComponentId_t id, Params& params) : Component(id)
{
    initial_blocks = params.find<int>("initial_blocks", 65536);

    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":PacketBuffer[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    current_avail_blocks = initial_blocks;
    blocks_in_use        = 0;
    num_pkts             = 0;

    out->verbose(CALL_INFO, MODERATE, 0, "-- Packet buffer --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Size: %d blocks\n", current_avail_blocks);

    base_tc = registerTimeBase("1ps", true);

    // Configure links
    buffer_manager = configureLink("buffer_manager", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "buffer_manager link was configured\n");

    acceptance_checker = configureLink(
        "acceptance_checker", base_tc, new Event::Handler<PacketBuffer>(this, &PacketBuffer::handleReceive));
    out->verbose(CALL_INFO, MODERATE, 0, "acceptance_checker link was configured\n");

    egress_ports =
        configureLink("egress_ports", base_tc, new Event::Handler<PacketBuffer>(this, &PacketBuffer::handleTransmit));
    out->verbose(CALL_INFO, MODERATE, 0, "egress_ports link was configured\n");

    // Verify that links are not NULL
    assert(acceptance_checker);
    assert(buffer_manager);
    assert(egress_ports);

    // Create statistics
    blocks_available = registerStatistic<uint32_t>("blocks_available", "1");

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Set up stage of SST
 *
 */
void
PacketBuffer::setup()
{
    // Initial blocks_available value
    blocks_available->addData(current_avail_blocks);
}

/**
 * @brief Computes the blocks for a packet size and decreases the space
 * available
 *
 * @param pkt_size Size of the packet
 * @return true if there is space
 * @return false if there is not space
 */
bool
PacketBuffer::receive_pkt(unsigned int pkt_size)
{
    bool success = true;

    int pkt_blocks = ceil(float(pkt_size + FDS_SIZE) / BLOCK_SIZE);

    out->verbose(CALL_INFO, DEBUG, 0, "Packet needs %d blocks\n", pkt_blocks);
    if ( current_avail_blocks - pkt_blocks > 0 ) {
        // Packet can be stored in packet buffer
        out->verbose(CALL_INFO, MODERATE, 0, "Packet stored in packet buffer, packet size = %dB\n", pkt_size);
        current_avail_blocks -= pkt_blocks;
        out->verbose(CALL_INFO, MODERATE, 0, "Available blocks in packet buffer = %d\n", current_avail_blocks);
        // Reduce blocks_available
        blocks_available->addData(pkt_blocks * -1);
    }
    else {
        success = false;
        out->verbose(CALL_INFO, DEBUG, 0, "No space available in packet buffer for packet of size %dB\n", pkt_size);
    }

    return success;
}

/**
 * @brief Computes the blocks for a packet size and increases the space
 * available
 *
 * @param pkt_size Size of the packet
 * @return true
 * @return false
 */
bool
PacketBuffer::transmit_pkt(unsigned int pkt_size)
{
    bool success = true;

    int pkt_blocks = ceil(float(pkt_size + FDS_SIZE) / BLOCK_SIZE);

    out->verbose(CALL_INFO, DEBUG, 0, "Packet releases %d blocks\n", pkt_blocks);

    if ( current_avail_blocks >= 0 ) {
        // Packet can be removed from packet buffer
        out->verbose(CALL_INFO, MODERATE, 0, "Packet released, pkt_size = %dB\n", pkt_size);
        current_avail_blocks += pkt_blocks;
        out->verbose(CALL_INFO, MODERATE, 0, "Available blocks in buffer = %d\n", current_avail_blocks);
        // Reduce blocks_available
        blocks_available->addData(pkt_blocks);
    }
    else {
        success = false;
        out->verbose(CALL_INFO, MODERATE, 0, "Packet not transmitted, buffer full\n");
    }

    return success;
}

/**
 * @brief Updates the space available to the buffer manager link
 *
 */
void
PacketBuffer::update_space()
{
    SpaceEvent* space_event  = new SpaceEvent();
    space_event->block_avail = current_avail_blocks;
    out->verbose(
        CALL_INFO, INFO, 0,
        "Sending information about available blocks to buffer_manager: current_avail_blocks = %d blocks\n",
        current_avail_blocks);
    buffer_manager->send(space_event);
}

/**
 * @brief Handles incoming packet to the packet buffer
 *
 * @param ev Event with a new packet
 */
void
PacketBuffer::handleReceive(SST::Event* ev)
{
    PacketEvent* event = dynamic_cast<PacketEvent*>(ev);

    if ( event ) {
        unsigned int pkt_size = event->pkt_size;
        out->verbose(
            CALL_INFO, INFO, 0,
            "A new packet arrives through acceptance_checker link: pkt_id "
            "= %u, size = %dB, src port = %d, dest port = %d\n",
            event->pkt_id, pkt_size, event->src_port, event->dest_port);

        safe_lock.lock();

        out->verbose(
            CALL_INFO, MODERATE, 0, "Request to store packet in buffer, size = %dB, pkt_id = %u\n", pkt_size,
            event->pkt_id);

        bool success = this->receive_pkt(pkt_size);
        if ( success ) {
            out->verbose(CALL_INFO, MODERATE, 0, "Updating space information after storing packet\n");
            this->update_space();
        }
        else {
            out->fatal(CALL_INFO, -1, "Adding packet but there are not blocks avail.\n");
        }
        safe_lock.unlock();

        // Release memory
        delete event;
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handles outcoming packet to the packet buffer
 *
 * @param ev Event with a new packet
 */
void
PacketBuffer::handleTransmit(SST::Event* ev)
{
    PacketEvent* event = dynamic_cast<PacketEvent*>(ev);

    if ( event ) {
        unsigned int pkt_size = event->pkt_size;
        out->verbose(
            CALL_INFO, INFO, 0,
            "A pkt was pulled from buffer, because the packet has left "
            "the egress_port[%d], pkt_id = %u, size = %dB, src port = %d, "
            "dest port = %d\n",
            event->dest_port, event->pkt_id, pkt_size, event->src_port, event->dest_port);

        safe_lock.lock();
        out->verbose(
            CALL_INFO, MODERATE, 0, "Request to remove packet from buffer, size = %dB, pkt_id = %u\n", pkt_size,
            event->pkt_id);
        bool success = this->transmit_pkt(pkt_size);
        if ( success ) {
            out->verbose(CALL_INFO, DEBUG, 0, "Updating space information\n");
            this->update_space();
        }
        else {
            out->fatal(CALL_INFO, -1, "Pulling packet but there are not blocks avail.\n");
        }
        safe_lock.unlock();

        // Release memory
        delete event;
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
PacketBuffer::finish()
{
    out->verbose(
        CALL_INFO, INFO, 0, "Packet Buffer finished with %d blocks and started %d blocks\n", current_avail_blocks,
        initial_blocks);
    if ( current_avail_blocks == initial_blocks ) {
        out->verbose(CALL_INFO, INFO, 0, "Packet Buffer finished correctly!\n");
    }
    else {
        out->verbose(
            CALL_INFO, INFO, 0, "Packet Buffer finished with %d blocks and expected %d blocks\n", current_avail_blocks,
            initial_blocks);
    }
}

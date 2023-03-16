#include <sst/core/sst_config.h>

#include "dualq_strict_priority_scheduler.h"

#include "sst/core/event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

DualQStrictPriScheduler::DualQStrictPriScheduler(ComponentId_t id, Params& params) : Scheduler(id, params)
{
    // Init l queue status
    l_queue_not_empty = false;

    // Flag that indicates the queue being served
    serving_l_queue = false;

    // L FIFO update link
    l_bytes_tracker = configureLink("l_bytes_tracker", base_tc);
    assert(l_bytes_tracker);
    // L FIFO update link
    l_port_fifo = configureLink(
        "l_port_fifo", base_tc,
        new Event::Handler<DualQStrictPriScheduler>(this, &DualQStrictPriScheduler::handle_update_l_fifo));
    assert(l_port_fifo);
    // Configure queue links
    Event::Handler<DualQStrictPriScheduler>* queues_handler =
        new Event::Handler<DualQStrictPriScheduler>(this, &DualQStrictPriScheduler::handle_receive_pkt);
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue = "priority_queue_" + std::to_string(i);
        queues[i]             = configureLink(pri_queue, base_tc, queues_handler);
        assert(queues[i]);
    }
    l_priority_queue = configureLink("l_priority_queue", base_tc, queues_handler);
}

/**
 * @brief Handler for update to logical FIFO
 *
 * @param ev Event that contains update information
 */
void
DualQStrictPriScheduler::handle_update_l_fifo(SST::Event* ev)
{
    QueueUpdateEvent* queues_status = dynamic_cast<QueueUpdateEvent*>(ev);

    if ( queues_status ) {
        safe_lock.lock();
        l_queue_not_empty = queues_status->queues_not_empty[0];

        out->verbose(CALL_INFO, DEBUG, 0, "Received QueueUpdateEvent\n");
        if ( send_to_egress ) {
            send_to_egress = false;
            int next_queue = schedule_next_pkt();

            out->verbose(CALL_INFO, DEBUG, 0, "Scheduled next packet to be sent from queue %d\n", next_queue);
            request_pkt(next_queue);
        }
        safe_lock.unlock();
        // Release memory
        delete queues_status->queues_not_empty;
        delete queues_status;
    }
}


/**
 * @brief Schedule the next packet to be sent based on priority
 *
 * @return int Index of queue where next packet is scheduled to be sent from
 */
int
DualQStrictPriScheduler::schedule_next_pkt()
{
    int next_queue = -1;
    if ( l_queue_not_empty ) {
        next_queue = highest_priority + 1;
        out->verbose(CALL_INFO, MODERATE, 0, "Next packet to be sent is from the L queue\n");

        return next_queue;
    }

    for ( int i = highest_priority; i >= 0; i-- ) {
        if ( queues_not_empty[i] ) {
            next_queue = i;
            out->verbose(CALL_INFO, MODERATE, 0, "Next packet to be sent is from queue %d\n", next_queue);

            break;
        }
    }

    return next_queue;
}

/**
 * @brief Requests a packet from the specified queue priority.
 *
 * @param queue_pri Queue to request packet from.
 */
void
DualQStrictPriScheduler::request_pkt(int queue_pri)
{
    queue_serviced->addData(queue_pri);
    if ( queue_pri > highest_priority ) {
        out->verbose(CALL_INFO, INFO, 0, "Requesting Packet: L4S queue\n");
        l_priority_queue->send(NULL);
        // Serving L queue
        serving_l_queue = true;
    }
    else {
        out->verbose(CALL_INFO, INFO, 0, "Requesting Packet: %d\n", queue_pri);
        queues[queue_pri]->send(NULL);
        // Serving L queue
        serving_l_queue = false;
    }
}

/**
 * @brief Handler for received packet
 *
 * @param ev Event that contains the packet to be sent
 */
void
DualQStrictPriScheduler::handle_receive_pkt(SST::Event* ev)
{
    // Send to egress port
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        out->verbose(
            CALL_INFO, 1, 0,
            "Received new packet to schedule: pkt_id = %u, size = %dB, destination "
            "= %d, priority = %d\n",
            pkt->pkt_id, pkt->pkt_size, pkt->dest_port, pkt->priority);

        source_serviced->addData(pkt->src_port);

        egress_port->send(pkt);
        if ( serving_l_queue ) {
            out->verbose(CALL_INFO, INFO, 0, "Sending packet to L4S bytes tracker\n");
            // Send event to L4S bytes tracker
            l_bytes_tracker->send(pkt->clone());
            l_port_fifo->send(NULL);
        }
        else {
            out->verbose(CALL_INFO, INFO, 0, "Sending packet to classic bytes tracker\n");
            // Send event to classic bytes tracker
            bytes_tracker->send(pkt->clone());
            port_fifo->send(NULL);
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

// Version
// 2-----------------------------------------------------------------------------------------------------------------------------------

DualQStrictPriSchedulerV2::DualQStrictPriSchedulerV2(ComponentId_t id, Params& params) : Scheduler(id, params)
{
    // Init l queue status
    l_queue_not_empty = false;

    // Flag that indicates the queue being served
    serving_l_queue = false;

    // L FIFO update link
    l_bytes_tracker = configureLink("l_bytes_tracker", base_tc);
    assert(l_bytes_tracker);
    // L FIFO update link
    l_port_fifo = configureLink(
        "l_port_fifo", base_tc,
        new Event::Handler<DualQStrictPriSchedulerV2>(this, &DualQStrictPriSchedulerV2::handle_update_l_fifo));
    assert(l_port_fifo);

    // Packet in links
    l_packet_in = configureLink(
        "l_packet_in", base_tc,
        new Event::Handler<DualQStrictPriSchedulerV2>(this, &DualQStrictPriSchedulerV2::handle_receive_pkt));
    packet_in = configureLink(
        "packet_in", base_tc,
        new Event::Handler<DualQStrictPriSchedulerV2>(this, &DualQStrictPriSchedulerV2::handle_receive_pkt));
    assert(l_packet_in);
    assert(packet_in);

    // Configure queue links
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue = "priority_queue_" + std::to_string(i);
        queues[i]             = configureLink(pri_queue, base_tc);
        assert(queues[i]);
    }
    l_priority_queue = configureLink("l_priority_queue", base_tc);
    assert(l_priority_queue);
}

/**
 * @brief Handler for updates in L queue
 *
 * @param ev Event to parse
 */
void
DualQStrictPriSchedulerV2::handle_update_l_fifo(SST::Event* ev)
{
    QueueUpdateEvent* queues_status = dynamic_cast<QueueUpdateEvent*>(ev);

    if ( queues_status ) {
        safe_lock.lock();

        out->verbose(CALL_INFO, INFO, 0, "Received QueueUpdateEvent for L queue\n");
        l_queue_not_empty = queues_status->queues_not_empty[0];

        if ( send_to_egress ) {
            send_to_egress = false;
            int next_queue = schedule_next_pkt();
            out->verbose(CALL_INFO, INFO, 0, "Scheduled next packet to be sent from queue %d\n", next_queue);

            request_pkt(next_queue);
        }
        safe_lock.unlock();
        // Release memory
        delete queues_status->queues_not_empty;
        delete queues_status;
    }
}

/**
 * @brief Schedule the next packet from the queue to be sent
 *
 * @return int The queue priority of the next packet to be sent
 */
int
DualQStrictPriSchedulerV2::schedule_next_pkt()
{
    int next_queue = -1;
    if ( l_queue_not_empty ) {
        next_queue = highest_priority + 1;
        out->verbose(CALL_INFO, INFO, 0, "Scheduling next packet to be sent from L4S queue\n");

        return next_queue;
    }

    for ( int i = highest_priority; i >= 0; i-- ) {
        if ( queues_not_empty[i] ) {
            next_queue = i;
            out->verbose(CALL_INFO, INFO, 0, "Scheduling next packet to be sent from queue %d\n", next_queue);
            break;
        }
    }

    return next_queue;
}

/**
 * @brief Sends a request to a queue to send a packet
 *
 * @param queue_pri The priority of the queue being requested
 */
void
DualQStrictPriSchedulerV2::request_pkt(int queue_pri)
{
    queue_serviced->addData(queue_pri);
    if ( queue_pri > highest_priority ) {
        out->verbose(CALL_INFO, INFO, 0, "Requesting packet from L4S queue\n");
        l_priority_queue->send(NULL);
        // Serving L queue
        serving_l_queue = true;
    }
    else {
        out->verbose(CALL_INFO, INFO, 0, "Requesting packet from queue %d\n", queue_pri);
        queues[queue_pri]->send(NULL);
        // Serving L queue
        serving_l_queue = false;
    }
}

/**
 * @brief Handler for receiving packets
 *
 * @param ev Event to handle
 */
void
DualQStrictPriSchedulerV2::handle_receive_pkt(SST::Event* ev)
{
    // If event is NULL, the packet to send was dropped
    if ( ev == NULL ) {
        out->verbose(CALL_INFO, INFO, 0, "New packet to schedule: received a NULL packet\n");
        // We must request an update from the FIFOs and request a new packet
        safe_lock.lock();
        // We set the send_to_ingress, so when the next update arrives we send a new
        // packet
        send_to_egress = true;
        safe_lock.unlock();
        out->verbose(
            CALL_INFO, DEBUG, 0, "Classic queue status: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d\n", queues_not_empty[0],
            queues_not_empty[1], queues_not_empty[2], queues_not_empty[3], queues_not_empty[4], queues_not_empty[5],
            queues_not_empty[6], queues_not_empty[7]);

        out->verbose(CALL_INFO, MODERATE, 0, "L queue status: %d\n", l_queue_not_empty);

        // Request an update to both FIFO
        l_port_fifo->send(NULL);
        port_fifo->send(NULL);
    }
    else {
        // Send to egress port
        PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
        if ( pkt ) {
            out->verbose(
                CALL_INFO, INFO, 0,
                "New packet to schedule: size = %dB, destination = %d, "
                "priority = %d, pkt_id = %u\n",
                pkt->pkt_size, pkt->dest_port, pkt->priority, pkt->pkt_id);
            source_serviced->addData(pkt->src_port);

            egress_port->send(pkt);
            if ( serving_l_queue ) {
                // Send event to L4S bytes tracker
                l_bytes_tracker->send(pkt->clone());
                l_port_fifo->send(NULL);
            }
            else {
                // Send event to classic bytes tracker
                bytes_tracker->send(pkt->clone());
                port_fifo->send(NULL);
            }
        }
        else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
        }
    }
}

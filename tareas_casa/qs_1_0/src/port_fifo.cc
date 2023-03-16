#include <sst/core/sst_config.h>

#include "port_fifo.h"

#include "sst/core/event.h"

#include "dualq_priority_queue.h"
#include "packet_event.h"
#include "queue_update_event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new PortFIFOs::PortFIFOs object
 *
 * @param id
 * @param params
 */
PortFIFOs::PortFIFOs(ComponentId_t id, Params& params) : Component(id)
{
    // Port number
    port                      = params.find<int>("port", 0);
    // Number of queues
    num_queues                = params.find<int>("num_queues", 8);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":PortFIFOs[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Port FIFOs --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port: %d\n", port);

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    wred = configureLink("wred", base_tc, new Event::Handler<PortFIFOs>(this, &PortFIFOs::handle_accepted_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "wred link was configured\n");

    packet_selector = configureLink(
        "packet_selector", base_tc, new Event::Handler<PortFIFOs>(this, &PortFIFOs::handle_buffer_update));
    out->verbose(CALL_INFO, MODERATE, 0, "packet_selector link was configured\n");

    // Check it is not null
    assert(wred);
    assert(packet_selector);

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");

    // Loading priority queue subcomponents
    create_queues();
}

/**
 * @brief Create the priority queues subcomponents
 *
 */
void
PortFIFOs::create_queues()
{
    SubComponentSlotInfo* lists = getSubComponentSlotInfo("priority_queues");
    if ( lists ) {
        for ( unsigned int i = 0; i < num_queues; i++ ) {
            // Checking if it was declared
            if ( lists->isPopulated(i) ) {
                queues[i] = lists->create<PriorityQueue>(i, ComponentInfo::SHARE_STATS);

                // If the queue created is NULL, it means it actually a DualQ object
                if ( !queues[i] ) {
                    queues[i] = lists->create<DualQPriorityQueue>(i, ComponentInfo::SHARE_STATS);
                    out->verbose(
                        CALL_INFO, MODERATE, 0, "Created dualQ priority queue for port_fifo[%d], priority = %d\n", port,
                        i);
                }
                else {
                    out->verbose(
                        CALL_INFO, MODERATE, 0, "Created priority queue for port_fifo[%d], priority = %d\n", port, i);
                }
                assert(queues[i]);
            }
            else {
                out->fatal(CALL_INFO, -1, "Priority queue %d was not provided!\n", i);
            }
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "Should provide priorirty queues subcomponents!\n");
    }
}

/**
 * @brief Checks the status of each queue and sends an update to the scheduler
 *
 */
void
PortFIFOs::update_queues_status()
{
    QueueUpdateEvent* queue_update = new QueueUpdateEvent();

    queue_update->queues_not_empty = new bool[num_queues];
    safe_lock.lock();
    for ( int i = 0; i < num_queues; i++ ) {
        queue_update->queues_not_empty[i] = !queues[i]->is_empty();
        out->verbose(
            CALL_INFO, MODERATE, 0, "Queue status: queue %d is %s\n", i,
            queue_update->queues_not_empty[i] ? "not empty" : "empty");
    }
    safe_lock.unlock();

    packet_selector->send(queue_update);

    out->verbose(
        CALL_INFO, INFO, 0,
        "Sending the current status of the queues through "
        "packet_selector link\n");
}

/**
 * @brief Handler for accepted packets
 *
 * @param ev Event with the new packet
 */
void
PortFIFOs::handle_accepted_pkt(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);

    if ( pkt ) {
        out->verbose(
            CALL_INFO, INFO, 0,
            "Packet %u (size: %dB) was accepted on port_fifo %d, src_port: %d, dest_port: %d, priority: %d\n",
            pkt->pkt_id, pkt->pkt_size, port, pkt->src_port, pkt->dest_port, pkt->priority);

        // If the priority is lower than the number of queue
        if ( pkt->priority < num_queues ) {
            // Send new packet to given priority queue
            queues[pkt->priority]->enqueue_pkt(pkt);
            out->verbose(
                CALL_INFO, INFO, 0, "Packet %u was enqueued in priority queue %d\n", pkt->pkt_id, pkt->priority);
        }
        else {
            // Send it to the highest queue
            queues[num_queues - 1]->enqueue_pkt(pkt);
            out->verbose(
                CALL_INFO, INFO, 0, "Packet %u was enqueued in the highest priority queue (%d)\n", pkt->pkt_id,
                num_queues - 1);
        }
        // Update queues status in scheduler
        update_queues_status();
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for update requests
 *
 * @param ev
 */
void
PortFIFOs::handle_buffer_update(SST::Event* ev)
{
    out->verbose(CALL_INFO, INFO, 0, "Receiving a request for the current status of the queues\n");
    update_queues_status();
}

/**
 * @brief Finish stage of SST
 *
 */
void
PortFIFOs::finish()
{
    out->verbose(CALL_INFO, DEBUG, 0, "Port FIFOS %d final state:\n", port);
    int pkt_count = 0;
    for ( int i = 0; i < num_queues; i++ ) {
        out->verbose(
            CALL_INFO, DEBUG, 0, "port_fifo[%d] and priority queue %d: pending packets = %d\n", port, i,
            queues[i]->size());
        pkt_count += queues[i]->size();
    }
    if ( pkt_count == 0 )
        out->verbose(CALL_INFO, DEBUG, 0, "No packets left in the priority queues\n");
    else
        out->verbose(CALL_INFO, DEBUG, 0, "%d packets left in the priority queues\n", pkt_count);
}

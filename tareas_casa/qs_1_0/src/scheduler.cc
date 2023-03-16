#include <sst/core/sst_config.h>

#include "scheduler.h"

#include "sst/core/event.h"

#include "packet_event.h"
#include "queue_update_event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new Scheduler::Scheduler object
 *
 * @param id
 * @param params
 */
Scheduler::Scheduler(ComponentId_t id, Params& params) : Component(id)
{
    // Number of queues
    num_queues                = params.find<int>("num_queues", 8);
    // Queue with highest priority
    highest_priority          = params.find<int>("highest_priority", 7);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":Scheduler[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Scheduler --\n");

    send_to_egress = true;

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    port_fifo =
        configureLink("port_fifo", base_tc, new Event::Handler<Scheduler>(this, &Scheduler::handle_update_fifos));
    out->verbose(CALL_INFO, MODERATE, 0, "port_fifo link was configured\n");

    egress_port = configureLink(
        "egress_port", base_tc, new Event::Handler<Scheduler>(this, &Scheduler::handle_egress_port_ready));
    out->verbose(CALL_INFO, MODERATE, 0, "egress_port link was configured\n");

    bytes_tracker = configureLink("bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "bytes_tracker link was configured\n");

    // Configure queue links
    Event::Handler<Scheduler>* queues_handler = new Event::Handler<Scheduler>(this, &Scheduler::handle_receive_pkt);
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue = "priority_queue_" + std::to_string(i);
        queues[i]             = configureLink(pri_queue, base_tc, queues_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "priority_queue_%d link was configured\n", i);
        assert(queues[i]);
    }

    // Check it is not null
    assert(port_fifo);
    assert(egress_port);
    assert(bytes_tracker);

    for ( int i = 0; i < num_queues; i++ ) {
        queues_not_empty[i] = false;
    }

    // Queue serviced stat
    queue_serviced  = registerStatistic<uint32_t>("queue_serviced", "1");
    source_serviced = registerStatistic<uint32_t>("source_serviced", "1");

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Scheduling for next pkt
 *
 * @return int Number of priority to request packet, if it returns -1 there are
 * no packets
 */
int
Scheduler::schedule_next_pkt()
{
    int next_queue = -1;
    for ( int i = highest_priority; i >= 0; i-- ) {
        if ( queues_not_empty[i] ) {
            next_queue = i;
            break;
        }
    }

    if ( next_queue != -1 ) {
        out->verbose(CALL_INFO, DEBUG, 0, "queue_%d is the highest priority queue with packets\n", next_queue);
    }
    else {
        out->verbose(CALL_INFO, DEBUG, 0, "All priority queues are empty\n");
    }

    return next_queue;
}

/**
 * @brief Request a packet to the priority FIFO
 *
 * @param queue_pri The priority of the next packet to schedule
 */
void
Scheduler::request_pkt(int queue_pri)
{
    out->verbose(CALL_INFO, DEBUG, 0, "Requesting a packet from the priority_queue_%d\n", queue_pri);
    queue_serviced->addData(queue_pri);
    queues[queue_pri]->send(NULL);
}

/**
 * @brief Handler know when the egress port is ready to send a packet
 *
 * @param ev Event that will always be null
 */
void
Scheduler::handle_egress_port_ready(Event* ev)
{
    safe_lock.lock();
    // From which queue we'll pull a pkt?
    int next_queue = schedule_next_pkt();
    // if result is -1 there are only empty queues
    if ( next_queue != -1 ) {
        request_pkt(next_queue);
        out->verbose(
            CALL_INFO, INFO, 0, "Egress port is ready, requesting a pkt from the priority_queue_%d\n", next_queue);
    }
    else {
        // By setting this flag, the next update will schedule a new packet directly
        send_to_egress = true;
        out->verbose(CALL_INFO, INFO, 0, "Egress port is ready, but in this moment all queues are empty\n");
    }
    safe_lock.unlock();
}

/**
 * @brief Handler for new packet to schedue
 *
 * @param ev Event with the packet
 */
void
Scheduler::handle_receive_pkt(SST::Event* ev)
{
    // Send to egress port
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        out->verbose(
            CALL_INFO, INFO, 0,
            "New packet to schedule: pkt_id = %u, size = %dB, destination "
            "= %d, priority = %d\n",
            pkt->pkt_id, pkt->pkt_size, pkt->dest_port, pkt->priority);
        source_serviced->addData(pkt->src_port);

        egress_port->send(pkt);
        out->verbose(
            CALL_INFO, INFO, 0, "Sending pkt with pkt_id = %u to egress_port[%d]\n", pkt->pkt_id, pkt->dest_port);

        // Send event to bytes tracker
        bytes_tracker->send(pkt->clone());
        out->verbose(CALL_INFO, INFO, 0, "Sending pkt with pkt_id = %u to bytes_tracker\n", pkt->pkt_id);

        port_fifo->send(NULL);
        out->verbose(CALL_INFO, INFO, 0, "Requesting the current status of the queues to the port_fifo\n");
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for queue status update
 *
 * @param ev Event with the status of each queue
 */
void
Scheduler::handle_update_fifos(SST::Event* ev)
{

    QueueUpdateEvent* queues_status = dynamic_cast<QueueUpdateEvent*>(ev);

    if ( queues_status ) {
        safe_lock.lock();
        for ( int i = 0; i < num_queues; i++ ) {
            queues_not_empty[i] = queues_status->queues_not_empty[i];
        }
        out->verbose(
            CALL_INFO, DEBUG, 0,
            "New update in priority queues: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d "
            "6:%d 7:%d\n",
            queues_not_empty[0], queues_not_empty[1], queues_not_empty[2], queues_not_empty[3], queues_not_empty[4],
            queues_not_empty[5], queues_not_empty[6], queues_not_empty[7]);

        if ( send_to_egress ) {
            send_to_egress = false;
            int next_queue = schedule_next_pkt();

            out->verbose(CALL_INFO, INFO, 0, "Requesting pkt from the priority_queue_%d\n", next_queue);
            request_pkt(next_queue);
        }
        safe_lock.unlock();
        // Release memory
        delete queues_status->queues_not_empty;
        delete queues_status;
    }
}

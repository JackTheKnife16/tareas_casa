#include <sst/core/sst_config.h>

#include "priority_queue.h"

#include "sst/core/event.h"

#include "packet_event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new PriorityQueue::PriorityQueue object
 *
 * @param id
 * @param params
 */
PriorityQueue::PriorityQueue(ComponentId_t id, Params& params) : SubComponent(id)
{
    port                      = params.find<int>("port", 0);
    priority                  = params.find<int>("priority", 0);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":PriorityQueue[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Priority Queue --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port: %d\n", port);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Priority: %d\n", priority);

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    scheduler = configureLink(
        "scheduler", base_tc, new Event::Handler<PriorityQueue>(this, &PriorityQueue::handle_pkt_request));
    out->verbose(CALL_INFO, MODERATE, 0, "scheduler link was configured\n");

    scheduler_size = configureLink(
        "scheduler_sizes", base_tc, new Event::Handler<PriorityQueue>(this, &PriorityQueue::handle_pkt_size_request));
    out->verbose(CALL_INFO, MODERATE, 0, "scheduler_sizes link was configured\n");

    // Check it is not null
    assert(scheduler);

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");

    // Stats init
    num_packets = registerStatistic<uint32_t>("num_packets", "1");
    num_bytes   = registerStatistic<uint32_t>("num_bytes", "1");
}

/**
 * @brief Return the current size of the queue
 *
 * @return int Queue size
 */
int
PriorityQueue::size()
{
    int queue_size = pkt_queue.size();
    out->verbose(CALL_INFO, DEBUG, 0, "Queue Size = %d\n", queue_size);
    return queue_size;
}

/**
 * @brief Add a new packet to the queue
 *
 * @param pkt Packet to enqueue
 */
void
PriorityQueue::enqueue_pkt(PacketEvent* pkt)
{
    safe_lock.lock();
    // Enqueueing packet
    pkt_queue.push(pkt);
    out->verbose(
        CALL_INFO, INFO, 0, "Adding a new packet to priority_queue[%d], pkt_id = %u\n", pkt->priority, pkt->pkt_id);
    // Stat collection
    num_packets->addData(1);
    num_bytes->addData(pkt->pkt_size);
    safe_lock.unlock();
}

/**
 * @brief Checks if the queue is empty
 *
 * @return true When queue is empty
 * @return false When queue is with packets
 */
bool
PriorityQueue::is_empty()
{
    safe_lock.lock();
    bool queque_status = pkt_queue.empty();
    if ( queque_status )
        out->verbose(CALL_INFO, DEBUG, 0, "queue is empty\n");
    else
        out->verbose(CALL_INFO, DEBUG, 0, "queue is not empty\n");

    safe_lock.unlock();

    return queque_status;
}

/**
 * @brief Handler for packet requests
 *
 * @param ev Event that will always be null
 */
void
PriorityQueue::handle_pkt_request(SST::Event* ev)
{
    safe_lock.lock();
    out->verbose(CALL_INFO, INFO, 0, "Received a packet request from scheduler[%d]\n", port);
    // Dequeueing packet
    PacketEvent* pkt = pkt_queue.front();
    pkt_queue.pop();
    // Stat collection
    // Negative so it can be subtracted
    num_packets->addData(-1);
    num_bytes->addData(pkt->pkt_size * -1);
    safe_lock.unlock();

    scheduler->send(pkt);
    out->verbose(CALL_INFO, INFO, 0, "Sending a packet to scheduler, pkt_id = %u\n", pkt->pkt_id);
}

/**
 * @brief Handler for size of packet requests
 *
 * @param ev Event always will be NULL
 */
void
PriorityQueue::handle_pkt_size_request(SST::Event* ev)
{
    safe_lock.lock();

    out->verbose(CALL_INFO, INFO, 0, "Received a packet size request from scheduler_dwrr\n");

    PacketEvent* pkt = pkt_queue.front();

    safe_lock.unlock();

    out->verbose(
        CALL_INFO, INFO, 0,
        "Sending information about the size of packet in front of "
        "queue, size = %d",
        pkt->pkt_size);

    scheduler_size->send(pkt);
}

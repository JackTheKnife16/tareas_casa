#include <sst/core/sst_config.h>

#include "dualq_priority_queue.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new DualQPriorityQueue::DualQPriorityQueue object
 *
 * @param id
 * @param params
 */
DualQPriorityQueue::DualQPriorityQueue(ComponentId_t id, Params& params) : PriorityQueue(id, params)
{
    // Configure in links
    scheduler = configureLink(
        "scheduler", base_tc, new Event::Handler<DualQPriorityQueue>(this, &DualQPriorityQueue::handle_pkt_request));
    out->verbose(CALL_INFO, MODERATE, 0, "scheduler link was configured\n");

    output = configureLink("output", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "output link was configured\n");

    // Check it is not null
    assert(scheduler);
    assert(output);
}

/**
 * @brief Handler for packet requests
 *
 * @param ev Event that will always be null
 */
void
DualQPriorityQueue::handle_pkt_request(SST::Event* ev)
{
    safe_lock.lock();
    PacketEvent* pkt = pkt_queue.front();
    out->verbose(CALL_INFO, INFO, 0, "Received a request to send a new packet\n");
    pkt_queue.pop();
    // Stat collection
    // Negative so it can be subtracted
    num_packets->addData(-1);
    num_bytes->addData(pkt->pkt_size * -1);
    safe_lock.unlock();

    out->verbose(
        CALL_INFO, INFO, 0, "Sending packet: size=%d, src_port=%d, ip_ecn=0x%X, pkt_id=%u\n", pkt->pkt_size,
        pkt->src_port, pkt->ip_ecn, pkt->pkt_id);

    output->send(pkt);
}

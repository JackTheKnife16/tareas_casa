#include <sst/core/sst_config.h>

#include "scheduler_dwrr.h"

#include "sst/core/event.h"

#include "packet_event.h"
#include "queue_update_event.h"

#include <assert.h>
#include <cmath>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new SchedulerDWRR::SchedulerDWRR object
 *
 * @param id
 * @param params
 */
SchedulerDWRR::SchedulerDWRR(ComponentId_t id, Params& params) : Component(id)
{
    // Number of queues
    num_queues                = params.find<int>("num_queues", 8);
    // Queue with highest priority
    highest_priority          = params.find<int>("highest_priority", 7);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":SchedulerDWRR[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);
    // Weigths config
    std::string dwrr_config_path =
        params.find<std::string>("dwrr_config_path", "qs_1_0/config/scheduler/dwrr_config_basic.json");
    JSON          dwrr_config;
    std::ifstream dwrr_config_file(dwrr_config_path);
    dwrr_config_file >> dwrr_config;
    // Init weights map
    std::string weights_string = "[ ";
    if ( dwrr_config.size() >= num_queues ) {
        for ( int i = 0; i < num_queues; i++ ) {
            std::string queue_index = std::to_string(i);
            weights[i]              = dwrr_config[queue_index]["weight"];
            // For logging
            weights_string.append(queue_index + ":" + std::to_string(weights[i]) + " ");
        }
        weights_string.append("]");
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Config file %s doesn't have enough queues!\n");
    }

    out->verbose(CALL_INFO, MODERATE, 0, "-- SchedulerDWRR --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of queues: %d\n", num_queues);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Highest priority: %d\n", highest_priority);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Weight config: %s\n", weights_string.c_str());

    send_egress = false;

    base_tc = registerTimeBase("1ps", true);

    for ( int k = 0; k < num_queues; k++ ) {
        activity[k] = INACTIVE;
        credits[k]  = 0;
    }

    all_queues_empty = true;

    // Configure in links
    port_fifo = configureLink(
        "port_fifo", base_tc, new Event::Handler<SchedulerDWRR>(this, &SchedulerDWRR::handle_update_fifos));
    out->verbose(CALL_INFO, MODERATE, 0, "port_fifo link was configured\n");

    egress_port = configureLink(
        "egress_port", base_tc, new Event::Handler<SchedulerDWRR>(this, &SchedulerDWRR::handle_egress_port_ready));
    out->verbose(CALL_INFO, MODERATE, 0, "egress_port link was configured\n");

    bytes_tracker = configureLink("bytes_tracker", base_tc);

    // Configure queue links
    Event::Handler<SchedulerDWRR>* queues_handler =
        new Event::Handler<SchedulerDWRR>(this, &SchedulerDWRR::handle_receive_pkt);
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue = "priority_queue_" + std::to_string(i);
        queues[i]             = configureLink(pri_queue, base_tc, queues_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "priority_queue_%d link was configured\n", i);
        assert(queues[i]);
    }

    // Configure queues_size_front links
    Event::Handler<SchedulerDWRR>* queues_size_front_handler =
        new Event::Handler<SchedulerDWRR>(this, &SchedulerDWRR::handle_receive_pkt_size);
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue_front_size = "front_size_priority_queue_" + std::to_string(i);
        front_size[i]                    = configureLink(pri_queue_front_size, base_tc, queues_size_front_handler);
        out->verbose(CALL_INFO, MODERATE, 0, "front_size_priority_queue_%d link was configured\n", i);
        assert(front_size[i]);
    }

    // Check it is not null
    assert(port_fifo);
    assert(egress_port);
    // assert(bytes_tracker);

    for ( int i = 0; i < num_queues; i++ ) {
        queues_not_empty[i] = false;
    }

    // Queue serviced stat
    queue_serviced  = registerStatistic<uint32_t>("queue_serviced", "1");
    source_serviced = registerStatistic<uint32_t>("source_serviced", "1");

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Simple contructor for children
 *
 * @param id
 */
SchedulerDWRR::SchedulerDWRR(ComponentId_t id) : Component(id) {}

/**
 * @brief Request size of packet to the priority FIFO
 *
 * @param queue_pri The priority of the next packet size to schedule
 */
void
SchedulerDWRR::request_pkt_size(int queue_pri)
{
    out->verbose(CALL_INFO, INFO, 0, "Requesting packet size to priority_queue_%d\n", queue_pri);
    front_size[queue_pri]->send(NULL);
}

/**
 * @brief Request size of packet to the priority FIFO
 *
 * @param queue_pri The priority of the packet to schedule
 */
void
SchedulerDWRR::request_pkt(int queue_pri)
{
    out->verbose(CALL_INFO, INFO, 0, "Requesting packet to priority_queue_%d\n", queue_pri);
    queue_serviced->addData(queue_pri);
    queues[queue_pri]->send(NULL);
}

/**
 * @brief Update the credits of each priority queue
 *
 * @return int index of high priority queues with min number of cycles to reach
 * a non negative credit
 */
int
SchedulerDWRR::update_credits()
{
    int min_cycle = 1000000; // high value for the first comparison
    int k_cycle;
    int value;
    int index;

    // gets minimum number of cycles to reach at least one non negative credit
    for ( int k = 0; k < num_queues; k++ ) {
        if ( credits[k] < 0 ) {
            value   = -1 * credits[k];
            k_cycle = (int)ceil((float)value / (weights[k]));

            if ( k_cycle <= min_cycle ) {
                min_cycle = k_cycle;
                index     = k;
            }
            out->verbose(
                CALL_INFO, DEBUG, 0, "Queue_%d will need %d cycles to reach a non-negative credit\n", k, k_cycle);
        }
    }

    // update the credits of priority queues, ever at least one credits[k] >= 0
    for ( int k = 0; k < num_queues; k++ ) {
        if ( credits[k] < 0 ) {
            credits[k] += min_cycle * (weights[k]); // weights[k] is the weight of queue[k]
            out->verbose(CALL_INFO, DEBUG, 0, "Queue_%d credits updated to %d\n", k, credits[k]);
        }
    }

    return index;
}

/**
 * @brief Handler know when the egress port is ready to send a packet
 *
 * @param ev Event that will always be null
 */
void
SchedulerDWRR::handle_egress_port_ready(Event* ev)
{

    int  index;
    bool all_inactive = true;
    // When one or more lower priority packets reached non-negative credits in the
    // above calculation, they are eligible to be sent to the egress port, no
    // calculations are made. The one with the highest priority that meets the
    // conditions is always sent first and the others wait for the next request
    // from the egress port.
    for ( int k = highest_priority; k > -1; k-- ) {
        if ( activity[k] == ACTIVE && credits[k] >= 0 ) {
            send_egress = true;
            index       = k;
            out->verbose(CALL_INFO, INFO, 0, "Packet eligible for egress port with priority %d found\n", index);
            break;
        }
    }
    // direct request without calculations
    if ( send_egress ) {
        send_egress = false;
        request_pkt(index);
        out->verbose(CALL_INFO, INFO, 0, "Packet with priority %d sent directly to the egress port\n", index);
    }
    // request with calculation or scheduler waiting
    else {
        // check the activity of each queue
        for ( int k = 0; k < num_queues; k++ ) {
            if ( activity[k] != INACTIVE ) {
                all_inactive = false;
                break;
            }
        }
        // if at least one activity[k] is not inactive, then it does the credit
        // calculation with the function update_credits() and sends a request for
        // the pkt that gets non-negative credits
        if ( !all_inactive ) {
            index = update_credits();
            request_pkt(index);
            out->verbose(
                CALL_INFO, INFO, 0, "Packet with priority %d sent to the egress port after credit calculation\n",
                index);
        }
        // if ALL activity[k] == INACTIVE this means that the priority queues are
        // empty
        else {
            all_queues_empty = true;
            out->verbose(CALL_INFO, INFO, 0, "All priority queues are empty, waiting for incoming packets\n");
        }
    }
}

/**
 * @brief Handler for new packet to schedue
 *
 * @param ev Event with the packet
 */
void
SchedulerDWRR::handle_receive_pkt(SST::Event* ev)
{

    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        out->verbose(
            CALL_INFO, INFO, 0,
            "New packet to schedule: pkt_id = %u, size = %dB, destination "
            "= %d, priority = %d\n",
            pkt->pkt_id, pkt->pkt_size, pkt->dest_port, pkt->priority);
        source_serviced->addData(pkt->src_port);
        activity[pkt->priority] = INACTIVE;

        egress_port->send(pkt);
        out->verbose(
            CALL_INFO, INFO, 0, "Sending pkt with pkt_id = %u to egress_port[%d]\n", pkt->pkt_id, pkt->dest_port);

        // Send event to bytes tracker
        bytes_tracker->send(pkt->clone());
        out->verbose(CALL_INFO, INFO, 0, "Sending a clone pkt with pkt_id = %u to bytes_tracker\n", pkt->pkt_id);

        port_fifo->send(NULL);
        out->verbose(CALL_INFO, INFO, 0, "Requesting the current status of the queues to the port_fifo\n");
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Handler for new size packet to schedule
 *
 * @param ev Event with the packet
 */
void
SchedulerDWRR::handle_receive_pkt_size(SST::Event* ev)
{

    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        out->verbose(
            CALL_INFO, INFO, 0, "Received size of the packet that is at the front of priority_queue_%d, pkt_size = %dB",
            pkt->priority, pkt->pkt_size);

        // updated activity and credits
        activity[pkt->priority] = ACTIVE;
        credits[pkt->priority] += -1 * pkt->pkt_size;
        // send Null to egress_port if flag all_queues_empty is true
        if ( all_queues_empty ) {
            egress_port->send(NULL);
            out->verbose(CALL_INFO, INFO, 0, "All priority queues are empty, sending Null event to egress_port\n");
            all_queues_empty = false;
        }
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
SchedulerDWRR::handle_update_fifos(SST::Event* ev)
{

    QueueUpdateEvent* queues_status = dynamic_cast<QueueUpdateEvent*>(ev);

    if ( queues_status ) {
        safe_lock.lock();
        for ( int i = 0; i < num_queues; i++ ) {
            queues_not_empty[i] = queues_status->queues_not_empty[i];
            if ( queues_not_empty[i] && activity[i] == INACTIVE && credits[i] >= 0 ) {
                request_pkt_size(i);
                // prevents a pkt that has already been requested from being
                // re-requested
                activity[i] = WAIT;
                out->verbose(
                    CALL_INFO, INFO, 0,
                    "Requesting packet size from priority_queue_%d since it has packets and is currently inactive and "
                    "has non-negative credits\n",
                    i);
            }
        }

        out->verbose(
            CALL_INFO, DEBUG, 0,
            "new update in priority queues: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d "
            "6:%d 7:%d\n",
            queues_not_empty[0], queues_not_empty[1], queues_not_empty[2], queues_not_empty[3], queues_not_empty[4],
            queues_not_empty[5], queues_not_empty[6], queues_not_empty[7]);

        safe_lock.unlock();
        // Release memory
        delete queues_status;
    }
}

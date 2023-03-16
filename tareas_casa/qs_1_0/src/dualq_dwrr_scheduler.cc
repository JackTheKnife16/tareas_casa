#include <sst/core/sst_config.h>

#include "dualq_dwrr_scheduler.h"

#include "sst/core/event.h"

#include <assert.h>
#include <fstream>
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

DualQDWRRScheduler::DualQDWRRScheduler(ComponentId_t id, Params& params) : SchedulerDWRR(id)
{
    // Number of queues
    num_queues = params.find<int>("num_queues", 9);

    // Queue with highest priority
    highest_priority = params.find<int>("highest_priority", 8);

    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":DualQDWRRScheduler[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    // Weigths config
    std::string dwrr_config_path =
        params.find<std::string>("dwrr_config_path", "qs_1_0/config/scheduler/dwrr_config_dualq.json");

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
        out->fatal(CALL_INFO, -1, "Error! Config file %s doesn't have enough queues!\n", dwrr_config_path.c_str());
    }

    out->verbose(CALL_INFO, MODERATE, 0, "-- DualQ SchedulerDWRR V1 --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of queues: %d\n", num_queues);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Highest priority: %d\n", highest_priority);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Weight config: %s\n", weights_string.c_str());

    base_tc = registerTimeBase("1ps", true);

    send_egress = false;

    for ( int k = 0; k < num_queues; k++ ) {
        activity[k] = INACTIVE;
        credits[k]  = 0;
    }

    for ( int i = 0; i < num_queues; i++ ) {
        queues_not_empty[i] = false;
    }

    all_queues_empty = true;

    // Flag that indicates the queue being served
    serving_l_queue = false;

    // Configure classic links
    port_fifo = configureLink(
        "port_fifo", base_tc, new Event::Handler<DualQDWRRScheduler>(this, &DualQDWRRScheduler::handle_update_fifos));
    out->verbose(CALL_INFO, MODERATE, 0, "port_fifo link was configured\n");

    egress_port = configureLink(
        "egress_port", base_tc,
        new Event::Handler<DualQDWRRScheduler>(this, &DualQDWRRScheduler::handle_egress_port_ready));
    out->verbose(CALL_INFO, MODERATE, 0, "egress_port link was configured\n");

    bytes_tracker = configureLink("bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "bytes_tracker link was configured\n");

    // Check it is not null
    assert(port_fifo);
    assert(egress_port);
    assert(bytes_tracker);

    // L FIFO update link
    l_bytes_tracker = configureLink("l_bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "l_bytes_tracker link was configured\n");
    assert(l_bytes_tracker);
    // L FIFO update link
    l_port_fifo = configureLink(
        "l_port_fifo", base_tc,
        new Event::Handler<DualQDWRRScheduler>(this, &DualQDWRRScheduler::handle_update_l_fifo));
    out->verbose(CALL_INFO, MODERATE, 0, "l_port_fifo link was configured\n");
    assert(l_port_fifo);
    // Configure queue links
    Event::Handler<DualQDWRRScheduler>* queues_handler =
        new Event::Handler<DualQDWRRScheduler>(this, &DualQDWRRScheduler::handle_receive_pkt);
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue = "priority_queue_" + std::to_string(i);
        if ( i == (num_queues - 1) ) { pri_queue = "l_priority_queue"; }
        out->verbose(CALL_INFO, MODERATE, 0, "Linking queue %d with port name %s\n", i, pri_queue.c_str());
        queues[i] = configureLink(pri_queue, base_tc, queues_handler);
        assert(queues[i]);
    }

    // Configure queues_size_front links
    Event::Handler<DualQDWRRScheduler>* queues_size_front_handler =
        new Event::Handler<DualQDWRRScheduler>(this, &DualQDWRRScheduler::handle_receive_pkt_size);
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue_front_size = "front_size_priority_queue_" + std::to_string(i);
        if ( i == (num_queues - 1) ) {
            pri_queue_front_size = "l_front_size_priority_queue";
            queues_size_front_handler =
                new Event::Handler<DualQDWRRScheduler>(this, &DualQDWRRScheduler::handle_l_receive_pkt_size);
        }
        out->verbose(
            CALL_INFO, MODERATE, 0, "Linking front size queue %d with port name %s\n", i, pri_queue_front_size.c_str());
        front_size[i] = configureLink(pri_queue_front_size, base_tc, queues_size_front_handler);
        assert(front_size[i]);
    }

    // Queue serviced stat
    queue_serviced  = registerStatistic<uint32_t>("queue_serviced", "1");
    source_serviced = registerStatistic<uint32_t>("source_serviced", "1");

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

void
DualQDWRRScheduler::handle_update_fifos(SST::Event* ev)
{
    out->verbose(CALL_INFO, 1, 0, "New update fifo normal: ptr=%p\n", ev);
    QueueUpdateEvent* queues_status = dynamic_cast<QueueUpdateEvent*>(ev);

    if ( queues_status ) {
        safe_lock.lock();
        for ( int i = 0; i < num_queues - 1; i++ ) {
            queues_not_empty[i] = queues_status->queues_not_empty[i];
            if ( queues_not_empty[i] && activity[i] == INACTIVE && credits[i] >= 0 ) {
                request_pkt_size(i);
                // prevents a pkt that has already been requested from being
                // re-requested
                activity[i] = WAIT;
            }
        }

        out->verbose(
            CALL_INFO, DEBUG, 0,
            "New update in priority queues: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d "
            "6:%d 7:%d\n",
            queues_not_empty[0], queues_not_empty[1], queues_not_empty[2], queues_not_empty[3], queues_not_empty[4],
            queues_not_empty[5], queues_not_empty[6], queues_not_empty[7]);

        safe_lock.unlock();
        // Release memory
        delete queues_status;
    }
}

void
DualQDWRRScheduler::handle_update_l_fifo(SST::Event* ev)
{
    QueueUpdateEvent* queues_status = dynamic_cast<QueueUpdateEvent*>(ev);

    if ( queues_status ) {
        safe_lock.lock();
        int l_index               = num_queues - 1;
        queues_not_empty[l_index] = queues_status->queues_not_empty[0];
        if ( queues_not_empty[l_index] && activity[l_index] == INACTIVE && credits[l_index] >= 0 ) {
            request_pkt_size(l_index);
            activity[l_index] = WAIT;
        }

        out->verbose(
            CALL_INFO, INFO, 0, "New update from L4S queue to priority queues: Is it not empty? %d\n",
            queues_not_empty[l_index]);

        safe_lock.unlock();
        // Release memory
        delete queues_status->queues_not_empty;
        delete queues_status;
    }
}

void
DualQDWRRScheduler::handle_l_receive_pkt_size(SST::Event* ev)
{
    front_size_lock.lock();
    if ( ev != NULL ) {
        PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
        if ( pkt ) {
            int l_index       = num_queues - 1;
            // updated activity and credits
            activity[l_index] = ACTIVE;
            credits[l_index] += -1 * pkt->pkt_size;
            // send Null to egress_port if flag all_queues_empty is true
            if ( all_queues_empty ) {
                egress_port->send(NULL);
                all_queues_empty = false;
            }
            out->verbose(
                CALL_INFO, INFO, 0, "New front packet size event: activity = %d, credits = %d, pkt_size = %d\n",
                activity[l_index], credits[l_index], pkt->pkt_size);
        }
        else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
        }
    }
    front_size_lock.unlock();
}

/**
 * @brief Overriding just add the mutex lock
 *
 * @param ev Event with the front size
 */
void
DualQDWRRScheduler::handle_receive_pkt_size(SST::Event* ev)
{
    front_size_lock.lock();
    SchedulerDWRR::handle_receive_pkt_size(ev);
    front_size_lock.unlock();
}

void
DualQDWRRScheduler::request_pkt(int queue_pri)
{
    // Set serving_l_queue when we are about to request a packet for that queue
    serving_l_queue = queue_pri == (num_queues - 1);
    // Call parent's method
    SchedulerDWRR::request_pkt(queue_pri);
}

void
DualQDWRRScheduler::handle_receive_pkt(SST::Event* ev)
{
    // Send to egress port
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        out->verbose(
            CALL_INFO, INFO, 0,
            "New packet to schedule: id = %u, size = %dB, destination = "
            "%d, priority = %d\n",
            pkt->pkt_id, pkt->pkt_size, pkt->dest_port, pkt->priority);
        source_serviced->addData(pkt->src_port);

        egress_port->send(pkt);
        if ( serving_l_queue ) {
            activity[num_queues - 1] = INACTIVE;
            // Send event to L4S bytes tracker
            l_bytes_tracker->send(pkt->clone());
            out->verbose(CALL_INFO, INFO, 0, "Sending pkt through l_bytes_tracker link, pkt_id = %u\n", pkt->pkt_id);
            l_port_fifo->send(NULL);
        }
        else {
            activity[pkt->priority] = INACTIVE;
            // Send event to classic bytes tracker
            bytes_tracker->send(pkt->clone());
            out->verbose(CALL_INFO, INFO, 0, "Sending pkt through bytes_tracker link, pkt_id = %u\n", pkt->pkt_id);
            port_fifo->send(NULL);
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

// Version
// 2-----------------------------------------------------------------------------------------------------------------------------------

DualQDWRRSchedulerV2::DualQDWRRSchedulerV2(ComponentId_t id, Params& params) : SchedulerDWRR(id)
{
    // Number of queues
    num_queues = params.find<int>("num_queues", 9);

    // Queue with highest priority
    highest_priority = params.find<int>("highest_priority", 8);

    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":DualQDWRRSchedulerV2[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    // Weigths config
    std::string dwrr_config_path =
        params.find<std::string>("dwrr_config_path", "qs_1_0/config/scheduler/dwrr_config_dualq.json");

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

    out->verbose(CALL_INFO, MODERATE, 0, "-- DualQ SchedulerDWRR V2 --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of queues: %d\n", num_queues);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Highest priority: %d\n", highest_priority);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Weight config: %s\n", weights_string.c_str());

    base_tc = registerTimeBase("1ps", true);

    send_egress   = false;
    current_queue = 0;

    for ( int k = 0; k < num_queues; k++ ) {
        activity[k] = INACTIVE;
        credits[k]  = 0;
    }

    for ( int i = 0; i < num_queues; i++ ) {
        queues_not_empty[i] = false;
    }

    all_queues_empty = true;

    // Flag that indicates the queue being served
    serving_l_queue = false;

    // Configure classic links
    port_fifo = configureLink(
        "port_fifo", base_tc,
        new Event::Handler<DualQDWRRSchedulerV2>(this, &DualQDWRRSchedulerV2::handle_update_fifos));
    out->verbose(CALL_INFO, MODERATE, 0, "port_fifo link was configured\n");

    egress_port = configureLink(
        "egress_port", base_tc,
        new Event::Handler<DualQDWRRSchedulerV2>(this, &DualQDWRRSchedulerV2::handle_egress_port_ready));
    out->verbose(CALL_INFO, MODERATE, 0, "egress_port link was configured\n");

    bytes_tracker = configureLink("bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "bytes_tracker link was configured\n");

    // Check it is not null
    assert(port_fifo);
    assert(egress_port);
    assert(bytes_tracker);

    // L FIFO update link
    l_bytes_tracker = configureLink("l_bytes_tracker", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "l_bytes_tracker link was configured\n");
    assert(l_bytes_tracker);
    // L FIFO update link
    l_port_fifo = configureLink(
        "l_port_fifo", base_tc,
        new Event::Handler<DualQDWRRSchedulerV2>(this, &DualQDWRRSchedulerV2::handle_update_l_fifo));
    out->verbose(CALL_INFO, MODERATE, 0, "l_port_fifo link was configured\n");
    assert(l_port_fifo);
    // Configure queue links
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue = "priority_queue_" + std::to_string(i);
        if ( i == (num_queues - 1) ) { pri_queue = "l_priority_queue"; }
        out->verbose(CALL_INFO, MODERATE, 0, "Linking queue %d with port name %s\n", i, pri_queue.c_str());
        queues[i] = configureLink(pri_queue, base_tc);
        assert(queues[i]);
    }

    // Configure queues_size_front links
    Event::Handler<DualQDWRRSchedulerV2>* queues_size_front_handler =
        new Event::Handler<DualQDWRRSchedulerV2>(this, &DualQDWRRSchedulerV2::handle_receive_pkt_size);
    for ( int i = 0; i < num_queues; i++ ) {
        std::string pri_queue_front_size = "front_size_priority_queue_" + std::to_string(i);
        if ( i == (num_queues - 1) ) {
            pri_queue_front_size = "l_front_size_priority_queue";
            queues_size_front_handler =
                new Event::Handler<DualQDWRRSchedulerV2>(this, &DualQDWRRSchedulerV2::handle_l_receive_pkt_size);
        }
        out->verbose(
            CALL_INFO, MODERATE, 0, "Linking front size queue %d with port name %s\n", i, pri_queue_front_size.c_str());
        front_size[i] = configureLink(pri_queue_front_size, base_tc, queues_size_front_handler);
        assert(front_size[i]);
    }

    // Packet in links
    l_packet_in = configureLink(
        "l_packet_in", base_tc,
        new Event::Handler<DualQDWRRSchedulerV2>(this, &DualQDWRRSchedulerV2::handle_receive_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "l_packet_in link was configured\n");

    packet_in = configureLink(
        "packet_in", base_tc,
        new Event::Handler<DualQDWRRSchedulerV2>(this, &DualQDWRRSchedulerV2::handle_receive_pkt));
    out->verbose(CALL_INFO, MODERATE, 0, "packet_in link was configured\n");

    assert(l_packet_in);
    assert(packet_in);

    // Queue serviced stat
    queue_serviced  = registerStatistic<uint32_t>("queue_serviced", "1");
    source_serviced = registerStatistic<uint32_t>("source_serviced", "1");

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

void
DualQDWRRSchedulerV2::handle_update_fifos(SST::Event* ev)
{
    QueueUpdateEvent* queues_status = dynamic_cast<QueueUpdateEvent*>(ev);

    if ( queues_status ) {
        safe_lock.lock();
        for ( int i = 0; i < num_queues - 1; i++ ) {
            queues_not_empty[i] = queues_status->queues_not_empty[i];
            if ( queues_not_empty[i] && activity[i] == INACTIVE && credits[i] >= 0 ) {
                request_pkt_size(i);
                // prevents a pkt that has already been requested from being
                // re-requested
                activity[i] = WAIT;
            }
        }

        out->verbose(
            CALL_INFO, DEBUG, 0,
            "New update in priority queues: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d "
            "6:%d 7:%d\n",
            queues_not_empty[0], queues_not_empty[1], queues_not_empty[2], queues_not_empty[3], queues_not_empty[4],
            queues_not_empty[5], queues_not_empty[6], queues_not_empty[7]);

        safe_lock.unlock();
        // Release memory
        delete queues_status;
    }
}

void
DualQDWRRSchedulerV2::handle_update_l_fifo(SST::Event* ev)
{
    QueueUpdateEvent* queues_status = dynamic_cast<QueueUpdateEvent*>(ev);

    if ( queues_status ) {
        safe_lock.lock();
        int l_index               = num_queues - 1;
        queues_not_empty[l_index] = queues_status->queues_not_empty[0];
        if ( queues_not_empty[l_index] && activity[l_index] == INACTIVE && credits[l_index] >= 0 ) {
            request_pkt_size(l_index);
            activity[l_index] = WAIT;
        }

        out->verbose(
            CALL_INFO, DEBUG, 0, "New update from L4S queue to priority queues: Is it not empty? %d\n",
            queues_not_empty[l_index]);

        safe_lock.unlock();
        // Release memory
        delete queues_status->queues_not_empty;
        delete queues_status;
    }
}

void
DualQDWRRSchedulerV2::handle_l_receive_pkt_size(SST::Event* ev)
{
    front_size_lock.lock();
    if ( ev != NULL ) {
        PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
        if ( pkt ) {
            int l_index       = num_queues - 1;
            // updated activity and credits
            activity[l_index] = ACTIVE;
            credits[l_index] += -1 * pkt->pkt_size;
            // send Null to egress_port if flag all_queues_empty is true
            if ( all_queues_empty ) {
                egress_port->send(NULL);
                all_queues_empty = false;
            }
            out->verbose(
                CALL_INFO, INFO, 0,
                "New front packet size event: activity = %d, credits = %d, "
                "pkt_size = %d\n",
                activity[l_index], credits[l_index], pkt->pkt_size);
        }
        else {
            out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
        }
    }
    front_size_lock.unlock();
}

/**
 * @brief Overriding just add the mutex lock
 *
 * @param ev Event with the front size
 */
void
DualQDWRRSchedulerV2::handle_receive_pkt_size(SST::Event* ev)
{
    front_size_lock.lock();
    SchedulerDWRR::handle_receive_pkt_size(ev);
    front_size_lock.unlock();
}

void
DualQDWRRSchedulerV2::request_pkt(int queue_pri)
{
    // Set serving_l_queue when we are about to request a packet for that queue
    serving_l_queue = queue_pri == (num_queues - 1);
    // Set the current queue being serviced
    current_queue   = queue_pri;
    // Call parent's method
    SchedulerDWRR::request_pkt(queue_pri);
}

void
DualQDWRRSchedulerV2::handle_receive_pkt(SST::Event* ev)
{
    // If event is NULL, the packet to send was dropped
    if ( ev == NULL ) {
        // We must request a update from the FIFOs and request a new packet
        safe_lock.lock();
        // We set the all_queues_empty, so when the next update arrives we send a
        // new packet
        all_queues_empty        = true;
        // Packet requested from current_queue is dropped, then move it to INACTIVE
        activity[current_queue] = INACTIVE;
        safe_lock.unlock();
        // Request updates from the port FIFOs
        out->verbose(CALL_INFO, MODERATE, 0, "Requesting updates from the port FIFOs\n");
        l_port_fifo->send(NULL);
        port_fifo->send(NULL);
    }
    else {
        // Send to egress port
        PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
        if ( pkt ) {
            out->verbose(
                CALL_INFO, INFO, 0,
                "New packet to schedule: id = %u, size = %dB, destination = "
                "%d, priority = %d l_queue = %d\n",
                pkt->pkt_id, pkt->pkt_size, pkt->dest_port, pkt->priority, serving_l_queue);
            source_serviced->addData(pkt->src_port);

            egress_port->send(pkt);
            if ( serving_l_queue ) {
                activity[num_queues - 1] = INACTIVE;
                // Send event to L4S bytes tracker
                l_bytes_tracker->send(pkt->clone());
                l_port_fifo->send(NULL);
            }
            else {
                activity[pkt->priority] = INACTIVE;
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

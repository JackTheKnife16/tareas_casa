#include <sst/core/sst_config.h>

#include "port_group.h"

#include "sst/core/event.h"

#include "fds_event.h"

#include <assert.h>
#include <fstream>
#include <functional> // std::bind
#include <json.hpp>

using JSON = nlohmann::json;
using namespace SST;

/**
 * @brief Construct a new PortGroup::PortGroup object
 *
 * @param id
 * @param params
 */
PortGroup::PortGroup(ComponentId_t id, Params& params) : Component(id)
{
    // Clock frequency
    UnitAlgebra frequency       = params.find<UnitAlgebra>("frequency", "1500 MHz");
    // Port group index
    pg_index                    = params.find<uint32_t>("pg_index", 0);
    // Number of ports
    num_ports                   = params.find<uint32_t>("num_ports", 16);
    // Ethernet config file
    std::string ethernet_config = params.find<std::string>("ethernet_config", "osb_1_0/config/1x400G.json");
    // Bus width
    bus_width                   = params.find<uint32_t>("bus_width", 128);
    // Verbosity
    const int         verbose   = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":PortGroup[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Port Group --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Port Group index: %d\n", pg_index);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Frequency: %s\n", frequency.toStringBestSI().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Number of ports: %d\n", num_ports);
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Ethernet config: %s\n", ethernet_config.c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Bus width: %d\n", bus_width);

    base_tc = registerTimeBase("1ps", true);

    // Compute the frequency delay
    UnitAlgebra interval = (UnitAlgebra("1") / frequency) / UnitAlgebra("1ps");
    freq_delay           = interval.getRoundedValue();
    out->verbose(CALL_INFO, MODERATE, 0, "Frequency delay is: %lu ps\n", freq_delay);

    // Scheduler routine starts disabled
    scheduler_running   = false;
    // Init scheduler attributes
    queue_idx           = -1;
    chunk_sent_complete = true;

    // Configure in links
    // Packet bus link without handler
    packet_bus_link = configureLink("packet_bus", base_tc);
    // FDS bus link without handler
    fds_bus_link    = configureLink("fds_bus", base_tc);
    assert(packet_bus_link);
    assert(fds_bus_link);

    // open JSON file for front plane info
    JSON          eth_config_data;
    std::ifstream eth_config_file(ethernet_config);
    eth_config_file >> eth_config_data;
    // Create the Receiver Buffers depending on the Ethernet configuration
    for ( int i = 0; i < eth_config_data.size(); i++ ) {
        if ( eth_config_data[i]["enable"] ) {
            uint32_t port_number = eth_config_data[i]["port"];
            out->verbose(
                CALL_INFO, MODERATE, 0, "Connecting Ethernet Port %d to the Port Group %d\n", port_number, pg_index);

            // Create function pointer to notify method
            std::function<void(void)> notify = std::bind(&PortGroup::notify_scheduler_routine, this);


            // Connect the port link to the buffer's handler
            std::string     e_port        = "port_" + std::to_string(port_number);
            // Port is enable, then create a buffer
            ReceiverBuffer* buffer        = new ReceiverBuffer(e_port, port_number, notify, out);
            SST::Link*      ethernet_link = configureLink(
                e_port, base_tc, new Event::Handler<ReceiverBuffer>(buffer, &ReceiverBuffer::new_transaction_handler));
            assert(ethernet_link);

            // Add it to the buffes's list
            port_buffers.push_back(buffer);
        }
    }

    // Scheduler routine
    scheduler_routine_link = configureSelfLink(
        "scheduler_routine", base_tc, new Event::Handler<PortGroup>(this, &PortGroup::scheduler_routine));

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");

    // Stats config
    bytes_sent   = registerStatistic<uint32_t>("bytes_sent", "1");
    sent_packets = registerStatistic<uint32_t>("sent_packets", "1");
}

/**
 * @brief If the scheduler is disabled, it is enabled. This method is called by the port buffers, when
 * they receive a new transaction and the buffer was empty.
 *
 */
void
PortGroup::notify_scheduler_routine()
{
    safe_lock.lock();
    if ( !scheduler_running ) {
        out->verbose(CALL_INFO, DEBUG, 0, "Enabling sheduler routine in %lu ps\n", freq_delay);
        scheduler_running = true;
        scheduler_routine_link->send(freq_delay, NULL);
    }
    safe_lock.unlock();
}

/**
 * @brief It disables the scheduler routine
 *
 */
void
PortGroup::disable_sheduler_routine()
{
    out->verbose(CALL_INFO, DEBUG, 0, "Disabling sheduler routine\n");
    safe_lock.lock();
    scheduler_running = false;
    safe_lock.unlock();
}

/**
 * @brief Checks if there is at least one port buffer with transactions.
 *
 * @return true When there are transaction in the port buffers
 * @return false When there are not transactions in the ports buffers
 */
bool
PortGroup::packet_data_in_buffers()
{
    bool found_packet_data = false;
    for ( int i = 0; i < port_buffers.size(); i++ ) {
        if ( !port_buffers[i]->is_empty() ) {
            found_packet_data = true;
            break;
        }
    }

    return found_packet_data;
}


/**
 * @brief Handler for input events
 *
 * @param ev Event
 */
void
PortGroup::scheduler_routine(SST::Event* ev)
{
    if ( packet_data_in_buffers() ) {
        out->verbose(CALL_INFO, DEBUG, 0, "There are packet words, so scheduler routine is running\n");
        if ( chunk_sent_complete ) {
            bool search_queue = true;
            do {
                queue_idx++;
                if ( queue_idx >= port_buffers.size() ) { queue_idx = 0; }

                if ( !port_buffers[queue_idx]->is_empty() ) { search_queue = false; }
            } while ( search_queue );

            out->verbose(
                CALL_INFO, DEBUG, 0, "Selected queue index %d which correspond to port %d\n", queue_idx,
                port_buffers[queue_idx]->get_index());
        }

        SST::Event* trans = port_buffers[queue_idx]->pop();
        if ( trans ) {
            PacketWordEvent* pkt_word = dynamic_cast<PacketWordEvent*>(trans);

            out->verbose(
                CALL_INFO, MODERATE, 0,
                "Sending word through output packet bus: pkt_id=%u src_port=%d pri=%d sop=%d eop=%d first=%d last=%d "
                "seq=%d vbc=%d\n",
                pkt_word->pkt_id, pkt_word->src_port, pkt_word->priority, pkt_word->start_of_packet,
                pkt_word->end_of_packet, pkt_word->first_chunk_word, pkt_word->last_chunk_word,
                pkt_word->word_sequence_number, pkt_word->valid_byte_count);

            // It is is the first word, send the FDS
            if ( pkt_word->start_of_packet && pkt_word->first_chunk_word ) {
                FDSEvent* fds = new FDSEvent();
                fds->pkt_id   = pkt_word->pkt_id;
                fds->pkt_size = pkt_word->pkt_size;
                fds->src_node = pkt_word->src_node;
                fds->src_port = pkt_word->src_port;
                // Priority is assigned at the packet classifier, but it is the same as the packet
                fds->priority = 0;

                out->verbose(
                    CALL_INFO, MODERATE, 0, "Sending the FDS through output fds bus: pkt_id=%u\n", fds->pkt_id);
                fds_bus_link->send(fds);
            }
            packet_bus_link->send(pkt_word);

            // Stat collection
            uint32_t bytes_to_sent = pkt_word->valid_byte_count;
            // Sent packets stat is updated when a eop is seen
            if ( pkt_word->end_of_packet && pkt_word->last_chunk_word ) { sent_packets->addData(1); }
            bytes_sent->addData(bytes_to_sent);

            chunk_sent_complete = pkt_word->last_chunk_word;

            scheduler_routine_link->send(freq_delay, NULL);
        }
        else {
            disable_sheduler_routine();
        }
    }
    else {
        disable_sheduler_routine();
    }
}

/**
 * @brief Finish stage of SST
 *
 */
void
PortGroup::finish()
{}
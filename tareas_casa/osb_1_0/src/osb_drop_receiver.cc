#include <sst/core/sst_config.h>

#include "osb_drop_receiver.h"

#include "sst/core/event.h"

#include "packet_event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new DropReceiver::DropReceiver object
 *
 * @param id
 * @param params
 */
DropReceiver::DropReceiver(ComponentId_t id, Params& params) : Component(id)
{
    // Verbosity
    num_port_ingress_slices   = params.find<uint32_t>("num_port_ingress_slices", 0);
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":DropReceiver[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, 1, 0, "-- Drop Receiver --\n");
    out->verbose(CALL_INFO, 1, 0, "|--> Number of Port Ingress Slices: %d\n", num_port_ingress_slices);

    dropped_pkts = 0;

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    for ( int i = 0; i < num_port_ingress_slices; i++ ) {
        std::string link_name = "acceptance_checker_" + std::to_string(i);
        SST::Link*  ac_link =
            configureLink(link_name, base_tc, new Event::Handler<DropReceiver>(this, &DropReceiver::handle_new_drop));
        acceptance_checker_links.push_back(ac_link);
        assert(ac_link);
    }

    finisher = configureLink("finisher", base_tc);
    assert(finisher);

    // Create statistics
    dropped_packets = registerStatistic<uint32_t>("dropped_packets", "1");
    source          = registerStatistic<uint32_t>("source", "1");
    priority        = registerStatistic<uint32_t>("priority", "1");

    out->verbose(CALL_INFO, 1, 0, "Links configured\n");
}


/**
 * @brief Handler for packet discards
 *
 * @param ev Event with dropped packet
 */
void
DropReceiver::handle_new_drop(SST::Event* ev)
{
    PacketEvent* pkt = dynamic_cast<PacketEvent*>(ev);
    if ( pkt ) {
        safe_lock.lock();

        // Count pkts
        dropped_pkts++;

        // Add statistics
        dropped_packets->addData(1);
        source->addData(pkt->src_port);
        priority->addData(pkt->priority);

        finisher->send(pkt);

        safe_lock.unlock();
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
DropReceiver::finish()
{
    out->verbose(CALL_INFO, 0, 0, "%d packets were dropped during the simulation\n", dropped_pkts);
}
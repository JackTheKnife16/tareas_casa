#include <sst/core/sst_config.h>

#include "dualq_ecn_classifier.h"

#include "sst/core/event.h"

#include "packet_util_event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new ECNClassifier::ECNClassifier object
 *
 * @param id
 * @param params
 */
ECNClassifier::ECNClassifier(ComponentId_t id, Params& params) : Component(id)
{
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "@t:" << getName() << ":ECNClassifier[@p:@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- ECN Classifier --\n");

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    in = configureLink("in", base_tc, new Event::Handler<ECNClassifier>(this, &ECNClassifier::handle_new_packet));
    out->verbose(CALL_INFO, MODERATE, 0, "in link was configured\n");

    l_check = configureLink("l_check", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "l_check link was configured\n");

    c_check = configureLink("c_check", base_tc);
    out->verbose(CALL_INFO, MODERATE, 0, "c_check link was configured\n");

    // Check it is not null
    assert(in);
    assert(l_check);
    assert(c_check);

    out->verbose(CALL_INFO, MODERATE, 0, "All links were configured\n");
}

/**
 * @brief Handler for new packets from acceptance checker
 *
 * @param ev Event with packet utilization
 */
void
ECNClassifier::handle_new_packet(SST::Event* ev)
{
    DualQPacketUtilEvent* pkt = dynamic_cast<DualQPacketUtilEvent*>(ev);
    if ( pkt ) {
        out->verbose(CALL_INFO, INFO, 0, "New packet with ECN 0x%X...\n", pkt->ip_ecn);
        if ( pkt->ip_ecn % 2 == 1 ) { send_to_l_queue(pkt); }
        else {
            send_to_c_queue(pkt);
        }
    }
    else {
        out->fatal(CALL_INFO, -1, "Error! Bad Event Type!\n");
    }
}

/**
 * @brief Forward the packet to the L AQM
 *
 * @param packet packet util event to forward
 */
void
ECNClassifier::send_to_l_queue(DualQPacketUtilEvent* packet)
{
    out->verbose(CALL_INFO, INFO, 0, "Sending pkt through l_check link, pkt_id = %u\n", packet->pkt_id);
    l_check->send(packet);
}

/**
 * @brief Forward the packet to the C AQM
 *
 * @param packet packet util event to forward
 */
void
ECNClassifier::send_to_c_queue(DualQPacketUtilEvent* packet)
{
    out->verbose(CALL_INFO, INFO, 0, "Sending pkt through c_check link, pkt_id = %u\n", packet->pkt_id);
    c_check->send(packet);
}

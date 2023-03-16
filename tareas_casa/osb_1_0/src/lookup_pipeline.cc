#include <sst/core/sst_config.h>

#include "lookup_pipeline.h"

#include "sst/core/event.h"

#include "model_global.h"

#include <assert.h>
#include <math.h>

using namespace SST;

/**
 * @brief Construct a new LookupPipeline::LookupPipeline object
 *
 * @param id
 * @param params
 */
LookupPipeline::LookupPipeline(ComponentId_t id, Params& params) : Component(id)
{
    // Clock frequency
    UnitAlgebra frequency = params.find<UnitAlgebra>("frequency", "1500 MHz");
    // Number of cycles
    num_cycles            = params.find<uint32_t>("num_cycles", 3);
    // Verbosity
    const int verbose     = params.find<int>("verbose", 0);

    // set the sender activated false
    sender_activated = false;

    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":LookupPipeline[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    out->verbose(CALL_INFO, MODERATE, 0, "-- Lookup Pipeline --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Frequency: %s\n", frequency.toStringBestSI().c_str());
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Num Cycles: %d\n", num_cycles);

    base_tc = registerTimeBase("1ps", true);

    // Compute the frequency delay
    UnitAlgebra interval = (UnitAlgebra("1") / frequency) / UnitAlgebra("1ps");
    freq_delay           = interval.getRoundedValue();
    out->verbose(CALL_INFO, MODERATE, 0, "Frequency delay is: %lu ps\n", freq_delay);


    // Configure in links
    // in fds bus link with handler
    in_fds_bus_link =
        configureLink("in_fds_bus", base_tc, new Event::Handler<LookupPipeline>(this, &LookupPipeline::fds_receiver));

    // out fds bus link without handler
    out_fds_bus_link = configureLink("out_fds_bus", base_tc);

    // sender link with handler
    sender_link = configureSelfLink(
        "sender_link", base_tc, new Event::Handler<LookupPipeline>(this, &LookupPipeline::sender_routine));

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");

    // Check if links are connected
    assert(in_fds_bus_link);
    // TODO: check this when everything is connected
    // assert(out_fds_bus_link);
}


/**
 * @brief Function that calculates the delay in sending a packet and
 * sends a NULL event through the sender_link.
 */
void
LookupPipeline::delay_routine()
{
    send_delay = fds_time_queue.front()->t_fds - getCurrentSimCycle();
    sender_link->send(send_delay, NULL);
}

/**
 * @brief This function sends the fds at the front of the fds_time_queue
 * through the out_fds_bus_link, then removes it from the queue.
 *
 * @param ev Event that always will be null
 */
void
LookupPipeline::sender_routine(Event* ev)
{
    FDSEvent* fds = fds_time_queue.front()->fds;
    // TODO: Uncomment this link
    out_fds_bus_link->send(fds);
    out->verbose(
        CALL_INFO, INFO, 0, "Sending the FDS to Final Slice: pkt_id=%u src_port=%d pri=%d\n", fds->pkt_id,
        fds->src_port, fds->priority);
    fds_time_queue.pop();

    if ( !fds_time_queue.empty() ) { delay_routine(); }
    else {
        sender_activated = false;
    }
}

/**
 * @brief Handler for incoming fds
 *
 * @param ev Packet Word Event
 */
void
LookupPipeline::fds_receiver(SST::Event* ev)
{
    // Cast the event
    FDSEvent* fds = dynamic_cast<FDSEvent*>(ev);

    if ( fds ) {
        out->verbose(
            CALL_INFO, INFO, 0, "Received a new FDS, pkt_id=%u src_port=%d pri=%d\n", fds->pkt_id, fds->src_port,
            fds->priority);
        FDSTime* fds_s = new FDSTime();
        fds_s->fds     = fds;
        fds_s->t_fds   = getCurrentSimCycle() + num_cycles * freq_delay;

        fds_time_queue.push(fds_s);

        if ( !sender_activated ) {
            sender_activated = true;
            delay_routine();
        }
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
LookupPipeline::finish()
{
    // TODO: delete this comment when adds final slice component
    out->verbose(CALL_INFO, INFO, 0, "simulation is finish, size queue = %ld\n", fds_time_queue.size());
}

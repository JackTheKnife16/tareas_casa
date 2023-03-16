#include <sst/core/sst_config.h>

#include "packet_delay_line.h"



#include "sst/core/event.h"

#include <assert.h>

using namespace SST;

/**
 * @brief Construct a new PacketDelayLine::PacketDelayLine object
 *
 * @param id
 * @param params
 */
PacketDelayLine::PacketDelayLine(ComponentId_t id, Params& params) : Component(id)
{
    // Index number
    idx                      = params.find<uint32_t>("idx", 0);
    // Verbosity
    const int         verbose = params.find<int>("verbose", 0);
    std::stringstream prefix;
    prefix << "[@t ps]:" << getName() << ":PacketDelayLine[@l]: ";
    out = new Output(prefix.str(), verbose, 0, SST::Output::STDOUT);

    empty_queue_flag = true;

    out->verbose(CALL_INFO, MODERATE, 0, "-- Packet Delay Line --\n");
    out->verbose(CALL_INFO, MODERATE, 0, "|--> Index: %d\n", idx);

    base_tc = registerTimeBase("1ps", true);

    // Configure in links
    // Input link with handler
    input_link1 = configureLink(
        "input1", base_tc, new Event::Handler<PacketDelayLine>(this, &PacketDelayLine::input_handler1));
    input_link2 = configureLink(
        "input2", base_tc, new Event::Handler<PacketDelayLine>(this, &PacketDelayLine::input_handler2));

    // Output link with no handler
    output_link1 = configureLink("output1", base_tc);
    output_link2 = configureLink("output2", base_tc);

    // Check if links are connected
    //assert(input_link1);
    //assert(input_link2);
    //assert(output_link1);
    //assert(output_link2);

    out->verbose(CALL_INFO, MODERATE, 0, "Links configured\n");
}

/**
 * @brief Handler for input events
 *
 * @param ev Event
 */
void
PacketDelayLine::input_handler1(SST::Event* ev)
{
    // TODO AGREGAR CONDICION NULL Y TAL
    PacketWordEvent* pkt = dynamic_cast<PacketWordEvent*>(ev);
    out->verbose(CALL_INFO, INFO, 0, "Receives a new packet, id = %u, sop = %d\n", pkt->pkt_id, pkt->start_of_packet);
    queue_pkt.push(pkt);

    // running when queue_pkt is empty and arrives a new pkt
    if(empty_queue_flag) {
        send_status();
        empty_queue_flag = false;
    }
    // TODO BORRAR CUANDO SE CONECTE EL FINAL SLICE
    send_pkt();
    send_status();
}

/**
 * @brief Handler for input events
 *
 * @param ev Event
 */
void
PacketDelayLine::input_handler2(SST::Event* ev)
{
    send_pkt();
    send_status();
}

void 
PacketDelayLine::send_pkt() {
    PacketWordEvent* pkt_word = queue_pkt.front();
    //TODO REVISAR ESTE MENSAJE
    out->verbose(
        CALL_INFO, MODERATE, 0,
        "Sending new word through output_link1: pkt_id=%u src_port=%d pri=%d sop=%d eop=%d first=%d last=%d "
        " seq=%d vbc=%d\n",
        pkt_word->pkt_id, pkt_word->src_port, pkt_word->priority, pkt_word->start_of_packet, pkt_word->end_of_packet,
        pkt_word->first_chunk_word, pkt_word->last_chunk_word, pkt_word->word_sequence_number,
        pkt_word->valid_byte_count);

    output_link1->send(pkt_word);
    queue_pkt.pop();
}

void 
PacketDelayLine::send_status() {
    // TODO: PREGUNTAR A DENNIS SOBRE LOS START PACKETS QUE ENVIA EL DATA_PACKER
    // SI ES CORRECTO QUE LOS DOS PRIMEROS DE 256 BYTES EN ESTOS CASOS SON SOP
    // EN CASO DE NO SERLO ENTONCES CAMBIAR status->head_sop = pkt_word->word_sequence_number
    // POR status->head_sop = pkt_word->start_of_packet Y QUITAR EL CONDICIONAL 
    // OTRA OPCION POR LA QUE PREGUNTAR ES SI HACEMOS UN EVENTO EXCLUSIVO PARA ESTE
    // COMPONENTE EN EL CUAL SE COMPONGA DE UN INDICADOR UNICO PARA EL ESTADO(EMPTY, SOP, NSOP)
    // EN EL CASO ACTUAL TENDRIAMOS QUE:
    // status->empty = true + status->head_sop = false equivale a EMPTY
    // status->empty = false + status->head_sop = true equivale a SOP
    // status->empty = false + status->head_sop = false equivale a NSOP
    FIFOStatusEvent* status = new FIFOStatusEvent();
    std::string status_str = "EMPTY";
    if(!queue_pkt.empty()){
        PacketWordEvent* pkt_word = queue_pkt.front();
        status->empty = false;
        if(pkt_word->word_sequence_number == 0) {
            status->head_sop = true;
            status_str = "SOP";
        }
        else {
            status->head_sop = false;
            status_str = "NSOP";
        }
    }
    else{
        status->empty = true;
        status->head_sop = false;
        empty_queue_flag = true;
    }

    out->verbose(CALL_INFO, INFO, 0, "Sending a new status = %s\n", status_str.c_str());
    // TODO uncomment when connect the component to final slice 
    //output_link2->send(status);
}

/**
 * @brief Finish stage of SST
 *
 */
void
PacketDelayLine::finish()
{}
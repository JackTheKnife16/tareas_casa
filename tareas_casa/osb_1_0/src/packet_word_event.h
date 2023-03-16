#ifndef PACKET_WORD_EVENT_T
#define PACKET_WORD_EVENT_T

#include "queue_element.h"

/**
 * @brief Represents a Packet Word in the OSB model
 *
 */
class PacketWordEvent : public QueueElement
{

public:
    PacketWordEvent()
    {
        pkt_id               = 0;
        pkt_size             = 0;
        src_node             = 0;
        src_port             = 0;
        dest_node            = 0;
        dest_port            = 0;
        priority             = 0;
        ip_ecn               = 0;
        is_tcp               = false;
        // Init word attributes
        start_of_packet      = false;
        end_of_packet        = false;
        first_chunk_word     = false;
        last_chunk_word      = false;
        word_sequence_number = 0;
        valid_byte_count     = 0;
        fe_priority          = 0;
    }
    /**
     * @brief Construct a new Packet Word Event object
     *
     * @param pkt original packet to copy
     */
    PacketWordEvent(PacketEvent* pkt)
    {
        pkt_id               = pkt->pkt_id;
        pkt_size             = pkt->pkt_size;
        src_node             = pkt->src_node;
        src_port             = pkt->src_port;
        dest_node            = pkt->dest_node;
        dest_port            = pkt->dest_port;
        priority             = pkt->priority;
        ip_ecn               = pkt->ip_ecn;
        is_tcp               = pkt->is_tcp;
        // Init word attributes
        start_of_packet      = false;
        end_of_packet        = false;
        first_chunk_word     = false;
        last_chunk_word      = false;
        word_sequence_number = 0;
        valid_byte_count     = 0;
        fe_priority          = 0;
    }

    bool     start_of_packet;
    bool     end_of_packet;
    bool     first_chunk_word;
    bool     last_chunk_word;
    uint32_t word_sequence_number;
    uint32_t valid_byte_count;
    uint32_t fe_priority;

    PacketEvent* clone() override
    {
        PacketWordEvent* n_packet = new PacketWordEvent();

        n_packet->pkt_size             = pkt_size;
        n_packet->src_node             = src_node;
        n_packet->src_port             = src_port;
        n_packet->dest_node            = dest_node;
        n_packet->dest_port            = dest_port;
        n_packet->priority             = priority;
        n_packet->ip_ecn               = ip_ecn;
        n_packet->is_tcp               = is_tcp;
        // Word attributes
        n_packet->start_of_packet      = start_of_packet;
        n_packet->end_of_packet        = end_of_packet;
        n_packet->first_chunk_word     = first_chunk_word;
        n_packet->last_chunk_word      = last_chunk_word;
        n_packet->word_sequence_number = word_sequence_number;
        n_packet->valid_byte_count     = valid_byte_count;
        n_packet->fe_priority          = fe_priority;

        return n_packet;
    }

    virtual uint32_t get_src_port() { return src_port; };

    virtual uint32_t get_priority() { return priority; };

    virtual uint32_t get_sop() { return start_of_packet; };

    virtual uint32_t get_eop() { return end_of_packet; };

    virtual uint32_t get_first() { return first_chunk_word; };

    virtual uint32_t get_last() { return last_chunk_word; };

    virtual uint32_t get_seq_num() { return word_sequence_number; };

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& pkt_size;
        ser& src_node;
        ser& src_port;
        ser& dest_node;
        ser& dest_port;
        ser& priority;
        ser& ip_ecn;
        ser& is_tcp;
        // Word attributes
        ser& start_of_packet;
        ser& end_of_packet;
        ser& first_chunk_word;
        ser& last_chunk_word;
        ser& word_sequence_number;
        ser& valid_byte_count;
        ser& fe_priority;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(PacketWordEvent);
};

#endif
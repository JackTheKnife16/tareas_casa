#ifndef FDS_EVENT_T
#define FDS_EVENT_T

#include "queue_element.h"

/**
 * @brief Represents an Forwarding Data Structure in the OSB model
 *
 */
class FDSEvent : public QueueElement
{

public:
    FDSEvent* clone() override
    {
        FDSEvent* n_packet = new FDSEvent();

        n_packet->pkt_id   = pkt_id;
        n_packet->pkt_size = pkt_size;
        n_packet->src_node = src_node;
        n_packet->src_port = src_port;
        n_packet->priority = priority;

        return n_packet;
    }

    virtual uint32_t get_src_port() { return src_port; };

    virtual uint32_t get_priority() { return priority; };

    virtual uint32_t get_sop() { return 1; };

    virtual uint32_t get_eop() { return 1; };

    virtual uint32_t get_first() { return 1; };

    virtual uint32_t get_last() { return 1; };

    virtual uint32_t get_seq_num() { return 0; };

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& pkt_id;
        ser& pkt_size;
        ser& src_node;
        ser& src_port;
        ser& priority;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(FDSEvent);
};

#endif
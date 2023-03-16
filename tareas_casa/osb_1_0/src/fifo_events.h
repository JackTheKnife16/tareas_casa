#ifndef FIFO_EVENTS_T
#define FIFO_EVENTS_T

#include <sst/core/event.h>

class FIFOStatusEvent : public SST::Event
{

public:
    uint32_t port_idx;
    uint32_t pri_idx;
    uint32_t pg_idx;
    uint32_t space_available;
    bool     empty;
    bool     head_sop;
    bool     full_packet_window;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& port_idx;
        ser& pri_idx;
        ser& pg_idx;
        ser& space_available;
        ser& empty;
        ser& head_sop;
        ser& full_packet_window;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(FIFOStatusEvent);
};


class FIFORequestEvent : public SST::Event
{

public:
    uint32_t port;
    uint32_t priority;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& port;
        ser& priority;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(FIFORequestEvent);
};


#endif
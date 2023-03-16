#ifndef ARBITER_EVENTS_T
#define ARBITER_EVENTS_T

#include <sst/core/event.h>

/**
 * @brief Arbiter Request is an event to indicate that a FIFO has enough data to send downstream.
 *
 */
class ArbiterRequestEvent : public SST::Event
{

public:
    bool     initial_chunk;
    uint32_t port_idx;
    uint32_t pri_idx;
    uint32_t pg_idx;
    uint32_t buffer_slice_idx;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& initial_chunk;
        ser& port_idx;
        ser& pri_idx;
        ser& pg_idx;
        ser& buffer_slice_idx;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(ArbiterRequestEvent);
};

/**
 * @brief Arbiter Grant is an event sent by the Central Arbiter to let a FIFO send packet data.
 *
 */
class ArbiterGrantEvent : public SST::Event
{

public:
    bool     initial_chunk;
    uint32_t port_idx;
    uint32_t pri_idx;
    uint32_t buffer_slice_idx;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& initial_chunk;
        ser& port_idx;
        ser& pri_idx;
        ser& buffer_slice_idx;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(ArbiterGrantEvent);
};


#endif
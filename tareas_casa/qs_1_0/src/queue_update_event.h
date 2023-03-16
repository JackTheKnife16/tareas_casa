#ifndef QUEUE_UPDATE_T
#define QUEUE_UPDATE_T

#include <sst/core/event.h>

class QueueUpdateEvent : public SST::Event
{

public:
    bool* queues_not_empty;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& queues_not_empty;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(QueueUpdateEvent);
};

#endif
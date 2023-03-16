#ifndef COUPLING_EVENT_T
#define COUPLING_EVENT_T

#include <sst/core/event.h>

class CouplingEvent : public SST::Event
{

public:
    int lq_index;

    float coupling_probability;

private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& lq_index;
        ser& coupling_probability;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(CouplingEvent);
};

#endif
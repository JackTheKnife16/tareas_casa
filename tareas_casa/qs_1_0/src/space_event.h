#ifndef SPACE_EVENT_T
#define SPACE_EVENT_T

#include <sst/core/event.h>

class SpaceEvent : public SST::Event
{

public:
    unsigned int block_avail;

    // void execute() override {
    //     std::cout << "Space Event" << std::endl;
    //     Event::execute();
    // }
private:
    /**
     * @brief Serialize the attributes to send them through the link
     *
     * @param ser
     */
    void serialize_order(SST::Core::Serialization::serializer& ser) override
    {
        Event::serialize_order(ser);
        ser& block_avail;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(SpaceEvent);
};

#endif
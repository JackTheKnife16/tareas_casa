#ifndef LOOKUP_EVENT_T
#define LOOKUP_EVENT_T

#include <sst/core/event.h>
class LookUpResponseEvent : public SST::Event
{

public:
    unsigned int result;

    // void execute() override {
    //     std::cout << "Lookup Response Event" << std::endl;
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
        ser& result;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(LookUpResponseEvent);
};

class LookUpRequestEvent : public SST::Event
{

public:
    unsigned int lookup_address;

    int bias;

    int          type;
    unsigned int index;

    // void execute() override {
    //     std::cout << "Lookup Request Event" << type << " - " << index <<
    //     std::endl; Event::execute();
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
        ser& lookup_address;
        ser& bias;
        ser& type;
        ser& index;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(LookUpRequestEvent);
};

#endif
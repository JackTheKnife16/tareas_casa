#ifndef UTIL_EVENT_T
#define UTIL_EVENT_T

#include <sst/core/event.h>

class UtilEvent : public SST::Event
{

public:
    unsigned int utilization;

    int element_index;

    UtilEvent* clone() override
    {
        UtilEvent* n_util = new UtilEvent();

        n_util->utilization   = utilization;
        n_util->element_index = element_index;

        return n_util;
    }

    // void execute() override {
    //     std::cout << "Util Event" << std::endl;
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
        ser& utilization;
        ser& element_index;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(UtilEvent);
};

#endif
#ifndef BYTES_USE_EVENT_T
#define BYTES_USE_EVENT_T

#include <sst/core/event.h>

class BytesUseEvent : public SST::Event
{

public:
    unsigned int bytes_in_use;

    int element_index;

    BytesUseEvent* clone() override
    {
        BytesUseEvent* n_bytes = new BytesUseEvent();

        n_bytes->bytes_in_use  = bytes_in_use;
        n_bytes->element_index = element_index;

        return n_bytes;
    }

    // void execute() override {
    //     std::cout << "Bytes in Use" << std::endl;
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
        ser& bytes_in_use;
        ser& element_index;
    }

    /**
     * @brief Construct a new Implement Serializable object
     *
     */
    ImplementSerializable(BytesUseEvent);
};

#endif
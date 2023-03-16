#ifndef QUEUE_ELEMENT_T
#define QUEUE_ELEMENT_T

#include "packet_event.h"

#include <cstdint>

/**
 * @brief Interface for any transaction that any FIFO in the model can receive.
 *
 */
class QueueElement : public PacketEvent
{
public:
    virtual uint32_t get_src_port() = 0;
    virtual uint32_t get_priority() = 0;
    virtual uint32_t get_sop()      = 0;
    virtual uint32_t get_eop()      = 0;
    virtual uint32_t get_first()    = 0;
    virtual uint32_t get_last()     = 0;
    virtual uint32_t get_seq_num()  = 0;
};

#endif
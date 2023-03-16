#ifndef LOGICAL_FIFO_T
#define LOGICAL_FIFO_T

#include "sst/core/event.h"

#include "fifo_events.h"
#include "model_global.h"
#include "queue_element.h"

#include <mutex> // std::mutex
#include <sst/core/output.h>
#include <vector> // std::queue

/**
 * @brief Element that receives events and store them. It can notify a routine to start.
 *
 */
class LogicalFIFO
{
public:
    LogicalFIFO(
        uint32_t _port, uint32_t _priority, uint32_t _pg_index, uint32_t _init_limit, uint32_t _packet_window_size,
        uint32_t _pop_size, bool _overflow, SST::Output* _out);
    ~LogicalFIFO() {};

    void          push_transaction(QueueElement* trans);
    QueueElement* pop();

    bool             get_head_sop();
    bool             get_full_packet_window();
    bool             get_pop_ready();
    uint32_t         get_space_avail();
    uint32_t         get_port();
    uint32_t         get_priority();
    uint32_t         get_pg_index();
    uint32_t         get_limit();
    FIFOStatusEvent* get_update();
    bool             is_empty();

private:
    std::mutex   safe_lock;
    SST::Output* out;

    uint32_t                   port;
    uint32_t                   priority;
    uint32_t                   pg_index;
    std::vector<QueueElement*> fifo;
    uint32_t                   usage;
    uint32_t                   initial_limit;
    uint32_t                   limit;
    uint32_t                   space_avail;
    uint32_t                   packet_window_size;
    uint32_t                   pop_size;
    bool                       overflow;

    bool head_sop;
    bool full_packet_window;
    bool pop_ready;

    void check_head();
};

#endif
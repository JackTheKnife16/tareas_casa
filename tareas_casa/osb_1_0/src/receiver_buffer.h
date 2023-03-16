#ifndef RECEIVER_BUFFET_T
#define RECEIVER_BUFFET_T

#include "sst/core/event.h"

#include "model_global.h"

#include <functional> // std::function
#include <mutex>      // std::mutex
#include <queue>      // std::queue
#include <sst/core/output.h>

/**
 * @brief Element that receives events and store them. It can notify a routine to start.
 *
 */
class ReceiverBuffer
{
public:
    ReceiverBuffer(std::string _name, uint32_t _index, SST::Output* _out);
    ReceiverBuffer(std::string _name, uint32_t _index, std::function<void(void)> _notify, SST::Output* _out);
    ~ReceiverBuffer() {};

    void        new_transaction_handler(SST::Event* ev);
    bool        is_empty();
    uint32_t    get_index();
    SST::Event* pop();

private:
    std::mutex                safe_lock;
    SST::Output*              out;
    std::string               receiver_name;
    uint32_t                  index;
    std::queue<SST::Event*>   transaction_queue;
    std::function<void(void)> notify_function = NULL;
};

#endif
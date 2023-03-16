#ifndef PORT_ACCUMULATOR_T
#define PORT_ACCUMULATOR_T

#include "sst/core/event.h"

#include "model_global.h"
#include "packet_word_event.h"

#include <functional> // std::function
#include <mutex>      // std::mutex
#include <queue>      // std::queue
#include <sst/core/output.h>

/**
 * @brief Element that receives events and store them. It can notify a routine to start.
 *
 */
class PortAccumulator
{
public:
    PortAccumulator(
        uint32_t _port_number, uint32_t _out_bus_width, uint32_t _fds_size, uint32_t _init_chunk,
        uint32_t _non_init_chunk, std::function<void(void)> _notify, SST::Output* _out);
    ~PortAccumulator() {};

    void             add_new_word(PacketWordEvent* pkt_word);
    bool             word_ready();
    PacketWordEvent* pop();

private:
    std::mutex   safe_lock;
    SST::Output* out;

    uint32_t port_number;
    uint32_t current_pkt_id;
    uint32_t current_src_port;
    uint32_t current_pkt_size;
    uint32_t current_priority;

    bool     pkt_build_in_progress;
    uint32_t byte_count;
    uint32_t out_bus_width;
    uint32_t fds_size;
    uint32_t chunk_count;
    uint32_t word_count;
    uint32_t seq_num;
    uint32_t init_chunk;
    uint32_t non_init_chunk;

    std::queue<PacketWordEvent*> words_queue;
    std::function<void(void)>    notify_function;

    void create_new_word(bool is_eop);
};

#endif
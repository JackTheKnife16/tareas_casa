#ifndef RECEIVER_BUFFET_T
#define RECEIVER_BUFFET_T

#include "sst/core/event.h"

#include "arbitration_result.h"
#include "model_global.h"

#include <functional> // std::function
#include <map>        // std::map
#include <mutex>      // std::mutex
#include <sst/core/output.h>

/**
 * @brief Non-Initial Chunk Arbitration element which manages the NIC request from the buffer slices.
 *
 */
class NICArbitration
{
public:
    NICArbitration(
        uint32_t _buffer_slices, uint32_t _ports_per_buffer, uint32_t _priorities, uint32_t _buffer_slice_idx,
        std::function<void(void)> _notify, SST::Output* _out);

    ~NICArbitration() {};

    // Methods
    void                 nic_request_receiver(SST::Event* ev);
    arbitration_result_t non_initial_chunk_arbitration();
    bool                 nic_reqs_available();
    void                 clean_nic_req(uint32_t port);
    bool                 get_conflict();
    void                 set_conflict(bool value);

private:
    std::mutex   safe_lock;
    SST::Output* out;

    // Attributes
    uint32_t                  buffer_slices;
    uint32_t                  ports_per_buffer;
    uint32_t                  priorities;
    uint32_t                  buffer_slice_idx;
    int                       chosen_port;
    bool                      conflict;
    std::map<uint32_t, bool>  nic_requests;
    std::function<void(void)> notify_function = NULL;
};

#endif
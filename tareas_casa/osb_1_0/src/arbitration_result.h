#ifndef PQ_INDEX_T
#define PQ_INDEX_T

/**
 * @brief Represents an arbitration result from the Central Arbiter
 *
 */
struct arbitration_result_t
{
    bool         found;
    unsigned int port;
    unsigned int priority;
    unsigned int buffer_slice_index;
};

#endif
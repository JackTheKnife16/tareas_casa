#ifndef PQ_INDEX_T
#define PQ_INDEX_T

/**
 * @brief Physical queue index struct
 *
 */
struct pq_index_s
{
    unsigned short int port;
    unsigned short int priority;
};

/**
 * @brief Physical queue index union
 *
 */
union pq_index_t {
    pq_index_s   index_s;
    unsigned int index_i;
};

#endif
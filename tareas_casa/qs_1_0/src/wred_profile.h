#ifndef WRED_PROFILE_T
#define WRED_PROFILE_T

/**
 * @brief WRED profile struct
 *
 */
struct wred_profile_t
{
    float starting_point;
    float slope;
};

/**
 * @brief Explicit Congestion Notification WRED profile struct
 *
 */
struct ecn_wred_profile_t
{
    uint32_t       ecn_drop_point;
    wred_profile_t wred_profile;
};

#endif
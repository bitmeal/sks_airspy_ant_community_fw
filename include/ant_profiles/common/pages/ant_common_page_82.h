/**
 * Copyright (c) 2015 - 2021, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef ANT_COMMON_PAGE_82_H__
#define ANT_COMMON_PAGE_82_H__

/** @file
 *
 * @defgroup ant_sdk_common_pages ANT+ common pages
 * @{
 * @ingroup ant_sdk_profiles
 * @brief This module implements functions for the ANT+ common pages.
 * @details  ANT+ common data pages define common data formats that can be used by any device on any ANT network. The ability to send and receive these common pages is defined by the transmission type of the ANT channel parameter.
 *
 * Note that all unused pages in this section are not defined and therefore cannot be used.
 * @}
 *
 * @defgroup ant_common_page_82 ANT+ common page 82
 * @{
 * @ingroup ant_sdk_common_pages
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANT_COMMON_PAGE_82 (82) ///< @brief ID value of common page 82.

typedef enum {
    ANT_COMMON_page82_BATTERY_STATE_RESERVED_0 = 0,
    ANT_COMMON_page82_BATTERY_STATE_NEW = 1,
    ANT_COMMON_page82_BATTERY_STATE_GOOD = 2,
    ANT_COMMON_page82_BATTERY_STATE_OK = 3,
    ANT_COMMON_page82_BATTERY_STATE_LOW = 4,
    ANT_COMMON_page82_BATTERY_STATE_CRITICAL = 5,
    ANT_COMMON_page82_BATTERY_STATE_RESERVED_6 = 6,
    ANT_COMMON_page82_BATTERY_STATE_INVALID = 7
} ANT_COMMON_page82_BATTERY_STATE;

const static char* ant_common_page_82_battery_status_strings[] = {
    "RESERVED_0",
    "NEW",
    "GOOD",
    "OK",
    "LOW",
    "CRITICAL",
    "RESERVED_6",
    "INVALID"
};
/**@brief Data structure for ANT+ common data page 82.
 *
 * @note This structure implements only page 82 specific data.
 */
typedef struct
{
    uint8_t                         battery_count;         ///< Number of batteries. 4 bits
    uint8_t                         battery_id;            ///< Battery identifier. 4 bits
    uint32_t                        operating_time;        ///< Cumulative Operating Time in seconds [s].
    uint16_t                        battery_voltage_mv;    ///< Battery voltage in milli Volts [mV].
    ANT_COMMON_page82_BATTERY_STATE battery_status;        ///< Descriptive battery status: NEW, GOOD, OK, LOW, CRITICAL

} ant_common_page82_data_t;

/**@brief Initialize page 82.
 */
#define DEFAULT_ANT_COMMON_page82()                                 \
    (ant_common_page82_data_t)                                      \
    {                                                               \
        .battery_count = 0xF,                                       \
        .battery_id = 0xF,                                          \
        .operating_time = 0,                                        \
        .battery_voltage_mv = 0,                                    \
        .battery_status = ANT_COMMON_page82_BATTERY_STATE_INVALID   \
    }

/**@brief Initialize page 82.
 */
#define ANT_COMMON_page82(bat_cnt, bat_id, op_time, bat_vlt, bat_sta)   \
    (ant_common_page82_data_t)                                          \
    {                                                                   \
        .battery_count = (bat_cnt),                                     \
        .battery_id = (bat_id),                                         \
        .operating_time = (op_time),                                    \
        .battery_voltage_mv = (bat_vlt),                                \
        .battery_status = (bat_sta)                                     \
    }

/**@brief Function for encoding page 82.
 *
 * @param[in]  p_page_data      Pointer to the page data.
 * @param[out] p_page_buffer    Pointer to the data buffer.
 */
void ant_common_page_82_encode(uint8_t * p_page_buffer,
                               volatile ant_common_page82_data_t const * p_page_data);

/**@brief Function for decoding page 82.
 *
 * @param[in]  p_page_buffer    Pointer to the data buffer.
 * @param[out] p_page_data      Pointer to the page data.
 */
void ant_common_page_82_decode(uint8_t const * p_page_buffer,
                               volatile ant_common_page82_data_t * p_page_data);


#ifdef __cplusplus
}
#endif

#endif // ANT_COMMON_PAGE_82_H__
/** @} */

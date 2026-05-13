/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * 
 * Copyright (c) 2015 - 2021, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef ANT_TPMS_PAGE_16_H__
#define ANT_TPMS_PAGE_16_H__

/** @file
 *
 * @defgroup ant_sdk_profiles_tpms_page16 Tire Pressure profile page 16
 * @{
 * @ingroup ant_sdk_profiles_tpms_pages
 */

#include <stdint.h>
#include <ant_profiles/tpms/ant_tpms_common_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Using reference:
 * ISO 2533:1975 International Standard Atmosphere
 * at 0m above Mean Sea Level
 * p ^= 1013.25 hPa (rounded to full hPa)
 */
#define ANT_TPMS_AMBIENT_DEFAULT 1013


/** @brief TPMS Configuration command flags
 */
typedef enum
{
    ANT_TPMS_CONFIG_EMPTY_RESET     = 0x0, ///< Reset configuration; TODO(bitmeal): check, this is guesswork
    ANT_TPMS_CONFIG_SET_ROLE        = 0x1, ///< Set sensor role (front/rear)
    ANT_TPMS_CONFIG_SET_AMBIENT     = 0x2, ///< Set environmente pressure compensation
    ANT_TPMS_CONFIG_SET_ALARM_LOW   = 0x4, ///< Set lower alarm limit; TODO(bitmeal): check, this is guesswork
    ANT_TPMS_CONFIG_SET_ALARM_HIGH  = 0x8, ///< Set upper alarm limit; TODO(bitmeal): check, this is guesswork
} ant_tpms_config_command_t;

/** @brief Data structure for Tire Pressure data page 16.
 */
typedef struct
{
    uint8_t command;                ///< ant_tpms_config_command_t bitfield of flags with configuration commands
    ant_tpms_role_t role;           ///< Sensor role (front/rear)
    uint16_t ambient_pressure;      ///< Pressure type; in 0.1 bar or hPa.
    uint16_t alarm_low_pressure;    ///< Pressure type; in 0.1 bar or hPa.
    uint16_t alarm_high_pressure;   ///< Pressure type; in 0.1 bar or hPa.
} ant_tpms_page16_data_t;

/** @brief Initialize page 16.
 */
#define DEFAULT_ANT_TPMS_page16()                               \
    (ant_tpms_page16_data_t)                                    \
    {                                                           \
        .command = ANT_TPMS_CONFIG_EMPTY_RESET,                 \
        .role = ANT_TPMS_ROLE_NONE,                             \
        .ambient_pressure = ANT_TPMS_AMBIENT_DEFAULT,           \
        .alarm_low_pressure = 0xffff,                           \
        .alarm_high_pressure = 0xffff,                          \
    }

/** @brief Function for encoding page 16.
 *
 * @param[in]  p_page_data      Pointer to the page data.
 * @param[out] p_page_buffer    Pointer to the data buffer.
 */
void ant_tpms_page_16_encode(uint8_t                     * p_page_buffer,
                            ant_tpms_page16_data_t const * p_page_data);

/** @brief Function for decoding page 16.
 *
 * @param[in]  p_page_buffer    Pointer to the data buffer.
 * @param[out] p_page_data      Pointer to the page data.
 */
void ant_tpms_page_16_decode(uint8_t const         * p_page_buffer,
                            ant_tpms_page16_data_t * p_page_data);



#ifdef __cplusplus
}
#endif

#endif // ANT_TPMS_PAGE_16_H__
/** @} */

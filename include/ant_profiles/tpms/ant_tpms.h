/*
 * Copyright (c) 2024 by Garmin Ltd. or its subsidiaries.
 * All rights reserved.
 *
 * Use of this Software is limited and subject to the License Agreement for ANT SoftDevice
 * and Associated Software. The Agreement accompanies the Software in the root directory of
 * the repository.
 *
 * Copyright (c) 2015 - 2021, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef ANT_BICYCLE_POWER_H__
#define ANT_BICYCLE_POWER_H__

#include <stdint.h>
#include <stdbool.h>
#include <ant_parameters.h>
#include <ant_channel_config.h>
#include <ant_profiles/tpms/pages/ant_tpms_pages.h>
#include <ant_host_init.h>

#define TPMS_DEVICE_TYPE            0x30u               ///< Device type reserved for ANT+ Tire Pressure.
#define TPMS_ANTPLUS_RF_FREQ        0x39u               ///< Frequency, decimal 57 (2457 MHz).
#define TPMS_MSG_PERIOD             8192u               ///< Message period, decimal 8192 (4 Hz).

#define TPMS_EXT_ASSIGN             0x00                ///< ANT ext assign (see Ext. Assign Channel Parameters in ant_parameters.h: @ref ant_parameters).
#define TPMS_DISP_CHANNEL_TYPE      CHANNEL_TYPE_SLAVE  ///< Display Tire Pressure channel type.
#define TPMS_SENS_CHANNEL_TYPE      CHANNEL_TYPE_MASTER ///< Sensor Tire Pressure channel type.

/**@brief Helper macro with two level expansion for concatenation of two parameters.*/
#define _ANT_TPMS_CONCAT_2(p1, p2)     _ANT_TPMS_CONCAT_2_(p1, p2)
#define _ANT_TPMS_CONCAT_2_(p1, p2)    p1##p2

/**@brief Initialize an ANT channel configuration structure for the Tire Pressure profile (Display).
 *
 * @param[in]  NAME                 Name of related instance.
 * @param[in]  CHANNEL_NUMBER       Number of the channel assigned to the profile instance.
 * @param[in]  TRANSMISSION_TYPE    Type of transmission assigned to the profile instance.
 * @param[in]  DEVICE_NUMBER        Number of the device assigned to the profile instance.
 * @param[in]  NETWORK_NUMBER       Number of the network assigned to the profile instance.
 */
#define TPMS_DISP_CHANNEL_CONFIG_DEF(NAME,                                      \
                                     CHANNEL_NUMBER,                            \
                                     TRANSMISSION_TYPE,                         \
                                     DEVICE_NUMBER,                             \
                                     NETWORK_NUMBER)                            \
static const ant_channel_config_t   _ANT_TPMS_CONCAT_2(NAME, _channel_tpms_disp_config) = \
    {                                                                           \
        .channel_number    = (CHANNEL_NUMBER),                                  \
        .channel_type      = TPMS_DISP_CHANNEL_TYPE,                            \
        .ext_assign        = TPMS_EXT_ASSIGN,                                   \
        .rf_freq           = TPMS_ANTPLUS_RF_FREQ,                              \
        .transmission_type = (TRANSMISSION_TYPE),                               \
        .device_type       = TPMS_DEVICE_TYPE,                                  \
        .device_number     = (DEVICE_NUMBER),                                   \
        .channel_period    = TPMS_MSG_PERIOD,                                   \
        .network_number    = (NETWORK_NUMBER),                                  \
    }
#define TPMS_DISP_CHANNEL_CONFIG(NAME) &_ANT_TPMS_CONCAT_2(NAME, _channel_tpms_disp_config)

/**@brief Initialize an ANT channel configuration structure for the Tire Pressure profile (Sensor).
 *
 * @param[in]  NAME                 Name of related instance.
 * @param[in]  CHANNEL_NUMBER       Number of the channel assigned to the profile instance.
 * @param[in]  TRANSMISSION_TYPE    Type of transmission assigned to the profile instance.
 * @param[in]  DEVICE_NUMBER        Number of the device assigned to the profile instance.
 * @param[in]  NETWORK_NUMBER       Number of the network assigned to the profile instance.
 */
#define TPMS_SENS_CHANNEL_CONFIG_DEF(NAME,                                      \
                                     CHANNEL_NUMBER,                            \
                                     TRANSMISSION_TYPE,                         \
                                     DEVICE_NUMBER,                             \
                                     NETWORK_NUMBER)                            \
static const ant_channel_config_t   _ANT_TPMS_CONCAT_2(NAME, _channel_tpms_sens_config) = \
    {                                                                           \
        .channel_number    = (CHANNEL_NUMBER),                                  \
        .channel_type      = TPMS_SENS_CHANNEL_TYPE,                            \
        .ext_assign        = TPMS_EXT_ASSIGN,                                   \
        .rf_freq           = TPMS_ANTPLUS_RF_FREQ,                              \
        .transmission_type = (TRANSMISSION_TYPE),                               \
        .device_type       = TPMS_DEVICE_TYPE,                                  \
        .device_number     = (DEVICE_NUMBER),                                   \
        .channel_period    = TPMS_MSG_PERIOD,                                   \
        .network_number    = (NETWORK_NUMBER),                                  \
    }
#define TPMS_SENS_CHANNEL_CONFIG(NAME) &_ANT_TPMS_CONCAT_2(NAME, _channel_tpms_sens_config)

/**@brief Initialize an ANT profile configuration structure for the TPMS profile (Display).
 *
 * @param[in]  NAME                 Name of related instance.
 * @param[in]  EVT_HANDLER          Event handler to be called for handling events in the TPMS profile.
 */
#define TPMS_DISP_PROFILE_CONFIG_DEF(NAME,                                          \
                                     EVT_HANDLER)                                   \
static ant_tpms_disp_cb_t            _ANT_TPMS_CONCAT_2(NAME, _tpms_disp_cb);                 \
static const ant_tpms_disp_config_t  _ANT_TPMS_CONCAT_2(NAME, _profile_tpms_disp_config) =    \
    {                                                                               \
        .p_cb               = &_ANT_TPMS_CONCAT_2(NAME, _tpms_disp_cb),                       \
        .evt_handler        = (EVT_HANDLER),                                        \
    }
#define TPMS_DISP_PROFILE_CONFIG(NAME) &_ANT_TPMS_CONCAT_2(NAME, _profile_tpms_disp_config)


/**@brief Initialize an ANT profile configuration structure for the TPMS profile (Sensor).
 *
 * @param[in]  NAME                 Name of related instance.
 * @param[in]  EVT_HANDLER          Event handler to be called for handling events in the TPMS profile.
 */
#define TPMS_SENS_PROFILE_CONFIG_DEF(NAME,                                          \
                                     EVT_HANDLER)                                   \
static ant_tpms_sens_cb_t            _ANT_TPMS_CONCAT_2(NAME, _tpms_sens_cb);                 \
static const ant_tpms_sens_config_t  _ANT_TPMS_CONCAT_2(NAME, _profile_tpms_sens_config) =    \
    {                                                                               \
        .p_cb               = &_ANT_TPMS_CONCAT_2(NAME, _tpms_sens_cb),                       \
        .evt_handler        = (EVT_HANDLER),                                        \
    }
#define TPMS_SENS_PROFILE_CONFIG(NAME) &NAME##_profile_tpms_sens_config


/**@brief Tire Pressure page number type. */
typedef enum
{
    ANT_TPMS_PAGE_1  = 1,  ///< Tire pressure main data page.
    ANT_TPMS_PAGE_80 = ANT_COMMON_PAGE_80,
    ANT_TPMS_PAGE_81 = ANT_COMMON_PAGE_81,
    ANT_TPMS_PAGE_82 = ANT_COMMON_PAGE_82
} ant_tpms_page_t;

/**@brief TPMS profile event type. */
typedef enum
{
    ANT_TPMS_PAGE_1_UPDATED  = ANT_TPMS_PAGE_1,  ///< Data page 1 and speed have been updated (Display) or sent (Sensor).
    ANT_TPMS_PAGE_80_UPDATED = ANT_TPMS_PAGE_80, ///< Data page 80 has been updated (Display) or sent (Sensor).
    ANT_TPMS_PAGE_81_UPDATED = ANT_TPMS_PAGE_81, ///< Data page 81 has been updated (Display) or sent (Sensor).
    ANT_TPMS_PAGE_82_UPDATED = ANT_TPMS_PAGE_82 ///< Data page 82 has been updated (Display) or sent (Sensor).
} ant_tpms_evt_t;

// Forward declaration of the ant_tpms_profile_t type.
typedef struct ant_tpms_profile_s ant_tpms_profile_t;

/**@brief TPMS event handler type. */
typedef void (* ant_tpms_evt_handler_t) (ant_tpms_profile_t *, ant_tpms_evt_t);


#include "ant_profiles/tpms/ant_tpms_local.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Tire Pressure Sensor configuration structure. */
typedef struct
{
    ant_tpms_sens_cb_t     * p_cb;          ///< Pointer to the data buffer for internal use.
    ant_tpms_evt_handler_t   evt_handler;   ///< Event handler to be called for handling events in the TPMS profile.
} ant_tpms_sens_config_t;

/**@brief Tire Pressure Display configuration structure. */
typedef struct
{
    ant_tpms_disp_cb_t   * p_cb;            ///< Pointer to the data buffer for internal use.
    ant_tpms_evt_handler_t evt_handler;     ///< Event handler to be called for handling events in the TPMS profile.
} ant_tpms_disp_config_t;

/**@brief Tire Pressure profile structure. */
struct ant_tpms_profile_s
{
    uint8_t                  channel_number; ///< Channel number assigned to the profile.
    union {
        ant_tpms_disp_cb_t * p_disp_cb;
        ant_tpms_sens_cb_t * p_sens_cb;
    } _cb;                                ///< Pointer to internal control block.
    ant_tpms_evt_handler_t   evt_handler;    ///< Event handler to be called for handling events in the TPMS profile.
    ant_tpms_page1_data_t    page_1;         ///< Page 1.
    ant_common_page80_data_t page_80;        ///< Page 80.
    ant_common_page81_data_t page_81;        ///< Page 81.
    ant_common_page82_data_t page_82;        ///< Page 82.
};

/** @name Defines for accessing ant_tpms_profile_t member variables
   @{ */
#define TPMS_PROFILE_pressure               page_1.pressure

#define TPMS_PROFILE_manuf_id               page_80.manuf_id
#define TPMS_PROFILE_hw_revision            page_80.hw_revision
#define TPMS_PROFILE_manufacturer_id        page_80.manufacturer_id
#define TPMS_PROFILE_model_number           page_80.model_number

#define TPMS_PROFILE_sw_revision_minor      page_81.sw_revision_minor
#define TPMS_PROFILE_sw_revision_major      page_81.sw_revision_major
#define TPMS_PROFILE_serial_number          page_81.serial_number

#define TPMS_PROFILE_battery_count          page_82.battery_count
#define TPMS_PROFILE_battery_id             page_82.battery_id
#define TPMS_PROFILE_operating_time         page_82.operating_time
#define TPMS_PROFILE_battery_voltage_mv     page_82.battery_voltage_mv
#define TPMS_PROFILE_battery_status         page_82.battery_status


/** @} */

/**@brief Function for initializing the ANT Tire Pressure Display profile instance.
 *
 * @param[in]  p_profile        Pointer to the profile instance.
 * @param[in]  p_channel_config Pointer to the ANT channel configuration structure.
 * @param[in]  p_disp_config    Pointer to the Tire Pressure Display configuration structure.
 *
 * @retval     0 If initialization was successful. Otherwise, an error code is returned.
 */
int ant_tpms_disp_init(ant_tpms_profile_t           * p_profile,
                              ant_channel_config_t const   * p_channel_config,
                              ant_tpms_disp_config_t const * p_disp_config);

/**@brief Function for initializing the ANT Tire Pressure Sensor profile instance.
 *
 * @param[in]  p_profile        Pointer to the profile instance.
 * @param[in]  p_channel_config Pointer to the ANT channel configuration structure.
 * @param[in]  p_sens_config    Pointer to the Tire Pressure Sensor configuration structure.
 *
 * @retval     0 If initialization was successful. Otherwise, an error code is returned.
 */
int ant_tpms_sens_init(ant_tpms_profile_t           * p_profile,
                              ant_channel_config_t const   * p_channel_config,
                              ant_tpms_sens_config_t const * p_sens_config);

/**@brief Function for opening the profile instance channel for ANT TPMS Display.
 *
 * Before calling this function, pages should be configured.
 *
 * @param[in]  p_profile        Pointer to the profile instance.
 *
 * @retval     0 If the channel was successfully opened. Otherwise, an error code is returned.
 */
int ant_tpms_disp_open(ant_tpms_profile_t * p_profile);

/**@brief Function for opening the profile instance channel for ANT TPMS Sensor.
 *
 * Before calling this function, pages should be configured.
 *
 * @param[in]  p_profile        Pointer to the profile instance.
 *
 * @retval     0 If the channel was successfully opened. Otherwise, an error code is returned.
 */
int ant_tpms_sens_open(ant_tpms_profile_t * p_profile);


/**@brief Function for handling the Sensor ANT events.
 *
 * @details This function handles all events from the ANT stack that are of interest to the Tire Pressure Display profile.
 *
 * @param[in]   p_ant_evt     Event received from the ANT stack.
 * @param[in]   p_context       Pointer to the profile instance.
 */
void ant_tpms_sens_evt_handler(ant_evt_t * p_ant_evt, void * p_context);

/**@brief Function for handling the Display ANT events.
 *
 * @details This function handles all events from the ANT stack that are of interest to the Tire Pressure Display profile.
 *
 * @param[in]   p_ant_evt     Event received from the ANT stack.
 * @param[in]   p_context       Pointer to the profile instance.
 */
void ant_tpms_disp_evt_handler(ant_evt_t * p_ant_evt, void * p_context);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // ANT_BICYCLE_POWER_H__


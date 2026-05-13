/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * 
 * Copyright (c) 2023 by Garmin Ltd. or its subsidiaries.
 * All rights reserved.
 *
 * Use of this Software is limited and subject to the License Agreement for ANT SoftDevice
 * and Associated Software. The Agreement accompanies the Software in the root directory of
 * the repository.
 *
 * Copyright (c) 2015 - 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <string.h>
#include <ant_profiles/tpms/pages/ant_tpms_page_16.h>
#include <ant_profiles/tpms/ant_tpms_utils.h>
#include <zephyr/logging/log.h>
// LOG_MODULE_REGISTER(ant_tpms_page_16, CONFIG_TPMS_PAGES_LOG_LEVEL);
LOG_MODULE_REGISTER(ant_tpms_page_16, LOG_LEVEL_WRN);

/**@brief tire pressure page 16 data layout structure. */
typedef struct
{
    ant_tpms_role_t role :4;
    uint8_t command : 4;
    uint8_t ambient_pressure[2];
    uint8_t alarm_low_pressure[2];
    uint8_t alarm_high_pressure[2];
} ant_tpms_page16_data_layout_t;


static void page16_data_log(ant_tpms_page16_data_t const * p_page_data)
{
    LOG_INF("Command: %#x; Role: %#x; Ambient comp. [hPa]: %u; Alarm Thres. [hPa] [%u, %u]",
        p_page_data->command,
        p_page_data->role,
        p_page_data->ambient_pressure,
        p_page_data->alarm_low_pressure,
        p_page_data->alarm_high_pressure);
}


void ant_tpms_page_16_encode(uint8_t                     * p_page_buffer,
                            ant_tpms_page16_data_t const * p_page_data)
{
    ant_tpms_page16_data_layout_t * p_outcoming_data = (ant_tpms_page16_data_layout_t *)p_page_buffer;

    page16_data_log(p_page_data);

    p_outcoming_data->command = p_page_data->command;
    p_outcoming_data->role = p_page_data->role;
    uint16_encode(p_page_data->ambient_pressure, p_outcoming_data->ambient_pressure);
    uint16_encode(p_page_data->alarm_low_pressure, p_outcoming_data->alarm_low_pressure);
    uint16_encode(p_page_data->alarm_high_pressure, p_outcoming_data->alarm_high_pressure);
}


void ant_tpms_page_16_decode(uint8_t const         * p_page_buffer,
                            ant_tpms_page16_data_t * p_page_data)
{
    ant_tpms_page16_data_layout_t const * p_incoming_data =
        (ant_tpms_page16_data_layout_t *)p_page_buffer;
    
    p_page_data->command = p_incoming_data->command;
    p_page_data->role = p_incoming_data->role;
    p_page_data->ambient_pressure = uint16_decode(p_incoming_data->ambient_pressure);
    p_page_data->alarm_low_pressure = uint16_decode(p_incoming_data->alarm_low_pressure);
    p_page_data->alarm_high_pressure = uint16_decode(p_incoming_data->alarm_high_pressure);

    page16_data_log(p_page_data);
}

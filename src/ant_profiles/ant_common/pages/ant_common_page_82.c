/*
 * Copyright (c) 2023 by Garmin Ltd. or its subsidiaries.
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
#include <string.h>
#include <ant_profiles/common/pages/ant_common_page_82.h>
#include <ant_profiles/tpms/ant_tpms_utils.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ant_common_page_82, LOG_LEVEL_INF);

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
const char* get_ant_common_page_82_battery_status_string(const ANT_COMMON_page82_BATTERY_STATE state)
{
    return ant_common_page_82_battery_status_strings[(uint8_t)state];
}

/**@brief ant+ common page 82 data layout structure. */
typedef struct
{
    uint8_t _reserved[1]; ///< unused, fill by 0xFF
    uint8_t battery_identifier;
    uint8_t operating_time[3];
    uint8_t fractional_voltage;
    uint8_t descriptive_field;
}ant_common_page82_data_layout_t;


/**@brief Function for tracing page 82 data.
 *
 * @param[in]  p_page_data      Pointer to the page 82 data.
 */
static void page82_data_log(volatile ant_common_page82_data_t const * p_page_data)
{
    
    if((p_page_data->battery_count & 0xF & p_page_data->battery_id) == 0xF)
    {
        LOG_INF("Battery count: %s", "unused");
        LOG_INF("Battery ID: %s", "unused");
    }
    else
    {
        LOG_INF("Battery count: %u", p_page_data->battery_count);
        LOG_INF("Battery ID: %u", p_page_data->battery_id);
    }

    LOG_INF("Operating time: %u", p_page_data->operating_time);

    if(p_page_data->battery_status != ANT_COMMON_page82_BATTERY_STATE_INVALID)
    {
        LOG_INF("Battery Voltage [mV]: %u", p_page_data->battery_voltage_mv);
    }
    LOG_INF("Battery Status: %u (%s)", (uint8_t)(p_page_data->battery_status), get_ant_common_page_82_battery_status_string(p_page_data->battery_status));
}

// encodes with 2 second time resolution only
void ant_common_page_82_encode(uint8_t                                 * p_page_buffer,
                               volatile ant_common_page82_data_t const * p_page_data)
{
    ant_common_page82_data_layout_t * p_outcoming_data =
        (ant_common_page82_data_layout_t *)p_page_buffer;

    memset(p_page_buffer, UINT8_MAX, sizeof (p_outcoming_data->_reserved));

    p_outcoming_data->battery_identifier = (
            (p_page_data->battery_count & 0xF) |
            ((p_page_data->battery_id & 0xF) << 4)
        );

    uint24_encode(p_page_data->operating_time / 2,
                                   p_outcoming_data->operating_time);

    uint8_t fractional_voltage = 0xFF;
    uint8_t coarse_voltage = 0xF;

    if(p_page_data->battery_status != ANT_COMMON_page82_BATTERY_STATE_INVALID)
    {
        fractional_voltage = (uint8_t)((uint32_t)(p_page_data->battery_voltage_mv % 1000) * 256 / 1000);
        coarse_voltage = (uint8_t)(p_page_data->battery_voltage_mv / 1000);
    }

    p_outcoming_data->fractional_voltage = fractional_voltage;

    p_outcoming_data->descriptive_field = (
            (coarse_voltage & 0xF) |
            ((p_page_data->battery_status & 0x7) << 4) |
            (0x1 << 7) // 2 second cumulative operating time resolution
        );

    page82_data_log(p_page_data);
}


void ant_common_page_82_decode(uint8_t const                     * p_page_buffer,
                               volatile ant_common_page82_data_t * p_page_data)
{
    ant_common_page82_data_layout_t const * p_incoming_data =
        (ant_common_page82_data_layout_t *)p_page_buffer;

    p_page_data->battery_count = (p_incoming_data->battery_identifier & 0xF);
    p_page_data->battery_id = ((p_incoming_data->battery_identifier >> 4) & 0xF);

    p_page_data->operating_time = uint24_decode(p_incoming_data->operating_time) * ( ((p_incoming_data->descriptive_field >> 7) & 0x1) == 0x1 ? 2 : 16 );


    p_page_data->battery_voltage_mv = (uint16_t)(p_incoming_data->descriptive_field & 0xF) * 1000 + (uint16_t)(((uint32_t)p_incoming_data->fractional_voltage) * 1000 / 256);
    
    p_page_data->battery_status = (p_incoming_data->descriptive_field >> 4) & 0x7;

    page82_data_log(p_page_data);
}


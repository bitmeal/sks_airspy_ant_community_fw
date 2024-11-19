/*
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
#include <ant_profiles/tpms/pages/ant_tpms_page_1.h>
#include <ant_profiles/tpms/ant_tpms_utils.h>
#include <zephyr/logging/log.h>
// LOG_MODULE_REGISTER(ant_tpms_page_1, CONFIG_TPMS_PAGES_LOG_LEVEL);
LOG_MODULE_REGISTER(ant_tpms_page_1, LOG_LEVEL_DBG);

/**@brief tire pressure page 1 data layout structure. */
typedef struct
{
    uint8_t _reserved[5];
    uint8_t tpms_pressure_LSB;
    uint8_t tpms_pressure_MSB;
} ant_tpms_page1_data_layout_t;


static void page1_data_log(ant_tpms_page1_data_t const * p_page_data)
{
    LOG_INF("Pressure [hPa]: %u", p_page_data->pressure);
}


void ant_tpms_page_1_encode(uint8_t                     * p_page_buffer,
                            ant_tpms_page1_data_t const * p_page_data)
{
    ant_tpms_page1_data_layout_t * p_outcoming_data = (ant_tpms_page1_data_layout_t *)p_page_buffer;

    page1_data_log(p_page_data);

    uint16_encode(p_page_data->pressure, &p_outcoming_data->tpms_pressure_LSB);
}


void ant_tpms_page_1_decode(uint8_t const         * p_page_buffer,
                            ant_tpms_page1_data_t * p_page_data)
{
    ant_tpms_page1_data_layout_t const * p_incoming_data =
        (ant_tpms_page1_data_layout_t *)p_page_buffer;

    p_page_data->pressure = uint16_decode(&p_incoming_data->tpms_pressure_LSB);

    page1_data_log(p_page_data);
}

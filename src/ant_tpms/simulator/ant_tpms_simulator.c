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

#include <ant_profiles/tpms/simulator/ant_tpms_simulator.h>
#include <ant_profiles/tpms/ant_tpms_utils.h>

#define PRESSURE_MIN  1000
#define PRESSURE_MAX  5000
#define PRESSURE_INCR 100

#define SIMULATOR_TIME_INCREMENT TPMS_MSG_PERIOD


void ant_tpms_simulator_init(ant_tpms_simulator_t           * p_simulator,
                             ant_tpms_simulator_cfg_t const * p_config,
                             bool                             auto_change)
{
    p_simulator->p_profile      = p_config->p_profile;
    p_simulator->_cb.auto_change = auto_change;
    // p_simulator->_cb.tick_incr   = 0;

    p_simulator->_cb.pressure_sensorsim_cfg.min          = PRESSURE_MIN;
    p_simulator->_cb.pressure_sensorsim_cfg.max          = PRESSURE_MAX;
    p_simulator->_cb.pressure_sensorsim_cfg.incr         = PRESSURE_INCR;
    p_simulator->_cb.pressure_sensorsim_cfg.start_at_max = false;

    sensorsim_init(&(p_simulator->_cb.pressure_sensorsim_state),
                   &(p_simulator->_cb.pressure_sensorsim_cfg));
}


void ant_tpms_simulator_one_iteration(ant_tpms_simulator_t * p_simulator, ant_tpms_evt_t event)
{
    switch (event)
    {
        case ANT_TPMS_PAGE_1_UPDATED:

            if (p_simulator->_cb.auto_change)
            {
                sensorsim_measure(&(p_simulator->_cb.pressure_sensorsim_state),
                                                   &(p_simulator->_cb.pressure_sensorsim_cfg));
            }

            p_simulator->p_profile->TPMS_PROFILE_pressure =
                p_simulator->_cb.pressure_sensorsim_state.current_val;
            break;

        default:
            break;
    }
}


void ant_tpms_simulator_increment(ant_tpms_simulator_t * p_simulator)
{
    if (!p_simulator->_cb.auto_change)
    {
        sensorsim_increment(&(p_simulator->_cb.pressure_sensorsim_state),
                            &(p_simulator->_cb.pressure_sensorsim_cfg));
    }
}


void ant_tpms_simulator_decrement(ant_tpms_simulator_t * p_simulator)
{
    if (!p_simulator->_cb.auto_change)
    {
        sensorsim_decrement(&(p_simulator->_cb.pressure_sensorsim_state),
                            &(p_simulator->_cb.pressure_sensorsim_cfg));
    }
}



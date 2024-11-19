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

#ifndef ANT_TPMS_SIMULATOR_LOCAL_H__
#define ANT_TPMS_SIMULATOR_LOCAL_H__

#include <stdint.h>
#include <stdbool.h>
#include <ant_profiles/tpms/ant_tpms.h>
#include <sensorsim.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup ant_sdk_tpms_simulator
 * @brief TPMS simulator control block structure. */
typedef struct
{
    bool              auto_change;             ///< Pressure will change automatically (if auto_change is set) or manually.
    // uint32_t          tick_incr;               ///< Fractional part of tick increment.
    sensorsim_state_t pressure_sensorsim_state;   ///< Pressure state of the simulated sensor.
    sensorsim_cfg_t   pressure_sensorsim_cfg;     ///< Pressure configuration of the simulated sensor.
}ant_tpms_simulator_cb_t;



#ifdef __cplusplus
}
#endif

#endif // ANT_TPMS_SIMULATOR_LOCAL_H__

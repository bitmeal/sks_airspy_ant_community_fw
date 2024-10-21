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

#ifndef ANT_TPMS_SIMULATOR_H__
#define ANT_TPMS_SIMULATOR_H__

/** @file
 *
 * @defgroup ant_sdk_simulators ANT simulators
 * @ingroup ant_sdk_utils
 * @brief Modules that simulate sensors.
 *
 * @defgroup ant_sdk_tpms_simulator ANT TPMS simulator
 * @{
 * @ingroup ant_sdk_simulators
 * @brief ANT TPMS simulator module.
 *
 * @details This module simulates power for the ANT TPMS profile. The module calculates
 * abstract values, which are handled by the TPMS pages data model to ensure that they are
 * compatible. It provides a handler for changing the power value manually and functionality
 * for changing the power automatically.
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <ant_profiles/tpms/ant_tpms.h>
#include <ant_profiles/tpms/simulator/ant_tpms_simulator_local.h>
#include <sensorsim.h>

#ifdef __cplusplus
extern "C" {
#endif


/**@brief TPMS simulator configuration structure. */
typedef struct
{
    ant_tpms_profile_t * p_profile;   ///< Related profile.
} ant_tpms_simulator_cfg_t;

/**@brief TPMS simulator structure. */
typedef struct
{
    ant_tpms_profile_t      * p_profile;    ///< Related profile.
    ant_tpms_simulator_cb_t   _cb;          ///< Internal control block.
} ant_tpms_simulator_t;


/**@brief Function for initializing the ANT TPMS simulator instance.
 *
 * @param[in]  p_simulator      Pointer to the simulator instance.
 * @param[in]  p_config         Pointer to the simulator configuration structure.
 * @param[in]  auto_change      Enable or disable automatic changes of the power.
 */
void ant_tpms_simulator_init(ant_tpms_simulator_t           * p_simulator,
                             ant_tpms_simulator_cfg_t const * p_config,
                             bool                             auto_change);

/**@brief Function for simulating a device event.
 *
 * @details Based on this event, the transmitter data is simulated.
 *
 * This function should be called in the TPMS TX event handler.
 */
void ant_tpms_simulator_one_iteration(ant_tpms_simulator_t * p_simulator, ant_tpms_evt_t event);

/**@brief Function for incrementing the power value.
 *
 * @param[in]  p_simulator      Pointer to the simulator instance.
 */
void ant_tpms_simulator_increment(ant_tpms_simulator_t * p_simulator);

/**@brief Function for decrementing the power value.
 *
 * @param[in]  p_simulator      Pointer to the simulator instance.
 */
void ant_tpms_simulator_decrement(ant_tpms_simulator_t * p_simulator);


#ifdef __cplusplus
}
#endif

#endif // ANT_TPMS_SIMULATOR_H__
/** @} */

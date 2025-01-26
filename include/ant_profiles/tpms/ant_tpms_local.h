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

#ifndef ANT_TPMS_LOCAL_H__
#define ANT_TPMS_LOCAL_H__

#include <stdint.h>
#include <stdbool.h>
#include "ant_profiles/tpms/ant_tpms.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ant_tpms
 * @{
 */

/** @brief Tire Pressure Sensor control block. */
typedef struct
{
    uint8_t           message_counter;
} ant_tpms_sens_cb_t;

/**@brief Tire Pressure Sensor RX control block. */
typedef struct
{
} ant_tpms_disp_cb_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // ANT_TPMS_LOCAL_H__

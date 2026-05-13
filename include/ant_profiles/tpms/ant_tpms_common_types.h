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

#ifndef ANT_TPMS_COMMON_TYPES_H__
#define ANT_TPMS_COMMON_TYPES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ant_tpms
 * @{
 */

/** @brief TPMS Role
 */
typedef enum
{
    ANT_TPMS_ROLE_NONE  = 0x00,
    ANT_TPMS_ROLE_FRONT = 0x01, ///< Front tire sensor
    ANT_TPMS_ROLE_REAR  = 0x02, ///< Rear tire sensor
} ant_tpms_role_t;

/** @brief TPMS Alarm Flags
 */
typedef enum
{
    ANT_TPMS_ALARM_ALL      = 0x00, ///< ALL Alarms on; TODO(bitmeal): check, this is guesswork
    ANT_TPMS_ALARM_LOW_OK   = 0x01, ///< Lower limit OK, no alarm; TODO(bitmeal): check, this is guesswork
    ANT_TPMS_ALARM_HIGH_OK  = 0x02, ///< Upper limit OK, no alarm; TODO(bitmeal): check, this is guesswork
    ANT_TPMS_ALARM_NONE     = 0x03, ///< ALL OK, no alarm; TODO(bitmeal): check, this is guesswork
} ant_tpms_alarm_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // ANT_TPMS_COMMON_TYPES_H__

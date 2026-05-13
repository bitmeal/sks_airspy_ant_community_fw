/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_APP_ANT_H__
#define INCLUDE_APP_ANT_H__

#include <zephyr/zbus/zbus.h>

#define ANT_TPMS_CONFIG_ROLE_SETTINGS_KEY "role"
#define ANT_TPMS_CONFIG_ALARM_LOW_SETTINGS_KEY "alL"
#define ANT_TPMS_CONFIG_ALARM_HIGH_SETTINGS_KEY "alH"

int start_ant_device(void);

void ant_sensor_data_handler_cb(const struct zbus_channel *chan);

#endif // INCLUDE_APP_ANT_H__
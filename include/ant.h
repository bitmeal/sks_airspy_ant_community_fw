/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_APP_ANT_H__
#define INCLUDE_APP_ANT_H__

#include <zephyr/zbus/zbus.h>

int start_ant_device(void);

void ant_sensor_data_handler_cb(const struct zbus_channel *chan);

#endif // INCLUDE_APP_ANT_H__
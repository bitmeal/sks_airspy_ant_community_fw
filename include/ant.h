/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_APP_ANT_H__
#define INCLUDE_APP_ANT_H__

#include <zephyr/zbus/zbus.h>


int init_ant(void);

void ant_sensor_data_handler_cb(const struct zbus_channel *chan);
void ant_config_update_notification_handler_cb(const struct zbus_channel *chan);

#endif // INCLUDE_APP_ANT_H__
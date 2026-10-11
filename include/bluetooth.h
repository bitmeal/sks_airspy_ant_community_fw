/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_BLUETOOTH_H__
#define INCLUDE_BLUETOOTH_H__

#include <zephyr/bluetooth/uuid.h>
#include <zephyr/zbus/zbus.h>


void init_bluetooth(void);

void ble_sensor_data_handler_cb(const struct zbus_channel *chan);
void ble_config_update_notification_handler_cb(const struct zbus_channel *chan);

#endif // INCLUDE_BLUETOOTH_H__
/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include "zbus_com.h"
#include "sensor.h"

ZBUS_CHAN_DEFINE(sensor_data_chan,
		 struct sensor_readings_t,

		 NULL, // no validator
		 NULL, // no user data
		 ZBUS_OBSERVERS(ant_sensor_data_handler),
		 ZBUS_MSG_INIT( .pressure_hpa = 0, .temperature_c = 0, .voltage_mv = 0, .flags = 0, .checksum = 0)
);

#include "ant.h"
ZBUS_LISTENER_DEFINE(ant_sensor_data_handler, ant_sensor_data_handler_cb);
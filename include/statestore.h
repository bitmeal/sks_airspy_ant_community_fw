/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_STATESTORE_H__
#define INCLUDE_STATESTORE_H__

#include <stdlib.h>
#include <stdint.h>

// decoded and compensated sensor environmental readings
typedef struct {

} sensor_state_t;

// compensation values; alarm thresholds
typedef struct {

} sensor_config_t;

// boot count, uptime, voltage
typedef struct {

} device_state_t;

// ID, role
typedef struct {

} device_config_t;

typedef struct {
    sensor_state_t sensor_state;
    sensor_config_t sensor_config;
    device_state_t device_state;
    device_config_t device_config;
} state_store_t;

#endif // INCLUDE_STATESTORE_H__
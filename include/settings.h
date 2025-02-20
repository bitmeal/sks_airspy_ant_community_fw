/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_SETTINGS_H__
#define INCLUDE_SETTINGS_H__

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <zephyr/settings/settings.h>

// implement first-boot initialization of all shared and required settings!
#define DEVICE_ID_SETTINGS_KEY "id"

int start_settings_subsys();

int load_immediate_value(const char *name, void *dest, size_t len);


#endif // INCLUDE_SETTINGS_H__
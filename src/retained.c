/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include "retained.h"

#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/retention/retention.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(retained, LOG_LEVEL_INF);

const static struct device *retained_mem_part0 = DEVICE_DT_GET(DT_ALIAS(retained_part0));

struct retained_data retained;

void retained_init()
{
    if (
		!retention_is_valid(retained_mem_part0) ||
		retention_read(retained_mem_part0, 0, (uint8_t *)&retained, sizeof(retained)) < 0
	)
	{
        LOG_WRN("Retention data invalid or uninitialized. Resetting...");
		memset(&retained, 0, sizeof(retained));
    }
}

void retained_update()
{
    uint64_t now = k_uptime_seconds();
    retained.uptime_sum += (now - retained.uptime_latest);
    retained.uptime_latest = now;

    if (retention_write(retained_mem_part0, 0, (const uint8_t *)&retained, sizeof(retained)) < 0)
	{
        LOG_ERR("Failed to write retention data!");
    }
}

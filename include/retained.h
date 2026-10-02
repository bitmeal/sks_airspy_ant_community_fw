/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_RETAINED_H__
#define INCLUDE_RETAINED_H__

#include <stdbool.h>
#include <stdint.h>

struct retained_data {
	// last update of retained data
	uint64_t uptime_latest;
	// cumulative uptime up to uptime_latest
	uint64_t uptime_sum;
	uint32_t boots;
	uint32_t crc;
};


extern struct retained_data retained;

void retained_init();
void retained_update();

#endif // INCLUDE_RETAINED_H__

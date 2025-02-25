/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include "common.h"

#include <stdlib.h>
#include <hw_id.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(common, LOG_LEVEL_INF);

uint16_t get_hwid_16bit()
{
	char hw_id_buf[HW_ID_LEN];
    uint16_t hwid_16bit = 0;
	int ret = hw_id_get(hw_id_buf, ARRAY_SIZE(hw_id_buf));
	if (ret) {
		LOG_ERR("hw_id_get failed (err %d)", ret);
	}
	else
	{
        hwid_16bit = (uint16_t)strtoul(hw_id_buf + (HW_ID_LEN - 1 - 2*sizeof(uint16_t)), NULL, 16);
		LOG_DBG("hwid: %s, hwid_16bit: %d", hw_id_buf, hwid_16bit);
	}
    
    return hwid_16bit;
}

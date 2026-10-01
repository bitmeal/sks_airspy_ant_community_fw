/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_RESOURCE_MANAGER_H__
#define INCLUDE_RESOURCE_MANAGER_H__

#include <zephyr/sys/atomic.h>

extern atomic_t res_mgr_BT;
extern atomic_t res_mgr_ANT;

void res_mgr_use(atomic_t * res);
void res_mgr_free(atomic_t * res);
bool res_mgr_in_use(atomic_t * res);

#endif // INCLUDE_RESOURCE_MANAGER_H__
/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include "resource_manager.h"

// if a resource has a usage count > 1, the subsystem has to enable it
// if a resource has a usage count = 0, the subsystem is free to disable the resource
// if all resouces have a usage count equal to zero, the system may (usually: shall) be powered off

//// resource usage counters
atomic_t res_mgr_BT = ATOMIC_INIT(0);
atomic_t res_mgr_ANT = ATOMIC_INIT(0);

void res_mgr_use(atomic_t * res) {
    atomic_inc(res);
}

void res_mgr_free(atomic_t * res) {
    atomic_dec(res);
}

bool res_mgr_in_use(atomic_t * res) {
    return atomic_get(res) > 0;
}

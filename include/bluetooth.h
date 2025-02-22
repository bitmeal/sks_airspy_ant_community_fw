/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_BLUETOOTH_H__
#define INCLUDE_BLUETOOTH_H__

#include <zephyr/bluetooth/uuid.h>

// config GATT service
#define BT_CFG_SRV_UUID_STR "2079cd72-8955-487c-bfbf-0bf85b255f3c"
#define BT_CFG_SRV_UUID_ENC BT_UUID_128_ENCODE(0x2079cd72, 0x8955, 0x487c, 0xbfbf, 0x0bf85b255f3c)
#define BT_CFG_SRV_UUID BT_UUID_DECLARE_128(BT_CFG_SRV_UUID_ENC)
// config service characteristics
#define BT_CFG_SRV_DEVID_CHRX_UUID_STR "f819d540-6c73-44e5-94bb-dfeb32926c2b"
#define BT_CFG_SRV_DEVID_CHRX_UUID_ENC BT_UUID_128_ENCODE(0xf819d540, 0x6c73, 0x44e5, 0x94bb, 0xdfeb32926c2b)
#define BT_CFG_SRV_DEVID_CHRX_UUID BT_UUID_DECLARE_128(BT_CFG_SRV_DEVID_CHRX_UUID_ENC)


void start_bluetooth_services(void);

#endif // INCLUDE_BLUETOOTH_H__
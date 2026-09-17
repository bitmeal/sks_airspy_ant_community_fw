/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_SETTINGS_H__
#define INCLUDE_SETTINGS_H__


#include <stdbool.h>
#include <ant_profiles/tpms/ant_tpms_common_types.h>


#define TPMS_CONFIG_ROLE_DEFAULT                    ANT_TPMS_ROLE_NONE
#define TPMS_CONFIG_AMBIENT_COMPENSATION_DEFAULT    1013
#define TPMS_CONFIG_ALARM_DEFAULT                   0xffff
#define TPMS_CONFIG_TYPE_DEFAULT                    0x03
#define TPMS_CONFIG_PADDING_DEFAULT                 0xff

#define DEVICE_CONFIG_BT_TIMEOUT_DEFAULT            30000

#define APP_CONFIG_VERSION 1        // /ver        

typedef enum
{
    CONFIG_UPDATE_SOURCE_EMPTY  = 0x00,
    CONFIG_UPDATE_SOURCE_ANT    = 0x01,
    CONFIG_UPDATE_SOURCE_BLE    = 0x02,
} config_update_source_t;


typedef struct {
    uint8_t  role;                              // /tpms/role
    uint16_t ambient_compensation;              // /tpms/ambcomp
    uint16_t ambient_compensation_dynamic;      // NOT PERSISTENT
    bool     ambient_compensation_is_dynamic;   // NOT PERSISTENT
    uint16_t alarm_low;                         // /tpms/alm/low
    uint16_t alarm_high;                        // /tpms/alm/high
    uint8_t  type;                              // /tpms/type
    uint8_t  padding;                           // /tpms/padding
} tpms_config_t;

typedef struct {
    uint16_t id;                    // /dev/id
    uint64_t bt_timeout_ms;         // /dev/bttmo
} device_config_t;

typedef struct {
    tpms_config_t tpms;
    device_config_t device;
} app_config_t;

// TODO: read mutex
// TODO: write mutex

extern app_config_t app_config;

int start_settings_subsys();

int commit_settings(config_update_source_t source);

void log_settings();

#endif // INCLUDE_SETTINGS_H__
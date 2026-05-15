/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/kernel.h>

#include "app_version.h"

#include "ant.h"

#include <ant_parameters.h>
#include <ant_key_manager.h>

#include <ant_profiles/tpms/ant_tpms.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ant, LOG_LEVEL_INF);

#include "common.h"
#include "retained.h"
#include "settings.h"
#include "zbus_com.h"
#include "sensor.h"

static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event);
static void ant_tpms_config_handler(ant_tpms_profile_t *p_profile, ant_tpms_page16_data_t *p_page16);

TPMS_SENS_PROFILE_CONFIG_DEF(tpms, ant_tpms_config_handler, ant_tpms_evt_handler);
ant_channel_config_t tpms_channel_tpms_sens_config;
static ant_tpms_profile_t tpms;

static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event)
{
  switch (event) {
    case ANT_TPMS_PAGE_1_UPDATED:
      break;
    case ANT_TPMS_PAGE_16_UPDATED:
      break;
    case ANT_TPMS_PAGE_80_UPDATED:
      break;
    case ANT_TPMS_PAGE_81_UPDATED:
      break;
    case ANT_TPMS_PAGE_82_UPDATED:
      break;
    default:
      break;
  }
}

static void ant_evt_handler(ant_evt_t *p_ant_evt)
{
  ant_tpms_sens_evt_handler(p_ant_evt, &tpms);
}

static void ant_tpms_config_handler(ant_tpms_profile_t *p_profile, ant_tpms_page16_data_t *p_page16) {
  // configuration changes for runtime use will be stored in data pages 16 and 1 directly

  // reset
  // allows config reset by sending 10-00-ff-ff-ff-ff-ff-ff
  // cannot find the source log entry for this behavior anymore; to be tested
  if ( p_page16->command == ANT_TPMS_CONFIG_EMPTY_RESET &&
        p_page16->ambient_pressure == 0xffff &&
        p_page16->alarm_low_pressure == 0xffff &&
        p_page16->alarm_high_pressure == 0xffff )
  {
    tpms.page_16 = DEFAULT_ANT_TPMS_page16();
  }

  // configure role
  if ( p_page16->command & ANT_TPMS_CONFIG_SET_ROLE)
  {
    tpms.page_16.role = p_page16->role;

    int rc = settings_save_one(ANT_TPMS_CONFIG_ROLE_SETTINGS_KEY, &tpms.page_16.role, sizeof(tpms.page_16.role));
    if (rc)
    {
      LOG_WRN("failed saving %s; (rc %d)", ANT_TPMS_CONFIG_ROLE_SETTINGS_KEY, rc);
    }
    else
    {
      LOG_INF("saved %s", ANT_TPMS_CONFIG_ROLE_SETTINGS_KEY);
    }
  }

  // configure ambient pressure offset
  if ( p_page16->command & ANT_TPMS_CONFIG_SET_AMBIENT)
  {
    // do not store persistently
    tpms.page_16.ambient_pressure = p_page16->ambient_pressure;
  }

  // configure low pressure alarm
  if ( p_page16->command & ANT_TPMS_CONFIG_SET_ALARM_LOW)
  {
    tpms.page_16.alarm_low_pressure = p_page16->alarm_low_pressure;

    int rc = settings_save_one(ANT_TPMS_CONFIG_ALARM_LOW_SETTINGS_KEY, &tpms.page_16.alarm_low_pressure, sizeof(tpms.page_16.alarm_low_pressure));
    if (rc)
    {
      LOG_WRN("failed saving %s; (rc %d)", ANT_TPMS_CONFIG_ALARM_LOW_SETTINGS_KEY, rc);
    }
    else
    {
      LOG_INF("saved %s", ANT_TPMS_CONFIG_ALARM_LOW_SETTINGS_KEY);
    }
  }

  // configure high pressure alarm
  if ( p_page16->command & ANT_TPMS_CONFIG_SET_ALARM_HIGH)
  {
    tpms.page_16.alarm_high_pressure = p_page16->alarm_high_pressure;

    int rc = settings_save_one(ANT_TPMS_CONFIG_ALARM_HIGH_SETTINGS_KEY, &tpms.page_16.alarm_high_pressure, sizeof(tpms.page_16.alarm_high_pressure));
    if (rc)
    {
      LOG_WRN("failed saving %s; (rc %d)", ANT_TPMS_CONFIG_ALARM_HIGH_SETTINGS_KEY, rc);
    }
    else
    {
      LOG_INF("saved %s", ANT_TPMS_CONFIG_ALARM_HIGH_SETTINGS_KEY);
    }
  }

  // match page 1 to page 16
  tpms.page_1.role = tpms.page_16.role;
}

void ant_sensor_data_handler_cb(const struct zbus_channel *chan)
{
	const struct sensor_readings_t *msg = zbus_chan_const_msg(chan);

	LOG_DBG("Updating ANT+ pages with sensor data");

  // only data depending on sensor readings or time will be updated here

  // page 1: pressure & alarms
  int16_t pressure_compensated = ANT_TPMS_AMBIENT_DEFAULT -
                                  (tpms.page_16.ambient_pressure == 0xffff ?
                                    ANT_TPMS_AMBIENT_DEFAULT :
                                    tpms.page_16.ambient_pressure) +
                                  msg->pressure_hpa;
  // clamp under 75 hPa
  tpms.page_1.pressure = (uint16_t)(pressure_compensated <= 75 ? 0 : pressure_compensated);
  
  if ( tpms.page_16.alarm_low_pressure != 0xffff && tpms.page_16.alarm_high_pressure != 0xffff)
  {
    // reset (set all) alarms as baseline before check
    tpms.page_1.alarms = ANT_TPMS_ALARM_ALL;
    if (tpms.page_16.alarm_low_pressure <= tpms.page_1.pressure)
    {
      tpms.page_1.alarms |= ANT_TPMS_ALARM_LOW_OK;
    }

    if (tpms.page_1.pressure <= tpms.page_16.alarm_high_pressure)
    {
      tpms.page_1.alarms |= ANT_TPMS_ALARM_HIGH_OK;
    }
  }
  else
  {
    // ignore and do not send alarms
    tpms.page_1.alarms = ANT_TPMS_ALARM_NONE;
  }

  // page 82: battery state and uptime
  uint8_t battery_perc = battery_level_percent(msg->voltage_mv);
  ANT_COMMON_page82_BATTERY_STATE battery_state = ANT_COMMON_page82_BATTERY_STATE_INVALID;
  if(battery_perc >= 100)
  {
    battery_state = ANT_COMMON_page82_BATTERY_STATE_NEW;
  }
  else if(battery_perc >= 50) // < 100
  {
    battery_state = ANT_COMMON_page82_BATTERY_STATE_GOOD;
  }
  else if(battery_perc >= 15) // < 50
  {
    battery_state = ANT_COMMON_page82_BATTERY_STATE_OK;
  }
  else if(battery_perc >= 5) // < 15
  {
    battery_state = ANT_COMMON_page82_BATTERY_STATE_LOW;
  }
  else // < 5
  {
    battery_state = ANT_COMMON_page82_BATTERY_STATE_CRITICAL;
  }

  tpms.page_82.operating_time = retained.uptime_sum + (k_uptime_seconds() - retained.uptime_latest);
  tpms.page_82.battery_voltage_mv = msg->voltage_mv;
  tpms.page_82.battery_status = battery_state;
}

static int profile_setup(void)
{
    int rc;

    // TODO: load all config from persistent storage
    // - role
    // - alert high/low pressure
    // - runtime changes will be commited to the data pages directly!
    // TODO: set role and alert values below

  	uint16_t device_id;
    rc = load_immediate_value(DEVICE_ID_SETTINGS_KEY, &device_id, sizeof(device_id));
    if(rc)
    {
      LOG_ERR("failed reading %s to set BT name", DEVICE_ID_SETTINGS_KEY);
      return rc;
    }

  tpms_channel_tpms_sens_config = (ant_channel_config_t) {
        .channel_number    = 0,                       // hardware ANT channel 0
        .channel_type      = TPMS_SENS_CHANNEL_TYPE,
        .ext_assign        = TPMS_EXT_ASSIGN,
        .rf_freq           = TPMS_ANTPLUS_RF_FREQ,
        .transmission_type = 5,                       // transmission type: use common pages
        .device_type       = TPMS_DEVICE_TYPE,
        .device_number     = device_id,               // device number id
        .channel_period    = TPMS_MSG_PERIOD,
        .network_number    = 0,                       // network number
    };

  int err = ant_tpms_sens_init(&tpms,
    TPMS_SENS_CHANNEL_CONFIG(tpms),
    TPMS_SENS_PROFILE_CONFIG(tpms));
  if (err) {
    LOG_ERR("ant_tpms_sens_init failed: %d", err);
    return err;
  }

  ant_tpms_role_t role_init = ANT_TPMS_ROLE_NONE;
  ant_tpms_role_t role;
  load_immediate_value_init_default(ANT_TPMS_CONFIG_ROLE_SETTINGS_KEY, &role, sizeof(role),
                                      &role_init, sizeof(role_init));

  tpms.page_1.role = role;
  tpms.page_16.role = role;


  uint16_t alarm_thres_init = 0xffff;
  uint16_t alarm_thres;
  load_immediate_value_init_default(ANT_TPMS_CONFIG_ALARM_LOW_SETTINGS_KEY, (void*)&alarm_thres, sizeof(alarm_thres),
                                      (void*)&alarm_thres_init, sizeof(alarm_thres_init));

  tpms.page_16.alarm_low_pressure = alarm_thres;

  load_immediate_value_init_default(ANT_TPMS_CONFIG_ALARM_HIGH_SETTINGS_KEY, (void*)&alarm_thres, sizeof(alarm_thres),
                                      (void*)&alarm_thres_init, sizeof(alarm_thres_init));

  tpms.page_16.alarm_high_pressure = alarm_thres;

  tpms.page_81.sw_revision_minor = APP_VERSION_MINOR % 100;
  tpms.page_81.sw_revision_major = ((uint16_t) APP_VERSION_MAJOR * 1000 + APP_VERSION_MINOR) / 100;
  tpms.page_81.serial_number  = get_hwid_16bit();

  // mark as unused with 0xff
  tpms.page_82.battery_count = 0xf;
  tpms.page_82.battery_id = 0xf;

  err = ant_tpms_sens_open(&tpms);
  if (err) {
    LOG_ERR("ant_tpms_sens_open failed: %d", err);
    return err;
  }

  LOG_DBG("OK ant_tpms_sens_open");
  return err;
}

static int ant_stack_setup(void)
{
  int err = ant_init();
  if (err) {
    LOG_ERR("ant_init failed: %d", err);
    return err;
  }
  LOG_DBG("OK ant_init");
  LOG_INF("ANT Version %s", ANT_VERSION_STRING);

  err = ant_cb_register(&ant_evt_handler);
  if (err) {
    LOG_ERR("ant_cb_register failed: %d", err);
    return err;
  }

  err = ant_plus_key_set(0); // default network number
  if (err) {
    LOG_ERR("ant_plus_key_set failed: %d", err);
  }
  return err;
}

int start_ant_device(void)
{
  LOG_INF("ANT+ TPMS device starting...");

  int err = ant_stack_setup();
  if (err) {
    LOG_ERR("ANT stack setup failed (rc %d)", err);
    return err;
  }

  err = profile_setup();
  if (err) {
    LOG_ERR("ANT+ profile setup failed (rc %d)", err);
    return err;
  }

  return 0;
}

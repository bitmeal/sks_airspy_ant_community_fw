/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/kernel.h>

#include "app_version.h"

#include "ant.h"
#include <ant_interface.h>

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

static int profile_start(void);
static int profile_setup(void);

static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event);
static void ant_tpms_config_handler(ant_tpms_profile_t *p_profile, ant_tpms_page16_data_t *p_page16);
static void ant_stack_restart_work_wrapper(struct k_work *work);
K_WORK_DEFINE(restart_ant_work, ant_stack_restart_work_wrapper);

TPMS_SENS_PROFILE_CONFIG_DEF(tpms, ant_tpms_config_handler, ant_tpms_evt_handler);
TPMS_SENS_CHANNEL_CONFIG_DEF(tpms, 0, 5, 0, 0);

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
  
  if (p_ant_evt->event == EVENT_CHANNEL_CLOSED) {
    profile_start();
  }
}

//TODO: send page 16 if configuration change trigger source is not ANT stack, but BT

static void ant_tpms_config_page_updater()
{
  tpms.page_16.role = app_config.tpms.role;
  tpms.page_1.role = app_config.tpms.role;
  tpms.page_1.type = app_config.tpms.type;
  tpms.page_1._padding = app_config.tpms.padding;

  tpms.page_16.ambient_pressure = app_config.tpms.ambient_compensation_is_dynamic ?
                                  app_config.tpms.ambient_compensation_dynamic :
                                  app_config.tpms.ambient_compensation;

  tpms.page_16.alarm_low_pressure = app_config.tpms.alarm_low;
  tpms.page_16.alarm_high_pressure = app_config.tpms.alarm_high;
}

static void ant_tpms_config_handler(ant_tpms_profile_t *p_profile, ant_tpms_page16_data_t *p_page16)
{
  // configuration changes for runtime use will be stored in data pages 16 and 1 directly

  // TODO: implement sane reset logic with new settings model
  // // reset
  // // allows config reset by sending 10-00-ff-ff-ff-ff-ff-ff
  // // cannot find the source log entry for this behavior anymore; to be tested
  // if ( p_page16->command == ANT_TPMS_CONFIG_EMPTY_RESET &&
  //       p_page16->ambient_pressure == 0xffff &&
  //       p_page16->alarm_low_pressure == 0xffff &&
  //       p_page16->alarm_high_pressure == 0xffff )
  // {
  //   tpms.page_16 = DEFAULT_ANT_TPMS_page16();
  // }

  // configure role
  if ( p_page16->command & ANT_TPMS_CONFIG_SET_ROLE)
  {
    LOG_INF("updating role: %#x", p_page16->role);
    app_config.tpms.role = p_page16->role;
  }

  // configure ambient pressure offset
  if ( p_page16->command & ANT_TPMS_CONFIG_SET_AMBIENT)
  {
    // do not store persistently
    LOG_INF("updating dynamic ambient compensation: %d", p_page16->ambient_pressure);
    app_config.tpms.ambient_compensation_is_dynamic = (p_page16->ambient_pressure != 0xffff && p_page16->ambient_pressure != 0x0000);
    app_config.tpms.ambient_compensation_dynamic = p_page16->ambient_pressure;
  }

  // configure low pressure alarm
  if ( p_page16->command & ANT_TPMS_CONFIG_SET_ALARM_LOW)
  {
    LOG_INF("updating low alarm: %d", p_page16->alarm_low_pressure);
    app_config.tpms.alarm_low = p_page16->alarm_low_pressure;
  }

  // configure high pressure alarm
  if ( p_page16->command & ANT_TPMS_CONFIG_SET_ALARM_HIGH)
  {
    LOG_INF("updating high alarm: %d", p_page16->alarm_high_pressure);
    app_config.tpms.alarm_high = p_page16->alarm_high_pressure;
  }

  ant_tpms_config_page_updater();
  commit_settings(CONFIG_UPDATE_SOURCE_ANT);
}

void ant_config_update_notification_handler_cb(const struct zbus_channel *chan)
{
	const config_update_source_t *msg_source = zbus_chan_const_msg(chan);

	LOG_INF("Updating ANT+ configuration");

  ant_tpms_config_page_updater();

  if (tpms_channel_tpms_sens_config.device_number != app_config.device.id)
  {
    k_work_submit(&restart_ant_work);
  }
}

void ant_sensor_data_handler_cb(const struct zbus_channel *chan)
{
	const struct sensor_readings_t *msg = zbus_chan_const_msg(chan);

	LOG_INF("Updating ANT+ pages with sensor data");

  // only data depending on sensor readings or time will be updated here

  // page 1: pressure & alarms
  int16_t pressure_compensation = app_config.tpms.ambient_compensation_is_dynamic ?
                                  app_config.tpms.ambient_compensation_dynamic : app_config.tpms.ambient_compensation;

  int16_t pressure_compensated = ANT_TPMS_AMBIENT_DEFAULT - pressure_compensation + msg->pressure_hpa;
  // clamp under 75 hPa
  tpms.page_1.pressure = (uint16_t)(pressure_compensated <= 75 ? 0 : pressure_compensated);
  
  // reset (set all) alarms as baseline before check
  tpms.page_1.alarms = ANT_TPMS_ALARM_NONE;
  
  if ( app_config.tpms.alarm_low != TPMS_CONFIG_ALARM_DEFAULT && app_config.tpms.alarm_high != TPMS_CONFIG_ALARM_DEFAULT)
  {
    if (tpms.page_1.pressure <= app_config.tpms.alarm_low)
    {
      tpms.page_1.alarms |= ANT_TPMS_ALARM_LOW;
    }

    if (app_config.tpms.alarm_high <= tpms.page_1.pressure)
    {
      tpms.page_1.alarms |= ANT_TPMS_ALARM_HIGH;
    }
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
  int err = ant_tpms_sens_init(&tpms,
    TPMS_SENS_CHANNEL_CONFIG(tpms),
    TPMS_SENS_PROFILE_CONFIG(tpms));
  if (err) {
    LOG_ERR("ant_tpms_sens_init failed: %d", err);
    return err;
  }

  return err;
}

static int profile_start(void)
{
  LOG_INF("starting TPMS sensor");

  int err = ant_channel_id_set((TPMS_SENS_CHANNEL_CONFIG(tpms))->channel_number,
                            app_config.device.id,
                            (TPMS_SENS_CHANNEL_CONFIG(tpms))->device_type,
                            (TPMS_SENS_CHANNEL_CONFIG(tpms))->transmission_type);
  if (err) {
    LOG_ERR("failed setting ANT device number: %d", err);
    return err;
  }

  // set page 1 & 16 values
  ant_tpms_config_page_updater();

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

int ant_stack_restart(void)
{
  LOG_INF("Restarting ANT Stack");
  
  int err;
  
  err = ant_channel_close(tpms_channel_tpms_sens_config.channel_number);
  if (err) {
    LOG_ERR("failed closing ANT channel %d (rc %d)", tpms_channel_tpms_sens_config.channel_number, err);
    return err;
  }

  LOG_INF("Closed ANT channel; will restart on receiving channel close event");

  return EXIT_SUCCESS;
}


static void ant_stack_restart_work_wrapper(struct k_work *work)
{
	ant_stack_restart();
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

  err = profile_start();
  if (err) {
    LOG_ERR("starting ANT+ sensor failed (rc %d)", err);
    return err;
  }

  return 0;
}

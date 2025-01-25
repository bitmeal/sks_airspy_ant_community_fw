/**
 *
 */

#include <zephyr/kernel.h>

#include "app_version.h"

#include "ant.h"
#include "common.h"

#include <ant_parameters.h>
#include <ant_key_manager.h>

#include <ant_profiles/tpms/ant_tpms.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ant, LOG_LEVEL_INF);

#include "zbus_com.h"
#include "sensor.h"
#include "retained.h"

// typedef typeof(((struct sensor_readings_t){}).pressure_hpa) pressure_hpa_t;

static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event);

TPMS_SENS_PROFILE_CONFIG_DEF(tpms, ant_tpms_evt_handler);
ant_channel_config_t tpms_channel_tpms_sens_config;
static ant_tpms_profile_t tpms;

static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event)
{
  switch (event) {
    case ANT_TPMS_PAGE_1_UPDATED:
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

void ant_sensor_data_handler_cb(const struct zbus_channel *chan)
{
	const struct sensor_readings_t *msg = zbus_chan_const_msg(chan);

	LOG_INF("Updating ANT+ pages with sensor data");

  // page 1: pressure
  tpms.page_1.pressure = msg->pressure_hpa;

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
  // tpms.page_82.battery_status

}

static int profile_setup(void)
{
  tpms_channel_tpms_sens_config = (ant_channel_config_t) {
        .channel_number    = 0,                       // hardware ANT channel 0
        .channel_type      = TPMS_SENS_CHANNEL_TYPE,
        .ext_assign        = TPMS_EXT_ASSIGN,
        .rf_freq           = TPMS_ANTPLUS_RF_FREQ,
        .transmission_type = 5,                       // transmission type: use common pages
        .device_type       = TPMS_DEVICE_TYPE,
        .device_number     = get_hwid_16bit(),        // device number id
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

  tpms.page_81.sw_revision_minor = APP_VERSION_MINOR;
  tpms.page_81.sw_revision_major = APP_VERSION_MAJOR;
  tpms.page_81.serial_number  = get_hwid_16bit();

  // tpms.page_82.battery_count = 1;
  // tpms.page_82.battery_id = 0;

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

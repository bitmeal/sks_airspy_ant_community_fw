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

#include "com.h"
#include "decoder.h"
// typedef typeof(((struct sensor_readings_t){}).pressure_hpa) pressure_hpa_t;

static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event);

TPMS_SENS_PROFILE_CONFIG_DEF(tpms, ant_tpms_evt_handler);
ant_channel_config_t tpms_channel_tpms_sens_config;
static ant_tpms_profile_t tpms;

#define LOG_MSGQ_EMPTY_WRN_EVERY 4
static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event)
{
  static size_t msgq_empty_counter = 0;

  switch (event) {
    case ANT_TPMS_PAGE_1_UPDATED:
      // TODO: get type from struct
      int16_t data;
      
      if (k_msgq_get(&spi_ant_queue, &data, K_NO_WAIT) != 0)
      {
        if(msgq_empty_counter % LOG_MSGQ_EMPTY_WRN_EVERY == 0)
        {
          LOG_WRN("no new data in sensor queue");
        }
        msgq_empty_counter++;
      }
      else
      {
        msgq_empty_counter = 0;
        p_profile->TPMS_PROFILE_pressure = data;
      }
      break;
    case ANT_TPMS_PAGE_82_UPDATED:
      // TODO: use actual sensor data
      p_profile->TPMS_PROFILE_battery_voltage_mv = 3125;
      p_profile->TPMS_PROFILE_battery_status = ANT_COMMON_page82_BATTERY_STATE_OK;
      break;
    default:
      break;
  }
}

static void ant_evt_handler(ant_evt_t *p_ant_evt)
{
  ant_tpms_sens_evt_handler(p_ant_evt, &tpms);
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

  tpms.TPMS_PROFILE_sw_revision_minor = APP_VERSION_MINOR;
  tpms.TPMS_PROFILE_sw_revision_major = APP_VERSION_MAJOR;
  tpms.TPMS_PROFILE_serial_number  = get_hwid_16bit();

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

//   err = ant_plus_key_set(TPMS_TX_NETWORK_NUM);
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


  // simulator_setup();

  err = profile_setup();
  if (err) {
    LOG_ERR("ANT+ profile setup failed (rc %d)", err);
    return err;
  }

  return 0;
}

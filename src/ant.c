/**
 *
 */

#include <zephyr/kernel.h>

#include "ant.h"

#include <ant_parameters.h>
#include <ant_key_manager.h>

#include <ant_profiles/tpms/ant_tpms.h>
#include <ant_profiles/tpms/simulator/ant_tpms_simulator.h>


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ant, LOG_LEVEL_DBG);

static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event);

TPMS_SENS_PROFILE_CONFIG_DEF(tpms, ant_tpms_evt_handler);
TPMS_SENS_CHANNEL_CONFIG_DEF(tpms,
  0,    // hardware ANT channel 0
  5,    // transmission type: use common pages
  1234, // device number id
  0     // network number
);

// #define TIMER_TICK_MS (ANT_TPMS_OPERATING_TIME_UNIT * 1000)
// static void timeout_handler(struct k_timer *timer_id);
// static K_TIMER_DEFINE(timer, timeout_handler, NULL);

static ant_tpms_profile_t tpms;
static ant_tpms_simulator_t tpms_simulator;

static void ant_tpms_evt_handler(ant_tpms_profile_t * p_profile, ant_tpms_evt_t event)
{
  switch (event) {
    case ANT_TPMS_PAGE_1_UPDATED:
      ant_tpms_simulator_one_iteration(&tpms_simulator, event);
      break;
    default:
      break;
  }
}

static void simulator_setup(void)
{
  const ant_tpms_simulator_cfg_t simulator_cfg = {
      .p_profile = &tpms,
  };

  ant_tpms_simulator_init(&tpms_simulator, &simulator_cfg, false);
}

// static void timeout_handler(struct k_timer *timer_id)
// {
//     /* noop */
//     /* cyclic timer callback */
// }

static void ant_evt_handler(ant_evt_t *p_ant_evt)
{
  ant_tpms_sens_evt_handler(p_ant_evt, &tpms);
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

//   tpms.TPMS_PROFILE_manuf_id   = TPMS_TX_MFG_ID;
//   tpms.TPMS_PROFILE_serial_num = TPMS_TX_SERIAL_NUM;
//   tpms.TPMS_PROFILE_hw_version = TPMS_TX_HW_VERSION;
//   tpms.TPMS_PROFILE_sw_version = TPMS_TX_SW_VERSION;
//   tpms.TPMS_PROFILE_model_num  = TPMS_TX_MODEL_NUM;

  err = ant_tpms_sens_open(&tpms);
  if (err) {
    LOG_ERR("ant_tpms_sens_open failed: %d", err);
    return err;
  }

  LOG_INF("OK ant_tpms_sens_open");
  return err;
}

int ant_stack_setup(void)
{
  int err = ant_init();
  if (err) {
    LOG_ERR("ant_init failed: %d", err);
    return err;
  }
  LOG_INF("OK ant_init");
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
  LOG_INF("ANT+ TPMS TX sample starting...");

  int err = ant_stack_setup();
  if (err) {
    LOG_ERR("ANT stack setup failed (rc %d)", err);
    return err;
  }


  simulator_setup();

  err = profile_setup();
  if (err) {
    LOG_ERR("ANT+ profile setup failed (rc %d)", err);
    return err;
  }

//   k_timer_start(&timer, K_MSEC(TIMER_TICK_MS), K_MSEC(TIMER_TICK_MS));
  return 0;

  /*
  // ERR case
  k_oops();
  */
  
  return 0;
}

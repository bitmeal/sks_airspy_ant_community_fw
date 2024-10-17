/**
 *
 */

#include <zephyr/kernel.h>

#include "ant.h"

#include <ant_parameters.h>
#include <ant_key_manager.h>
#include <ant_profiles/hrm/simulator/ant_hrm_simulator.h>


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ant, LOG_LEVEL_DBG);

#define TIMER_TICK_MS (ANT_HRM_OPERATING_TIME_UNIT * 1000)

static void ant_hrm_evt_handler(ant_hrm_profile_t * p_profile, ant_hrm_evt_t event);

HRM_SENS_PROFILE_CONFIG_DEF(hrm, true, ANT_HRM_PAGE_0, ant_hrm_evt_handler);
HRM_SENS_CHANNEL_CONFIG_DEF(hrm,
  HRM_TX_CHANNEL_NUM,
  HRM_TX_CHAN_ID_TRANS_TYPE,
  HRM_TX_CHAN_ID_DEV_NUM,
  HRM_TX_NETWORK_NUM);

static void timeout_handler(struct k_timer *timer_id);
static K_TIMER_DEFINE(timer, timeout_handler, NULL);

static ant_hrm_profile_t hrm;
static ant_hrm_simulator_t hrm_simulator;

static void ant_hrm_evt_handler(ant_hrm_profile_t * p_profile, ant_hrm_evt_t event)
{
  switch (event) {
    case ANT_HRM_PAGE_0_UPDATED:
      /* fall through */
    case ANT_HRM_PAGE_1_UPDATED:
      /* fall through */
    case ANT_HRM_PAGE_2_UPDATED:
      /* fall through */
    case ANT_HRM_PAGE_3_UPDATED:
      /* fall through */
    case ANT_HRM_PAGE_4_UPDATED:
      ant_hrm_simulator_one_iteration(&hrm_simulator);
      break;
    default:
      break;
  }
}

static void simulator_setup(void)
{
  const ant_hrm_simulator_cfg_t simulator_cfg = DEFAULT_ANT_HRM_SIMULATOR_CFG(&hrm,
    CONFIG_SIMULATOR_MIN,
    CONFIG_SIMULATOR_MAX,
    CONFIG_SIMULATOR_INCREMENT);

  ant_hrm_simulator_init(&hrm_simulator, &simulator_cfg, false);
}

static void timeout_handler(struct k_timer *timer_id)
{
  /** NOTE: Only the first 3 bytes of this value are taken into account. */
  hrm.HRM_PROFILE_operating_time++;
}

static void ant_evt_handler(ant_evt_t *p_ant_evt)
{
  ant_hrm_sens_evt_handler(p_ant_evt, &hrm);
}

static int profile_setup(void)
{
  int err = ant_hrm_sens_init(&hrm,
    HRM_SENS_CHANNEL_CONFIG(hrm),
    HRM_SENS_PROFILE_CONFIG(hrm));
  if (err) {
    LOG_ERR("ant_hrm_sens_init failed: %d", err);
    return err;
  }

  hrm.HRM_PROFILE_manuf_id   = HRM_TX_MFG_ID;
  hrm.HRM_PROFILE_serial_num = HRM_TX_SERIAL_NUM;
  hrm.HRM_PROFILE_hw_version = HRM_TX_HW_VERSION;
  hrm.HRM_PROFILE_sw_version = HRM_TX_SW_VERSION;
  hrm.HRM_PROFILE_model_num  = HRM_TX_MODEL_NUM;

  err = ant_hrm_sens_open(&hrm);
  if (err) {
    LOG_ERR("ant_hrm_sens_open failed: %d", err);
    return err;
  }

  LOG_INF("OK ant_hrm_sens_open");
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

  err = ant_plus_key_set(HRM_TX_NETWORK_NUM);
  if (err) {
    LOG_ERR("ant_plus_key_set failed: %d", err);
  }
  return err;
}

int start_ant_device(void)
{
  LOG_INF("ANT+ HRM TX sample starting...");

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

  k_timer_start(&timer, K_MSEC(TIMER_TICK_MS), K_MSEC(TIMER_TICK_MS));
  return 0;

  /*
  // ERR case
  k_oops();
  */
  
  return 0;
}

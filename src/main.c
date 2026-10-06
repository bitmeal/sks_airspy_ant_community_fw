/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdlib.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/pm/device.h>

#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#include "retained.h"
#include "settings.h"

#include "bluetooth.h"
#if CONFIG_AIRSPY_ANT
#include "ant.h"
#endif
#include "spi.h"
#include "resource_manager.h"


#define SUPERVISION_CYCLE_TIME_MS 1000

static void supervise(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(supervision_work, supervise);

static void end_bt_keepalive(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(end_bt_keepalive_work, end_bt_keepalive);


static void supervise(struct k_work *work)
{
	retained_update();
	// dummy supervision
	k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));
}

static void end_bt_keepalive(struct k_work *work)
{
	res_mgr_free(&res_mgr_BT);
}

int main(void)
{
	// /* using __TIME__ ensure that a new binary will be built on every
	//  * compile which is convenient when testing firmware upgrade.
	//  */
	// LOG_INF("build time: " __DATE__ " " __TIME__);
	
	///////////////////////////////////////////
	LOG_INF("reading BOOT state...");

	retained_init();
    retained.uptime_latest = 0;
	retained.boots += 1;
	retained_update();

	LOG_INF("boot count: %u; uptime sum: %llus", retained.boots, retained.uptime_sum);

	///////////////////////////////////////////
	LOG_INF("initializing settings storage...");
	start_settings_subsys();

	///////////////////////////////////////////
	LOG_INF("initializing bluetooth...");
	init_bluetooth();
	
	LOG_INF("using resource: BT");
	res_mgr_use(&res_mgr_BT);
	k_work_schedule(&end_bt_keepalive_work, K_MSEC(app_config.device.bt_timeout_ms));

	///////////////////////////////////////////
#if CONFIG_AIRSPY_ANT
	LOG_INF("starting ANT+ device...");
	init_ant();
#endif

	///////////////////////////////////////////
	LOG_INF("starting SPI sensor interface...");
	init_spim();

	///////////////////////////////////////////
	// wait for subsystems to start operation
	k_sleep(K_MSEC(2000));
	// supervise system
	LOG_INF("Scheduling application supervision");
	k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));

	///////////////////////////////////////////
	return EXIT_SUCCESS;
}

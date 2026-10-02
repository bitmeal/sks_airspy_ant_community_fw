/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdlib.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/pm/device.h>
#include <zephyr/sys/poweroff.h>

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
#define SYS_POWEROFF_DELAY 500

static const struct gpio_dt_spec wake_signal = GPIO_DT_SPEC_GET(DT_ALIAS(wake_pin), gpios);

static void poweroff(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(poweroff_work, poweroff);

static void supervise(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(supervision_work, supervise);

static void end_bt_keepalive(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(end_bt_keepalive_work, end_bt_keepalive);

enum mgmt_cb_return dfu_pending_cb(uint32_t event, enum mgmt_cb_return _prev_status,
                                int32_t *_rc, uint16_t *_group, bool *_abort_more,
                                void *_data, size_t _data_size)
{
    if (event == MGMT_EVT_OP_IMG_MGMT_DFU_PENDING ) {
		// reset boot count on firmware update
		retained.boots = 0;
		retained_update();

		LOG_INF("Reset boot count after DFU; enable BT on next boot");
    }

    return MGMT_CB_OK;
}

struct mgmt_callback dfu_pending_reg = {
	.callback = dfu_pending_cb,
    .event_id = MGMT_EVT_OP_IMG_MGMT_DFU_PENDING,
};

static void poweroff(struct k_work *work)
{
	int ret = gpio_pin_interrupt_configure_dt(&wake_signal, GPIO_INT_LEVEL_ACTIVE);

	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, wake_signal.port->name, wake_signal.pin);

		k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));
	}
	else
	{
		LOG_INF("Set up wake signal at %s pin %d\n", wake_signal.port->name, wake_signal.pin);

		// update/write retained memory
		retained.off_count += 1;
		retained_update();

		LOG_INF("Powering OFF NOW");
		sys_poweroff();

		// infinite loop for emulated power off when debugging
		while(true){ continue; }
	}
}

bool wake_pin_uses_resources = false;
static void supervise(struct k_work *work)
{
	if (!res_mgr_in_use(&res_mgr_ANT) && !res_mgr_in_use(&res_mgr_BT))
	{
		LOG_INF("No resources in use; will power off in %dms", SYS_POWEROFF_DELAY);
		k_work_schedule(&poweroff_work, K_MSEC(SYS_POWEROFF_DELAY));
	}
	else k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));
}

static void end_bt_keepalive(struct k_work *work)
{
	res_mgr_free(&res_mgr_BT);
}

int main(void)
{
	int ret;

	// /* using __TIME__ ensure that a new binary will be built on every
	//  * compile which is convenient when testing firmware upgrade.
	//  */
	// LOG_INF("build time: " __DATE__ " " __TIME__);
	
	///////////////////////////////////////////
	LOG_INF("reading BOOT state...");

	bool retained_ok = retained_validate();
	if( !retained_ok )
	{
		LOG_WRN("Retained data is INVALID; initializing");
	}

	/* Increment for this boot attempt and update. */
	retained.boots += 1;
	retained_update();

	LOG_INF("Boot: %u; Uptime: %llus", retained.boots, retained.uptime_sum);

	///////////////////////////////////////////
	LOG_INF("initializing settings storage...");
	start_settings_subsys();

	///////////////////////////////////////////
	LOG_INF("initializing bluetooth...");
	init_bluetooth();
	
	if (retained.boots <= 1 || app_config.device.bt_timeout_ms == 0)
	{
		LOG_INF("using resource: BT");
		res_mgr_use(&res_mgr_BT);
		k_work_schedule(&end_bt_keepalive_work, K_MSEC(app_config.device.bt_timeout_ms));
	}

	LOG_INF("registering SMP callbacks...");
	mgmt_callback_register(&dfu_pending_reg);

	///////////////////////////////////////////
#if CONFIG_AIRSPY_ANT
	LOG_INF("starting ANT+ device...");
	init_ant();
#endif

	///////////////////////////////////////////
	LOG_INF("starting GPIO and power management...");

	if (!gpio_is_ready_dt(&wake_signal))
	{
		LOG_ERR("Error: wake signal device %s is not ready\n",
				wake_signal.port->name);
		return EXIT_FAILURE;
	}

	ret = gpio_pin_configure_dt(&wake_signal, GPIO_INPUT);
	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure %s pin %d\n",
			   ret, wake_signal.port->name, wake_signal.pin);
		return EXIT_FAILURE;
	}

	///////////////////////////////////////////
	LOG_INF("starting SPI sensor interface...");
	gpio_flags_t spi_int_polarity = gpio_pin_get_dt(&wake_signal) ? GPIO_INT_EDGE_TO_INACTIVE : GPIO_INT_EDGE_TO_ACTIVE;
	init_spim(spi_int_polarity);

	///////////////////////////////////////////
	// wait for subsystems to start operation
	k_sleep(K_MSEC(2000));
	// supervise system
	LOG_INF("Scheduling application supervision");
	k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));

	///////////////////////////////////////////
	return EXIT_SUCCESS;
}

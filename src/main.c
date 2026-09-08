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
#include "ant.h"
#include "spi.h"


// TODO(bitmeal): check DT nodes on compile time
// #if !DT_NODE_EXISTS(DT_NODELABEL(wake_pin))
// #error "DT nodes not properly configured."
// #endif
// #define SW0_NODE	DT_ALIAS(sw0)
// #if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
// #error "Unsupported board: sw0 devicetree alias is not defined"
// #endif
// static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
// 							      {0});

static const struct gpio_dt_spec wake_signal = GPIO_DT_SPEC_GET(DT_ALIAS(wake_pin), gpios);

#define SUPERVISION_CYCLE_TIME_MS 1000
#define SYS_POWEROFF_DELAY 500
// Number of seconds we should keep transmitting after a display (like a Karoo)
// actively communicated with
#define DISPLAY_ACTIVITY_KEEPALIVE_S 120
// Force on time after bootup, regardless of the wake signal
#define COLD_BOOT_MINIMUM_ON_TIME_S 600

static void poweroff(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(poweroff_work, poweroff);

static void supervise(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(supervision_work, supervise);


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

static bool keep_awake(void)
{
	int val = gpio_pin_get_dt(&wake_signal);
	static int wake_signal_last = -1;

	if (val != wake_signal_last)
	{
		LOG_INF("Wake signal changed: %d -> %d (uptime %us)",
			    wake_signal_last, val, k_uptime_seconds());
	}
	wake_signal_last = val;

	if (val != 0)
	{
		return true;
	}

	if (retained.boots <= 1 && k_uptime_seconds() < COLD_BOOT_MINIMUM_ON_TIME_S)
	{
		return true;
	}

	if (bt_connection_active())
	{
		return true;
	}

	if (ant_seconds_since_display_activity() < DISPLAY_ACTIVITY_KEEPALIVE_S)
	{
		return true;
	}

	return false;
}

static void poweroff(struct k_work *work)
{
	// spis_suspend();

	if (keep_awake())
	{
		LOG_INF("Aborting power off; wake signal or display activity present");
		k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));
		return;
	}

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

		/* Update the retained state */
		retained.off_count += 1;
		retained_update();

		LOG_INF("Powering OFF NOW");
		sys_poweroff();

		// infinite loop for emulated power off
		while(true){ continue; }
	}
}

static void supervise(struct k_work *work)
{
	if (!keep_awake())
	{
		LOG_INF("Will power off in %dms", SYS_POWEROFF_DELAY);
		k_work_schedule(&poweroff_work, K_MSEC(SYS_POWEROFF_DELAY));
	}
	else
	{
		k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));
	}
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
	LOG_INF("registering management callbacks...");
	mgmt_callback_register(&dfu_pending_reg);

	///////////////////////////////////////////
	LOG_INF("initializing settings storage...");
	start_settings_subsys();

	///////////////////////////////////////////
	if( retained.boots <= 1)
	{
		LOG_INF("starting bluetooth services...");

		start_bluetooth_services();
		LOG_INF("OK bluetooth advertising");
	}
	else
	{
		LOG_INF("will not start bluetooth");
	}
	///////////////////////////////////////////
	LOG_INF("starting ANT+ device...");

	start_ant_device();
	LOG_INF("OK ANT+ device");

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
	spim_init();

	///////////////////////////////////////////
	LOG_INF("Scheduling application supervision");
	k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));
	

	return EXIT_SUCCESS;
}

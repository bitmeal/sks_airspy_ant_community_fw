/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdlib.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/pm/device.h>
#include <zephyr/sys/poweroff.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include "retained.h"
#include "settings.h"

#include "bluetooth.h"
#include "ant.h"
#include "spi.h"



// TODO: check DT nodes on compile time
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

static void poweroff(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(poweroff_work, poweroff);

static void supervise(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(supervision_work, supervise);


static void poweroff(struct k_work *work)
{
	// spis_suspend();
	
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
	int val = gpio_pin_get_dt(&wake_signal);

	if (val == 0)
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

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/pm/device.h>
#include <zephyr/sys/poweroff.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#define STORAGE_PARTITION_LABEL storage_partition
#define STORAGE_PARTITION_ID FIXED_PARTITION_ID(STORAGE_PARTITION_LABEL)

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage);
static struct fs_mount_t littlefs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &cstorage,
	.storage_dev = (void *)STORAGE_PARTITION_ID,
	.mnt_point = "/lfs1"};

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include "bluetooth.h"
#include "ant.h"

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
// static struct gpio_callback wake_signal_cb_data;
// void wake_signal_received(const struct device *dev, struct gpio_callback *cb,
// 						  uint32_t pins)
// {
// 	LOG_INF("Wake signal received");
// }

#define SUPERVISION_CYCLE_TIME_MS 1000
#define SYS_POWEROFF_DELAY 500

static void poweroff(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(poweroff_work, poweroff);

static void supervise(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(supervision_work, supervise);

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
		
		LOG_INF("powering off now");
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
	LOG_INF("starting DFU components...");

	ret = fs_mount(&littlefs_mnt);
	if (ret < 0)
	{
		LOG_ERR("Error mounting littlefs [%d]", ret);
	}
	LOG_INF("OK mounted littlefs");

	///////////////////////////////////////////
	LOG_INF("starting bluetooth advertising...");

	start_bluetooth_services();
	LOG_INF("OK bluetooth advertising");

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

	// ret = gpio_pin_interrupt_configure_dt(&wake_signal,
	// 									  GPIO_INT_LEVEL_ACTIVE);
	// 									//   GPIO_INT_EDGE_TO_ACTIVE);
	// 									//   GPIO_INT_LEVEL_HIGH);
	// if (ret != 0)
	// {
	// 	printk("Error %d: failed to configure interrupt on %s pin %d\n",
	// 		   ret, wake_signal.port->name, wake_signal.pin);
	// 	return EXIT_FAILURE;
	// }

	// // gpio_init_callback(&wake_signal_cb_data, wake_signal_received, BITwake_signal.pin));
	// // gpio_add_callback(wake_signal.port, &wake_signal_cb_data);
	// printk("Set up wake signal at %s pin %d\n", wake_signal.port->name, wake_signal.pin);


	///////////////////////////////////////////
	LOG_INF("Scheduling application supervision");
	k_work_schedule(&supervision_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));
	
	// while (1)
	// {
	// 	k_msleep(SUPERVISION_CYCLE_TIME_MS);

	// 	int val = gpio_pin_get_dt(&wake_signal);

	// 	if (val == 0)
	// 	{
	// 		ret = gpio_pin_interrupt_configure_dt(&wake_signal, GPIO_INT_LEVEL_ACTIVE);

	// 		if (ret != 0)
	// 		{
	// 			LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
	// 				ret, wake_signal.port->name, wake_signal.pin);

	// 			// try again
	// 			continue;
	// 		}

	// 		LOG_INF("Set up wake signal at %s pin %d\n", wake_signal.port->name, wake_signal.pin);

	// 		LOG_INF("Will power off in 500ms");
	// 		k_msleep(500);

	// 		sys_poweroff();

	// 		// infinite loop for emulated power off
	// 		while(true){ continue; }
	// 	}
	// }


	return EXIT_SUCCESS;
}

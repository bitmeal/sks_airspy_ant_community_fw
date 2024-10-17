/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include "bluetooth.h"
#include "ant.h"

#define STORAGE_PARTITION_LABEL storage_partition
#define STORAGE_PARTITION_ID FIXED_PARTITION_ID(STORAGE_PARTITION_LABEL)

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage);
static struct fs_mount_t littlefs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &cstorage,
	.storage_dev = (void *)STORAGE_PARTITION_ID,
	.mnt_point = "/lfs1"};

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 250

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	int ret;

	/* using __TIME__ ensure that a new binary will be built on every
	 * compile which is convenient when testing firmware upgrade.
	 */
	LOG_INF("build time: " __DATE__ " " __TIME__);


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

	start_bluetooth_adverts();
	LOG_INF("OK bluetooth advertising");


	///////////////////////////////////////////
	LOG_INF("starting ANT+ device...");

	start_ant_device();
	LOG_INF("OK ANT+ device");


	///////////////////////////////////////////
	LOG_INF("starting blinky...");

	bool led_state = true;

	if (!gpio_is_ready_dt(&led))
	{
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0)
	{
		return 0;
	}

	while (1)
	{
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0)
		{
			return 0;
		}

		led_state = !led_state;
		LOG_INF("LED: %s", led_state ? "ON" : "OFF");

		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}

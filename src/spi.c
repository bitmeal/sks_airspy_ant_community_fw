/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(spim, LOG_LEVEL_INF);

#include "spi.h"
#include "zbus_com.h"
#include "sensor.h"

// SPI:
//  /INT: active low
//  CLK: idle low
//  sample on CLK rise
//  freq: 47 / 4.5317e-3 ^= 10.37 kHz

#define SPI_MASTER_DT_LABEL spi_master
#define SPI_MASTER_NODE DT_NODELABEL(SPI_MASTER_DT_LABEL)

// SPI slave 
const struct device *spim_dev;
static const struct gpio_dt_spec int_gpio = GPIO_DT_SPEC_GET(SPI_MASTER_NODE, cs_gpios);

static const struct spi_config spim_cfg = {
	// .frequency = 125000,
	.frequency = 10000,
	.operation =	SPI_WORD_SET(8) |
					// SPI_TRANSFER_MSB |
					SPI_OP_MODE_MASTER,
	.slave = 0,
	.cs = NULL
};
#define SPIM_INT_TRANSFER_DELAY_MS 0


static void spim_receive(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(spim_receive_work, spim_receive);

static struct gpio_callback int_cb_data;
void int_cb_handler(const struct device *dev, struct gpio_callback *cb,
						  uint32_t pins)
{
	LOG_INF("interrupt signal received from slave; scheduling SPI receive");
	
	k_work_schedule(&spim_receive_work, K_MSEC(SPIM_INT_TRANSFER_DELAY_MS));
}

int spim_init(void)
{
	int ret;

	spim_dev = DEVICE_DT_GET(SPI_MASTER_NODE);

	if (spim_dev == NULL)
	{
		LOG_ERR("Could not get %s device", spim_dev->name);
		return EXIT_FAILURE;
	}

	if (!device_is_ready(spim_dev))
	{
		LOG_ERR("SPI slave device not ready!");
		return EXIT_FAILURE;
	}

	if (!gpio_is_ready_dt(&int_gpio))
	{
		LOG_ERR("Error: SPIM interrupt GPIO device %s is not ready",
				int_gpio.port->name);
		return EXIT_FAILURE;
	}

	ret = gpio_pin_configure_dt(&int_gpio, GPIO_INPUT);
	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure %s pin %d",
			   ret, int_gpio.port->name, int_gpio.pin);
		return EXIT_FAILURE;
	}

	ret = gpio_pin_interrupt_configure_dt(&int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, int_gpio.port->name, int_gpio.pin);
		return EXIT_FAILURE;
	}

	gpio_init_callback(&int_cb_data, int_cb_handler, BIT(int_gpio.pin));

	ret = gpio_add_callback(int_gpio.port, &int_cb_data);
	if (ret != 0)
	{
		LOG_ERR("Error %d: failed to configure callback for interrupt on %s pin %d\n",
			ret, int_gpio.port->name, int_gpio.pin);
		return EXIT_FAILURE;
	}

	LOG_DBG("SPI device %s OK", spim_dev->name);

	return EXIT_SUCCESS;
}

static void spim_receive(struct k_work *work)
{
	int err;

	struct sensor_readings_t sensor_data;

	static uint8_t rx_buffer[SENSOR_BUFFER_SIZE];
	struct spi_buf rx_buf = {.buf = rx_buffer, .len = sizeof(rx_buffer),};
	const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

	err = spi_read(spim_dev, &spim_cfg, &rx);
	if (err < 0)
	{
		LOG_ERR("SPI error: %d", err);
	}
	else
	{
		err = decode_sensor_buffer(rx_buffer, &sensor_data);

		if (err == SENSOR_ERROR_CHK)
		{
			LOG_WRN("SPI sensor data checksum error! buff: %x; decoder: %x", rx_buffer[5], sensor_data.checksum);
			return;
		}
		
		LOG_HEXDUMP_INF(rx_buffer, sizeof(rx_buffer), "SPI rx:");
		LOG_INF("P[hPa]: %u; T[C]: %d; V[mV]: %u", sensor_data.pressure_hpa, sensor_data.temperature_c, sensor_data.voltage_mv);

		zbus_chan_pub(&sensor_data_chan, &sensor_data, K_MSEC(250));
	}
}

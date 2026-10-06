/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/pinctrl.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(spim, LOG_LEVEL_INF);

#include "spi.h"
#include "zbus_com.h"
#include "sensor.h"
#include "resource_manager.h"

// SPI:
//  /INT: active low for NON-TL or old versions
//  INT: active high for TL or newer versions
//  CLK: idle low
//  sample on CLK rise
//  freq: 47 / 4.5317e-3 ^= 10.37 kHz

#define SPI_MASTER_DT_LABEL spi_master
#define SPI_MASTER_NODE DT_NODELABEL(SPI_MASTER_DT_LABEL)
#define SPI_MASTER_INT_TRANSFER_DELAY_MS 0
#define SPI_MASTER_LAST_TRANSMISION_KEEPALIVE_MS 10000

const struct device *spim_dev;
static const struct gpio_dt_spec int_gpio = GPIO_DT_SPEC_GET(SPI_MASTER_NODE, cs_gpios);
static const struct gpio_dt_spec clk_gpio = GPIO_DT_SPEC_GET(SPI_MASTER_NODE, clk_gpios);
static const struct gpio_dt_spec miso_gpio = GPIO_DT_SPEC_GET(SPI_MASTER_NODE, miso_gpios);

static const struct spi_config spim_cfg = {
	.frequency = 10000,
	.operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER,
	.slave = 0,
};

// start with polarity: falling edge trigger
gpio_flags_t polarity = GPIO_INT_EDGE_FALLING;

static void spim_receive(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(spim_receive_work, spim_receive);

static void end_spim_lifetime(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(end_spim_lifetime_work, end_spim_lifetime);

static struct gpio_callback int_cb_data;
void int_cb_handler(const struct device *dev, struct gpio_callback *cb,
						  uint32_t pins)
{
	LOG_DBG("interrupt signal received from slave; scheduling SPI receive");
	
	k_work_schedule(&spim_receive_work, K_MSEC(SPI_MASTER_INT_TRANSFER_DELAY_MS));
}

static bool spim_lifetime_active = false;
static void spim_receive(struct k_work *work)
{
	//// handle lifetime
	if (!spim_lifetime_active)
	{
		res_mgr_use(&res_mgr_ANT);
		if (app_config.device.bt_timeout_ms == 0)
		{
			res_mgr_use(&res_mgr_BT);
		}
		spim_lifetime_active = true;
		LOG_INF("SPI interrupt received: requesting ANT resource use, scheduling resource free in %dms", SPI_MASTER_LAST_TRANSMISION_KEEPALIVE_MS);
	}

	// extend deadline to free use
	k_work_reschedule(&end_spim_lifetime_work, K_MSEC(SPI_MASTER_LAST_TRANSMISION_KEEPALIVE_MS));

	
	switch(polarity)
	{
		// case GPIO_INT_EDGE_TO_INACTIVE:
		case GPIO_INT_EDGE_FALLING:
			LOG_INF("SPIM interrupt configured as /INT; classic");
			break;

		// case GPIO_INT_EDGE_TO_ACTIVE:
		case GPIO_INT_EDGE_RISING:
			LOG_INF("SPIM interrupt configured as INT; TL, new");
			break;

		default: break;
	}

	//// enable pins
	if (
		// gpio_pin_configure_dt(&clk_gpio, GPIO_OUTPUT_INACTIVE) ||
		gpio_pin_configure_dt(&miso_gpio, GPIO_INPUT)
	)
	{
		LOG_ERR("Failed enabling CLK and MISO pin for SPI transmission!");
	}

	//// read sensor
	struct sensor_readings_t sensor_data;

	static uint8_t rx_buffer[SENSOR_BUFFER_SIZE];
	struct spi_buf rx_buf = {.buf = rx_buffer, .len = sizeof(rx_buffer),};
	const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

	if (spi_read(spim_dev, &spim_cfg, &rx) < 0)
	{
		LOG_ERR("Failed reading from SPI");
	}
	else
	{
		// ALL ZERO? test first byte by value; test remainder all 
		if ( (rx_buffer[0] == 0) && (memcmp(rx_buffer, rx_buffer + 1, sizeof(rx_buffer) - 1) == 0) )
		{
			LOG_WRN("SPI buffer empty (all 0x00); reconfiguring polarity");
			polarity = (polarity == GPIO_INT_EDGE_FALLING ? GPIO_INT_EDGE_RISING : GPIO_INT_EDGE_FALLING);
			// delay polarity change to not trigger on interrupt signal level change on transmission end
			k_sleep(K_MSEC(1));
			if ( gpio_pin_interrupt_configure_dt(&int_gpio, polarity) )
			{
				LOG_ERR("Error: failed to reconfigure SPI interrupt polarity");
			}
		}
		else if (decode_sensor_buffer(rx_buffer, &sensor_data) == SENSOR_ERROR_CHK)
		{
			LOG_WRN("SPI sensor data checksum error! buff: %x; decoder: %x", rx_buffer[5], sensor_data.checksum);
		}
		else
		{
			LOG_HEXDUMP_DBG(rx_buffer, sizeof(rx_buffer), "SPI rx:");
			LOG_INF("P[hPa]: %u; T[C]: %d; V[mV]: %u", sensor_data.pressure_hpa, sensor_data.temperature_c, sensor_data.voltage_mv);

			zbus_chan_pub(&sensor_data_chan, &sensor_data, K_MSEC(250));
		}
	}

	//// disable pins to save power
	if (
		// gpio_pin_configure_dt(&clk_gpio, GPIO_DISCONNECTED) ||
		gpio_pin_configure_dt(&miso_gpio, GPIO_DISCONNECTED)
	)
	{
		LOG_WRN("Failed disabling CLK and MISO pin to save power");
	}

}

static const struct sensor_readings_t invalid_sensor_data_c = {
	.pressure_hpa = 0xFFFF,
	.temperature_c = 0xFF,
	.voltage_mv = 0xFFFF,
};
static void end_spim_lifetime(struct k_work *work)
{
	LOG_INF("No SPI transmission for %dms; freeing ANT resource use", SPI_MASTER_LAST_TRANSMISION_KEEPALIVE_MS);
	spim_lifetime_active = false;
	res_mgr_free(&res_mgr_ANT);
	if (app_config.device.bt_timeout_ms == 0)
	{
		res_mgr_free(&res_mgr_BT);
	}

	LOG_INF("No SPI transmission for %dms; invalidating sensor reading state", SPI_MASTER_LAST_TRANSMISION_KEEPALIVE_MS);
	zbus_chan_pub(&sensor_data_chan, &invalid_sensor_data_c, K_MSEC(250));
}

int init_spim()
{
	spim_dev = DEVICE_DT_GET(SPI_MASTER_NODE);

	if (gpio_pin_configure_dt(&int_gpio, GPIO_INPUT | int_gpio.dt_flags) != 0)
	{
		LOG_ERR("Error: failed to configure SPI interrupt pin");
		return EXIT_FAILURE;
	}

	gpio_init_callback(&int_cb_data, int_cb_handler, BIT(int_gpio.pin));
	if (
		gpio_pin_interrupt_configure_dt(&int_gpio, polarity) ||
		gpio_add_callback(int_gpio.port, &int_cb_data)
	)
	{
		LOG_ERR("Error: failed to configure interrupt and callback on SPI interrupt pin");
		return EXIT_FAILURE;
	}

	LOG_DBG("SPI device %s OK", spim_dev->name);

	return EXIT_SUCCESS;
}

// #include "spi.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
// #include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
// #include <zephyr/drivers/pinctrl.h>
#include <zephyr/pm/device.h>


// #include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(spis, LOG_LEVEL_DBG);

#include "spi.h"
#include "com.h"
#include "decoder.h"

// SPI:
//  CS: active low
//  CLK: idle low
//  sample on CLK rise
//  freq: 47 / 4.5317e-3 ^= 10.37 kHz

// #define DT_DRV_COMPAT nordic_nrf_spis
#define SPI_SLAVE_DT_LABEL spi_slave
#define SPI_SLAVE_NODE DT_NODELABEL(SPI_SLAVE_DT_LABEL)

// SPI slave functionality
const struct device *spis_dev;

static const struct spi_config spis_cfg = {
	.frequency = 0,
	.operation =	SPI_WORD_SET(8) |
					SPI_TRANSFER_MSB |
					// SPI_MODE_CPHA |
					// SPI_MODE_CPOL |
					SPI_OP_MODE_SLAVE,
	.slave = 0,
	.cs = NULL
};

#define SPIS_STACK_SIZE 500
#define SPIS_PRIORITY 5

extern void spis_thread(void *, void *, void *);

K_THREAD_DEFINE(spis_tid, SPIS_STACK_SIZE, spis_thread, NULL, NULL, NULL,
				SPIS_PRIORITY, 0, 0);

#include <hal/nrf_gpio.h>
#include <math.h>
// // TODO: read from dts
#define SPI_SLAVE_CSN_PIN 22
#define SPI_SLAVE_CLK_PIN 19
#define SPI_SLAVE_MOSI_PIN 15

#define SPI_PIN_STATES 3
#define SPI_PINS_ACTIVE 3

#define SPI_PINS_CHANGE_STATE_EVERY 4

// change pull/bias state of all SPI pins to find working one: pin state forumla:
// pprint.pp([(i / (outputs**0) % states, int(i / (outputs**1)) % states, int(i / (outputs**2)) % states) for i in range(states**outputs)])
static void cycle_spi_pin_config(void)
{
	const static nrf_gpio_pin_pull_t spi_pin_pull_lookup[SPI_PIN_STATES] = { NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_PULLUP };
	// const static char* spi_pin_pull_lookup_str[SPI_PIN_STATES] = { "NRF_GPIO_PIN_NOPULL", "NRF_GPIO_PIN_PULLDOWN", "NRF_GPIO_PIN_PULLUP" };

	static int cycle_counter = 0;

	if(cycle_counter % SPI_PINS_CHANGE_STATE_EVERY == 0)
	{
		int cycle_idx = cycle_counter / SPI_PINS_CHANGE_STATE_EVERY;

		int csn_pull_idx = cycle_idx % SPI_PIN_STATES;
		int clk_pull_idx = ( cycle_idx / SPI_PINS_ACTIVE ) % SPI_PIN_STATES;
		int mosi_pull_idx = ( cycle_idx / ( SPI_PINS_ACTIVE * SPI_PINS_ACTIVE ) ) % SPI_PIN_STATES;

		// LOG_INF("CSN: %s; CLK: %s; MOSI: %s", spi_pin_pull_lookup_str[csn_pull_idx], spi_pin_pull_lookup_str[clk_pull_idx], spi_pin_pull_lookup_str[mosi_pull_idx]);
		LOG_INF("CSN: %d; CLK: %d; MOSI: %d", csn_pull_idx, clk_pull_idx, mosi_pull_idx);

		nrf_gpio_reconfigure(
			SPI_SLAVE_CSN_PIN,
			NULL,
			NULL,
			&spi_pin_pull_lookup[csn_pull_idx],
			NULL,
			NULL
		);

		nrf_gpio_reconfigure(
			SPI_SLAVE_CLK_PIN,
			NULL,
			NULL,
			&spi_pin_pull_lookup[clk_pull_idx],
			NULL,
			NULL
		);

		nrf_gpio_reconfigure(
			SPI_SLAVE_MOSI_PIN,
			NULL,
			NULL,
			&spi_pin_pull_lookup[mosi_pull_idx],
			NULL,
			NULL
		);

	}
	
	cycle_counter++;
}

static void spis_init(void)
{
	spis_dev = DEVICE_DT_GET(SPI_SLAVE_NODE);

	if (spis_dev == NULL)
	{
		LOG_ERR("Could not get %s device", spis_dev->name);
		return;
	}

	if (!device_is_ready(spis_dev))
	{
		LOG_ERR("SPI slave device not ready!");
		return;
	}

	LOG_DBG("Device name: %s", spis_dev->name);

	// nrf_gpio_pin_pull_t csn_pull_cfg = nrf_gpio_pin_pull_get(SPI_SLAVE_CSN_PIN);
	// nrf_gpio_pin_pull_t csn_pull_down_cfg = NRF_GPIO_PIN_PULLDOWN;
	// nrf_gpio_reconfigure(
	// 	SPI_SLAVE_CSN_PIN,
	// 	NULL,
	// 	NULL,
	// 	&csn_pull_down_cfg,
	// 	NULL,
	// 	NULL
	// );
	
	// k_sleep(K_MSEC(52));

	// nrf_gpio_reconfigure(
	// 	SPI_SLAVE_CSN_PIN,
	// 	NULL,
	// 	NULL,
	// 	&csn_pull_cfg,
	// 	NULL,
	// 	NULL
	// );

	// LOG_INF("SPI CSN %d, MISO %d, MOSI %d, CLK %d\n",
	// 		DT_PROP_BY_IDX(SPI_SLAVE_PINCTRL_NODE, psels, NRF_FUN_SPIS_CSN),
	// 		DT_PROP_BY_IDX(SPI_SLAVE_PINCTRL_NODE, psels, NRF_FUN_SPIS_MISO),
	// 		DT_PROP_BY_IDX(SPI_SLAVE_PINCTRL_NODE, psels, NRF_FUN_SPIS_MOSI),
	// 		DT_PROP_BY_IDX(SPI_SLAVE_PINCTRL_NODE, psels, NRF_FUN_SPIS_SCK)
	// 	);
}

int spis_suspend(void)
{
	// spis_dev = DEVICE_DT_GET(DT_NODELABEL(SPI_SLAVE_DT_LABEL));

	if (spis_dev == NULL)
	{
		LOG_ERR("Could not get %s device", spis_dev->name);
		return EXIT_FAILURE;
	}

	int rc = pm_device_action_run(spis_dev, PM_DEVICE_ACTION_SUSPEND);
	if(rc != 0)
	{
		LOG_ERR("Could not suspend %s device", spis_dev->name);
	}

	return rc;
}

static void spis_receive(void)
{
	int err;

	struct sensor_readings_t sensor_data;

	static uint8_t rx_buffer[SENSOR_BUFFER_SIZE];
	struct spi_buf rx_buf = {.buf = rx_buffer, .len = sizeof(rx_buffer),};
	const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

	const struct spi_buf_set tx = {.buffers = NULL, .count = 0};

	cycle_spi_pin_config();

	err = spi_transceive(spis_dev, &spis_cfg, &tx, &rx);
	if (err < 0)
	{
		LOG_ERR("SPI error: %d", err);
	}
	else
	{
		LOG_HEXDUMP_INF(rx_buffer, err, "SPI rx:");

		err = decode_sensor_buffer(rx_buffer, &sensor_data);

		LOG_DBG("CHK: buff: %x; decoder: %x", rx_buffer[5], sensor_data.checksum);

		if (err != SENSOR_ERROR_CHK)
		{
		
			LOG_INF("P[hPa]: %u; T[C]: %d; V[mV]: %u", sensor_data.pressure_hpa, sensor_data.temperature_c, sensor_data.voltage_mv);

			while (k_msgq_put(&spi_ant_queue, &(sensor_data.pressure_hpa), K_NO_WAIT) != 0)
			{
				/* message queue is full: purge old data & try again */
				LOG_WRN("SPI message queue exhausted; purging messages");
				k_msgq_purge(&spi_ant_queue);
			}
		}
		else
		{
			LOG_ERR("SPI sensor data checksum error");
		}
	}
}

extern void spis_thread(void *unused1, void *unused2, void *unused3)
{
	spis_init();

	while (1)
	{
		spis_receive();
	}
}

// #include "spi.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
// #include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

// #include <zephyr/dt-bindings/pinctrl/nrf-pinctrl.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(spis, LOG_LEVEL_DBG);

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
#define SPI_SLAVE_PINCTRL_NODE DT_CHILD(DT_PINCTRL_0(SPI_SLAVE_NODE, 0), group1)

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

static void spis_init(void)
{
	spis_dev = DEVICE_DT_GET(DT_NODELABEL(SPI_SLAVE_DT_LABEL));

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

	LOG_INF("Device name: %s", spis_dev->name);

	// LOG_INF("SPI CSN %d, MISO %d, MOSI %d, CLK %d\n",
	// 		DT_PROP_BY_IDX(SPI_SLAVE_PINCTRL_NODE, psels, NRF_FUN_SPIS_CSN),
	// 		DT_PROP_BY_IDX(SPI_SLAVE_PINCTRL_NODE, psels, NRF_FUN_SPIS_MISO),
	// 		DT_PROP_BY_IDX(SPI_SLAVE_PINCTRL_NODE, psels, NRF_FUN_SPIS_MOSI),
	// 		DT_PROP_BY_IDX(SPI_SLAVE_PINCTRL_NODE, psels, NRF_FUN_SPIS_SCK)
	// 	);
}

void spis_receive(void)
{
	int err;

	struct sensor_readings_t sensor_data;

	static uint8_t rx_buffer[SENSOR_BUFFER_SIZE];
	struct spi_buf rx_buf = {.buf = rx_buffer, .len = sizeof(rx_buffer),};
	const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

	const struct spi_buf_set tx = {.buffers = NULL, .count = 0};

	err = spi_transceive(spis_dev, &spis_cfg, &tx, &rx);
	if (err < 0)
	{
		LOG_ERR("SPI error: %d", err);
	}
	else
	{
		// LOG_INF("byte received: %d", err);
		// LOG_INF("TX buffer [0]: %x\n", tx_buffer[0]);
		LOG_HEXDUMP_INF(rx_buffer, err, "SPI rx:");
		// LOG_INF("RX buffer [0]: %x\n", rx_buffer[0]);

		err = decode_sensor_buffer(rx_buffer, &sensor_data);

		LOG_INF("CHK: buff: %x; decoder: %x", rx_buffer[5], sensor_data.checksum);

		if (err != SENSOR_ERROR_CHK)
		{
		
			LOG_INF("P[hPa]: %u; T[C]: %d; V[mV]: %u", sensor_data.pressure_hpa, sensor_data.temperature_c, sensor_data.voltage_mv);

			// TODO: make msgq hold actual data
			while (k_msgq_put(&spi_ant_queue, &(sensor_data.pressure_hpa), K_NO_WAIT) != 0)
			{
				/* message queue is full: purge old data & try again */
				LOG_WRN("purging SPI message queue");
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

// void start_spi_slave()
// {

// }
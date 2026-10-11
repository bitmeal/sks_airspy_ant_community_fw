/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_SPI_H__
#define INCLUDE_SPI_H__

#include <zephyr/drivers/gpio.h>
#include <zephyr/zbus/zbus.h>
#include "settings.h"

// start polarity learning with falling edge trigger
#define DEVICE_SPI_POLARITY_STARTUP DEVICE_SPI_POLARITY_FALLING
#define DEVICE_SPI_POLARITY_SUCCESS_COUNT 10

int init_spim();

void spi_config_update_notification_handler_cb(const struct zbus_channel *chan);


#endif // INCLUDE_SPI_H__

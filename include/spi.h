/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_SPI_H__
#define INCLUDE_SPI_H__

#include <zephyr/drivers/gpio.h>

int init_spim(gpio_flags_t polarity);

#endif // INCLUDE_SPI_H__

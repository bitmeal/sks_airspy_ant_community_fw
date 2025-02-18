/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef INCLUDE_DECODER_H__
#define INCLUDE_DECODER_H__

#include <stdlib.h>
#include <stdint.h>

#define SENSOR_BUFFER_SIZE 6

/*
layout:
    [0, 17): pressure --> (b[9:17].int - 17) * 17
    [17, 25) - 55 ^= temperature
    [25, 33) + 122 ^= voltage
    35: FLAG: under voltage
    [40,48): XOR checksum; ignore lowest bit after shifting whole buffer
*/

struct __attribute__((__packed__)) sensor_readings_t {
    int16_t pressure_hpa;
    int8_t temperature_c;
    int16_t voltage_mv;
    unsigned char flags;
    unsigned char checksum;
};

#define SENSOR_COMP_CONST_PRESS 17
#define SENSOR_COMP_CONST_TEMP -55
#define SENSOR_COMP_CONST_VOLT 122

#define SENSOR_FLAG_UNDERVOLTAGE = 0x20

#define SENSOR_ERROR_CHK 1

int decode_sensor_buffer(uint8_t* buffer, struct sensor_readings_t* sensor_readings);
uint8_t battery_level_percent(const int16_t voltage_mv);

#endif // INCLUDE_DECODER_H__
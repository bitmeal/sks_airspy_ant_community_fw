#include "sensor.h"

/*
layout:
    [0, 8): unknown
    [8, 17): pressure --> (b[9:17].int - 17) * 17
    [17, 25) - 55 ^= temperature
    [25, 33) + 122 ^= voltage
    35: FLAG: under voltage
    [40,48): XOR checksum; ignore lowest bit after shifting whole buffer
*/

int decode_sensor_buffer(uint8_t* buffer, struct sensor_readings_t* sensor_readings)
{
    // shift whole buffer left by 1
    for(int i = 0; i < SENSOR_BUFFER_SIZE; i++)
    {
        buffer[i] <<= 1;
        if ( i < SENSOR_BUFFER_SIZE - 1)
        {
            buffer[i] |= buffer[i + 1] >> 7;
        }
    }

    // (*(uint32_t*)buffer) = (*(uint32_t*)buffer) << 1;
    // buffer[3] |= (buffer[4] >> 7);
    // (*(uint16_t*)(buffer + 4)) = (*(uint16_t*)(buffer + 4)) << 1;

    // compensate and assign data
    sensor_readings->pressure_hpa = ( (int16_t)( ( (*(uint16_t*)buffer) >> 8 ) | ( (*(uint16_t*)buffer) << 8 ) ) - SENSOR_COMP_CONST_PRESS ) * SENSOR_COMP_CONST_PRESS;
    sensor_readings->pressure_hpa = sensor_readings->pressure_hpa > 0 ? sensor_readings->pressure_hpa : 0; // clamp at 0

    sensor_readings->temperature_c = buffer[2] + SENSOR_COMP_CONST_TEMP;

    sensor_readings->voltage_mv = ( buffer[3] + SENSOR_COMP_CONST_VOLT ) * 10;

    sensor_readings->flags = buffer[4];

    // build xor checksum of data, but ignore lowest bit
    sensor_readings->checksum = 0xFE & (buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3] ^ buffer[4]);

    if (sensor_readings->checksum != buffer[5])
    {
        return SENSOR_ERROR_CHK;
    }

    return EXIT_SUCCESS;
}

/*  based on NRF5 SDK components/libraries/util/app_util.h
 *
 *  The calculation is based on a linearized version of the battery's discharge
 *  curve. 3.0V returns 100% battery level. The limit for power failure is 2.1V and
 *  is considered to be the lower boundary.
 *
 *  The discharge curve for CR2032 is non-linear. In this model it is split into
 *  4 linear sections:
 *  - Section 1: 3.0V - 2.9V = 100% - 42% (58% drop on 100 mV)
 *  - Section 2: 2.9V - 2.74V = 42% - 18% (24% drop on 160 mV)
 *  - Section 3: 2.74V - 2.44V = 18% - 6% (12% drop on 300 mV)
 *  - Section 4: 2.44V - 2.1V = 6% - 0% (6% drop on 340 mV)
 */
uint8_t battery_level_percent(const int16_t voltage_mv)
{
    if (voltage_mv >= 3000)
    {
        return 100;
    }
    else if (voltage_mv > 2900)
    {
        return 100 - ((3000 - voltage_mv) * 58) / 100;
    }
    else if (voltage_mv > 2740)
    {
        return 42 - ((2900 - voltage_mv) * 24) / 160;
    }
    else if (voltage_mv > 2440)
    {
        return 18 - ((2740 - voltage_mv) * 12) / 300;
    }
    else if (voltage_mv > 2100)
    {
        return 6 - ((2440 - voltage_mv) * 6) / 340;
    }
    else
    {
        return 0;
    }
}
#include "decoder.h"

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
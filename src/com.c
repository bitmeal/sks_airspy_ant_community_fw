#include <zephyr/kernel.h>
#include <decoder.h>

K_MSGQ_DEFINE(spi_ant_queue, sizeof(((struct sensor_readings_t){}).pressure_hpa), 1, 1);

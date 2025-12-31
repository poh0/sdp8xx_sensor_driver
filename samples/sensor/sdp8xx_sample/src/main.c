/*
 * Copyright (c) 2025 Lauri Pöhö
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/sensor/sdp8xx.h>

LOG_MODULE_REGISTER(APP, LOG_LEVEL_INF);
 
const struct device *dev = DEVICE_DT_GET_ANY(sensirion_sdp8xx);

int main(void)
{
    if (!device_is_ready(dev)) {
        LOG_ERR("Sensor not ready.");
        return 0;
    }

    struct sensor_value pressure, temp;
    while (1) {
        if (sensor_sample_fetch(dev) < 0) {
            LOG_WRN("Fetch failed");
            k_sleep(K_MSEC(500)); 
            continue;
        }

        sensor_channel_get(dev, SENSOR_CHAN_PRESS, &pressure);
        sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);

        float diff_pressure = sensor_value_to_float(&pressure);

        LOG_INF("Pressure: %.1f Pa", diff_pressure);

        k_sleep(K_MSEC(100));
    }
    return 0;
}
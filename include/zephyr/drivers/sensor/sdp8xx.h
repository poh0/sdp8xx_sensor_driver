#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_SDP8XX_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_SDP8XX_H_

#include <zephyr/drivers/sensor.h>

/**
 * @brief Custom sensor attributes for SDP8xx
 */
enum sensor_attribute_sdp8xx {
    /**
     * Set Measurement Mode
     * Val1: enum sdp8xx_measurement_mode
     */
    SENSOR_ATTR_SDP8XX_MEASUREMENT_MODE = SENSOR_ATTR_PRIV_START,
};

/**
 * @brief Measurement Modes for SDP8xx
 */
enum sdp8xx_measurement_mode {
    SDP8XX_MODE_DIFF_PRESSURE = 0,
    SDP8XX_MODE_MASS_FLOW     = 1,
};

#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_SDP8XX_H_ */

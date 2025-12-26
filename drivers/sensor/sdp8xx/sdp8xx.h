#ifndef ZEPHYR_DRIVERS_SENSOR_SDP8XX_SDP8XX_H_
#define ZEPHYR_DRIVERS_SENSOR_SDP8XX_SDP8XX_H_

#include <zephyr/device.h>

#define SDP8XX_CMD_STOP_CONT            0x3FF9
#define SDP8XX_CMD_SOFT_RESET           0x0006
#define SDP8XX_CMD_ENTER_SLEEP          0x3677
#define SDP8XX_CMD_EXIT_SLEEP           0x0000 /* Dummy write to wake */

/* Triggered Modes */
#define SDP8XX_CMD_TRIG_DP              0x362F
#define SDP8XX_CMD_TRIG_DP_CS           0x372D
#define SDP8XX_CMD_TRIG_MF              0x3624
#define SDP8XX_CMD_TRIG_MF_CS           0x3726

#define SDP8XX_CRC_POLY                 0x31
#define SDP8XX_CRC_INIT                 0xFF

/* new measurement is ready after 45 ms */
#define SDP8XX_TRIG_MEASURE_WAIT_MS     45

#define SDP8XX_POLL_INTERVAL            5

#define TEMP_SCALE_FACTOR 200.f

struct sdp8xx_config {
    struct i2c_dt_spec i2c;
    bool clock_stretching;
    uint16_t default_mode;
};

struct sdp8xx_data {
    int16_t pressure_raw;
    int16_t temp_raw;
    uint16_t scale_factor;
    uint16_t meas_mode;
};

#endif /* ZEPHYR_DRIVERS_SENSOR_SDP8XX_SDP8XX_H_ */
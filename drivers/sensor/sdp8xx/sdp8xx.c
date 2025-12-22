/*
 * Copyright (c) 2025 Lauri Pöhö
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sensirion_sdp8xx

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(SDP8XX, CONFIG_SENSOR_LOG_LEVEL);

#if DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 0
	#error "SDP8xx sensor is not defined in DTS"
#endif

/*
*  Bare minimum trigger mode, differential pressure, no clock stretching
*/

// new sample is available 45 ms after reading value in trigger mode
#define SDP8XX_TRIG_WAIT_MS           55

#define SDP8XX_CMD_TRIG_DIFF_PRESSURE 0x362F

struct sdp8xx_data {
    int16_t diff_pressure_raw;
    int16_t temperature;
    uint16_t scale_factor;
};

struct sdp8xx_config {
    struct i2c_dt_spec i2c;
};

static int sdp8xx_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct sdp8xx_data *data = dev->data;
    const struct sdp8xx_config *cfg = dev->config;
    
	int ret;
    uint8_t tx_buf[2];
    uint8_t rx_buf[9];
	
	if (chan != SENSOR_CHAN_ALL && 
	    chan != SENSOR_CHAN_PRESS && 
	    chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}

	sys_put_be16(SDP8XX_CMD_TRIG_DIFF_PRESSURE, tx_buf);
    
	ret = i2c_write_dt(&cfg->i2c, tx_buf, sizeof(tx_buf));

	if (ret < 0) {
		LOG_ERR("Failed to trigger measurement");
		return ret;
	}

	/* Wait for measurement */
	/* Todo: Is allowed in driver? */
	k_sleep(K_MSEC(SDP8XX_TRIG_WAIT_MS));

	ret = i2c_read_dt(&cfg->i2c, rx_buf, sizeof(rx_buf));
	
	if (ret < 0) {
		LOG_ERR("Failed to read data");
		return ret;
	}

	data->diff_pressure_raw = sys_get_be16(&rx_buf[0]);
	data->temperature = sys_get_be16(&rx_buf[3]);
	data->scale_factor = sys_get_be16(&rx_buf[6]);

	if (data->scale_factor == 0) {
		return -EIO;
	}

	return 0;
}

static int sdp8xx_channel_get(const struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct sdp8xx_data *data = dev->data;
	
	if (data->scale_factor == 0) return -EINVAL;

	switch (chan) {
	case SENSOR_CHAN_PRESS:
		/* Convert raw to Pa */
		float pressure = (float) data->diff_pressure_raw/data->scale_factor;
		sensor_value_from_float(val, pressure);
		break;
	case SENSOR_CHAN_AMBIENT_TEMP:
		/* Convert raw to Celsius */
		float temp = (float) data->temperature / 200.f;
		sensor_value_from_float(val, temp);
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

static int sdp8xx_init(const struct device *dev)
{
	const struct sdp8xx_config *cfg = dev->config;
	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}
	return 0;
}

static const struct sensor_driver_api sdp8xx_api = {
	.sample_fetch = &sdp8xx_sample_fetch,
	.channel_get = &sdp8xx_channel_get,
};

#define SDP8XX_DEFINE(inst)                                     \
	static struct sdp8xx_data sdp8xx_data_##inst;               \
	static const struct sdp8xx_config sdp8xx_config_##inst = {  \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                      \
	};                                                          \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, sdp8xx_init, NULL,       \
			      &sdp8xx_data_##inst, &sdp8xx_config_##inst,   \
			      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,     \
			      &sdp8xx_api);

DT_INST_FOREACH_STATUS_OKAY(SDP8XX_DEFINE)

/*
 * Copyright (c) 2025 Lauri Pöhö
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sensirion_sdp8xx

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

#include <zephyr/drivers/sensor/sdp8xx.h>
#include "sdp8xx.h"

LOG_MODULE_REGISTER(SDP8XX, CONFIG_SENSOR_LOG_LEVEL);

static uint8_t sdp8xx_compute_crc(uint16_t value)
{
    uint8_t buf[2];
    sys_put_be16(value, buf);
    return crc8(buf, 2, SDP8XX_CRC_POLY, SDP8XX_CRC_INIT, false);
}

static int sdp8xx_write_command(const struct device *dev, uint16_t cmd)
{
    const struct sdp8xx_config *cfg = dev->config;
    uint8_t tx_buf[2];

    sys_put_be16(cmd, tx_buf);
    return i2c_write_dt(&cfg->i2c, tx_buf, sizeof(tx_buf));
}

static int sdp8xx_attr_set(const struct device *dev,
							enum sensor_channel chan,
							enum sensor_attribute attr,
							const struct sensor_value *val)
{
	struct sdp8xx_data *data = dev->data;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_PRESS) {
		return -ENOTSUP;
	}

	if ((enum sensor_attribute_sdp8xx)attr == SENSOR_ATTR_SDP8XX_MEASUREMENT_MODE) {
		if (val->val1 == SDP8XX_MODE_DIFF_PRESSURE ||
			val->val1 == SDP8XX_MODE_MASS_FLOW) {

			data->meas_mode = val->val1;
			return 0;
		}
		return -EINVAL;
	}
	return -ENOTSUP;
}

static int sdp8xx_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct sdp8xx_data *data = dev->data;
    const struct sdp8xx_config *cfg = dev->config;
    
	int ret;
    uint8_t rx_buf[9];
	
	if (chan != SENSOR_CHAN_ALL && 
	    chan != SENSOR_CHAN_PRESS && 
	    chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}

	uint16_t cmd;

	/* Todo: implement clock stretching */
	switch(data->meas_mode) {
	case SDP8XX_MODE_DIFF_PRESSURE:
		cmd = cfg->clock_stretching ? SDP8XX_CMD_TRIG_DP_CS : SDP8XX_CMD_TRIG_DP;
		break;
	case SDP8XX_MODE_MASS_FLOW:
		cmd = cfg->clock_stretching ? SDP8XX_CMD_TRIG_MF_CS : SDP8XX_CMD_TRIG_MF;
		break;
	default:
		return -ENOTSUP;
	}

	ret = sdp8xx_write_command(dev, cmd);

	if (ret < 0) {
		LOG_ERR("Failed to trigger measurement");
		return ret;
	}

	/* Wait for measurement */
	k_sleep(K_MSEC(SDP8XX_TRIG_MEASURE_WAIT_MS));

	ret = i2c_read_dt(&cfg->i2c, rx_buf, sizeof(rx_buf));
	
	if (ret < 0) {
		LOG_ERR("Failed to read data");
		return ret;
	}

	data->pressure_raw = sys_get_be16(&rx_buf[0]);
	data->temp_raw = sys_get_be16(&rx_buf[3]);
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
		float pressure = (float) data->pressure_raw/data->scale_factor;
		sensor_value_from_float(val, pressure);
		break;
	case SENSOR_CHAN_AMBIENT_TEMP:
		/* Convert raw to Celsius */
		float temp = (float) data->temp_raw / TEMP_SCALE_FACTOR;
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
	struct sdp8xx_data *data = dev->data;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	data->meas_mode = cfg->default_mode;

	return 0;
}

static DEVICE_API(sensor, sdp8xx_api) = {
	.sample_fetch = sdp8xx_sample_fetch,
	.channel_get = sdp8xx_channel_get,
	.attr_set = sdp8xx_attr_set
};

#define SDP8XX_INIT(inst)                                       \
	static struct sdp8xx_data sdp8xx_data_##inst;               \
																\
	static const struct sdp8xx_config sdp8xx_config_##inst = {  \
		.i2c = I2C_DT_SPEC_INST_GET(inst),						\
		.clock_stretching = DT_INST_PROP(inst, clock_stretching), \
		.default_mode = DT_INST_ENUM_IDX(inst, measurement_mode)\
	};                                                          \
																\
	SENSOR_DEVICE_DT_INST_DEFINE(inst,							\
							sdp8xx_init,						\
							NULL,       						\
			      			&sdp8xx_data_##inst,				\
							&sdp8xx_config_##inst,   			\
			      			POST_KERNEL,						\
							CONFIG_SENSOR_INIT_PRIORITY,     	\
			      			&sdp8xx_api);

DT_INST_FOREACH_STATUS_OKAY(SDP8XX_INIT)
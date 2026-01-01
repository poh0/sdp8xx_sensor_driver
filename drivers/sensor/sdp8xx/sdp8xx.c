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
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>

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

static int sdp8xx_i2c_get(const struct device *dev)
{
	const struct sdp8xx_config *config = dev->config;

	return pm_device_runtime_get(config->i2c.bus);
}

static int sdp8xx_i2c_put(const struct device *dev)
{
	const struct sdp8xx_config *config = dev->config;

	return pm_device_runtime_put(config->i2c.bus);
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
	uint16_t cmd;
	
	if (chan != SENSOR_CHAN_ALL && 
	    chan != SENSOR_CHAN_PRESS && 
	    chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}

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

	ret = sdp8xx_i2c_get(dev);
	if (ret < 0) {
		LOG_WRN("Couldn't get I2C bus");
		return ret;
	}

	ret = pm_device_runtime_get(dev);
	if (ret < 0) {
		LOG_WRN("Couldn't get device");
		goto exit_bus;
	}

	ret = sdp8xx_write_command(dev, cmd);
	if (ret < 0) {
		LOG_ERR("Couldn't write I2C");
		goto exit_dev;
	}

	k_sleep(K_MSEC(SDP8XX_TRIG_MEASURE_WAIT_MS));

	ret = i2c_read_dt(&cfg->i2c, rx_buf, sizeof(rx_buf));
	
	if (ret < 0) {
		LOG_ERR("Failed to read data");
		goto exit_dev;
	}

	data->pressure_raw = sys_get_be16(&rx_buf[0]);
	data->temp_raw = sys_get_be16(&rx_buf[3]);
	data->scale_factor = sys_get_be16(&rx_buf[6]);

	if (data->scale_factor == 0) {
		ret = -EIO;
		goto exit_dev;
	}

exit_dev:
	pm_device_runtime_put(dev);

exit_bus:
	sdp8xx_i2c_put(dev);

	return ret;
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

#ifdef CONFIG_PM_DEVICE
static int sdp8xx_pm_action(const struct device *dev,
                            enum pm_device_action action)
{
    const struct sdp8xx_config *cfg = dev->config;
    int ret = 0;

    switch (action) {
    case PM_DEVICE_ACTION_RESUME:
        int ret;
		ret = sdp8xx_i2c_get(dev);
		if (ret < 0) {
			LOG_WRN("Couldn't get I2C bus");
			break;
		}

		/* Send write bit + wait 2ms (datasheet)*/
		uint8_t buffer[2];
		(void)i2c_write_dt(&cfg->i2c, &buffer[0], 1);

        sdp8xx_i2c_put(dev);

        k_sleep(K_MSEC(2));
        break;

    case PM_DEVICE_ACTION_SUSPEND:
		ret = sdp8xx_i2c_get(dev);
		if (ret < 0) {
			LOG_WRN("Couldn't get I2C bus");
			break;
		}
		ret = sdp8xx_write_command(dev, SDP8XX_CMD_ENTER_SLEEP);
		if (ret < 0) {
			LOG_WRN("Couldn't enter sleep: %d", ret);
		}
		sdp8xx_i2c_put(dev);
        break;

    default:
        ret = -ENOTSUP;
    }

    return ret;
}
#endif /* CONFIG_PM_DEVICE */

static int sdp8xx_init(const struct device *dev)
{
	const struct sdp8xx_config *cfg = dev->config;
	struct sdp8xx_data *data = dev->data;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	/* max 25ms power up time */
	k_sleep(K_MSEC(SDP8XX_POWERUP_TIME_MS));

	data->meas_mode = cfg->default_mode;

	return pm_device_runtime_enable(dev);
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
		.clock_stretching = DT_INST_PROP(inst, clock_stretching),\
		.averaging = DT_INST_PROP(inst, averaging),				\
		.default_mode = DT_INST_ENUM_IDX(inst, measurement_mode)\
	};                                                          \
																\
	PM_DEVICE_DT_INST_DEFINE(inst, sdp8xx_pm_action);			\
	SENSOR_DEVICE_DT_INST_DEFINE(inst,							\
							sdp8xx_init,						\
							PM_DEVICE_DT_INST_GET(inst),		\
			      			&sdp8xx_data_##inst,				\
							&sdp8xx_config_##inst,   			\
			      			POST_KERNEL,						\
							CONFIG_SENSOR_INIT_PRIORITY,     	\
			      			&sdp8xx_api);

DT_INST_FOREACH_STATUS_OKAY(SDP8XX_INIT)
/*
 * Copyright (c) Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT pixart_paj7620

#include <zephyr/drivers/i2c.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor/paj7620.h>

#include "paj7620.h"
#include "paj7620_reg.h"

#ifdef LOG_ERR
#undef LOG_ERR
#define LOG_ERR(...) {printf(__VA_ARGS__); printf("\n");}
#endif

#ifdef LOG_DBG
#undef LOG_DBG
#define LOG_DBG(...) {printf(__VA_ARGS__); printf("\n");}
#endif

/** TODO:
 * Enable Z-Axis gestures (Backward, Forward) as KConfig option - disabled by default
 * Enable Circular gestures (Clockwise, Anticlockwise, Wave) as Kconfig option - disabled by default
 * Another Kconfig option -> Sensor is Gesture mode / Sensor is Cursor Mode
 */

LOG_MODULE_REGISTER(PAJ7620, CONFIG_SENSOR_LOG_LEVEL);

static int paj7620_byte_read(const struct device *dev, uint8_t reg, uint8_t *byte)
{
	const struct paj7620_config *config = dev->config;

	return i2c_reg_read_byte_dt(&config->i2c, reg, byte);
}

static int paj7620_byte_write(const struct device *dev, uint8_t reg, uint8_t byte)
{
	const struct paj7620_config *config = dev->config;

	return i2c_reg_write_byte_dt(&config->i2c, reg, byte);
}

static int paj7620_select_register_bank(const struct device *dev, enum paj7620_mem_bank bank)
{
	int ret = 0;

	if (bank > PAJ7620_MEMBANK_1) {
		LOG_ERR("Unexistent memory bank %d", (int) bank);
		return -EINVAL;
	}

	ret = paj7620_byte_write(dev, PAJ7620_REGISTER_BANK_SEL, (int)bank);
	if (ret) {
		LOG_ERR("Failed to set memory bank %d", (int) bank);
		return -EIO;
	}

	return 0;
}

static int paj7620_get_hwId(const struct device *dev, uint16_t *result)
{
	uint8_t ret = 0;
	uint8_t hwId[2] = {0, 0};

	/* Part ID is stored in bank 0 */
	ret = paj7620_select_register_bank(dev, PAJ7620_MEMBANK_0);
	if (ret) {
		return -EIO;
	}

	ret = paj7620_byte_read(dev, PAJ7620_REG_PART_ID_0, &hwId[0]);
	ret += paj7620_byte_read(dev, PAJ7620_REG_PART_ID_1, &hwId[1]);
	if (ret) {
		LOG_ERR("Failed to read hardware ID");
		return -EIO;
	}

	*result = (hwId[1] << 8 ) | (hwId[0] & 0x00FF);
	LOG_DBG("Obtained hardware ID 0x%04x", *result);

	return 0;
}

static int paj7620_write_register_array(const struct device *dev,
		                        const uint16_t *array,
					size_t array_size)
{
	int ret = 0;
	uint16_t word = 0;
	uint16_t reg_addr = 0;
	uint16_t value = 0;

	for (size_t i = 0; i < array_size; i++) {

		word = array[i];
		reg_addr = (word & 0xFF00) >> 8;
		value = (word & 0x00FF);

		ret = paj7620_byte_write(dev, reg_addr, value);
		if (ret) {
			return -EIO;
		}

		k_usleep(100);
	}

	// Go back to select bank 0
	return paj7620_select_register_bank(dev, PAJ7620_MEMBANK_0);
}


/**
 * Initializes registers for device to default values
 * The values are taken from the PAJ7620U2 v0.8 documentation and encoded
 */
static int paj7620_init_device_settings(const struct device *dev)
{
	return paj7620_write_register_array(dev, initRegisterArray, INIT_REG_ARRAY_SIZE);
}


/**
 * Initializes registers for Gesture mode and enables only the gesture interrupts
 */
static int paj7620_set_gesture_mode(const struct device *dev)
{
	return paj7620_write_register_array(dev,
			setGestureModeRegisterArray, SET_GES_MODE_REG_ARRAY_SIZE);
}

/**
 * Double check to see if user is executing a Z-axis gesture
 * Entry- and exit-time delays are executed to give time for the sensor
 * to detect the second, more complicated gesture (as lateral gestures
 * are always detected first).
 */
static int paj7620_fwd_bkwd_gesture_check(const struct device *dev,
		                          enum paj7620_gesture initial_gesture,
					  enum paj7620_gesture *return_gesture)
{
	int ret = 0;
	struct paj7620_data *data = dev->data;
	uint8_t gesture_data = 0;
	enum paj7620_gesture result = initial_gesture;

	k_msleep(data->gest_entry_time);

	ret = paj7620_byte_read(dev, PAJ7620_REG_GES_RESULT_0, &gesture_data);
	if (ret) {
		return -EIO;
	}

	if (gesture_data == GES_FORWARD_FLAG)
	{
		k_msleep(data->gest_exit_time);
		result = PAJ7620_GES_FORWARD;
	}
	else if (gesture_data == GES_BACKWARD_FLAG)
	{
		k_msleep(data->gest_exit_time);
		result = PAJ7620_GES_BACKWARD;
	}

	*return_gesture = result;
	return 0;
}

static int paj7620_read_gesture(const struct device *dev, enum paj7620_gesture *result)
{
	int ret = 0;
	struct paj7620_data *data = dev->data;
	uint8_t gest_data_reg0 = 0;
	uint8_t gest_data_reg1 = 0;

	ret = paj7620_byte_read(dev, PAJ7620_REG_GES_RESULT_0, &gest_data_reg0);

	if (ret) {
		LOG_ERR("Failed to read gesture data");
		*result = PAJ7620_GES_NONE;
		return -EIO;
	}
	else {
		switch (gest_data_reg0) {
		case PAJ7620_GES_RIGHT_FLAG:
			ret = paj7620_fwd_bkwd_gesture_check(dev, PAJ7620_GES_RIGHT, result);
			break;

		case PAJ7620_GES_LEFT_FLAG:
			ret = paj7620_fwd_bkwd_gesture_check(dev, PAJ7620_GES_LEFT, result);
			break;

		case PAJ7620_GES_UP_FLAG:
			ret = paj7620_fwd_bkwd_gesture_check(dev, PAJ7620_GES_UP, result);
			break;

		case PAJ7620_GES_DOWN_FLAG:
			ret = paj7620_fwd_bkwd_gesture_check(dev, PAJ7620_GES_DOWN, result);
			break;

		case PAJ7620_GES_FORWARD_FLAG:
			k_msleep(data->gest_exit_time);
			*result = PAJ7620_GES_FORWARD;
			break;

		case PAJ7620_GES_BACKWARD_FLAG:
			k_msleep(data->gest_exit_time);
			*result = PAJ7620_GES_BACKWARD;
			break;

		case PAJ7620_GES_CLOCKWISE_FLAG:
			*result = PAJ7620_GES_CLOCKWISE;
			break;

		case PAJ7620_GES_ANTI_CLOCKWISE_FLAG:
			*result = PAJ7620_GES_ANTICLOCKWISE;
			break;

		default:
			/* Bank 1 (Reg 0x44) has wave flag */
			ret = paj7620_byte_read(dev, PAJ7620_REG_GES_RESULT_1, &gest_data_reg0);
			if (ret == 0 && gest_data_reg1 == PAJ7620_GES_WAVE_FLAG) {
				*result = PAJ7620_GES_WAVE;
			}
			break;
		}
	}

	return ret;
}

static int paj7620_set_sampling_rate(const struct device *dev, const struct sensor_value *val)
{
	int ret = 0;
	int fps = 0;

	switch (val->val1) {
	case 120:
		fps = PAJ7620_NORMAL_SPEED;
		break;
	case 240:
		fps = PAJ7620_GAME_SPEED;
		break;
	default:
		LOG_ERR("Unsupported sample rate");
		break;
	}

	ret = paj7620_select_register_bank(dev, PAJ7620_MEMBANK_1);
	ret += paj7620_byte_write(dev, PAJ7620_REG_R_IDLE_TIME_0, fps);
	ret += paj7620_select_register_bank(dev, PAJ7620_MEMBANK_0);
	if (ret) {
		LOG_ERR("Failed to set sample rate");
		return -EIO;
	}

	LOG_DBG("Sample rate set to %s mode", fps == PAJ7620_GAME_SPEED? "game" : "normal");

	return 0;
}

static int paj7620_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct paj7620_data *data = dev->data;
	enum paj7620_gesture detected_gesture = PAJ7620_GES_NONE;

	if (chan != SENSOR_CHAN_ALL) {
		return -ENOTSUP;
	}

	/* Fetch gesture data */
	if (paj7620_read_gesture(dev, &detected_gesture)) {
		return -EIO;
	}

	data->gesture = detected_gesture;

	return 0;
}

static int paj7620_channel_get(const struct device *dev,
		               enum sensor_channel chan,
			       struct sensor_value *val)
{
	int ret = 0;
	struct paj7620_data *data = dev->data;

	switch ((uint32_t)chan) {
		case SENSOR_CHAN_PAJ7620_GESTURES:
			val->val1 = (int32_t)data->gesture;
			val->val2 = 0;
			break;
		default:
			LOG_ERR("Unsupported sensor channel");
			ret = -ENOTSUP;
			break;
	}

	return ret;
}

static int paj7620_attr_set(const struct device *dev,
			    enum sensor_channel chan,
			    enum sensor_attribute attr,
			    const struct sensor_value *val)
{
	int ret = 0;
	struct paj7620_data *data = dev->data;

	if (chan != SENSOR_CHAN_ALL)
	{
		return -ENOTSUP;
	}

	switch ((uint32_t)attr)
	{
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		ret = paj7620_set_sampling_rate(dev, val);
		break;

	case SENSOR_ATTR_PAJ7620_GESTURE_ENTRY_TIME:
		data->gest_entry_time = val->val1;
		break;

	case SENSOR_ATTR_PAJ7620_GESTURE_EXIT_TIME:
		data->gest_exit_time = val->val1;
		break;

	default:
		return -ENOTSUP;
	}

	return ret;
}

static int paj7620_init(const struct device *dev)
{
	int ret = 0;
	uint16_t hwID = 0x00;
	struct paj7620_data *data = dev->data;
	const struct paj7620_config *config = dev->config;

	LOG_DBG("Initing the PAJ7620\n");

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("I2C bus device not ready");
		return -ENODEV;
	}

	/* Reasonable timing delay values to make algorithm insensitive to
	 * hand entry and exit moves before and after detecting a gesture */
	data->gest_entry_time = PAJ7620_DEFAULT_GEST_ENTRY_TIME_MS;
	data->gest_exit_time = PAJ7620_DEFAULT_GEST_EXIT_TIME_MS;

	/* Select bank 0 to be the default. We read two times on purpose, because
	 * sometimes the PAJ7620 misses the first message as it just wakes up
	 */
	(void)paj7620_select_register_bank(dev, PAJ7620_MEMBANK_0);
	(void)paj7620_select_register_bank(dev, PAJ7620_MEMBANK_0);

	/** Verify this is not some other sensor with the same address */
	ret = paj7620_get_hwId(dev, &hwID);
	if (ret) {
		return ret;
	}

	if (hwID != PAJ7620_PART_ID) {
		LOG_ERR("Hardware ID %d does not match for PAJ7620", hwID);
		return -ENOTSUP;
	}

	/** Initialize settings and set to default gesture mode */
	ret = paj7620_init_device_settings(dev);
	if (ret) {
		LOG_ERR("Failed to initialize device registers");
		return ret;
	}

	ret = paj7620_set_gesture_mode(dev);
	if (ret) {
		LOG_ERR("Failed to set Gesture mode");
		return ret;
	}

	return 0;
}

static DEVICE_API(sensor, paj7620_driver_api) = {
	.sample_fetch = paj7620_sample_fetch,
	.channel_get = paj7620_channel_get,
	.attr_set = paj7620_attr_set,
#if CONFIG_PAJ7620_TRIGGER
	.trigger_set = paj7620_trigger_set,
#endif
};

#define PAJ7620_INIT(n) \
	static const struct paj7620_config paj7620_config_##n = { \
		.i2c = I2C_DT_SPEC_INST_GET(n),                   \
	};                                                        \
                                                                  \
	static struct paj7620_data paj7620_data_##n;              \
		                                                  \
	SENSOR_DEVICE_DT_INST_DEFINE(n,                           \
			             paj7620_init,                \
				     NULL,                        \
				     &paj7620_data_##n,           \
				     &paj7620_config_##n,         \
				     POST_KERNEL,                 \
				     CONFIG_SENSOR_INIT_PRIORITY, \
				     &paj7620_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PAJ7620_INIT);

/*
 * Copyright (c) Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_mma7660

#include "mma7660.h"
#include "zephyr/devicetree.h"
#include "zephyr/drivers/sensor.h"
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MMA7660, CONFIG_SENSOR_LOG_LEVEL);

/**
 * TRIGGERS:
 * The interrupts available to be configured through the INT pin
 * are the following (there is a single INT pin):
 * - Sensor data changes (data ready interrupt) [GINT=1]   -> SENSOR_TRIG_DATA_READY
 * - Tilt Orientation (portrait / landscape)    [FBINT,PLINT=1]  -> don't know (maybe SENSOR_TRIG_DELTA)
 * - Gesture detection (Shake)                  [SHINT#=1] -> either SENSOR_TRIG_DELTA or SENSOR_TRIG_MOTION (need to check)
 * - Gesture detection (Tap)                    [PDINT=1]  -> SENSOR_TRIG_TAP
 * - Auto-Wake / Sleep                          [ASINT=1]  -> SENSOR_TRIG_??
 *
 * ATTRIBUTES:
 * + POWER MODES:                   -> SENSOR_ATTR_CONFIGURATION?
 *   - Off
 *   - Standby
 *   - Active
 *
 * - Sampling Frequency (ODR 1 to 120 samples per sec) -> SENSOR_ATTR_SAMPLING_FREQUENCY
 * - Tap Detection Threshold
 *
 * CHANNELS:
 * + SENSOR_CHAN_ACCEL_X
 * + SENSOR_CHAN_ACCEL_Y
 * + SENSOR_CHAN_ACCEL_Z
 *
 * USER CONFIGURABLE THINGS:
 * 1. Sample Rate (AMSR[2:0] bits in SR register)
 *    |--> SENSOR_ATTR_SAMPLING_FREQUENCY
 * 2. Mode change, from Standby to Active (MODE bit in
 *    MODE register)
 * 3. Debounce Filter for tilt register (FILT[2:0] bits
 *    in SR reg)
 *    If FILT[2:0] = 000, then values in TILT are updated
 *    on every reading without further analysis.
 *    If FILT[2:0] = 001 up to 111, then values in TILT
 *    register are only updated if sensor readings are the
 *    same for 1 to 7 consecutive readings.
 *    Debounce counter is reset after every mismatch, or if
 *    TILT reg is updated.
 * 4. Tap Detection threshold (PDET register)
 *    |--> SENSOR_ATTR_SLOPE_TH
 * 5. Tap Detection threshold duration (PD register)
 *    |--> SENSOR_ATTR_SLOPE_DUR
 * 5. Interrupt Tap Detection Enable (PDINT bit in
 *    INTSU register)
 * 6. Tap Detection on X axis (XDA bit in PDET register)
 * 7. Tap Detection on Y axis (YDA bit in PDET register)
 * 8. Tap Detection on Z axis (ZDA bit in PDET register)
 * 9. Interrupt Shake Detection X axis (SHINTX bit in
 *    INTSU register)
 * 10. Interrupt Shake Detection Y axis (SHINTY bit in
 *    INTSU register)
 * 11. Interrupt Shake Detection Z axis (SHINTZ bit in
 *    INTSU register)
 * 12. Interrupt pin is Active High or Active Low
 *     ( ALREADY HANDLED IN KCONFIG)
 * 13. Interrupt pin is Open Drain or Push Pull
 *     ( ALREADY HANDLED IN KCONFIG)
 * 14. Sleep counter prescaler
 *
 * IDEAS ON HOW TO CONFIGURE ALL PROPERTIES EVEN THOSE
 * THAT DO NOT APPEAR AS SENSOR ATTRIBUTES:
 * 1. First of all, ask community on discord
 * 2. Provide independent functions in header file to
 *    access functions specific to this device to set
 *    those properties.
 * 3. Use the SENSOR_ATTR_FEATURE_MASK to enable/disable
 *    features such as interrupts and auto wake / sleep
 * 4. Use Kconfig to enable/disable features and set
 *    values for filters, etc.
 */

int mma7660_burst_read(const struct device *dev, uint8_t reg, void *data, size_t length)
{
	const struct mma7660_config *config = dev->config;

	return i2c_burst_read_dt(&config->i2c, reg, data, length);
}

int mma7660_byte_read(const struct device *dev, uint8_t reg, uint8_t *byte)
{
	const struct mma7660_config *config = dev->config;

	return i2c_reg_read_byte_dt(&config->i2c, reg, byte);
}

int mma7660_byte_write(const struct device *dev,
			    uint8_t reg,
			    uint8_t byte)
{
	const struct mma7660_config *config = dev->config;

	return i2c_reg_write_byte_dt(&config->i2c, reg, byte);
}

int mma7660_reg_field_update(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
	const struct mma7660_config *config = dev->config;

	return i2c_reg_update_byte_dt(&config->i2c, reg, mask, val);
}

/******
 * TO MODIFY REGISTERS, DEVICE MUST BE SET TO STANDBY MODE FIRST !!!
 */

int mma7660_get_power(const struct device *dev, enum mma7660_power *power)
{
	uint8_t val;

	if (mma7660_byte_read(dev, MMA7660_REG_MODE, &val)) {
		LOG_ERR("Could not get power setting");
		return -EIO;
	}

	val &= MMA7660_REG_MODE_MODE_MASK;
	*power = val;

	return 0;
}

int mma7660_set_power(const struct device *dev, enum mma7660_power power)
{
	return mma7660_reg_field_update(dev, MMA7660_REG_MODE,
				      MMA7660_REG_MODE_MODE_MASK, power);
}

static int mma7660_set_odr(const struct device *dev,
		const struct sensor_value *val,
		enum mma7660_power_mode mode)
{
	int ret;
	uint8_t odr;

	switch(val->val1) {
	case 120:
		if (mode == MMA7660_PM_SLEEP) {
			return -EINVAL;
		}
		odr = MMA7660_SR_ODR_RATE_120;
		break;
	case 64:
		if (mode == MMA7660_PM_SLEEP) {
			return -EINVAL;
		}
		odr = MMA7660_SR_ODR_RATE_64;
		break;
	case 32:
		odr = MMA7660_SR_ODR_RATE_32;
		break;
	case 16:
		odr = MMA7660_SR_ODR_RATE_16;
		break;
	case 8:
		odr = MMA7660_SR_ODR_RATE_8;
		break;
	case 4:
		if (mode == MMA7660_PM_SLEEP) {
			return -EINVAL;
		}
		odr = MMA7660_SR_ODR_RATE_4;
		break;
	case 2:
		if (mode == MMA7660_PM_SLEEP) {
			return -EINVAL;
		}
		odr = MMA7660_SR_ODR_RATE_2;
		break;
	case 1:
		odr = MMA7660_SR_ODR_RATE_1;
		break;
	default:
		return -EINVAL;
	}

	/* Update Sample Rate (SR) register */
	if (mode == MMA7660_PM_WAKE) {
		ret = mma7660_reg_field_update(dev, MMA7660_REG_SR,
				MMA7660_SR_WAKE_ODR_MASK,  odr);
	} else {
		ret = mma7660_reg_field_update(dev, MMA7660_REG_SR,
				MMA7660_SR_SLEEP_ODR_MASK,  odr << MMA7660_SR_SLEEP_FIELD_OFFSET);
	}

	if (ret != 0) {
		return ret;
	}

	LOG_DBG("Set %s ODR to 0x%02x", (mode == MMA7660_PM_WAKE)? "wake" : "sleep", odr);
	return 0;
}

static int mma7660_accel_convert(struct sensor_value *val, int16_t raw)
{
	int64_t micro_ms2;

	/* Convert to micro m/s^2. */
	micro_ms2 = raw * MMA7660_MICRO_SCALE;

	/* Separate fractional part and remove nano scale */
	val->val1 = (int32_t) micro_ms2 / 1000000;
	val->val2 = (int32_t) micro_ms2 % 1000000;

	return 0;
}

static int mma7660_get_accel_data(const struct device *dev,
		struct sensor_value *val, enum sensor_channel chan)
{
	struct mma7660_data *data = dev->data;
	int16_t *raw;

	k_sem_take(&data->sem, K_FOREVER);

	if (chan == SENSOR_CHAN_ACCEL_XYZ || chan == SENSOR_CHAN_ALL) {
		raw = &data->raw[MMA7660_CHANNEL_ACCEL_X];
		for (int i = 0; i < MMA7660_MAX_NUM_CHANNELS; i++) {
			mma7660_accel_convert(val++, *raw++);
		}
	} else {
		switch (chan) {
		case SENSOR_CHAN_ACCEL_X:
			raw = &data->raw[MMA7660_CHANNEL_ACCEL_X];
			break;
		case SENSOR_CHAN_ACCEL_Y:
			raw = &data->raw[MMA7660_CHANNEL_ACCEL_Y];
			break;
		case SENSOR_CHAN_ACCEL_Z:
			raw = &data->raw[MMA7660_CHANNEL_ACCEL_Z];
			break;
		default:
			return -ENOTSUP;
		}
		mma7660_accel_convert(val, *raw);
	}

	k_sem_give(&data->sem);

	return 0;
}

static int mma7660_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct mma7660_data *data = dev->data;
	uint8_t buf[MMA7660_MAX_NUM_BYTES];
	int ret = 0;
	int retries = MMA7660_MAX_READ_RETRIES;
        uint8_t alert_bit_set = 0;

	if (chan != SENSOR_CHAN_ALL) {
		LOG_ERR("Unsupported sensor channel");
		return -ENOTSUP;
	}

	k_sem_take(&data->sem, K_FOREVER);

	/* Read all channels in one transaction. If the Alert bit was set on
	 * one of the registers, then it was read at the same time as the device
	 * was attempting to update the contents. The register must hence be
	 * read again. We will only do this MMA7660_MAX_READ_RETRIES times.
	 */
	do {
		alert_bit_set = false;

		if (mma7660_burst_read(dev, MMA7660_REG_XOUT, buf, MMA7660_MAX_NUM_BYTES)) {
			LOG_ERR("Could not fetch accelerometer data");
			ret = -EIO;
			goto exit;
		}

		/* If Alert bit set on at least one of the registers, read all again */
		for (int i = 0; i < MMA7660_MAX_NUM_BYTES; i++) {
			alert_bit_set |= (buf[i] & MMA7660_REG_OUT_BIT_ALERT);
		}

	} while ((retries-- > 0) && alert_bit_set);

	if (alert_bit_set) {
		LOG_ERR("All register read retries failed\n");
		ret = -ETIMEDOUT;
		goto exit;
	}

	/* Save to data buffer and convert from 6bit signed to 32bit signed */
	for (int i = 0; i < MMA7660_MAX_NUM_BYTES; i++) {
		data->raw[i] = sign_extend(buf[i], MMA7660_BIT_PRECISION - 1);
	}

exit:
	k_sem_give(&data->sem);

	return ret;
}

static int mma7660_channel_get(const struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
	switch (chan) {
	case SENSOR_CHAN_ALL:
		__fallthrough;
	case SENSOR_CHAN_ACCEL_XYZ:
		__fallthrough;
	case SENSOR_CHAN_ACCEL_X:
		__fallthrough;
	case SENSOR_CHAN_ACCEL_Y:
		__fallthrough;
	case SENSOR_CHAN_ACCEL_Z:
		return mma7660_get_accel_data(dev, val, chan);
	default:
		LOG_ERR("Unsupported channel");
		return -ENOTSUP;
	}

	return 0;
}

static int mma7660_attr_set(const struct device *dev,
			     enum sensor_channel chan,
			     enum sensor_attribute attr,
			     const struct sensor_value *val)
{
	if (chan != SENSOR_CHAN_ALL)
	{
		return -ENOTSUP;
	}

	switch (attr)
	{
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		/* Sampling rate on Wake (Auto-Sleep) mode */
		return mma7660_set_odr(dev, val, MMA7660_PM_WAKE);
	case SENSOR_ATTR_SLOPE_TH:
		/* Tap detection threshold */
		break;
	case SENSOR_ATTR_SLOPE_DUR:
		/* Tap detection duration for trigger to fire */
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int mma7660_init(const struct device *dev)
{
	int ret = 0;
	const struct mma7660_config *config = dev->config;
	const struct sensor_value default_odr = {.val1 = 120, .val2 = 0};

	// TODO: Add missing attributes to set. I'll need to do a public API
	// for the sensor and add the missing sensor attributes there.

	printf("Initializing mma7660\n");

	if (!device_is_ready(config->i2c.bus)) {
		LOG_ERR("I2C bus device not ready");
		return -ENODEV;
	}

#if CONFIG_MMA7660_TRIGGER
	if (mma7660_trigger_init(dev)) {
		LOG_ERR("Could not initialize interrupts");
		return -EIO;
	}
#endif

	/* We need STANDBY before changing any regs (e.g. setting ODR) */
	ret = mma7660_set_power(dev, MMA7660_POWER_STANDBY);
	if (ret) {
		LOG_ERR("Failed to set standby power mode");
		return ret;
	}

	/* Set default ODR (Sampling freq) */
	ret = mma7660_set_odr(dev, &default_odr, MMA7660_PM_WAKE);
	if (ret) {
		LOG_ERR("Could not set default ODR. ret = %d", ret);
	}

	/* Set AutoSleep / Autowake and stuff */

	/* Set Active */
	ret = mma7660_set_power(dev, MMA7660_POWER_ACTIVE);
	if (ret) {
		LOG_ERR("Failed to set active power mode");
		return ret;
	}

	return 0;
}

static DEVICE_API(sensor, mma7660_driver_api) = {
	.sample_fetch = mma7660_sample_fetch,
	.channel_get = mma7660_channel_get,
	.attr_set = mma7660_attr_set,
#if CONFIG_MMA7660_TRIGGER
	.trigger_set = mma7660_trigger_set,
#endif
};

#define MMA7660_INIT(n) \
	static const struct mma7660_config mma7660_config_##n = {    \
		.i2c = I2C_DT_SPEC_INST_GET(n),                      \
	};                                                           \
                                                                     \
	static struct mma7660_data mma7660_data_##n;                 \
                                                                     \
	SENSOR_DEVICE_DT_INST_DEFINE(n,                              \
                                 mma7660_init,                       \
                                 NULL,                               \
                                 &mma7660_data_##n,                  \
                                 &mma7660_config_##n,                \
                                 POST_KERNEL,                        \
                                 CONFIG_SENSOR_INIT_PRIORITY,        \
                                 &mma7660_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MMA7660_INIT)

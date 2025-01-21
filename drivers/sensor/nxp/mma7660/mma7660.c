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

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c)
int mma7660_read_i2c(const struct device *dev,
		      uint8_t reg,
		      void *data,
		      size_t length)
{
	const struct mma7660_config *config = dev->config;

	return i2c_burst_read_dt(&config->i2c, reg, data, length);
}

int mma7660_byte_read_i2c(const struct device *dev,
			   uint8_t reg,
			   uint8_t *byte)
{
	const struct mma7660_config *config = dev->config;

	return i2c_reg_read_byte_dt(&config->i2c, reg, byte);
}

int mma7660_byte_write_i2c(const struct device *dev,
			    uint8_t reg,
			    uint8_t byte)
{
	const struct mma7660_config *config = dev->config;

	return i2c_reg_write_byte_dt(&config->i2c, reg, byte);
}

int mma7660_reg_field_update_i2c(const struct device *dev,
				  uint8_t reg,
				  uint8_t mask,
				  uint8_t val)
{
	const struct mma7660_config *config = dev->config;

	return i2c_reg_update_byte_dt(&config->i2c, reg, mask, val);
}

static const struct mma7660_io_ops mma7660_i2c_ops = {
	.read = mma7660_read_i2c,
	.byte_read = mma7660_byte_read_i2c,
	.byte_write = mma7660_byte_write_i2c,
	.reg_field_update = mma7660_reg_field_update_i2c,
};
#endif

/******
 * TO MODIFY REGISTERS, DEVICE MUST BE SET TO STANDBY MODE FIRST !!!
 */

int mma7660_get_power(const struct device *dev, enum mma7660_power *power)
{
	const struct mma7660_config *config = dev->config;
	uint8_t val;

	if (config->ops->byte_read(dev, MMA7660_REG_MODE, &val)) {
		LOG_ERR("Could not get power setting");
		return -EIO;
	}
	val &= MMA7660_REG_MODE_MODE_MASK;
	*power = val;

	return 0;
}

int mma7660_set_power(const struct device *dev, enum mma7660_power power)
{
	const struct mma7660_config *config = dev->config;

	return config->ops->reg_field_update(dev, MMA7660_REG_MODE,
				      MMA7660_REG_MODE_MODE_MASK, power);
}

static int mma7660_set_odr(const struct device *dev,
		const struct sensor_value *val)
{
    const struct fxos8700_config *config = dev->config;
	uint8_t dr;
	enum mma7660_power power;

    return 0;
}

static int mma7660_sample_fetch(const struct device *dev,
				                enum sensor_channel chan)
{
    return 0;
}

static int mma7660_channel_get(const struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
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
        break;
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
    const struct mma7660_config *config = dev->config;

    printf("Initializing mma7660\n");

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(i2c)
    if (!device_is_ready(config->i2c.bus)) {
        LOG_ERR("I2C bus device not ready");
        return -ENODEV;
    }
#endif

#if CONFIG_MMA7660_TRIGGER
	if (mma7660_trigger_init(dev)) {
		LOG_ERR("Could not initialize interrupts");
		return -EIO;
	}
#endif

    // Set ODR (Sampling freq)

    // Set AutoSleep / Autowake and stuff

    /* Set Active */
    if (mma7660_set_power(dev, MMA7660_POWER_ACTIVE)) {
		LOG_ERR("Could not set active");
		return -EIO;
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
        .ops = &mma7660_i2c_ops,                                 \
    };                                                           \
                                                                 \
    static struct mma7660_data mma7660_data_##n;                 \
                                                                 \
    SENSOR_DEVICE_DT_INST_DEFINE(n,                              \
                                 mma7660_init,                   \
                                 NULL,                           \
                                 &mma7660_config_##n,            \
                                 &mma7660_data_##n,              \
                                 POST_KERNEL,                    \
                                 CONFIG_SENSOR_INIT_PRIORITY,    \
                                 &mma7660_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MMA7660_INIT)

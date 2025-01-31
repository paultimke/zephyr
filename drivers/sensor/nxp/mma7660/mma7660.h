/*
 * Copyright (c) Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define MMA7660_I2C_ADDR       (0x4c)

/* The only measurement range of the sensor is -1.5g to 1.5
 * as 6 signed bits. Hence, scale in ms^2 is:
 * scale = (1.5 - (-1.5) * SENSOR_G) / (2^6 - 1) = 0.466980952
 */
#define MMA7660_NANO_SCALE       466980952UL

#define MMA7660_MAX_READ_RETRIES 5

/* Registers */
#define MMA7660_REG_XOUT       0x00
#define MMA7660_REG_YOUT       0x01
#define MMA7660_REG_ZOUT       0x02
#define MMA7660_REG_TILT       0x03
#define MMA7660_REG_SRST       0x04
#define MMA7660_REG_SPCNT      0x05
#define MMA7660_REG_INTSU      0x06
#define MMA7660_REG_MODE       0x07
#define MMA7660_REG_SR         0x08
#define MMA7660_REG_PDET       0x09
#define MMA7660_REG_PD         0x0a

#define MMA7660_REG_MODE_MODE_MASK 0x01

#define MMA7660_SR_WAKE_FIELD_OFFSET  0x00
#define MMA7660_SR_SLEEP_FIELD_OFFSET 0x03
#define MMA7660_SR_WAKE_ODR_MASK      0x07
#define MMA7660_SR_SLEEP_ODR_MASK     0x18

#define MMA7660_REG_OUT_BIT_ALERT BIT(6)

/* Interrupt Sources */
#define MMA7660_INTSRC_NONE        0x00
#define MMA7660_INTSRC_FB          0x01
#define MMA7660_INTSRC_UDLR        0x02
#define MMA7660_INTSRC_TAP         0x04
#define MMA7660_INTSRC_AUTOSLEEP   0x08
#define MMA7660_INTSRC_MEASURE     0x10
#define MMA7660_INTSRC_SHX         0x20
#define MMA7660_INTSRC_SHY         0x40
#define MMA7660_INTSRC_SHZ         0x80

/* Output data rates */
#define MMA7660_SR_ODR_RATE_120 0x00
#define MMA7660_SR_ODR_RATE_64  0x01
#define MMA7660_SR_ODR_RATE_32  0x02
#define MMA7660_SR_ODR_RATE_16  0x03
#define MMA7660_SR_ODR_RATE_8   0x04
#define MMA7660_SR_ODR_RATE_4   0x05
#define MMA7660_SR_ODR_RATE_2   0x06
#define MMA7660_SR_ODR_RATE_1   0x07

/* Tap detection slope duration */
/* Each increment of 1 in the register corresponds to
 * an increment of 0.26 ms */
#define MMA7660_PDET_SLOPE_DUR_00_52_MS 0x00
#define MMA7660_PDET_SLOPE_DUR_00_78_MS 0x02
#define MMA7660_PDET_SLOPE_DUR_01_04_MS 0x03
#define MMA7660_PDET_SLOPE_DUR_01_30_MS 0x04
#define MMA7660_PDET_SLOPE_DUR_01_56_MS 0x05
// ... up to
#define MMA7660_PDET_SLOPE_DUR_66_04_MS 0xFD
#define MMA7660_PDET_SLOPE_DUR_66_30_MS 0xFE
#define MMA7660_PDET_SLOPE_DUR_66_56_MS 0xFF

/* Number of channels */
#define MMA7660_MAX_NUM_CHANNELS   3
#define MMA7660_MAX_NUM_BYTES      (MMA7660_MAX_NUM_CHANNELS)

enum mma7660_power {
        MMA7660_POWER_STANDBY       = 0,
        MMA7660_POWER_ACTIVE,
};

enum mma7660_power_mode {
        MMA7660_PM_WAKE             = 0,
        MMA7660_PM_SLEEP,
};

enum mma7660_channel {
        MMA7660_CHANNEL_ACCEL_X     = 0,
        MMA7660_CHANNEL_ACCEL_Y,
        MMA7660_CHANNEL_ACCEL_Z,
};

struct mma7660_config {
	const struct i2c_dt_spec i2c;
#if CONFIG_MMA7660_TRIGGER
	struct gpio_dt_spec int_gpio;
#endif
	enum mma7660_power_mode power_mode;
};

struct mma7660_data {
	struct k_sem sem;
	int16_t raw[MMA7660_MAX_NUM_CHANNELS];

#ifdef CONFIG_MMA7660_TRIGGER
	const struct device *dev;
	struct gpio_callback gpio_cb;
	sensor_trigger_handler_t drdy_handler;
	const struct sensor_trigger *drdy_trig;
#endif
};

int mma7660_get_power(const struct device *dev, enum mma7660_power *power);
int mma7660_set_power(const struct device *dev, enum mma7660_power power);


/*
 * Copyright (c) Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_PAJ7620_REG_H_
#define ZEPHYR_DRIVERS_SENSOR_PAJ7620_REG_H_

#include <stdint.h>

/**
 * @file
 * @brief PAJ7620 Register addresses and values. Not all registers from the
 * data sheet are listed here, only the ones directly used by the driver.
 * For more information about the registers, refer to:
 * https://files.seeedstudio.com/wiki/Grove_Gesture_V_1.0/res/PAJ7620U2_DS_v1.5_05012022_Confidential.pdf
 */

/**
 * BANK 0 REGISTERS ADDRESSES
 */

/* Chip / Version ID */
#define PAJ7620_REG_PART_ID_LSB 0x20
#define PAJ7620_REG_PART_ID_MSB 0x76

/** Register bank select */
#define PAJ7620_REG_BANK_SEL 0xEF

/** Interrupt Controls */
/* Note: If the corresponding bit is 1, the corresponding interrupt is enabled */
#define PAJ7620_REG_MCU_INT_CTRL 0x40 /* Configure auto clean and active high/low */
#define PAJ7620_REG_INT_1_EN     0x41 /* Enable int 1 interrupts */
#define PAJ7620_REG_INT_2_EN     0x42 /* Enable int 2 interrupts */
#define PAJ7620_REG_INT_FLAG_1   0x43 /* Gesture detection results */
#define PAJ7620_REG_INT_FLAG_2   0x44 /* Gesture detection results (wave and others) */

/**
 * BANK 0 REGISTER VALUES AND MASKS
 */
#define PAJ7620_VAL_PART_ID_LSB 0x20
#define PAJ7620_VAL_PART_ID_MSB 0x76

/** Register bank select values */
#define PAJ7620_VAL_BANK_SEL_BANK_0 0x00
#define PAJ7620_VAL_BANK_SEL_BANK_1 0x01

/** Interrupt controls masks and values */
#define PAJ7620_MASK_MCU_INT_FLAG_GCLR               BIT(1)
#define PAJ7620_VAL_MCU_INT_FLAG_AUTO_CLEAN_DISABLE  0x00
#define PAJ7620_VAL_MCU_INT_FLAG_AUTO_CLEAN_ENABLE   0x01

#define PAJ7620_MASK_MCU_INT_FLAG_INV                BIT(4)
#define PAJ7620_VAL_MCU_INT_FLAG_PIN_ACTIVE_LOW      0x00
#define PAJ7620_VAL_MCU_INT_FLAG_PIN_ACTIVE_HIGH     0x01

#define PAJ7620_MASK_INT_FLAG_1_GES_UP               BIT(0)
#define PAJ7620_MASK_INT_FLAG_1_GES_DOWN             BIT(1)
#define PAJ7620_MASK_INT_FLAG_1_GES_LEFT             BIT(2)
#define PAJ7620_MASK_INT_FLAG_1_GES_RIGHT            BIT(3)
#define PAJ7620_MASK_INT_FLAG_1_GES_FORWARD          BIT(4)
#define PAJ7620_MASK_INT_FLAG_1_GES_BACKWARD         BIT(5)
#define PAJ7620_MASK_INT_FLAG_1_GES_CLOCKWISE        BIT(6)
#define PAJ7620_MASK_INT_FLAG_1_GES_COUNTERCLOCKWISE BIT(7)

#define PAJ7620_MASK_INT_FLAG_2_GES_WAVE             BIT(0)

#define PAJ7620_MASK_ALL_GESTURE_INTS_ENABLE  0xFF
#define PAJ7620_MASK_ALL_GESTURE_INTS_DISABLE 0x00

/**
 * BANK 1 REGISTER ADDRESSES
 */
#define PAJ7620_REG_R_IDLE_TIME_LSB 0x65
#define PAJ7620_REG_R_IDLE_TIME_MSB 0x66

/**
 * INITIALIZATION ARRAYS
 * The following 'initial_register_array' and 'change_to_gesture_register_array'
 * are taken from Section 8 (Firmware Guides) of the PAJ7620 datasheet v1.5.
 * They encode pairs of register addresses and values for those registers
 * needed to initialize the sensor or change its operation mode
 *
 * Reference:
 * https://files.seeedstudio.com/wiki/Grove_Gesture_V_1.0/res/PAJ7620U2_DS_v1.5_05012022_Confidential.pdf
 */

const uint8_t initial_register_array[][2] = {
	{0xEF, 0x00}, /* Select memory bank 0 */
	{0x41, 0x00}, /* Disable interrupts for first 8 gestures */
	{0x42, 0x00}, /* Disable wave and other interrupts */
	{0x37, 0x07},
	{0x38, 0x17},
	{0x39, 0x06},
	{0x42, 0x01},
	{0x46, 0x2D}, /* Setting auto exposure / auto gain settings */
	{0x47, 0x0F},
	{0x48, 0x3C},
	{0x49, 0x00},
	{0x4A, 0x1E},
	{0x4C, 0x22},
	{0x51, 0x10},
	{0x5E, 0x10},
	{0x60, 0x27},
	{0x80, 0x42}, /* Configuring GPIO0 as output, GPIO1 as input */
	{0x81, 0x44}, /* Configuring both GPIO2 and GPIO3 as input */
	{0x82, 0x04},
	{0x8B, 0x01},
	{0x90, 0x06},
	{0x95, 0x0A},
	{0x96, 0x0C},
	{0x97, 0x05},
	{0x9A, 0x14},
	{0x9C, 0x3F},
	{0xA5, 0x19},
	{0xCC, 0x19},
	{0xCD, 0x0B},
	{0xCE, 0x13},
	{0xCF, 0x64},
	{0xD0, 0x21},
	{0xEF, 0x01}, /* Select memory bank 1 */
	{0x02, 0x0F},
	{0x03, 0x10},
	{0x04, 0x02},
	{0x25, 0x01},
	{0x27, 0x39},
	{0x28, 0x7F},
	{0x29, 0x08},
	{0x3E, 0xFF},
	{0x5E, 0x3D},
	{0x65, 0x96}, /* Set sensor fps to 'normal' mode */
	{0x67, 0x97},
	{0x69, 0xCD},
	{0x6A, 0x01},
	{0x6D, 0x2C},
	{0x6E, 0x01},
	{0x72, 0x01},
	{0x73, 0x35},
	{0x74, 0x00}, /* Initialize to gesture mode */
	{0x77, 0x01},
	{0xEF, 0x00}, /* Go back to reselect memory bank 0 */
	{0x41, 0xFF}, /* Re-enable interrupts for first 8 gestures */
	{0x42, 0x01}  /* Re-enable interrupts for wave gesture */
};

const uint8_t change_to_gesture_register_array[][2] = {
	{0xEF, 0x00}, /* Select memory bank 0 */
	{0x41, 0x00}, /* Disable interrupts for first 8 gestures */
	{0x42, 0x00}, /* Disable wave and other interrupt(s) */
	{0x48, 0x3C},
	{0x49, 0x00},
	{0x51, 0x10},
	{0x83, 0x20},
	{0x9f, 0xf9},
	{0xEF, 0x01}, /* Select memory bank 1 */
	{0x01, 0x1E},
	{0x02, 0x0F},
	{0x03, 0x10},
	{0x04, 0x02},
	{0x41, 0x40},
	{0x43, 0x30},
	{0x65, 0x96}, /* Set sensor fps to 'normal' mode */
	{0x66, 0x00},
	{0x67, 0x97},
	{0x68, 0x01},
	{0x69, 0xCD},
	{0x6A, 0x01},
	{0x6b, 0xb0},
	{0x6c, 0x04},
	{0x6D, 0x2C},
	{0x6E, 0x01},
	{0x74, 0x00}, /* Set gesture mode */
	{0xEF, 0x00}, /* Go back to select memory bank 0 */
	{0x41, 0xFF}, /* Re-enable interrupts for first 8 gestures */
	{0x42, 0x01}  /* Re-enable interrupts for wave gesture */
};

#endif /* ZEPHYR_DRIVERS_SENSOR_PAJ7620_REG_H_ */

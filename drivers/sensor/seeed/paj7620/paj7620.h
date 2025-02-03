/*
 * Copyright (c) Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
  Description: This demo can recognize 9 gestures and output the result,
        including move up, move down, move left, move right,
        move forward, move backward, circle-clockwise,
        circle-anti (counter) clockwise, and wave.

  PAJ7620U2 Sensor data sheet for reference found here:
    https://datasheetspdf.com/pdf-file/1309990/PixArt/PAJ7620U2/1

  Driver sources, latest code, and authors available at:
    https://github.com/acrandal/RevEng_PAJ7620
*/

#ifndef __PAJ7620_H__
#define __PAJ7620_H__

#include "paj7620_reg.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#if CONFIG_PAJ7620_TRIGGER
#include <zephyr/drivers/gpio.h>
#endif

/** DEVICE'S I2C ID - defined by manufacturer */
#define PAJ7620_I2C_BUS_ADDR             0x73

/** Device's hard coded Hardware ID values */
#define PAJ7620_PART_ID_LSB              0x20
#define PAJ7620_PART_ID_MSB              0x76

/** Register bank select address */
#define PAJ7620_REGISTER_BANK_SEL        0xEF  // W

/** Register bank IDs */
#define PAJ7620_BANK_0			 0x00
#define PAJ7620_BANK_1                   0x01

/** Suspend Control commands
* Written to #PAJ7620_ADDR_SUSPEND_CMD */
#define PAJ7620_CMD_WAKEUP               0x01
#define PAJ7620_CMD_SUSPEND              0x00

/** Enable Control commands */
/* Written to #PAJ7620_ADDR_OPERATION_ENABLE */
#define PAJ7620_CMD_ENABLE               0x01 /* Enable to start reading */
#define PAJ7620_CMD_DISABLE              0x00 /* Disable and stop reading */

/** Sensor FPS mode values for R_IDLE_TIME
 * Written to #PAJ7620_ADDR_R_IDLE_TIME_0
 * \note These values come directly from PixArt contact/email
 */
#define PAJ7620_NORMAL_SPEED             0xAC /* Normal speed 120 fps */
#define PAJ7620_GAME_SPEED               0x30 /* Game Mode speed 240 fps */

/** Gesture Bit Masks
 * Return values from gesture I2C memory reads in Bank 0 - 0x43 & 0x44
 * A set bit means that gesture has been detected
 *
 * \see #PAJ7620_ADDR_GES_RESULT_0 (all except wave flag)
 * \see #PAJ7620_ADDR_GES_RESULT_1 (wave flag gesture)
 */
#define GES_UP_FLAG                      0x01
#define GES_DOWN_FLAG                    0x02
#define GES_LEFT_FLAG                    0x04
#define GES_RIGHT_FLAG                   0x08
#define GES_FORWARD_FLAG                 0x10
#define GES_BACKWARD_FLAG                0x20
#define GES_CLOCKWISE_FLAG               0x40
#define GES_ANTI_CLOCKWISE_FLAG          0x80
#define GES_WAVE_FLAG                    0x01

/** Return values for cursor interrupt/status for cursor mode */
/*  Read from Bank 0, reg 0x44 */
#define CUR_HAS_OBJECT                   0x04      // Bit 2 - 0000 0100
#define CUR_NO_OBJECT                    0x80      // Bit 7 - 1000 0000

/** Values for Corners mode */
#define GESTURE_RANGE_MAX          3712      /* Gesture range max value (experimental) */
#define GESTURE_RANGE_MIN          0         /* Gesture range min value */
#define GESTURE_RANGE_MID          ((GESTURE_RANGE_MAX - GESTURE_RANGE_MIN) / 2)
#define CORNERS_BUFFER_WIDTH_PCT   0.20      /* 20% of range is "buffer"/"middle" */
#define CORNERS_BUFFER_WIDTH       (int)((GESTURE_RANGE_MAX - GESTURE_RANGE_MIN) * CORNERS_BUFFER_WIDTH_PCT)
#define CORNERS_BUFFER_LOWER       (int)(GESTURE_RANGE_MID - (CORNERS_BUFFER_WIDTH / 2))
#define CORNERS_BUFFER_UPPER       (int)(GESTURE_RANGE_MID + (CORNERS_BUFFER_WIDTH / 2))

/** Gesture Entry/Exit default values in milliseconds */
#define PAJ7620_DEFAULT_GEST_ENTRY_TIME_MS 0
#define PAJ7620_DEFAULT_GEST_EXIT_TIME_MS  200

/** Generated size of the register init array */
#define INIT_REG_ARRAY_SIZE        (sizeof(initRegisterArray)/sizeof(initRegisterArray[0]))

/**
 * Initial device register addresses and values.
 * \note Puts device into gesture mode with various "normal" mode values.
 * \note Values taken from PixArt reference documentation v0.8 & v1.0 - see <a href="https://github.com/acrandal/RevEng_PAJ7620/wiki">wiki</a> for files
 */
const uint16_t initRegisterArray[] = {
    0xEF00,       // Bank 0
    0x4100,       // Disable interrupts for first 8 gestures
    0x4200,       // Disable wave (and other modes') interrupt(s)
    0x3707,
    0x3817,
    0x3906,
    0x4201,
    0x462D,
    0x470F,
    0x483C,
    0x4900,
    0x4A1E,
    0x4C22,
    0x5110,
    0x5E10,
    0x6027,
    0x8042,
    0x8144,
    0x8204,
    0x8B01,
    0x9006,
    0x950A,
    0x960C,
    0x9705,
    0x9A14,
    0x9C3F,
    0xA519,
    0xCC19,
    0xCD0B,
    0xCE13,
    0xCF64,
    0xD021,
    0xEF01,       // Bank 1
    0x020F,
    0x0310,
    0x0402,
    0x2501,
    0x2739,
    0x287F,
    0x2908,
    0x3EFF,
    0x5E3D,
    0x6596,       // R_IDLE_TIME LSB - Set sensor speed to 'normal speed' - 120 fps
    0x6797,
    0x69CD,
    0x6A01,
    0x6D2C,
    0x6E01,
    0x7201,
    0x7335,
    0x7400,       // Set to gesture mode
    0x7701,
    0xEF00,       // Bank 0
    0x41FF,       // Re-enable interrupts for first 8 gestures
    0x4201        // Re-enable interrupts for wave gesture
};

/** Generated size of the register set gesture mode array */
#define SET_GES_MODE_REG_ARRAY_SIZE (sizeof(setGestureModeRegisterArray)/sizeof(setGestureModeRegisterArray[0]))

/**
 * Gesture mode specific register addresses and values
 * \note Puts device into gesture mode with appropriate values.
 * \note Values taken from PixArt reference documentation v0.8 & v1.0 - see <a href="https://github.com/acrandal/RevEng_PAJ7620/wiki">wiki</a> for files
 */
const uint16_t setGestureModeRegisterArray[] = {
    0xEF00,       // Bank 0
    0x4100,       // Disable interrupts for first 8 gestures
    0x4200,       // Disable wave (and other mode's) interrupt(s)
    0x483C,
    0x4900,
    0x5110,
    0x8320,
    0x9ff9,
    0xEF01,       // Bank 1
    0x011E,
    0x020F,
    0x0310,
    0x0402,
    0x4140,
    0x4330,
    0x6596,       // R_IDLE_TIME  - Normal mode LSB "120 fps" (supposedly)
    0x6600,
    0x6797,
    0x6801,
    0x69CD,
    0x6A01,
    0x6bb0,
    0x6c04,
    0x6D2C,
    0x6E01,
    0x7400,       // Set gesture mode
    0xEF00,       // Bank 0
    0x41FF,       // Re-enable interrupts for first 8 gestures
    0x4201        // Re-enable interrupts for wave gesture
};

/** Generated size of the register set cursor mode array */
#define SET_CURSOR_MODE_REG_ARRAY_SIZE (sizeof(setCursorModeRegisterArray)/sizeof(setCursorModeRegisterArray[0]))

/**
 * Cursor mode specific register addresses and values
 * \note Puts device into cursor mode with reasonable basic values.
 * \note Values taken from PixArt reference documentation v0.8 & v1.0 - see <a href="https://github.com/acrandal/RevEng_PAJ7620/wiki">wiki</a> for files
 */
const unsigned short setCursorModeRegisterArray[] = {
    0xEF00,   // Set Bank 0
    0x3229,   // Default  29  [0] Cursor use top - def 1
              //              [1] Cursor Use BG Model - def 0
              //              [2] Cursor Invert Y - def 0       -- Not sure, doesn't seem to work
              //              [3] Cursor Invert X - def 1
              //              [5:4] Cursor top Ratio - def 0x2
    0x3301,   // Default  01  R_PositionFilterStartSizeTh [7:0]
    0x3400,   // Default  00  R_PositionFilterStartSizeTh [8]
    0x3501,   // Default  01  R_ProcessFilterStartSizeTh [7:0]
    0x3600,   // Default  00  R_ProcessFilterStartSizeTh [8]
    0x3703,   // Default  09  R_CursorClampLeft [4:0]
    0x381B,   // Default  15  R_CursorClampRight [4:0]
    0x3903,   // Default  0A  R_CursorClampUp [4:0]
    0x3A1B,   // Default  12  R_CursorClampDown [4:0]
    0x4100,   // Interrupt enable mask - Should be 00 (disable gestures)
              //              All gesture flags [7:0]
    0x4284,   // Interrupt enable mask - Should be 84 (0b 1000 0100)
              //              bit 0: Wave, wave mode use only
              //              bit 1: Proximity, proximity mode use only
              //              bit 2: Has Object, cursor mode use only
              //              bit 3: Wake up trigger, trigger mode use only
              //              bit 4: Confirm, confirm mode use only
              //              bit 5: Abort, confirm mode use only
              //              bit 6: N/A
              //              bit 7:No Object, cursor mode use only
    0x8B01,   // Default  10  R_Cursor_ObjectSizeTh [7:0]
    0x8C07,   // Default  07  R_PositionResolution [2:0]
    0xEF01,   // Set Bank 1
    0x0403,   // Invert X&Y Axes in lens for GUI coordinates
              //  Where (0,0) is in upper left, positive down (Y) and right (X)
    0x7403,   // Enable cursor mode 0 - gesture, 3 - cursor, 5 - proximity
    0xEF00    // Set Bank 0 (parking it)
};

enum paj7620_gesture {
	GES_NONE = 0,      /* No gesture */
	GES_UP,            /* Upwards gesture */
	GES_DOWN,	   /* Downward gesture */
	GES_LEFT,          /* Leftward gesture */
	GES_RIGHT,         /* Rightward gesture */
	GES_FORWARD,       /* Forward gesture */
	GES_BACKWARD,      /* Backward gesture */
	GES_CLOCKWISE,     /* Clockwise circular gesture */
	GES_ANTICLOCKWISE, /* Anticlockwise circular gesture */
	GES_WAVE           /* Wave gesture */
};

/** Used for selecting PAJ7620 memory bank to read/write from */
enum paj7620_mem_bank {
	PAJ7620_MEMBANK_0 = PAJ7620_BANK_0,
	PAJ7620_MEMBANK_1 = PAJ7620_BANK_1
};

/** Used for reading the corners in corners mode and PIN mode
 * Note: Width of "middle" set by CORNERS_BUFFER_WIDTH_PCT value */
// TODO: Check if it's possible to separate this enum into two
enum paj7620_corner {
	PAJ7620_CORNER_NONE = 0,      /* No object in view */
	PAJ7620_CORNER_NE = 1,        /* Object in NE quadrant */
	PAJ7620_CORNER_NW = 2,        /* Object in NW quadrant */
	PAJ7620_CORNER_SW = 3,        /* Object in SW quadrant */
	PAJ7620_CORNER_SE = 4,        /* Object in SE quadrant */
	PAJ7620_CORNER_MIDDLE = 5,    /* Object in between quadrants */
	PAJ7620_QUADRANT_NONE = 0,    /* No object in view */
	PAJ7620_QUADRANT_I = 1,       /* Object in cartesian quadrant I (NE) */
	PAJ7620_QUADRANT_II = 2,      /* Object in cartesian quadrant II (NW) */
	PAJ7620_QUADRANT_III = 3,     /* Object in cartesian quadrant III (SW) */
	PAJ7620_QUADRANT_IV = 4,      /* Object in cartesian quadrant IV (SE) */
};

struct paj7620_config {
	const struct i2c_dt_spec i2c;
#if CONFIG_PAJ7620_TRIGGER
	struct gpio_dt_spec int_gpio;
#endif
};

struct paj7620_data {
	struct k_sem sem;
	uint64_t gest_entry_time; /* User set gesture entry delay in ms (default: 0) */
	uint64_t gest_exit_time;  /* User set gesture exit delay in ms (default: 200) */

#ifdef CONFIG_PAJ7620_TRIGGER
	const struct device *dev;
	struct gpio_callback gpio_cb;
	sensor_trigger_handler_t drdy_handler;
	const struct sensor_trigger *drdy_trig;
#endif
};

#endif /* __PAJ7620_H__ */

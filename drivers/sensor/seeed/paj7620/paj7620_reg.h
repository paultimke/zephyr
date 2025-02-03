/*
 * Copyright (c) Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PAJ7620_REG_H_
#define _PAJ7620_REG_H_

/** REGISTER BANK 0 */
/*  Addresses used within register bank 0 */

#define PAJ7620_REG_PART_ID_0            0x00  // R
#define PAJ7620_REG_PART_ID_1            0x01  // R
#define PAJ7620_REG_SUSPEND_CMD          0x03  // W
#define PAJ7620_REG_GES_PS_DET_MASK_0    0x41  // RW
#define PAJ7620_REG_GES_PS_DET_MASK_1    0x42  // RW
#define PAJ7620_REG_GES_PS_DET_FLAG_0    0x43  // R
#define PAJ7620_REG_GES_PS_DET_FLAG_1    0x44  // R
#define PAJ7620_REG_STATE_INDICATOR      0x45  // R
#define PAJ7620_REG_PS_HIGH_THRESHOLD    0x69  // RW
#define PAJ7620_REG_PS_LOW_THRESHOLD     0x6A  // RW
#define PAJ7620_REG_PS_APPROACH_STATE    0x6B  // R
#define PAJ7620_REG_PS_RAW_DATA          0x6C  // R
#define PAJ7620_REG_OBJECT_CENTER_X_LSB  0xAC  // R [7:0]
#define PAJ7620_REG_OBJECT_CENTER_X_MSB  0xAD  // R [4:0]
#define PAJ7620_REG_OBJECT_CENTER_Y_LSB  0xAE  // R [7:0]
#define PAJ7620_REG_OBJECT_CENTER_Y_MSB  0xAF  // R [4:0]
#define PAJ7620_REG_OBJECT_BRIGHTNESS    0xB0  // R [7:0]
#define PAJ7620_REG_OBJECT_SIZE_LSB      0xB1  // R [7:0]
#define PAJ7620_REG_OBJECT_SIZE_MSB      0xB2  // R [3:0]
#define PAJ7620_REG_WAVE_COUNT           0xB7  // R [4:0]
#define PAJ7620_REG_NO_OBJECT_COUNT      0xB8  // R
#define PAJ7620_REG_NO_MOTION_COUNT      0xB9  // R
#define PAJ7620_REG_OBJECT_VEL_X_LSB     0xC3  // R [7:0]
#define PAJ7620_REG_OBJECT_VEL_X_MSB     0xC4  // R [3:0]
#define PAJ7620_REG_OBJECT_VEL_Y_LSB     0xC5  // R [7:0]
#define PAJ7620_REG_OBJECT_VEL_Y_MSB     0xC6  // R [3:0]
#define PAJ7620_REG_GES_RESULT_0         0x43  // R
#define PAJ7620_REG_GES_RESULT_1         0x44  // R

/** Cursor Registers - Bank 0 */
#define PAJ7620_REG_CURSOR_X_LOW         0x3B  // R
#define PAJ7620_REG_CURSOR_X_HIGH        0x3C  // R
#define PAJ7620_REG_CURSOR_Y_LOW         0x3D  // R
#define PAJ7620_REG_CURSOR_Y_HIGH        0x3E  // R
#define PAJ7620_REG_CURSOR_INT           0x44  // R

/* Proximity Registers - Bank 0 */
/*  Only available in Proximity Detection (PS) mode */
#define PAJ7620_REG_PS_APPROACH_STATE    0x6B // R (Single bit - Approach == 1, Not approach == 0)
#define PAJ7620_REG_S_AVE_Y_BRIGHTNESS   0x6C // R (255 is near, lower is further)

/** REGISTER BANK 1 */
/*  Addresses used within register bank 1 */
#define PAJ7620_REG_PS_GAIN              0x44  // RW
#define PAJ7620_REG_R_IDLE_TIME_0        0x65  // RW
#define PAJ7620_REG_R_IDLE_TIME_1        0x66  // RW
#define PAJ7620_REG_IDLE_S1_STEP_0       0x67  // RW
#define PAJ7620_REG_IDLE_S1_STEP_1       0x68  // RW
#define PAJ7620_REG_IDLE_S2_STEP_0       0x69  // RW
#define PAJ7620_REG_IDLE_S2_STEP_1       0x6A  // RW
#define PAJ7620_REG_OP_TO_S1_STEP_0      0x6B  // RW
#define PAJ7620_REG_OP_TO_S1_STEP_1      0x6C  // RW
#define PAJ7620_REG_OP_TO_S2_STEP_0      0x6D  // RW
#define PAJ7620_REG_OP_TO_S2_STEP_1      0x6E  // RW
#define PAJ7620_REG_OPERATION_ENABLE     0x72  // RW

/* Cursor Registers - Bank 1 */
#define PAJ7620_REG_LENS_ORIENTATION     0x04  // RW

#endif /* _PAJ7620_REG_H_ */

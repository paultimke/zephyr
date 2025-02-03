/*
 * Copyright (c) 2024 Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Extended Public API for PAJ7620 sensor
 *
 * Some capabilities of the sensor cannot be expressed
 * within the sensor driver abstraction
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_PAJ7620_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_PAJ7620_H_

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <zephyr/drivers/sensor.h>

enum sensor_attribute_paj7620 {
	/**
	 * Time in milliseconds (ms) from which a first gesture is detected
	 * to give time to detect a second intended gesture.
	 * The interrupt pin will go high when a gesture is first detected, but
	 * if the user is trying to move their hand to do a Z-axis gesture
	 * (backward, forward), they will first trigger a lateral (up, down,
	 * left, right) gesture, which will immediately raise the interrupt.
	 */
	SENSOR_ATTR_PAJ7620_GESTURE_ENTRY_TIME = SENSOR_ATTR_PRIV_START,

	/** Time in milliseconds (ms) for exiting the sensor's field of view */
	SENSOR_ATTR_PAJ7620_GESTURE_EXIT_TIME,
};


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_PAJ7620_H_ */

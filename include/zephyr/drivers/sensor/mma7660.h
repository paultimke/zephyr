/*
 * Copyright (c) 2024 Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_MMA7660_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_MMA7660_H_

#include <zephyr/drivers/sensor.h>

enum sensor_attribute_mma7660 {
	/* Offset temperature: Toffset_actual = Tscd4x – Treference + Toffset_previous
	 * 0 - 20°C
	 */
	SENSOR_ATTR_SCD4X_TEMPERATURE_OFFSET = SENSOR_ATTR_PRIV_START,
	/* Altidude of the sensor;
	 * 0 - 3000m
	 */
	SENSOR_ATTR_SCD4X_SENSOR_ALTITUDE,
	/* Ambient pressure in hPa
	 * 700 - 1200hPa
	 */
	SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE,
	/* Set the current state (enabled: 1 / disabled: 0).
	 * Default: enabled.
	 */
	SENSOR_ATTR_SCD4X_AUTOMATIC_CALIB_ENABLE,
	/* Set the initial period for automatic self calibration correction in hours. Allowed values
	 * are integer multiples of 4 hours.
	 * Default: 44
	 */
	SENSOR_ATTR_SCD4X_SELF_CALIB_INITIAL_PERIOD,
	/* Set the standard period for automatic self calibration correction in hours. Allowed
	 * values are integer multiples of 4 hours. Default: 156
	 */
	SENSOR_ATTR_SCD4X_SELF_CALIB_STANDARD_PERIOD,
};


#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_MMA7660_H_ */

/*
 * Copyright (c) Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
  Description: This driver class can recognize 9 gestures and output the result,
        including move up, move down, move left, move right,
        move forward, move backward, circle-clockwise,
        circle-anti (counter) clockwise, and wave.
        The driver also allows changing the sensor to 'cursor mode' where it
        tracks the closest object in view on an (X,Y) coordinate system.

  PAJ7620U2 Sensor data sheet for reference found here:
    https://datasheetspdf.com/pdf-file/1309990/PixArt/PAJ7620U2/1

  Driver sources, latest code, and authors available at:
    https://github.com/acrandal/RevEng_PAJ7620
*/

#include "paj7620.h"
#include "paj7620_reg.h"
#include "zephyr/drivers/i2c.h"
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(PAJ7620, CONFIG_SENSOR_LOG_LEVEL);

static int paj7620_burst_read(const struct device *dev, uint8_t reg, void *data, size_t length)
{
	const struct paj7620_config *config = dev->config;

	return i2c_burst_read_dt(&config->i2c, reg, data, length);
}

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


/**
 * Select memory bank to read/write to
 * The PAJ7620 has two memory banks. The user must select which bank to use
 * when reading and writing over I2C.
 * Note: This driver defaults to operations resetting to BANK0 for general operation.
 */
static int paj7620_select_register_bank(const struct device *dev, enum paj7620_mem_bank bank)
{
	int ret = 0;

	if (bank > PAJ7620_MEMBANK_1) {
		LOG_ERR("Unexistent memory bank %d", (int) bank);
		return -ENOTSUP;
	}

	ret = paj7620_byte_write(dev, PAJ7620_REGISTER_BANK_SEL, (int)bank);
	if (ret) {
		LOG_ERR("Failed to set memory bank %d", (int) bank);
		return -EIO;
	}

	return 0;
}


/**
 * Reads device memory to check for the PAJ7620 hardware identifier (ID)
 * At memory address BANK0, 0x00 the device returns 0x20.
 * At memory address BANK0, 0x01 the device returns 0x76.
 * If this is not true, a non-PAJ7620 I2C device is attached at this I2C address.
 */
static bool paj7620_is_hwId_correct(const struct device *dev)
{
	uint8_t ret = 0;
	uint8_t hwId[2] = {0, 0};

	/* Device ID is stored in BANK0 */
	ret = paj7620_select_register_bank(dev, PAJ7620_MEMBANK_0);
	if (ret) {
		LOG_ERR("Failed to select register bank");
		return false;
	}

	ret = paj7620_byte_read(dev, PAJ7620_REG_PART_ID_0, &hwId[0]);
	ret += paj7620_byte_read(dev, PAJ7620_REG_PART_ID_1, &hwId[1]);
	if (ret) {
		LOG_ERR("Failed to read hardware ID");
		return false;
	}

	/* Verify part ID is corect for PAJ7620U2 */
	if ((hwId[0] != PAJ7620_PART_ID_LSB ) || (hwId[1] != PAJ7620_PART_ID_MSB)) {
		LOG_ERR("Read Hardware ID incorrect for PAJ7620");
		return false;
	}

	return true;
}


/**
 * Writes an array of values to the device memory
 *
 * \par
 * Writes over I2C to the memory banks a set of default values for operation.
 * The values are taken from the PAJ7620U2 v0.8 documentation and encoded
 * in the \link initRegisterArray \endlink from the driver's header file
 *
 * \note Expects array[] to be stored in PROGMEM if it is available on your microcontroller
 *
 * \param array : array of const unsigned shorts - first byte is address, second byte is data
 * \param arraySize : quantity of elements in array to write
 * \return none
 */
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
	}

	// Guarantee to be in bank 0
	paj7620_select_register_bank(dev, PAJ7620_MEMBANK_0);
}


/**
 * Initializes registers for device to default values
 * Writes over I2C to the memory banks a set of default values for operation.
 * The values are taken from the PAJ7620U2 v0.8 documentation and encoded
 * in the \link initRegisterArray \endlink from the driver's header file
 */
static int paj7620_init_device_settings(const struct device *dev)
{
	return paj7620_write_register_array(dev, initRegisterArray, INIT_REG_ARRAY_SIZE);
}


/**
 * Puts device into Gesture mode
 * Initializes registers for Gesture mode and enables only the gesture interrupts
 */
static int paj7620_set_gesture_mode(const struct device *dev)
{
	return paj7620_write_register_array(dev,
			setGestureModeRegisterArray, SET_GES_MODE_REG_ARRAY_SIZE);
}


/**
 * Puts device into Cursor mode
 * Initializes registers for Cursor mode and enables only the cursor interrupts
 */
static int paj7620_set_cursor_mode(const struct device *dev)
{
	return paj7620_write_register_array(dev,
			setCursorModeRegisterArray, SET_CURSOR_MODE_REG_ARRAY_SIZE);
}


/**
 * Gets cursor object's current X location
 *
 * \note Only works in cursor mode
 * \param none
 * \return int : X coordinate of cursor
 */
static int paj7620_getCursorX(const struct device *dev, int32_t *result)
{
	int ret = 0;
	int32_t tmp = 0;
	uint8_t cursor_x[2] = {0x00, 0x00};

	ret = paj7620_byte_read(dev, PAJ7620_REG_CURSOR_X_LOW, &cursor_x[0]);
	ret = paj7620_byte_read(dev, PAJ7620_REG_CURSOR_X_HIGH, &cursor_x[1]);
	if (ret) {
		return -EIO;
	}

	cursor_x[1] &= 0x0F;      // Mask off high bits (unused)
	tmp |= cursor_x[1];
	tmp = tmp << 8;
	tmp |= cursor_x[0];

	*result = tmp;

	return 0;
}


/**
 * Gets cursor object's current Y location
 *
 * \note Only works in cursor mode
 * \param none
 * \return int : Y coordinate of cursor
 */
int paj7620_getCursorY()
{
	/*
  int result = 0;
  uint8_t data0 = 0x00;
  uint8_t data1 = 0x00;

  readRegister(PAJ7620_ADDR_CURSOR_Y_LOW, 1, &data0);
  readRegister(PAJ7620_ADDR_CURSOR_Y_HIGH, 1, &data1);
  data1 &= 0x0F;      // Mask off high bits (unused)
  result |= data1;
  result = result << 8;
  result |= data0;

  return result;
  */
}


/**
 * Returns whether an object is in view as a cursor
 *
 * \note Only works in cursor mode
 * \param none
 * \return bool : True if object in view, False if no object in view
 */
bool paj7620_isCursorInView()
{
	/*
  bool result = false;
  uint8_t data = 0x00;
  readRegister(PAJ7620_ADDR_CURSOR_INT, 1, &data);
  switch(data)
  {
    case CUR_NO_OBJECT:   result = false;   break;
    case CUR_HAS_OBJECT:  result = true;    break;
    default:              result = false;   break;
  }
  return result;
  */
}


/**
 * Inverts the X (horizontal) axis
 *
 * \par
 * Allows you to choose the orientation of your coordinate system.
 * In all modes, the X axis is inverted. Left becomes Right, etc.
 * For cursor mode, the X values will flip
 *
 * \param none
 * \return none
 */
void paj7620_invertXAxis()
{
	/*
  uint8_t data = 0x00;
  selectRegisterBank(BANK1);
  readRegister(PAJ7620_ADDR_LENS_ORIENTATION, 1, &data);
  data ^= 1UL << 0;               // Bit[0] controls X axis
  writeRegister(PAJ7620_ADDR_LENS_ORIENTATION, data);
  selectRegisterBank(BANK0);
  */
}


/**
 * Inverts the Y (vertical) axis
 *
 * \par
 * Allows you to choose the orientation of your coordinate system.
 * In all modes, the Y axis is inverted. Up becomes Down, etc.
 * For cursor mode, the Y values will flip
 *
 * \param none
 * \return none
 */
void paj7620_invertYAxis()
{
	/*
  uint8_t data = 0x00;
  selectRegisterBank(BANK1);
  readRegister(PAJ7620_ADDR_LENS_ORIENTATION, 1, &data);
  data ^= 1UL << 1;                 // Bit[1] controls Y axis
  writeRegister(PAJ7620_ADDR_LENS_ORIENTATION, data);
  selectRegisterBank(BANK0);
  */
}


/**
 * Disables sensor for reading & interrupts
 * \note This is the light disable state, not the full I2C shutdown state
 * \param none
 * \return none
 */
void paj7620_disable()
{
	/*
  selectRegisterBank(BANK1);
  writeRegister(PAJ7620_ADDR_OPERATION_ENABLE, PAJ7620_DISABLE);
  selectRegisterBank(BANK0);
  */
}


/**
 * Enables sensor for reading & interrupts
 *
 *  \param none
 *  \return none
 */
void paj7620_enable()
{
	/*
  selectRegisterBank(BANK1);
  writeRegister(PAJ7620_ADDR_OPERATION_ENABLE, PAJ7620_ENABLE);
  selectRegisterBank(BANK0);
  */
}

/**
 * Sets time sensor waits between getGesture call to reading gesture from sensor
 * \par
 *  This time is most important in hardware interrupt driven use of the driver.
 *  The PAJ7620's interrupt pin will raise when a gesture is first recognized.
 *  If the user is trying to move their hand to do a Backward gesture, they will
 *  first trip a lateral (up, down, left, right) gesture, which will immediately
 *  raise the interrupt.
 *  By increasing this value, the user shall have more time to reach in and complete
 *  their intended gesture before the interrupt is handled.
 * \note Default value for entry time is 0
 * \param newGestureEntryTime : milliseconds (ms) for delay
 * \return none
 */
void paj7620_setGestureEntryTime(unsigned long newGestureEntryTime)
{
  gestureEntryTime = newGestureEntryTime;
}


/**
 * Sets time sensor waits during getGesture() after gesture value read
 * \par
 *  This value represents the time the user has to exit the sensor's field of view
 *  before the next gesture might be read, which is most important in the Z axis gestures
 *  (forward and backward).
 *  Setting this lower makes the driver delay less so the main program can control
 *  more of the global timing, but puts responsibility on the coder to take this higher
 *  sensitivity into account.
 * \note Default value for exit time is 200
 * \param newGestureEntryTime : milliseconds (ms) for delay
 * \return none
 */
void paj7620_setGestureExitTime(unsigned long newGestureExitTime)
{
  gestureExitTime = newGestureExitTime;
}


/**
 * Set sensor to "game mode" sampling speed of 240fps
 * \note Value of 0x30 for setting comes from PixArt contact
 *
 * \param none
 * \return none
 */
void paj7620_setGameSpeed()
{
  selectRegisterBank(BANK1);
  writeRegister(PAJ7620_ADDR_R_IDLE_TIME_0, PAJ7620_GAME_SPEED);
  selectRegisterBank(BANK0);
}


/**
 * Set sensor to "normal" sampling speed of 120fps
 * \note Value of 0xAC for setting comes from PixArt contact
 *
 * \param none
 * \return none
 */
/*
void paj7620_setNormalSpeed()
{
  selectRegisterBank(BANK1);
  writeRegister(PAJ7620_ADDR_R_IDLE_TIME_0, PAJ7620_NORMAL_SPEED);
  selectRegisterBank(BANK0);
}
*/


/**
 * Clear current gesture interrupt vectors without returning gesture value
 * Note: The gesture interrupt vectors are reset in hardware after any reads
 */
static void paj7620_clear_gesture_interrupts(void)
{
	int ret = 0;
	uint8_t gesture_data[2];

	ret = paj7620_byte_read(dev, PAJ7620_ADDR_GES_RESULT_0, &gesture_data[0]);
	ret += paj7620_byte_read(dev, PAJ7620_ADDR_GES_RESULT_1, &gesture_data[1]);
	if (ret) {
		return -EIO;
	}

	return 0;
}


/**
 * Get current count of waves by user
 * \param none
 * \return int : current count of "waves" over the sensor
 */
int paj7620_getWaveCount()
{
  uint8_t waveCount = 0;
  readRegister(PAJ7620_ADDR_WAVE_COUNT, 1, &waveCount);
  waveCount &= 0x0F;      // Count is [3:0] bits - values in 0..15
  return waveCount;
}


/**
 * Double check to see if user is executing a Z-axis gesture
 * This is where the gesture entry- and exit-time delays are executed
 * to buffer high speed polling & return against human gesture speeds.
 */
static int paj7620_fwd_bkwd_gesture_check(const struct device *dev,
		                          enum paj7620_gesture initial_gesture
					  enum paj7620_gesture *return_gesture)
{
	int ret = 0;
	struct paj7620_data *data = dev->data;
	uint8_t gesture_data = 0;
	paj7620_gesture result = initial_gesture;

	k_msleep(data->gest_entry_time);

	ret = paj7620_burst_read(dev, PAJ7620_ADDR_GES_RESULT_0, &gesture_data, 1);
	if (ret) {
		return -EIO;
	}

	if (gesture_data == GES_FORWARD_FLAG)
	{
		k_msleep(data->gest_exit_time);
		result = GES_FORWARD;
	}
	else if (data1 == GES_BACKWARD_FLAG)
	{
		k_msleep(data->gest_exit_time);
		result = GES_BACKWARD;
	}

	*return_gesture = result;
	return 0;
}

/**
 * Reads the latest gesture from the device
 *
 * \par
 *  This is the central method for reading and calculating the main 9 gestures
 *  the PAJ7620 can recognize. It returns a Gesture enum with the read gesture,
 *  which can by GES_NONE if no gesture was currently found.
 * \note Clears interrupt vector of gestures when called
 * \param none
 * \return \link Gesture \endlink found or \link GES_NONE Gesture::GES_NONE \endlink if no gesture found
 */
static int paj7620_read_gesture(const struct device *dev, enum paj7620_gesture *result)
{
	struct paj7620_data *data = dev->data;

	int ret = 0;
	uint8_t gest_data_reg0 = 0;
	uint8_t gest_data_reg1 = 0;

	ret = paj7620_burst_read(dev, PAJ7620_ADDR_GES_RESULT_0, &gest_data_reg0, 1);

	if (ret) {
		LOG_ERR("Failed to read gesture data");
		*result = GES_NONE;
		return -EIO;
	}
	else {
		switch (gest_data_reg0) {
		case GES_RIGHT_FLAG:
			ret = paj7620_fwd_bkwd_gesture_check(dev, GES_RIGHT, result);
			break;

		case GES_LEFT_FLAG:
			ret = paj7620_fwd_bkwd_gesture_check(dev, GES_LEFT, result);
			break;

		case GES_UP_FLAG:
			ret = paj7620_fwd_bkwd_gesture_check(dev, GES_UP, result);
			break;

		case GES_DOWN_FLAG:
			ret = paj7620_fwd_bkwd_gesture_check(dev, GES_DOWN, result);
			break;

		case GES_FORWARD_FLAG:
			k_msleep(data->gest_exit_time);
			*result = GES_FORWARD;
			break;

		case GES_BACKWARD_FLAG:
			k_msleep(data->gest_exit_time);
			*result = GES_BACKWARD;
			break;

		case GES_CLOCKWISE_FLAG:
			*result = GES_CLOCKWISE;
			break;

		case GES_ANTI_CLOCKWISE_FLAG:
			*result = GES_ANTICLOCKWISE;
			break;

		default:
			/* Bank 1 (Reg 0x44) has wave flag */
			ret = paj7620_burst_read(dev, PAJ7620_ADDR_GES_RESULT_0, &gest_data_reg0, 1);
			if (ret == 0 && gest_data_reg1 == GES_WAVE_FLAG) {
				*result = GES_WAVE;
			}
			break;
		}
	}

	return ret;
}


/**
 * Read object's "brightness"
 * Objects in view have their IR reflection measured. This interface returns a
 * measure of this brightness from 0..255
 */
static int paj7620_get_object_brightness(uint8_t *result)
{
	uint8_t brightness = 0x00;

	if (paj7620_byte_read(dev, PAJ7620_ADDR_OBJECT_BRIGHTNESS, &brightness)) {
		return -EIO;
	}

	*result = brightness;
	return 0;
}


/**
 * Read object's size (in pixels)
 * The sensor has a 30x30 IR LED array. This interface returns a count of
 *  how many pixels are part of the object in view being tracked.
 */
/*
int paj7620_getObjectSize()
{
  uint8_t data0, data1 = 0x00;
  int result = 0;
  readRegister(PAJ7620_ADDR_OBJECT_SIZE_LSB, 1, &data0);
  readRegister(PAJ7620_ADDR_OBJECT_SIZE_MSB, 1, &data1);
  result = data1;
  result = result << 8;
  result |= data0;
  return result;
}
*/


/**
 * Get how long since the object left the view in gesture mode
 *
 * \par
 * When an object has left the sensor's view, this register starts counting up.
 *  It's counting in ticks, roughly one per 7.2ms. It maxes out at 255, which
 *  happens at about 1830ms
 * \return int : ticks value 0..255
 */
/*
int paj7620_getNoObjectCount()
{
  uint8_t data0 = 0x00;
  readRegister(PAJ7620_ADDR_NO_OBJECT_COUNT, 1, &data0);
  return (int)data0;
}
*/


/**
 * Get how long no motion has been seen in gesture mode
 *
 * \par
 * This counts how long it has been since motions has occurred in front of the sensor.
 * This counts even if there's an object in view when it isn't moving.
 * Eratta: This *should* return 0..255, but seems to stop at 12.
 * Each "count" is probably 7.2ms, but it's been tough to figure out.
 * \return int : ticks value 0..12
 */
/*
int paj7620_getNoMotionCount()
{
  uint8_t data0 = 0x00;
  readRegister(PAJ7620_ADDR_NO_MOTION_COUNT, 1, &data0);
  return (int)data0;
}
*/


/**
 * Gets Gesture object's current X location
 *
 * \note Only works in gesture mode - default coordinates are 0 on right
 * \note Range seems to be 0..3712 in default gesture mode
 * \param none
 * \return int : X coordinate of cursor
 */
/*
int paj7620_getObjectCenterX()
{
  int result = 0;
  uint8_t data0 = 0x00;
  uint8_t data1 = 0x00;

  readRegister(PAJ7620_ADDR_OBJECT_CENTER_X_LSB, 1, &data0);
  readRegister(PAJ7620_ADDR_OBJECT_CENTER_X_MSB, 1, &data1);
  data1 &= 0x1F;      // Mask off high bits (unused)
  result |= data1;
  result = result << 8;
  result |= data0;

  return result;
}
*/


/**
 * Gets Gesture object's current Y location
 *
 * \note Only works in gesture mode - default coordinates are 0 on top
 * \note Range seems to be 0..3712 in default gesture mode
 * \param none
 * \return int : Y coordinate of cursor
 */
/*
int paj7620_getObjectCenterY()
{
  int result = 0;
  uint8_t data0 = 0x00;
  uint8_t data1 = 0x00;

  readRegister(PAJ7620_ADDR_OBJECT_CENTER_Y_LSB, 1, &data0);
  readRegister(PAJ7620_ADDR_OBJECT_CENTER_Y_MSB, 1, &data1);
  data1 &= 0x1F;      // Mask off high bits (unused)
  result |= data1;
  result = result << 8;
  result |= data0;

  return result;
}
*/


/**
 * Gets object's current X velocity's raw value
 *
 * \note Range seems to be -63..63
 * \param none
 * \return int : X velocity -63..63
 */
/*
int paj7620_getObjectVelocityX_raw()
{
  int result = 0;
  uint8_t data0 = 0x00;
  uint8_t data1 = 0x00;

  readRegister(PAJ7620_ADDR_OBJECT_VEL_X_LSB, 1, &data0);
  readRegister(PAJ7620_ADDR_OBJECT_VEL_X_MSB, 1, &data1);

  data0 &= 0x3F;        // Yup, see wiki for reason
  result = data0;
  if(data1) { result *= -1; }

  return result;
}
*/


/**
 * Gets object's current Y velocity's raw value
 *
 * \note Range seems to be -63..63
 * \param none
 * \return int : Y velocity -63..63
 */
/*
int paj7620_getObjectVelocityY_raw()
{
  int result = 0;
  uint8_t data0 = 0x00;
  uint8_t data1 = 0x00;

  readRegister(PAJ7620_ADDR_OBJECT_VEL_Y_LSB, 1, &data0);
  readRegister(PAJ7620_ADDR_OBJECT_VEL_Y_MSB, 1, &data1);
  data0 &= 0x3F;        // Yup, see wiki for reason
  result = data0;
  if(data1) { result *= -1; }

  return result;
}
*/


/**
 * Gets object's current X velocity's value
 *
 * \par
 * Value filtered to zero if object not in view
 * \note Range seems to be -63..63
 * \param none
 * \return int : X velocity -63..63
 */
/*
int paj7620_getObjectVelocityX()
{
  if(!isObjectInView()) {
    return 0;
  } else {
    return getObjectVelocityX_raw();
  }
}
*/


/**
 * Gets object's current Y velocity's value
 *
 * \par
 * Value filtered to zero if object not in view
 * \note Range seems to be -63..63
 * \param none
 * \return int : Y velocity -63..63
 */
/*
int paj7620_getObjectVelocityY()
{
  if(!isObjectInView()) {
    return 0;
  } else {
    return getObjectVelocityY_raw();
  }
}
*/


/**
 * Gets whether an object is in view or not
 *
 * \param none
 * \return bool : true if object in view
 */
/*
bool paj7620_isObjectInView()
{
  if(getNoObjectCount())
  {
    return false;
  }
  return true;
}
*/


/**
 * Gets which quadrant an object is in
 *
 * \param none
 * \return paj7660_corner : [NW, NW, SW, SE] quadrants, middle/buffer, NONE for no object in view
 */
/*
paj7620_corner paj7620_getpaj7660_corner()
{
  paj7660_corner ret = CORNER_NONE;
  int object_x, object_y = 0;

  if( !isObjectInView() ) {   // Bail if no object in view
    return CORNER_NONE;
  }

  object_x = getObjectCenterX();
  object_y = getObjectCenterY();

  if( object_x < CORNERS_BUFFER_LOWER && object_y < CORNERS_BUFFER_LOWER ) {
    return CORNER_NE;
  }
  else if( object_x > CORNERS_BUFFER_UPPER && object_y < CORNERS_BUFFER_LOWER ) {
    return CORNER_NW;
  }
  else if( object_x > CORNERS_BUFFER_UPPER && object_y > CORNERS_BUFFER_UPPER ) {
    return CORNER_SW;
  }
  else if( object_x < CORNERS_BUFFER_LOWER && object_y > CORNERS_BUFFER_UPPER ) {
    return CORNER_SE;
  }
  else {
    return CORNER_MIDDLE;     // Is in view, but not fully in a corner yet
  }
}
*/

static int paj7620_set_odr(const struct device *dev, const struct sensor_value *val)
{
	int ret = 0;

	return ret;
}

static int paj7620_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	int ret = 0;

	return ret;
}

static int paj7620_channel_get(const struct device *dev,
		               enum sensor_channel chan,
			       struct sensor_value *val)
{
	int ret = 0;

	return ret;
}

static int paj7620_attr_set(const struct device *dev,
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
		return paj7620_set_odr(dev, val);
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


/**
 * PAJ7620 device initialization and I2C connect on specified Wire bus
 *
 * Override version:
 * \par
 * Takes a TwoWire pointer allowing the user to pass
 *    in a specified I2C bus for devices using alternatives to bus 0 such
 *    as: begin(&Wire1) or begin(&Wire2)
 *
 * \param chosenWireHandle A pointer to the Wire handle that should be
 *   used to communicate with the PAJ7620
 * \return error code: 0 (false); success: return 1 (true)
 */
static int paj7620_init(const struct device *dev)
{
	int ret = 0;

	const struct paj7620_data *data = dev->data;

	/* Reasonable timing delay values to make algorithm insensitive to
	 *  hand entry and exit moves before and after detecting a gesture */
	data->gest_entry_time = PAJ7620_DEFAULT_GEST_ENTRY_TIME_MS;
	data->gest_exit_time = PAJ7620_DEFAULT_GEST_EXIT_TIME_MS;


	//wireHandle = chosenWireHandle;      // Save selected I2C bus for our use

	k_usleep(700);	// Wait 700us for PAJ7620U2 to stabilize
			// Reason: see v0.8 of 7620 documentation

	printf("Initing the PAJ7620\n");

	//wireHandle->begin();                // Start the I2C bus via wire library
	//TODO: Change for zephyr is_i2c_dt_ready or sth like that

	/* There's two register banks (0 & 1) to be selected between.
	 * BANK0 is where most data collection operations happen, so it's default.
	 * Selecting the bank is done here twice for a reason. When the 7620 turns
	 * on, the I2C bus is sleeping. When you first read/write to the bus
	 * the 7620 wakes up, but it sometimes misses that first message.
	 * Running the 7620 on an arduino with the USB power, a single call here
	 * usually works, but as soon as you use an external power bus it often
	 * fails to properly initialize and begin returns an error.
	 */
	//selectRegisterBank(BANK0);          // This is done twice on purpose
	//selectRegisterBank(BANK0);          // Default operations on BANK0

	// TODO: Replace with reading the WHO_AM_I register (HW ID) and failing
	// if not what expected
	//if( !isPAJ7620UDevice() ) {
	//return -EIO;                   // Return false - wrong device found
	//}

	//initializeDeviceSettings();         // Set registers up
	//setGestureMode();                   // Specifically set to gesture mode

	return 0;
}

static DEVICE_API(sensor, paj7620_driver_api) {
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

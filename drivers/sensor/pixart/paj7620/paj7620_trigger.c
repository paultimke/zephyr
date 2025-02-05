/*
 * Copyright (c) Paul Timke <ptimkec@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/drivers/gpio.h"
#define DT_DRV_COMPAT pixart_paj7620

#include "paj7620.h"
#include "paj7620_reg.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(PAJ7620, CONFIG_SENSOR_LOG_LEVEL);

static void paj7620_gpio_callback(const struct device *dev,
		                  struct gpio_callback *cb,
				  uint32_t pin_mask)
{
	struct paj7620_data *data =
		CONTAINER_OF(cb, struct paj7620_data, gpio_cb);
	const struct paj7620_config *config = data->dev->config;

	if ((pin_mask & BIT(config->int_gpio.pin)) == 0U) {
		return;
	}

	gpio_pin_interrupt_configure_dt(&config->ing_gpio, GPIO_INT_DISABLE);

#if defined(CONFIG_PAJ7620_TRIGGER_OWN_THREAD)
	k_sem_give(&data->trig_sem);
#elif defined(CONFIG_PAJ7620_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work);
#endif
}

static void paj7620_handle_int(const struct device *dev)
{
	struct paj7620_data *data = dev->data;
	const struct paj7620_config *config = dev->config;

	if (data->motion_handler) {
		data->motion_handler(dev, data->motion_trig);
	}

	// TODO: Configure maybe also as GPIO_INT_LEVEL_LOW depending on
	// the device tree option to have paj7620 int pin as active high/low
	gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_LEVEL_HIGH);
}

#ifdef CONFIG_PAJ7620_TRIGGER_OWN_THREAD
static void paj7620_thread_main(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);

	struct paj7620_data *data = p1;

	while (1) {
		k_sem_take(&data->trig_sem, K_FOREVER);
		paj7620_handle_int(data->dev);
	}
}
#endif

#ifdef CONFIG_PAJ7620_TRIGGER_GLOBAL_THREAD
static void paj7620_work_handler(struct k_work *work)
{
	struct paj7620_data &data =
		CONTAINER_OF(work, struct paj7620_data, work);

	paj7620_handle_int(data->dev);
}
#endif

int paj7620_trigger_set(const struct device *dev,
		        const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler)
{
	int ret = 0;
	struct paj7620_data *data = dev->data;

	if (trig->type == SENSOR_TRIG_MOTION) {
		data->motion_handler = handler;
		data->motion_trig = trig;
	} else {
		LOG_ERR("Unsupported sensor trigger");
		ret = -ENOTSUP;
	}

	return ret;
}

int paj7620_trigger_init(const struct device *dev)
{
	int ret = 0;
	const struct paj7620_config *config = dev->config;
	struct paj7620_data *dev = dev->data;

	data->dev = dev;

#if defined(CONFIG_PAJ7620_TRIGGER_OWN_THREAD)
	k_sem_init(&data->trig_sem, 0, K_SEM_MAX_LIMIT);
	k_thread_create(&data->thread,
			data->thread_stack,
			CONFIG_PAJ7620_THREAD_STACK_SIZE,
			paj7620_thread_main,
			data,
			NULL,
			NULL,
			K_PRIO_COOP(CONFIG_PAJ7620_THREAD_PRIORITY),
			0,
			K_NO_WAIT);
#elif defined(CONFIG_PAJ7620_TRIGGER_GLOBAL_THREAD)
	data->work.handler = paj7620_work_handler;
#endif

	/* Enable interrupts */
	// TODO: Check if we can disable interrupts by default and only enable
	// them if the trigger is set. I'm worried that the gesture data in the INT_FLAG_1
	// register only gets set when interrupts are configured.
	/*
	ret = paj7620_byte_write(dev, PAJ7620_REG_INT_1_EN, PAJ7620_MASK_ALL_GESTURE_INTS_ENABLE);
	ret += paj7620_byte_write(dev, PAJ7620_REG_INT_2_EN, PAJ7620_MASK_INT_FLAG_2_GES_WAVE);
	if (ret) {
		return -EIO;
	}
	*/

	/* Configure GPIO */
	if (!gpio_is_ready_dt(&config->int_gpio)) {
		LOG_ERR("GPIO device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT);
	if (ret) {
		return ret;
	}

	gpio_init_callback(&data->gpio_cb, paj7620_gpio_callback, BIT(config->int_gpio.pin));

	ret = gpio_add_callback(config->int_gpio.port, &data->gpio_cb);
	if (ret) {
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret) {
		return ret;
	}

	return 0;
}

/**
 * TODO: Check if we need/should do this. INTs are cleared after reading them, but maybe
 * reading them should be the responsibility of the application, otherwise I'm afraid if
 * we clear them here, they will have no data to read.
 */
static void paj7620_clear_gesture_interrupts(const struct device *dev)
{
	int ret = 0;
	uint8_t gesture_data;

	ret = paj7620_byte_read(dev, PAJ7620_REG_INT_FLAG_1, &gesture_data);
	ret += paj7620_byte_read(dev, PAJ7620_REG_INT_FLAG_2, &gesture_data);
	if (ret) {
		LOG_ERR("Failed to clear interrupts");
	}
}

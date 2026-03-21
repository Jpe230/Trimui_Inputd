#pragma once
#include <stdint.h>

#define SUNXI_GPIO_INPUT  0
#define SUNXI_GPIO_OUTPUT 1

/**
 * Initialize the sunxi GPIO controller.
 *
 * @return 0 on success, -1 on error.
 */
int sunxi_gpio_init(void);

/**
 * Close the sunxi GPIO controller and release resources.
 *
 * @return void.
 */
void sunxi_gpio_close(void);

/**
 * Read a GPIO pin value.
 *
 * @param pin GPIO number to sample.
 * @return Pin value on success, or -1 on failure.
 */
int sunxi_gpio_input(uint32_t pin);

/**
 * Drive a GPIO pin high or low.
 *
 * @param pin GPIO number to drive.
 * @param val Output value, 0 for low and non-zero for high.
 * @return 0 on success, -1 on failure.
 */
int sunxi_gpio_output(uint32_t pin, uint32_t val);

/**
 * Configure a GPIO pin as input or output.
 *
 * @param pin GPIO number to configure.
 * @param val SUNXI_GPIO_INPUT or SUNXI_GPIO_OUTPUT.
 * @return 0 on success, -1 on failure.
 */
int sunxi_gpio_set_cfgpin(uint32_t pin, uint32_t val);

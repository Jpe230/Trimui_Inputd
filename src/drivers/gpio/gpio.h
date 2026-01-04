#pragma once

#include <stdbool.h>

/**
 * Export a GPIO pin for userspace access.
 *
 * @param pin GPIO number to export.
 * @return 0 on success, -1 on error.
 */
int gpio_export(int pin);

/**
 * Configure GPIO direction.
 *
 * @param pin GPIO number to configure.
 * @param is_output Set true for output, false for input.
 * @return 0 on success, -1 on error.
 */
int gpio_set_direction(int pin, bool is_output);

/**
 * Write a value to a GPIO configured as output.
 *
 * @param pin GPIO number to drive.
 * @param value Logical value to set (0/1).
 * @return 0 on success, -1 on error.
 */
int gpio_write(int pin, int value);

/**
 * Read a value from a GPIO configured as input.
 *
 * @param pin GPIO number to sample.
 * @param value_out Output pointer receiving the sampled value.
 * @return 0 on success, -1 on error.
 */
int gpio_read(int pin, int *value_out);

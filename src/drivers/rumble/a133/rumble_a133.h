#pragma once

#include <stdbool.h>

#define RUMBLE_A133_GPIO_PIN 227

struct rumble_a133_hw {
    int gpio_pin;
    bool initialized;
};

/**
 * Initialize the A133 GPIO rumble driver.
 *
 * @param hw Hardware state to populate.
 * @return 0 on success, -1 on failure.
 */
int rumble_a133_init(struct rumble_a133_hw *hw);

/**
 * Drive the rumble motor to the desired state.
 *
 * @param hw Hardware state.
 * @param on True to enable the motor, false to disable.
 * @return 0 on success, -1 on failure.
 */
int rumble_a133_set(struct rumble_a133_hw *hw, bool on);

/**
 * Shut down the rumble driver and reset state.
 *
 * @param hw Hardware state to tear down.
 * @return void.
 */
void rumble_a133_close(struct rumble_a133_hw *hw);

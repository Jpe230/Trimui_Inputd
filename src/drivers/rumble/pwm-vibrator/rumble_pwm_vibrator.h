#pragma once

#include <stdbool.h>

struct device_rumble_driver;

struct rumble_pwm_vibrator_hw {
    int fd;
    int effect_id;
    bool initialized;
    bool motor_on;
};

int rumble_pwm_vibrator_init(struct rumble_pwm_vibrator_hw *hw);
int rumble_pwm_vibrator_set(struct rumble_pwm_vibrator_hw *hw, bool on);
void rumble_pwm_vibrator_close(struct rumble_pwm_vibrator_hw *hw);
const struct device_rumble_driver *rumble_pwm_vibrator_driver(void);

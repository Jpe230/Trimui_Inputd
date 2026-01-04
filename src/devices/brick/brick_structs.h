#pragma once

#include <stdbool.h>

#include "../device_helpers.h"

struct gamepad;

/* GPIO mapping and last-known state for a single Brick button. */
struct brick_button {
    int gpio;
    unsigned short code;
    int prev;
};

/* Runtime context for the Trimui Brick controller. */
struct brick_state {
    struct gamepad *gp;
    struct brick_button buttons[17];
    int hat_pins[4];
    struct device_hat_state hat;
    struct device_dirty_state dirty;
    bool active_low;
};

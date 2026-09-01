#pragma once

#include <stdbool.h>
#include <time.h>
#include <stddef.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include "../gamepad/uinput.h"

enum { DEVICE_RUMBLE_EFFECT_SLOTS = 4 };

struct device_rumble_driver {
    const char *name;
    size_t ctx_size;
    int  (*init)(void *ctx);
    int  (*set)(void *ctx, bool on);
    void (*close)(void *ctx);
};

struct device_rumble_slot {
    bool used;
    struct ff_effect effect;
};

struct device_rumble_state {
    struct device_rumble_slot slots[DEVICE_RUMBLE_EFFECT_SLOTS];
    int active_id;
    bool has_stop_time;
    struct timespec stop_time;
    const struct device_rumble_driver *driver;
    void *driver_ctx;
    bool motor_on;
    bool initialized;
};

/**
 * Initialize rumble handling using a provided hardware driver.
 *
 * @param st Rumble state to initialize.
 * @param driver Hardware driver operations (e.g., A133 or future A527).
 * @return 0 on success, -1 on failure.
 */
int device_rumble_init(struct device_rumble_state *st, const struct device_rumble_driver *driver);

/**
 * Tear down rumble handling and release driver resources.
 *
 * @param st Rumble state previously initialized.
 * @return void.
 */
void device_rumble_close(struct device_rumble_state *st);

/**
 * Process pending FF events and drive the hardware as needed.
 *
 * @param st Rumble state.
 * @param gp Gamepad handle used to read EV_UINPUT/EV_FF events.
 * @return true on success, false on fatal error.
 */
bool device_rumble_poll(struct device_rumble_state *st, struct gamepad *gp);

/**
 * Accessor for the current A133 rumble driver (Smart Pro / Brick).
 *
 * @return Pointer to the driver descriptor.
 */
const struct device_rumble_driver *rumble_a133_driver(void);
const struct device_rumble_driver *rumble_pwm_vibrator_driver(void);

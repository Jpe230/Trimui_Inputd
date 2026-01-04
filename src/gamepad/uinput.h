#pragma once

#include <stddef.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>

struct gamepad_abs_desc {
    unsigned int code;
    int min;
    int max;
    int flat;
    int fuzz;
    int resolution;
};

struct gamepad_desc {
    const char *name;
    struct input_id id;
    const unsigned short *keys;
    size_t key_count;
    const struct gamepad_abs_desc *axes;
    size_t axis_count;
    const unsigned short *switches;
    size_t switch_count;
    unsigned int ff_effects_max;
    bool enable_ff_rumble;
    const unsigned short *ff_effects;
    size_t ff_effect_count;
};

/**
 * Create a uinput gamepad device from the provided description.
 *
 * @param desc Gamepad descriptor containing keys, axes, and switches.
 * @return Allocated gamepad handle or NULL on failure.
 */
struct gamepad *gamepad_init(const struct gamepad_desc *desc);

/**
 * Emit a key event to the uinput device.
 *
 * @param gp Gamepad handle returned by gamepad_init.
 * @param code Linux input key code.
 * @param value 1 when pressed, 0 when released.
 * @return void.
 */
void gamepad_emit_key(struct gamepad *gp, unsigned short code, int value);

/**
 * Emit an absolute axis event to the uinput device.
 *
 * @param gp Gamepad handle returned by gamepad_init.
 * @param code Linux input absolute axis code.
 * @param value New axis value to report.
 * @return void.
 */
void gamepad_emit_abs(struct gamepad *gp, unsigned short code, int value);

/**
 * Emit a switch event to the uinput device.
 *
 * @param gp Gamepad handle returned by gamepad_init.
 * @param code Linux input switch code.
 * @param value Switch position to report.
 * @return void.
 */
void gamepad_emit_sw(struct gamepad *gp, unsigned short code, int value);

/**
 * Flush pending input events to uinput.
 *
 * @param gp Gamepad handle returned by gamepad_init.
 * @return void.
 */
void gamepad_sync(struct gamepad *gp);

/**
 * Destroy the uinput device and release resources.
 *
 * @param gp Gamepad handle returned by gamepad_init.
 * @return void.
 */
void gamepad_destroy(struct gamepad *gp);

/**
 * Retrieve the underlying uinput file descriptor for advanced handling (e.g. FF).
 *
 * @param gp Gamepad handle returned by gamepad_init.
 * @return File descriptor or -1 on error.
 */
int gamepad_get_fd(struct gamepad *gp);

/**
 * Attempt to read a pending input_event from the uinput device (non-blocking).
 *
 * @param gp Gamepad handle returned by gamepad_init.
 * @param ev Output struct populated when an event is available.
 * @return 1 when an event was read, 0 when no data is ready, -1 on fatal error.
 */
int gamepad_read_event(struct gamepad *gp, struct input_event *ev);

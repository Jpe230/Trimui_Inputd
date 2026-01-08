#define _GNU_SOURCE
#include "brick.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../drivers/sunxi-gpio/sunxi-gpio.h"
#include "../../gamepad/uinput.h"

static struct brick_state brick_ctx;
static const int BRICK_AXIS_MAX = 32767;

static bool file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static inline void update_button_state(struct brick_state *st, size_t idx, int pressed)
{
    if (idx >= 32) {
        return;
    }
    uint32_t mask = BRICK_BUTTON_BIT(idx);
    if (pressed) {
        st->state_buttons |= mask;
    } else {
        st->state_buttons &= ~mask;
    }
}

static enum brick_dpad2axis_mode is_dpad2axis_enable(const struct brick_state *st)
{
    if (st->toggle_dpad2axis) {
        return BRICK_D2A_MODE_GLOBAL;
    }
    if ((st->state_buttons & st->alt_dpad2axis_bits) != 0) {
        return BRICK_D2A_MODE_HOLD;
    }
    return BRICK_D2A_MODE_DISABLED;
}

static void check_dpad_to_axis_settings(struct brick_state *st)
{
    uint32_t alt_bits = 0;
    for (size_t i = 0; i < BRICK_HOLD_MAP_COUNT; ++i) {
        const struct brick_hold_map *entry = &BRICK_HOLD_MAP[i];
        if (file_exists(entry->path)) {
            alt_bits |= entry->bit;
        }
    }

    bool toggle_dpad2axis = file_exists(BRICK_DPAD_FLAG("input_dpad_to_joystick"));
    bool toggle_dpad = !file_exists(BRICK_DPAD_FLAG("input_no_dpad"));

    st->alt_dpad2axis_bits = alt_bits;
    st->toggle_dpad2axis = toggle_dpad2axis;
    st->toggle_dpad = toggle_dpad;
}

void brick_refresh_dpad_flags(struct brick_state *st)
{
    check_dpad_to_axis_settings(st);
}

static inline int axis_from_dpad(int negative, int positive)
{
    if (negative == positive) {
        return 0;
    }
    return negative ? -BRICK_AXIS_MAX : BRICK_AXIS_MAX;
}

static void emit_synth_axis(struct brick_state *st, int x, int y)
{
    if (st->last_axis_x == x && st->last_axis_y == y) {
        return;
    }
    st->last_axis_x = x;
    st->last_axis_y = y;
    gamepad_emit_abs(st->gp, ABS_X, x);
    gamepad_emit_abs(st->gp, ABS_Y, y);
    device_dirty_mark(&st->dirty);
}

static void clear_hat_output(struct brick_state *st)
{
    st->hat.left = st->hat.right = st->hat.up = st->hat.down = 0;
    device_dirty_merge(&st->dirty, device_hat_emit(st->gp, &st->hat));
}

static void init_active_low(struct brick_state *st)
{
    const char *env = getenv("BRICK_ACTIVE_LOW");
    if (env && strcmp(env, "0") == 0) {
        st->active_low = false;
    } else {
        st->active_low = true;
    }
}

static int init_pin(int pin)
{
    return sunxi_gpio_set_cfgpin((uint32_t)pin, SUNXI_GPIO_INPUT);
}

static bool poll_switch(struct brick_state *st)
{
    int val = sunxi_gpio_input((uint32_t)BRICK_GPIO_SWITCH);
    if (val < 0) {
        return false;
    }
    if (st->last_switch == val) {
        return false;
    }
    st->last_switch = val;
    gamepad_emit_sw(st->gp, SW_TABLET_MODE, val);
    return true;
}

int brick_init(void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx, struct axis_state *ry, bool verbose)
{
    (void)lx;
    (void)ly;
    (void)rx;
    (void)ry;
    (void)verbose;

    if (sunxi_gpio_init() < 0) {
        return -1;
    }

    struct brick_state *st = &brick_ctx;
    memset(st, 0, sizeof(*st));
    st->gp = gp;
    init_active_low(st);
    memcpy(st->buttons, BRICK_BUTTON_DEFS, sizeof(BRICK_BUTTON_DEFS));
    memcpy(st->hat_pins, BRICK_HAT_PINS, sizeof(BRICK_HAT_PINS));
    if (device_rumble_init(&st->rumble, rumble_a133_driver()) < 0) {
        sunxi_gpio_close();
        return -1;
    }

    for (size_t i = 0; i < BRICK_BUTTON_COUNT; ++i) {
        if (st->buttons[i].gpio < 0) {
            continue;
        }
        if (init_pin(st->buttons[i].gpio) < 0) {
            sunxi_gpio_close();
            return -1;
        }
    }
    for (size_t i = 0; i < BRICK_HAT_PIN_COUNT; ++i) {
        if (init_pin(st->hat_pins[i]) < 0) {
            sunxi_gpio_close();
            return -1;
        }
    }
    if (init_pin(BRICK_GPIO_SWITCH) < 0) {
        sunxi_gpio_close();
        return -1;
    }

    st->last_switch = -1;

    st->hat = (struct device_hat_state){.x = 0, .y = 0, .left = 0, .right = 0, .up = 0, .down = 0};
    st->turbo_count = BRICK_TURBO_CFG_COUNT;
    turbo_init_bindings(st->turbo, st->turbo_count, BRICK_TURBO_CFG);
    st->toggle_dpad = true;
    st->toggle_dpad2axis = false;
    st->alt_dpad2axis_bits = 0;
    st->state_buttons = 0;
    st->d2a_last_mode = BRICK_D2A_MODE_DISABLED;
    st->last_axis_x = 0;
    st->last_axis_y = 0;
    *ctx = st;
    return 0;
}

bool brick_poll(void *ctx)
{
    struct brick_state *st = ctx;
    if (!device_rumble_poll(&st->rumble, st->gp)) {
        return false;
    }
    device_dirty_reset(&st->dirty, poll_switch(st));

    for (size_t i = 0; i < BRICK_BUTTON_COUNT; ++i) {
        if (st->buttons[i].gpio < 0) {
            continue;
        }
        int v = sunxi_gpio_input((uint32_t)st->buttons[i].gpio);
        if (v < 0) {
            continue;
        }
        int pressed = st->active_low ? (v == 0) : (v != 0);
        update_button_state(st, i, pressed);
        if (pressed != st->buttons[i].prev) {
            st->buttons[i].prev = pressed;
            bool handled = turbo_note_physical(st->turbo, st->turbo_count, st->buttons[i].code, pressed != 0);
            if (!handled) {
                gamepad_emit_key(st->gp, st->buttons[i].code, pressed);
                device_dirty_mark(&st->dirty);
            }
        }
    }

    int dpad_vals[BRICK_HAT_PIN_COUNT] = {0};
    int *hat_targets[BRICK_HAT_PIN_COUNT] = {&st->hat.up, &st->hat.down, &st->hat.left, &st->hat.right};
    for (size_t i = 0; i < BRICK_HAT_PIN_COUNT; ++i) {
        int value = sunxi_gpio_input((uint32_t)st->hat_pins[i]);
        if (value < 0) {
            continue;
        }
        int pressed = st->active_low ? (value == 0) : (value != 0);
        *hat_targets[i] = pressed;
        dpad_vals[i] = pressed;
    }

    enum brick_dpad2axis_mode mode = is_dpad2axis_enable(st);

    int axis_x = axis_from_dpad(dpad_vals[2], dpad_vals[3]);
    int axis_y = axis_from_dpad(dpad_vals[0], dpad_vals[1]);

    bool allow_hat = st->toggle_dpad && mode != BRICK_D2A_MODE_HOLD;
    if (allow_hat) {
        device_dirty_merge(&st->dirty, device_hat_emit(st->gp, &st->hat));
    } else if (st->hat.x != 0 || st->hat.y != 0) {
        clear_hat_output(st);
    }

    if (mode != BRICK_D2A_MODE_DISABLED) {
        emit_synth_axis(st, axis_x, axis_y);
    } else if (st->d2a_last_mode != BRICK_D2A_MODE_DISABLED) {
        emit_synth_axis(st, 0, 0);
    }

    st->d2a_last_mode = mode;
    turbo_process_frame(st->turbo, st->turbo_count, st->gp, &st->dirty);
    device_dirty_flush(&st->dirty, st->gp);
    return true;
}

void brick_close(void *ctx)
{
    struct brick_state *st = ctx;
    if (st) {
        device_rumble_close(&st->rumble);
    }
    sunxi_gpio_close();
}

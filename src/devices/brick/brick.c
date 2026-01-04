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

    st->hat = (struct device_hat_state){.x = 0, .y = 0, .left = 0, .right = 0, .up = 0, .down = 0};
    *ctx = st;
    return 0;
}

bool brick_poll(void *ctx)
{
    struct brick_state *st = ctx;
    device_dirty_reset(&st->dirty, false);
    for (size_t i = 0; i < BRICK_BUTTON_COUNT; ++i) {
        if (st->buttons[i].gpio < 0) {
            continue;
        }
        int v = sunxi_gpio_input((uint32_t)st->buttons[i].gpio);
        if (v < 0) {
            continue;
        }
        int pressed = st->active_low ? (v == 0) : (v != 0);
        if (pressed != st->buttons[i].prev) {
            st->buttons[i].prev = pressed;
            gamepad_emit_key(st->gp, st->buttons[i].code, pressed);
            device_dirty_mark(&st->dirty);
        }
    }

    int *hat_targets[BRICK_HAT_PIN_COUNT] = {&st->hat.up, &st->hat.down, &st->hat.left, &st->hat.right};
    for (size_t i = 0; i < BRICK_HAT_PIN_COUNT; ++i) {
        int value = sunxi_gpio_input((uint32_t)st->hat_pins[i]);
        if (value < 0) {
            continue;
        }
        *hat_targets[i] = st->active_low ? (value == 0) : (value != 0);
    }

    device_dirty_merge(&st->dirty, device_hat_emit(st->gp, &st->hat));
    device_dirty_flush(&st->dirty, st->gp);
    return true;
}

void brick_close(void *ctx)
{
    (void)ctx;
    sunxi_gpio_close();
}

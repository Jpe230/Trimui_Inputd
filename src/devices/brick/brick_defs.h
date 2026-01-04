#pragma once

#include <stddef.h>

#include "../../gamepad/uinput.h"
#include "brick_structs.h"

/* Helper macro for describing Brick absolute axes. */
#define BRICK_AXIS_DESC(_code, _min, _max) \
    { .code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0 }

/* GPIO/button mapping table in the order expected by the input subsystem. */
static const struct brick_button BRICK_BUTTON_DEFS[] = {
    {-1, BTN_0, 0},          /* Unused slot 0 */
    {-1, BTN_1, 0},          /* Unused slot 1 */
    {116, KEY_VOLUMEDOWN, 0},
    {117, KEY_VOLUMEUP, 0},
    {109, BTN_SOUTH, 0},
    {108, BTN_EAST, 0},
    {115, BTN_WEST, 0},
    {114, BTN_NORTH, 0},
    {36, BTN_TL, 0},
    {241, BTN_TR, 0},
    {34, BTN_TL2, 0},
    {37, BTN_TR2, 0},
    {239, BTN_SELECT, 0},
    {240, BTN_START, 0},
    {238, BTN_MODE, 0},
    {236, BTN_THUMBL, 0},
    {237, BTN_THUMBR, 0},
};

/* Dedicated GPIO pins for the Brick hat directions: up, down, left, right. */
static const int BRICK_HAT_PINS[] = {113, 110, 111, 112};

/* Exposed key codes for the Brick device (includes two unused placeholders). */
static const unsigned short BRICK_KEYS[] = {
    BTN_0, BTN_1,
    KEY_VOLUMEDOWN, KEY_VOLUMEUP,
    BTN_SOUTH, BTN_EAST, BTN_WEST, BTN_NORTH,
    BTN_TL, BTN_TR, BTN_TL2, BTN_TR2,
    BTN_SELECT, BTN_START, BTN_MODE,
    BTN_THUMBL, BTN_THUMBR,
};

/* Absolute axis descriptors for the Brick hat. */
static const struct gamepad_abs_desc BRICK_AXES[] = {
    BRICK_AXIS_DESC(ABS_HAT0X, -1, 1),
    BRICK_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

/* Utility counts for iterating Brick tables. */
enum {
    BRICK_BUTTON_COUNT = sizeof(BRICK_BUTTON_DEFS) / sizeof(BRICK_BUTTON_DEFS[0]),
    BRICK_HAT_PIN_COUNT = sizeof(BRICK_HAT_PINS) / sizeof(BRICK_HAT_PINS[0]),
};

/* Gamepad description presented to the OS for the Brick controller. */
static const struct gamepad_desc BRICK_GAMEPAD_DESC = {
    .name = "TRIMUI Brick Controller",
    .id = {
        .bustype = 0x0003,
        .vendor  = 0x045e,
        .product = 0x028e,
        .version = 0x0114,
    },
    .keys = BRICK_KEYS,
    .key_count = sizeof(BRICK_KEYS) / sizeof(BRICK_KEYS[0]),
    .axes = BRICK_AXES,
    .axis_count = sizeof(BRICK_AXES) / sizeof(BRICK_AXES[0]),
    .switches = NULL,
    .switch_count = 0,
};

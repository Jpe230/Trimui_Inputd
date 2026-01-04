#pragma once

#include <stddef.h>

#include "../../gamepad/uinput.h"
#include "smart_pro_structs.h"

/* Bit masks for buttons reported by the Smart Pro left-hand frame. */
enum {
    SP_BTN_L1_MASK = 0x01,
    SP_BTN_L2_MASK = 0x02,
    SP_BTN_DPAD_UP_MASK = 0x04,
    SP_BTN_DPAD_LEFT_MASK = 0x08,
    SP_BTN_DPAD_RIGHT_MASK = 0x10,
    SP_BTN_DPAD_DOWN_MASK = 0x20,
    SP_BTN_MODE_MASK = 0x80,
};

/* Bit masks for buttons reported by the Smart Pro right-hand frame. */
enum {
    SP_BTN_R1_MASK = 0x01,
    SP_BTN_R2_MASK = 0x02,
    SP_BTN_Y_MASK = 0x04,
    SP_BTN_X_MASK = 0x08,
    SP_BTN_B_MASK = 0x10,
    SP_BTN_A_MASK = 0x20,
    SP_BTN_SELECT_MASK = 0x40,
    SP_BTN_START_MASK = 0x80,
};

/* Mask representing any D-pad bit in the Smart Pro payload. */
enum { SP_DPAD_MASK = SP_BTN_DPAD_UP_MASK | SP_BTN_DPAD_DOWN_MASK | SP_BTN_DPAD_LEFT_MASK | SP_BTN_DPAD_RIGHT_MASK };

/* GPIO pins driven to power the Smart Pro halves and read the hardware switch. */
enum {
    SP_GPIO_OUT1 = 110,
    SP_GPIO_OUT2 = 114,
    SP_GPIO_INPUT = 243,
};

/* Helper macro to quickly build Smart Pro axis descriptors. */
#define SP_AXIS_DESC(_code, _min, _max) \
    { .code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0 }

/* Map left-hand packed button bits to uinput codes. */
static const struct sp_button_map_entry SP_LEFT_BUTTON_MAP[] = {
    {SP_BTN_L1_MASK, BTN_TL},
    {SP_BTN_L2_MASK, BTN_TL2},
    {SP_BTN_MODE_MASK, BTN_MODE},
};

/* Map right-hand packed button bits to uinput codes. */
static const struct sp_button_map_entry SP_RIGHT_BUTTON_MAP[] = {
    {SP_BTN_R1_MASK, BTN_TR},
    {SP_BTN_R2_MASK, BTN_TR2},
    {SP_BTN_Y_MASK, BTN_NORTH},
    {SP_BTN_X_MASK, BTN_WEST},
    {SP_BTN_B_MASK, BTN_SOUTH},
    {SP_BTN_A_MASK, BTN_EAST},
    {SP_BTN_SELECT_MASK, BTN_SELECT},
    {SP_BTN_START_MASK, BTN_START},
};

/* Utility counts for iterating Smart Pro maps. */
enum {
    SP_LEFT_BUTTON_MAP_COUNT = sizeof(SP_LEFT_BUTTON_MAP) / sizeof(SP_LEFT_BUTTON_MAP[0]),
    SP_RIGHT_BUTTON_MAP_COUNT = sizeof(SP_RIGHT_BUTTON_MAP) / sizeof(SP_RIGHT_BUTTON_MAP[0]),
};

/* Exposed key codes for the Smart Pro controller. */
static const unsigned short SP_KEYS[] = {
    BTN_EAST, BTN_SOUTH, BTN_NORTH, BTN_WEST,
    BTN_TL, BTN_TR, BTN_TL2, BTN_TR2,
    BTN_SELECT, BTN_START, BTN_MODE,
};

/* Absolute axis descriptions for both Smart Pro sticks and the hat. */
static const struct gamepad_abs_desc SP_AXES[] = {
    SP_AXIS_DESC(ABS_X, -32767, 32767),
    SP_AXIS_DESC(ABS_Y, -32767, 32767),
    SP_AXIS_DESC(ABS_Z, -32767, 32767),
    SP_AXIS_DESC(ABS_RZ, -32767, 32767),
    SP_AXIS_DESC(ABS_HAT0X, -1, 1),
    SP_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

/* Switch list advertised by the Smart Pro controller (tablet mode). */
static const unsigned short SP_SWITCHES[] = {SW_TABLET_MODE};

/* Input device descriptor presented to the OS for Smart Pro. */
static const struct gamepad_desc SMART_PRO_GAMEPAD_DESC = {
    .name = "TRIMUI Smart Pro Controller",
    .id = {
        .bustype = 0x0003,
        .vendor  = 0x0000,
        .product = 0x0000,
        .version = 2,
    },
    .keys = SP_KEYS,
    .key_count = sizeof(SP_KEYS) / sizeof(SP_KEYS[0]),
    .axes = SP_AXES,
    .axis_count = sizeof(SP_AXES) / sizeof(SP_AXES[0]),
    .switches = SP_SWITCHES,
    .switch_count = sizeof(SP_SWITCHES) / sizeof(SP_SWITCHES[0]),
};

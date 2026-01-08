#pragma once

#include <stdbool.h>

#include "../../gamepad/uinput.h"
#include "brick_structs.h"
#include "brick_defs.h"

struct axis_state;

/**
 * Initialize the Trimui Brick device handler and prepare GPIO inputs.
 *
 * @param ctx Output pointer receiving the device context.
 * @param gp Gamepad handle used to emit events.
 * @param lx Pointer to the left stick X calibration state (unused for Brick).
 * @param ly Pointer to the left stick Y calibration state (unused for Brick).
 * @param rx Pointer to the right stick X calibration state (unused for Brick).
 * @param ry Pointer to the right stick Y calibration state (unused for Brick).
 * @param verbose Enable verbose logging when true.
 * @return 0 on success, -1 on initialization failure.
 */
int brick_init(void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx, struct axis_state *ry, bool verbose);

/**
 * Poll Brick GPIO inputs and emit uinput events as needed.
 *
 * @param ctx Device context provided by brick_init.
 * @return true to continue processing; false on fatal error.
 */
bool brick_poll(void *ctx);

/**
 * Release Brick device resources and close GPIO subsystem.
 *
 * @param ctx Device context provided by brick_init.
 * @return void.
 */
void brick_close(void *ctx);

struct brick_state;
void brick_refresh_dpad_flags(struct brick_state *st);

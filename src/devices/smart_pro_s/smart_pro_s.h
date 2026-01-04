#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "smart_pro_s_structs.h"
#include "smart_pro_s_defs.h"

struct gamepad;
struct axis_state;

/**
 * Initialize the Smart Pro S device, configure serial ports, and prepare state.
 *
 * @param ctx Output pointer receiving the allocated context.
 * @param gp Gamepad handle used to emit events.
 * @param lx Pointer to left stick X calibration data.
 * @param ly Pointer to left stick Y calibration data.
 * @param rx Pointer to right stick X calibration data.
 * @param ry Pointer to right stick Y calibration data.
 * @param verbose Enable verbose logging when true.
 * @return 0 on success, -1 when initialization fails.
 */
int smart_pro_s_init(void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx, struct axis_state *ry, bool verbose);

/**
 * Poll Smart Pro S serial endpoints and emit uinput events.
 *
 * @param ctx Device context provided by smart_pro_s_init.
 * @return true to continue processing; false on fatal error.
 */
bool smart_pro_s_poll(void *ctx);

/**
 * Close Smart Pro S serial endpoints and release resources.
 *
 * @param ctx Device context provided by smart_pro_s_init.
 * @return void.
 */
void smart_pro_s_close(void *ctx);

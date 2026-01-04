#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "../calibration/calibration.h"
#include "../gamepad/uinput.h"

/* Shared hat state used by all devices. */
struct device_hat_state {
    int x;
    int y;
    int left;
    int right;
    int up;
    int down;
};

/* Tracks whether any input event was emitted during a poll cycle. */
struct device_dirty_state {
    bool dirty;
};

/**
 * Emit hat axis events if the composed X/Y value changed.
 *
 * @param gp Gamepad handle.
 * @param hat Hat state to evaluate and update.
 * @return true if events were emitted, false otherwise.
 */
static inline bool device_hat_emit(struct gamepad *gp, struct device_hat_state *hat)
{
    const int horiz[2] = {hat->left, hat->right};
    const int vert[2] = {hat->up, hat->down};
    int new_x = (horiz[0] == horiz[1]) ? 0 : (horiz[0] ? -1 : 1);
    int new_y = (vert[0] == vert[1]) ? 0 : (vert[0] ? -1 : 1);
    bool changed = false;

    if (new_x != hat->x) {
        hat->x = new_x;
        gamepad_emit_abs(gp, ABS_HAT0X, hat->x);
        changed = true;
    }
    if (new_y != hat->y) {
        hat->y = new_y;
        gamepad_emit_abs(gp, ABS_HAT0Y, hat->y);
        changed = true;
    }
    return changed;
}

/**
 * Update hat direction bits from packed button masks.
 *
 * @param hat Hat state to mutate.
 * @param btn_raw Raw button byte/word value.
 * @param diff Bitmask of changed buttons.
 * @param up_mask Bit mask for "up".
 * @param down_mask Bit mask for "down".
 * @param left_mask Bit mask for "left".
 * @param right_mask Bit mask for "right".
 * @return void.
 */
static inline void device_hat_apply_masks(struct device_hat_state *hat, uint32_t btn_raw, uint32_t diff, uint32_t up_mask, uint32_t down_mask, uint32_t left_mask, uint32_t right_mask)
{
    const struct {
        uint32_t mask;
        int *target;
    } map[] = {
        {up_mask, &hat->up},
        {down_mask, &hat->down},
        {left_mask, &hat->left},
        {right_mask, &hat->right},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i) {
        if (diff & map[i].mask) {
            *map[i].target = (btn_raw & map[i].mask) ? 1 : 0;
        }
    }
}

/* Describes how to emit a calibrated X/Y pair. */
struct device_axis_cfg {
    int abs_code_x;
    int abs_code_y;
    bool invert_x;
    bool invert_y;
};

/**
 * Mark a device poll cycle as dirty (events emitted).
 *
 * @param state Dirty state to update.
 * @return void.
 */
static inline void device_dirty_mark(struct device_dirty_state *state)
{
    state->dirty = true;
}

/**
 * Merge a boolean changed flag into the dirty state.
 *
 * @param state Dirty state to update.
 * @param changed True when an event was emitted.
 * @return void.
 */
static inline void device_dirty_merge(struct device_dirty_state *state, bool changed)
{
    if (changed) {
        state->dirty = true;
    }
}

/**
 * Reset or seed the dirty flag for a new poll iteration.
 *
 * @param state Dirty state to reset.
 * @param initial_dirty Whether to start as already dirty.
 * @return void.
 */
static inline void device_dirty_reset(struct device_dirty_state *state, bool initial_dirty)
{
    state->dirty = initial_dirty;
}

/**
 * Flush pending events if dirty and reset the flag.
 *
 * @param state Dirty state to check and clear.
 * @param gp Gamepad handle to sync.
 * @return void.
 */
static inline void device_dirty_flush(struct device_dirty_state *state, struct gamepad *gp)
{
    if (state->dirty) {
        gamepad_sync(gp);
        state->dirty = false;
    }
}

/**
 * Apply calibration to a pair of axes and emit changes.
 *
 * @param gp Gamepad handle.
 * @param ax Calibration state for X.
 * @param ay Calibration state for Y.
 * @param raw_x Raw X input.
 * @param raw_y Raw Y input.
 * @param cfg Axis emission config (codes and inversion).
 * @param last_x Last emitted X value (updated).
 * @param last_y Last emitted Y value (updated).
 * @param dirty Dirty flag to mark when events are sent.
 * @return void.
 */
static inline void device_axes_process_pair(struct gamepad *gp,
                                            struct axis_state *ax,
                                            struct axis_state *ay,
                                            uint16_t raw_x,
                                            uint16_t raw_y,
                                            const struct device_axis_cfg *cfg,
                                            int *last_x,
                                            int *last_y,
                                            struct device_dirty_state *dirty)
{
    cal_update2(ax, ay, raw_x, raw_y);
    int x = cal_apply(ax, raw_x);
    int y = cal_apply(ay, raw_y);
    if (cfg->invert_x) x = -x;
    if (cfg->invert_y) y = -y;

    if (x != *last_x) {
        *last_x = x;
        gamepad_emit_abs(gp, cfg->abs_code_x, x);
        device_dirty_mark(dirty);
    }
    if (y != *last_y) {
        *last_y = y;
        gamepad_emit_abs(gp, cfg->abs_code_y, y);
        device_dirty_mark(dirty);
    }
}

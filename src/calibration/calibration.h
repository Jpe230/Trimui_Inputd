#pragma once

#include <stdbool.h>

struct axis_state {
    bool initialized;
    int samples;
    double center;
    double min;
    double max;
    double deadzone;
    int debug_id;
};

/**
 * Initialize a calibration axis state with default bounds.
 *
 * @param axis Axis calibration structure to reset.
 * @return void.
 */
void cal_init(struct axis_state *axis);

/**
 * Add a new raw sample to a single-axis calibration state.
 *
 * @param axis Axis calibration structure to update.
 * @param raw Raw input sample to incorporate.
 * @return void.
 */
void cal_update(struct axis_state *axis, int raw);

/**
 * Add new raw samples for a paired X/Y stick calibration state.
 *
 * @param ax Calibration data for the X axis.
 * @param ay Calibration data for the Y axis.
 * @param raw_x Raw X-axis sample to incorporate.
 * @param raw_y Raw Y-axis sample to incorporate.
 * @return void.
 */
void cal_update2(struct axis_state *ax, struct axis_state *ay, int raw_x, int raw_y);

/**
 * Apply calibration to a raw axis value.
 *
 * @param axis Calibration data to use for correction.
 * @param raw Raw input value.
 * @return Adjusted value after calibration.
 */
int cal_apply(struct axis_state *axis, int raw);

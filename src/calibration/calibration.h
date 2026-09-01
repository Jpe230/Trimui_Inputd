#pragma once
#include <stdbool.h>

struct axis_state {
    bool initialized;
    bool has_upstream_config;
    int samples;

    // Robust boot init
    int boot_count;
    double boot_buf[31];  // must match BOOT_SAMPLES in calibration.c

    // Learned calibration
    double center;

    // Soft-limit model: effective spans from center (not raw min/max)
    double neg_span;   // typical reach below center
    double pos_span;   // typical reach above center

    // Derived (debug/compat)
    double min;
    double max;

    // Noise-based deadzone in raw units
    double deadzone;

    // Upstream calibration, when supplied by a joypad.config file.
    double raw_min;
    double raw_max;
    double upstream_zero;
    double upstream_deadzone;

    // Filter + idle detection
    double filt;
    double prev_filt;
    int idle_count;

    // Idle noise estimate
    double noise_ema;

    // Parking recenter (fixes post-rotation joystick drift)
    double park_sum;
    int park_count;

    int debug_id;
};

void cal_init(struct axis_state *axis);
int cal_load_config(const char *path, struct axis_state *x, struct axis_state *y);
void cal_update(struct axis_state *axis, int raw);
void cal_update2(struct axis_state *ax, struct axis_state *ay, int raw_x, int raw_y);
int  cal_apply(struct axis_state *axis, int raw);
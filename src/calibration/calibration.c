#include "calibration.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define RAW_MIN            0.0
#define RAW_MAX            4095.0
#define RAW_CENTER_DEFAULT ((RAW_MAX - RAW_MIN) / 2.0)
#define OUTPUT_MAX         32767.0

#define DEADZONE_BASE      100.0    /* Small fixed deadzone to squash noise. */
#define DEADZONE_RATIO     0.06     /* Portion of the smallest half-span to use as deadzone. */
#define DEADZONE_LIMIT     400.0    /* Cap the deadzone so it never eats too much travel. */
#define MIN_ACTIVE_SPAN    64.0     /* Fallback span to avoid divide-by-zero when learning. */
#define START_WINDOW       1800.0   /* Initial assumed usable span on boot. */
#define DECAY_SPAN_MULT    1.3      /* Allow decay only while spans are near the assumed window. */

#define CENTER_WINDOW      32.0     /* How far from center we still consider the stick "at rest". */
#define CENTER_FILTER      0.05     /* EMA factor to slowly learn the resting center. */
#define EXTREME_DECAY_STEP 0.005    /* How quickly min/max crawl back toward center when idle. */

static double clampd(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool cal_debug_should_log_ms(long interval_ms, int slot)
{
    if (interval_ms <= 0) {
        return true;
    }
    static struct timespec last[2] = {{0, 0}, {0, 0}};
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct timespec *prev = &last[slot & 1];
    if (prev->tv_sec == 0 && prev->tv_nsec == 0) {
        *prev = now;
        return true;
    }
    long delta_ms = (long)((now.tv_sec - prev->tv_sec) * 1000L +
                           (now.tv_nsec - prev->tv_nsec) / 1000000L);
    if (delta_ms >= interval_ms) {
        *prev = now;
        return true;
    }
    return false;
}

void cal_init(struct axis_state *axis)
{
    axis->initialized = false;
    axis->samples = 0;
    axis->center = RAW_CENTER_DEFAULT;
    axis->min = RAW_MAX;
    axis->max = RAW_MIN;
    axis->deadzone = DEADZONE_BASE;
    axis->debug_id = 0;
}

void cal_update(struct axis_state *axis, int raw)
{
    double val = clampd((double)raw, RAW_MIN, RAW_MAX);

    if (!axis->initialized) {
        axis->initialized = true;
        axis->samples = 1;
        axis->center = val;
        double half = START_WINDOW * 0.5;
        axis->min = clampd(axis->center - half, RAW_MIN, RAW_MAX);
        axis->max = clampd(axis->center + half, RAW_MIN, RAW_MAX);
        axis->deadzone = DEADZONE_BASE;
        return;
    }

    axis->samples++;

    /* Capture a stable center whenever the stick looks idle. */
    double near_center_band = axis->deadzone + CENTER_WINDOW;
    if (fabs(val - axis->center) <= near_center_band) {
        axis->center = axis->center + (val - axis->center) * CENTER_FILTER;
    }

    /* Track observed extremes. */
    if (val < axis->min) axis->min = val;
    if (val > axis->max) axis->max = val;

    /* Keep bounds consistent with the current center. */
    if (axis->center < axis->min) axis->center = axis->min;
    if (axis->center > axis->max) axis->center = axis->max;

    /* Let extremes decay back toward center when we are idle near center. */
    if (fabs(val - axis->center) <= near_center_band) {
        double half = START_WINDOW * 0.5;
        double decay_cap = half * DECAY_SPAN_MULT;
        double lower_span_now = axis->center - axis->min;
        double upper_span_now = axis->max - axis->center;

        if (lower_span_now <= decay_cap) {
            double target_min = clampd(axis->center - half, RAW_MIN, axis->center);
            double new_min = fmin(axis->center, axis->min + EXTREME_DECAY_STEP);
            axis->min = fmin(new_min, target_min);
        }
        if (upper_span_now <= decay_cap) {
            double target_max = clampd(axis->center + half, axis->center, RAW_MAX);
            double new_max = fmax(axis->center, axis->max - EXTREME_DECAY_STEP);
            axis->max = fmax(new_max, target_max);
        }
    }

    double lower_span = axis->center - axis->min;
    double upper_span = axis->max - axis->center;
    double min_span = fmin(lower_span, upper_span);
    double dz = DEADZONE_BASE;
    if (min_span > 0.0) {
        double computed = min_span * DEADZONE_RATIO;
        dz = clampd(computed, DEADZONE_BASE, DEADZONE_LIMIT);
        double max_allowed = min_span * 0.5;
        if (max_allowed > DEADZONE_BASE && dz > max_allowed) {
            dz = max_allowed;
        }
    }
    axis->deadzone = dz;
}

void cal_update2(struct axis_state *ax, struct axis_state *ay, int raw_x, int raw_y)
{
    cal_update(ax, raw_x);
    cal_update(ay, raw_y);
}

int cal_apply(struct axis_state *axis, int raw)
{
    if (!axis->initialized) {
        return 0;
    }

    double val = clampd((double)raw, RAW_MIN, RAW_MAX);
    double center = axis->center;
    double deadzone = axis->deadzone;

    double lower_span = center - axis->min;
    double upper_span = axis->max - center;

    double pos_span = fmax(upper_span, MIN_ACTIVE_SPAN) - deadzone;
    double neg_span = fmax(lower_span, MIN_ACTIVE_SPAN) - deadzone;

    if (pos_span < 1.0) pos_span = 1.0;
    if (neg_span < 1.0) neg_span = 1.0;

    double out = 0.0;
    if (val > center + deadzone) {
        double norm = (val - center - deadzone) / pos_span;
        out = clampd(norm, 0.0, 1.0) * OUTPUT_MAX;
    } else if (val < center - deadzone) {
        double norm = (center - deadzone - val) / neg_span;
        out = -clampd(norm, 0.0, 1.0) * OUTPUT_MAX;
    }

    if (out > OUTPUT_MAX) out = OUTPUT_MAX;
    if (out < -OUTPUT_MAX) out = -OUTPUT_MAX;
    
    return (int)out;
}

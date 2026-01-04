#include "calibration.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>

// ADC characteristics
#define ADC_MIN 0.0
#define ADC_MAX 4095.0

// Initialization + spans
#define INIT_SPAN 900.0
#define MIN_SPAN 600.0

// Motion / stability
#define MOVE_THRESH 6.0
#define CENTER_WINDOW 160.0
#define STABLE_SAMPLES 100

// Edge learning
#define EDGE_BAND 24.0
#define EDGE_DWELL 35

// Learning gains
#define CENTER_BETA 0.001
#define NOISE_GAMMA 0.03125

// Deadzone shaping (raw counts)
#define DZ_K 4.0
#define DZ_BIAS 8.0
#define DZ_MIN 60.0
#define DZ_MAX 220.0
#define DZ_HYST 35.0
#define DZ_DECAY_STEP 0.05

// Filtering
#define FILTER_ALPHA 0.25

// Fast snap-to-center
#define SNAP_MARGIN 8.0
#define GLITCH_JUMP 300.0
#define REV_THRESH 2.0
#define ORTHO_MIN 300.0
#define ORTHO_SCALE 3.0
#define MOVE_THRESH_FAST 3.0
#define CENTER_LEARN_WINDOW 16.0
#define NOISE_LEARN_WINDOW 12.0
#define CENTER_IDLE_SAMPLES 400
#define CENTER_TRIM_IDLE_SAMPLES 200

// Internal limits
#define NOISE_INIT 8.0
#define MAX_AXES 4
#define EDGE_VEL_MAX 2.0
#define PEAK_SLEW 0.2
#define VEL_IDLE_MAX 2.0
#define CENTER_TRIM_ALPHA 0.002

struct axis_internal {
    struct axis_state *axis;
    int prev1;
    int prev2;
    double ema;
    double last_filt;
    double last_med;
    double last_fast;
    bool filt_initialized;
    double noise;
    int stable_count;
    double min_cand;
    double max_cand;
    int min_dwell;
    int max_dwell;
    double peak_min;
    double peak_max;
    int was_away;
    int zero_latched;
    int idle_center_count;
    int idle_latched_count;
    double prev_fast_med;
};

static struct axis_internal axis_table[MAX_AXES];

static double clamp_double(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int median3(int a, int b, int c)
{
    if (a > b) { int t = a; a = b; b = t; }
    if (b > c) { int t = b; b = c; c = t; }
    if (a > b) { int t = a; a = b; b = t; }
    return b;
}

static struct axis_internal *get_axis_slot(struct axis_state *axis)
{
    struct axis_internal *empty = NULL;
    for (int i = 0; i < MAX_AXES; i++) {
        if (axis_table[i].axis == axis) {
            return &axis_table[i];
        }
        if (!axis_table[i].axis && !empty) {
            empty = &axis_table[i];
        }
    }
    if (empty) {
        empty->axis = axis;
        return empty;
    }
    // Fallback: reuse first slot if all are occupied (should not happen with <=4 axes).
    axis_table[0].axis = axis;
    return &axis_table[0];
}

static double filter_sample(struct axis_internal *state, int raw)
{
    if (!state->filt_initialized) {
        state->prev1 = raw;
        state->prev2 = raw;
        state->ema = raw;
        state->last_filt = raw;
        state->last_med = raw;
        state->last_fast = raw;
        state->filt_initialized = true;
        return state->ema;
    }

    int med = median3(raw, state->prev1, state->prev2);
    state->prev2 = state->prev1;
    state->prev1 = raw;
    state->last_med = med;

    state->ema += FILTER_ALPHA * ((double)med - state->ema);
    state->last_filt = state->ema;
    return state->ema;
}

static int map_output(double filt, double center, double min, double max, double dz)
{
    // Snap full-scale near end-stops to avoid jitter at the extremes.
    if (filt >= max - EDGE_BAND) {
        return 32767;
    }
    if (filt <= min + EDGE_BAND) {
        return -32768;
    }

    double centered = filt - center;
    if (fabs(centered) <= dz + SNAP_MARGIN) {
        return 0;
    }

    double y = 0.0;
    if (centered > 0.0) {
        double num = filt - (center + dz);
        double den = max - (center + dz);
        if (den <= 0.0) {
            return 0;
        }
        y = (num / den) * 32767.0;
    } else {
        double num = (center - dz) - filt;
        double den = (center - dz) - min;
        if (den <= 0.0) {
            return 0;
        }
        y = -(num / den) * 32768.0;
    }

    if (y > 32767.0) y = 32767.0;
    if (y < -32768.0) y = -32768.0;
    return (int)y;
}

static void commit_peaks(struct axis_state *axis, struct axis_internal *st)
{
    if (st->peak_min < axis->min) {
        axis->min += PEAK_SLEW * (st->peak_min - axis->min);
    }
    if (st->peak_max > axis->max) {
        axis->max += PEAK_SLEW * (st->peak_max - axis->max);
    }
    st->peak_min = axis->min;
    st->peak_max = axis->max;
}

struct edge_ctx {
    double fast;
    double prev_fast;
    double offset;
    double dr_fast;
    int away;
    int glitch;
};

// Run filtering + stability/center/noise learning; return 1 when ready for min/max learning.
static int cal_stage(struct axis_state *axis, struct axis_internal *st, int raw, struct edge_ctx *ctx)
{
    if (!axis->initialized) {
        axis->center = raw;
        axis->min = clamp_double(raw - INIT_SPAN, ADC_MIN, ADC_MAX);
        axis->max = clamp_double(raw + INIT_SPAN, ADC_MIN, ADC_MAX);
        axis->deadzone = DZ_MIN;
        axis->samples = 0;
        axis->initialized = true;

        st->axis = axis;
        st->prev1 = raw;
        st->prev2 = raw;
        st->ema = raw;
        st->last_filt = raw;
        st->last_med = raw;
        st->last_fast = raw;
        st->filt_initialized = true;
        st->noise = NOISE_INIT;
        st->stable_count = 0;
        st->peak_min = axis->min;
        st->peak_max = axis->max;
        st->was_away = 0;
        st->zero_latched = 0;
        st->idle_center_count = 0;
        st->idle_latched_count = 0;
        st->prev_fast_med = raw;
        st->min_cand = axis->min;
        st->max_cand = axis->max;
        st->min_dwell = 0;
        st->max_dwell = 0;

        ctx->fast = raw;
        ctx->prev_fast = raw;
        ctx->offset = 0.0;
        ctx->dr_fast = 0.0;
        ctx->away = 0;
        ctx->glitch = 0;

        axis->samples++;
        return 0;
    }

    filter_sample(st, raw);

    double prev_fast = st->last_fast;

    // Compute motion from the *actual* new sample before any glitch substitution.
    // This prevents "glitch reject" from making motion look artificially stable.
    double dr_raw = fabs((double)raw - prev_fast);
    int glitch = (dr_raw > GLITCH_JUMP);

    // Use a low-jitter fast signal for calibration decisions.
    double fast = st->last_med;

    // If we detected a glitch-level jump, don't trust this sample for learning.
    if (glitch) {
        st->stable_count = 0;
    }

    double d_fast = fast - axis->center;
    double dr_fast = fast - prev_fast;

    // stable_motion gates learning; use dr_raw (pre-reject) so fast moves never look "stable".
    int stable_motion = 0;
    if (dr_raw > MOVE_THRESH_FAST || glitch) {
        st->stable_count = 0;
    } else {
        st->stable_count++;
        if (st->stable_count >= STABLE_SAMPLES) {
            stable_motion = 1;
        }
    }

    // Only learn center after sustained idle near center and very low velocity.
    if (stable_motion && fabs(d_fast) <= axis->deadzone && fabs(dr_fast) < 1.0) {
        st->idle_center_count++;
    } else {
        st->idle_center_count = 0;
    }
    if (st->idle_center_count >= CENTER_IDLE_SAMPLES) {
        double delta = CENTER_BETA * d_fast;
        if (delta > 0.5) delta = 0.5;
        if (delta < -0.5) delta = -0.5;
        axis->center += delta;
    }

    if (stable_motion && fabs(d_fast) <= NOISE_LEARN_WINDOW) {
        double noise_err = fabs(d_fast) - st->noise;
        st->noise += NOISE_GAMMA * noise_err;
        if (st->noise < 0.0) st->noise = 0.0;

        double target_dz = clamp_double(DZ_K * st->noise + DZ_BIAS, DZ_MIN, DZ_MAX);
        if (target_dz > axis->deadzone) {
            axis->deadzone = target_dz;
        } else {
            axis->deadzone -= DZ_DECAY_STEP;
            if (axis->deadzone < target_dz) {
                axis->deadzone = target_dz;
            }
        }
    }

    double offset = fast - axis->center;
    double away_thresh = 2.0 * axis->deadzone;

    ctx->fast = fast;
    ctx->prev_fast = prev_fast;
    ctx->offset = offset;
    ctx->dr_fast = dr_fast;
    ctx->away = fabs(offset) > away_thresh;
    ctx->glitch = glitch;

    return 1;
}

static void apply_minmax_with_gate(struct axis_state *axis, struct axis_internal *st, const struct edge_ctx *ctx, int gate_ok)
{
    if (ctx->glitch) {
        st->was_away = ctx->away;
        st->last_fast = ctx->fast;
        axis->samples++;
        return;
    }

    if (!gate_ok) {
        st->was_away = 0;
        st->peak_min = axis->min;
        st->peak_max = axis->max;
        st->last_fast = ctx->fast;
        st->min_cand = axis->min;
        st->max_cand = axis->max;
        st->min_dwell = 0;
        st->max_dwell = 0;
        axis->samples++;
        return;
    }

    if (ctx->away) {
        st->peak_min = fmin(st->peak_min, ctx->fast);
        st->peak_max = fmax(st->peak_max, ctx->fast);

        int slow_edge = fabs(ctx->dr_fast) < EDGE_VEL_MAX;

        if (ctx->fast < st->min_cand) {
            st->min_cand = ctx->fast;
            st->min_dwell = 0;
        } else if (ctx->fast <= st->min_cand + EDGE_BAND) {
            if (slow_edge) {
                st->min_dwell++;
            }
        } else {
            st->min_cand = ctx->fast;
            st->min_dwell = 0;
        }

        if (ctx->fast > st->max_cand) {
            st->max_cand = ctx->fast;
            st->max_dwell = 0;
        } else if (ctx->fast >= st->max_cand - EDGE_BAND) {
            if (slow_edge) {
                st->max_dwell++;
            }
        } else {
            st->max_cand = ctx->fast;
            st->max_dwell = 0;
        }

        if (st->min_dwell >= EDGE_DWELL) {
            commit_peaks(axis, st);
            st->min_dwell = 0;
            st->min_cand = axis->min;
        }
        if (st->max_dwell >= EDGE_DWELL) {
            commit_peaks(axis, st);
            st->max_dwell = 0;
            st->max_cand = axis->max;
        }

        if (st->was_away &&
            fabs(ctx->dr_fast) > REV_THRESH &&
            ((ctx->offset > 0.0 && ctx->dr_fast < 0.0) || (ctx->offset < 0.0 && ctx->dr_fast > 0.0))) {
            commit_peaks(axis, st);
        }
    } else if (st->was_away) {
        commit_peaks(axis, st);
        st->min_cand = axis->min;
        st->max_cand = axis->max;
        st->min_dwell = 0;
        st->max_dwell = 0;
    }

    st->was_away = ctx->away;
    st->last_fast = ctx->fast;

    axis->min = fmin(axis->min, axis->center - MIN_SPAN);
    axis->max = fmax(axis->max, axis->center + MIN_SPAN);
    axis->min = fmin(axis->min, axis->center - INIT_SPAN);
    axis->max = fmax(axis->max, axis->center + INIT_SPAN);

    axis->min = clamp_double(axis->min, ADC_MIN, ADC_MAX);
    axis->max = clamp_double(axis->max, ADC_MIN, ADC_MAX);

    axis->samples++;
}

void cal_init(struct axis_state *axis)
{
    struct axis_internal *st = get_axis_slot(axis);
    st->axis = axis;
    st->prev1 = 0;
    st->prev2 = 0;
    st->ema = 0.0;
    st->last_filt = 0.0;
    st->last_med = 0.0;
    st->last_fast = 0.0;
    st->filt_initialized = false;
    st->noise = NOISE_INIT;
    st->stable_count = 0;
    st->min_cand = 0.0;
    st->max_cand = 0.0;
    st->min_dwell = 0;
    st->max_dwell = 0;
    st->peak_min = 0.0;
    st->peak_max = 0.0;
    st->was_away = 0;
    st->zero_latched = 0;
    st->idle_center_count = 0;
    st->idle_latched_count = 0;
    st->prev_fast_med = 0.0;

    axis->initialized = false;
    axis->samples = 0;
    axis->center = 0.0;
    axis->min = 0.0; // min/max are undefined until first sample
    axis->max = 0.0; // min/max are undefined until first sample
    axis->deadzone = 0.05; // initial fraction placeholder
}

void cal_update(struct axis_state *axis, int raw)
{
    struct axis_internal *st = get_axis_slot(axis);
    struct edge_ctx ctx;
    if (!cal_stage(axis, st, raw, &ctx)) {
        return;
    }
    apply_minmax_with_gate(axis, st, &ctx, 1);
}

static int ortho_gate_pass(const struct axis_state *other, const struct edge_ctx *other_ctx)
{
    if (!other || !other_ctx) {
        return 1;
    }
    double window = ORTHO_MIN;
    if (other->initialized) {
        double scaled = ORTHO_SCALE * other->deadzone;
        if (scaled > window) {
            window = scaled;
        }
    }
    return fabs(other_ctx->fast - other->center) < window;
}

void cal_update2(struct axis_state *ax, struct axis_state *ay, int raw_x, int raw_y)
{
    struct axis_internal *stx = get_axis_slot(ax);
    struct axis_internal *sty = get_axis_slot(ay);
    struct edge_ctx cx;
    struct edge_ctx cy;

    int ready_x = cal_stage(ax, stx, raw_x, &cx);
    int ready_y = cal_stage(ay, sty, raw_y, &cy);

    if (ready_x) {
        int gate_x = ortho_gate_pass(ay, &cy);
        apply_minmax_with_gate(ax, stx, &cx, gate_x);
    }
    if (ready_y) {
        int gate_y = ortho_gate_pass(ax, &cx);
        apply_minmax_with_gate(ay, sty, &cy, gate_y);
    }
}

int cal_apply(struct axis_state *axis, int raw)
{
    struct axis_internal *st = get_axis_slot(axis);
    
    // Keep calibration smoothing separate; only seed filter if it hasn't run yet.
    if (!st->filt_initialized) {
        filter_sample(st, raw);
    }

    // Use the most recent median (no EMA) for responsive output while
    // calibration continues to use the smoothed EMA path in cal_update.
    double filt = st->last_med;

    if (!axis->initialized) {
        double center = raw;
        double min = clamp_double(center - INIT_SPAN, ADC_MIN, ADC_MAX);
        double max = clamp_double(center + INIT_SPAN, ADC_MIN, ADC_MAX);
        double dz = DZ_MIN;
        st->prev_fast_med = filt;
        return map_output(filt, center, min, max, dz);
    }

    double off = filt - axis->center;
    double dz_in = axis->deadzone + SNAP_MARGIN;
    double dz_out = dz_in + DZ_HYST;
    double vel = fabs(filt - st->prev_fast_med);

    if (st->zero_latched) {
        if (fabs(off) <= dz_out) {
            if (vel <= VEL_IDLE_MAX) {
                st->idle_latched_count++;
                if (st->idle_latched_count >= CENTER_TRIM_IDLE_SAMPLES) {
                    axis->center += CENTER_TRIM_ALPHA * off;
                }
            } else {
                st->idle_latched_count = 0;
            }
            st->prev_fast_med = filt;
            return 0;
        }
        st->zero_latched = 0;
        st->idle_latched_count = 0;
    } else if (fabs(off) <= dz_in) {
        st->zero_latched = 1;
        st->idle_latched_count = 0;
        st->prev_fast_med = filt;
        return 0;
    }

    st->idle_latched_count = 0;
    st->prev_fast_med = filt;

    return map_output(filt, axis->center, axis->min, axis->max, axis->deadzone);
}

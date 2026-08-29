/* calibration.c */
#include "calibration.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h> // qsort
#include <string.h> // memset

#define RAW_MIN            0.0
#define RAW_MAX            4096.0
#define RAW_CENTER_DEFAULT ((RAW_MAX - RAW_MIN) / 2.0)
#define OUTPUT_MAX         32767.0

#define BOOT_SAMPLES 31   // ~0.5s at 60Hz

// --- Tuning for ~60 Hz loop ---
// Input filtering (helps velocity detection & spike resistance)
#define FILTER_ALPHA       0.25     // 0.15..0.35

// "True idle" gating (prevents center drift while moving slowly through center)
#define CENTER_LEARN_BAND  20.0     // raw units from center
#define VEL_THRESH         4.0      // raw units per sample (after filtering)
#define IDLE_REQUIRED      12       // ~200ms at 60Hz
#define CENTER_ALPHA       0.003    // slow center tracking only when idle

// Deadzone based on measured idle jitter (stable vs span changes)
#define DEADZONE_NOISE_BASE  6.0
#define NOISE_ALPHA          0.08
#define NOISE_MULT           6.0
#define DEADZONE_USER_MIN 250.0 // suppress persistent TMR-stick idle drift
#define DEADZONE_USER_MAX    250.0

// Effective range (soft-limit) learning
#define START_HALF_SPAN     900.0   // initial guess (roughly START_WINDOW/2)
#define MIN_ACTIVE_SPAN     80.0    // avoid tiny span
#define OUTER_GATE_RATIO    0.65    // only learn spans when far out
#define SPAN_ALPHA_UP       0.0020  // slow growth (adapts over seconds)
#define SPAN_ALPHA_DOWN     0.0005  // very slow shrink
#define SPAN_MAX            2400.0  // safety cap (raw units)

// Output soft knee (makes edge less harsh even though we clamp)
#define KNEE_START          0.90    // start softening near 90%

#define PARK_BAND        100.0   // raw units around center to consider "parked"
#define PARK_REQUIRED    60      // ~1s at 60Hz
#define PARK_ALPHA       0.06    // how fast center moves once parked (0.01..0.05)

static double clampd(double v, double lo, double hi)
{
    if (!isfinite(v)) return lo;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

static double median_of(double *buf, int n)
{
    qsort(buf, (size_t)n, sizeof(double), cmp_double);
    if (n & 1) return buf[n / 2];
    return 0.5 * (buf[n / 2 - 1] + buf[n / 2]);
}

// smootherstep: 0..1 -> 0..1 with zero slope at both ends
static double smootherstep(double t)
{
    t = clampd(t, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Apply a "soft knee" near 1.0 (keeps f(1)=1, but slope goes to 0 at the edge)
static double soft_knee_0to1(double a)
{
    a = clampd(a, 0.0, 1.0);
    if (a <= KNEE_START) return a;
    double t = (a - KNEE_START) / (1.0 - KNEE_START);   // 0..1
    double s = smootherstep(t);
    return KNEE_START + (1.0 - KNEE_START) * s;
}

static void recompute_minmax(struct axis_state *a)
{
    a->neg_span = clampd(a->neg_span, MIN_ACTIVE_SPAN, SPAN_MAX);
    a->pos_span = clampd(a->pos_span, MIN_ACTIVE_SPAN, SPAN_MAX);

    a->center = clampd(a->center, RAW_MIN, RAW_MAX);

    a->min = clampd(a->center - a->neg_span, RAW_MIN, a->center);
    a->max = clampd(a->center + a->pos_span, a->center, RAW_MAX);
}

void cal_init(struct axis_state *a)
{
    memset(a, 0, sizeof(*a));

    a->initialized = false;
    a->has_upstream_config = false;
    a->samples = 0;

    a->boot_count = 0;
    for (int i = 0; i < BOOT_SAMPLES; i++) a->boot_buf[i] = RAW_CENTER_DEFAULT;

    a->center = RAW_CENTER_DEFAULT;

    a->neg_span = START_HALF_SPAN;
    a->pos_span = START_HALF_SPAN;

    a->deadzone = DEADZONE_USER_MIN;
    a->raw_min = RAW_MIN;
    a->raw_max = RAW_MAX;
    a->upstream_zero = RAW_CENTER_DEFAULT;
    a->upstream_deadzone = 0.0;

    a->filt = RAW_CENTER_DEFAULT;
    a->prev_filt = RAW_CENTER_DEFAULT;
    a->idle_count = 0;

    a->noise_ema = 0.0;

    a->park_sum = 0.0;
    a->park_count = 0;

    recompute_minmax(a);
}

struct upstream_config {
    double x_min, x_max, y_min, y_max;
    double x_zero, y_zero, deadzone;
    bool seen_x_min, seen_x_max, seen_y_min, seen_y_max;
    bool seen_x_zero, seen_y_zero, seen_deadzone;
};

static bool valid_upstream_axis(double min, double max, double zero)
{
    return isfinite(min) && isfinite(max) && isfinite(zero) &&
           min >= RAW_MIN && max <= RAW_MAX && min < zero && zero < max;
}

int cal_load_config(const char *path, struct axis_state *x, struct axis_state *y)
{
    struct upstream_config cfg = {0};
    FILE *fp = fopen(path, "r");
    if (!fp) return 1;

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        char key[32];
        double value;
        if (sscanf(line, " %31[^= ] = %lf", key, &value) != 2) continue;
        if (strcmp(key, "x_min") == 0) { cfg.x_min = value; cfg.seen_x_min = true; }
        else if (strcmp(key, "x_max") == 0) { cfg.x_max = value; cfg.seen_x_max = true; }
        else if (strcmp(key, "y_min") == 0) { cfg.y_min = value; cfg.seen_y_min = true; }
        else if (strcmp(key, "y_max") == 0) { cfg.y_max = value; cfg.seen_y_max = true; }
        else if (strcmp(key, "x_zero") == 0) { cfg.x_zero = value; cfg.seen_x_zero = true; }
        else if (strcmp(key, "y_zero") == 0) { cfg.y_zero = value; cfg.seen_y_zero = true; }
        else if (strcmp(key, "deadzone") == 0) { cfg.deadzone = value; cfg.seen_deadzone = true; }
    }
    fclose(fp);

    bool complete = cfg.seen_x_min && cfg.seen_x_max && cfg.seen_y_min && cfg.seen_y_max &&
                    cfg.seen_x_zero && cfg.seen_y_zero && cfg.seen_deadzone;
    bool valid = complete && cfg.deadzone >= 0.0 && cfg.deadzone < 1.0 &&
                 valid_upstream_axis(cfg.x_min, cfg.x_max, cfg.x_zero) &&
                 valid_upstream_axis(cfg.y_min, cfg.y_max, cfg.y_zero);
    if (!valid) return -1;

    x->has_upstream_config = true;
    x->initialized = true;
    x->raw_min = cfg.x_min;
    x->raw_max = cfg.x_max;
    x->upstream_zero = cfg.x_zero;
    x->upstream_deadzone = cfg.deadzone;
    y->has_upstream_config = true;
    y->initialized = true;
    y->raw_min = cfg.y_min;
    y->raw_max = cfg.y_max;
    y->upstream_zero = cfg.y_zero;
    y->upstream_deadzone = cfg.deadzone;
    return 0;
}

void cal_update(struct axis_state *a, int raw)
{
    if (a->has_upstream_config) return;
    double val = clampd((double)raw, RAW_MIN, RAW_MAX);

    // Boot phase: collect BOOT_SAMPLES, then init from median center.
    if (!a->initialized) {
        if (a->boot_count < BOOT_SAMPLES) {
            a->boot_buf[a->boot_count++] = val;
            return; // keep output at 0 during boot
        }

        a->initialized = true;
        a->samples = BOOT_SAMPLES;

        a->center = median_of(a->boot_buf, BOOT_SAMPLES);
        a->filt = a->center;
        a->prev_filt = a->center;

        a->neg_span = START_HALF_SPAN;
        a->pos_span = START_HALF_SPAN;

        a->deadzone = DEADZONE_USER_MIN;
        a->noise_ema = 0.0;
        a->idle_count = 0;

        recompute_minmax(a);
        return;
    }

    a->samples++;

    // Filter for stability
    a->filt = a->filt + FILTER_ALPHA * (val - a->filt);
    double vel = fabs(a->filt - a->prev_filt);
    a->prev_filt = a->filt;

    // True idle detection
    bool near_center = fabs(a->filt - a->center) <= CENTER_LEARN_BAND;
    bool slow = vel <= VEL_THRESH;

    if (near_center && slow) a->idle_count++;
    else a->idle_count = 0;

    bool idle = a->idle_count >= IDLE_REQUIRED;

    // When idle: learn center + noise-based deadzone
    if (idle) {
        a->center = a->center + CENTER_ALPHA * (a->filt - a->center);

        double dev = fabs(a->filt - a->center);
        a->noise_ema = a->noise_ema + NOISE_ALPHA * (dev - a->noise_ema);

        double dz = DEADZONE_NOISE_BASE + NOISE_MULT * a->noise_ema;
        a->deadzone = clampd(dz, DEADZONE_USER_MIN, DEADZONE_USER_MAX);
    }

    // Park center to possibly fix stick drift after rotation
    bool park_near = fabs(a->filt - a->center) <= PARK_BAND;
    bool park_slow = vel <= VEL_THRESH;

    if (park_near && park_slow) {
        a->park_sum += a->filt;
        a->park_count++;

        if (a->park_count >= PARK_REQUIRED) {
            double target = a->park_sum / (double)a->park_count;
            a->center = a->center + PARK_ALPHA * (target - a->center);

            // reset so it keeps refining if it stays parked
            a->park_sum = 0.0;
            a->park_count = 0;
        }
    } else {
        a->park_sum = 0.0;
        a->park_count = 0;
    }

    // Learn effective spans only when far from center
    double d = a->filt - a->center;

    if (d > 0.0) {
        double gate = a->pos_span * OUTER_GATE_RATIO;
        if (d >= gate) {
            double cand = d;
            double alpha = (cand > a->pos_span) ? SPAN_ALPHA_UP : SPAN_ALPHA_DOWN;
            a->pos_span = a->pos_span + alpha * (cand - a->pos_span);
        }
    } else if (d < 0.0) {
        double ad = -d;
        double gate = a->neg_span * OUTER_GATE_RATIO;
        if (ad >= gate) {
            double cand = ad;
            double alpha = (cand > a->neg_span) ? SPAN_ALPHA_UP : SPAN_ALPHA_DOWN;
            a->neg_span = a->neg_span + alpha * (cand - a->neg_span);
        }
    }

    recompute_minmax(a);
}

void cal_update2(struct axis_state *ax, struct axis_state *ay, int raw_x, int raw_y)
{
    cal_update(ax, raw_x);
    cal_update(ay, raw_y);
}

int cal_apply(struct axis_state *a, int raw)
{
    if (!a->initialized) return 0;

    if (a->has_upstream_config) {
        double val = clampd((double)raw, a->raw_min, a->raw_max);
        double normalized = (val >= a->upstream_zero)
            ? (val - a->upstream_zero) / (a->raw_max - a->upstream_zero)
            : (val - a->upstream_zero) / (a->upstream_zero - a->raw_min);
        double magnitude = fabs(normalized);
        if (magnitude <= a->upstream_deadzone) return 0;
        magnitude = (magnitude - a->upstream_deadzone) / (1.0 - a->upstream_deadzone);
        return (int)lrint((normalized < 0.0 ? -magnitude : magnitude) * OUTPUT_MAX);
    }

    double val = clampd((double)raw, RAW_MIN, RAW_MAX);
    double d = val - a->center;

    double dz = a->deadzone;

    // Deadzone
    double ad = fabs(d);
    if (ad <= dz) return 0;

    // Normalize using effective span (minus deadzone)
    double span = (d >= 0.0) ? a->pos_span : a->neg_span;
    span = fmax(span, MIN_ACTIVE_SPAN);

    double denom = span - dz;
    if (denom < 1.0) denom = 1.0;

    double a_norm = (ad - dz) / denom;     // nominally 0..1+
    a_norm = clampd(a_norm, 0.0, 1.0);     // clamp to max output (no rescale from extra force)

    // soften near edge
    a_norm = soft_knee_0to1(a_norm);

    double out = a_norm * OUTPUT_MAX;
    if (d < 0.0) out = -out;

    if (out > OUTPUT_MAX) out = OUTPUT_MAX;
    if (out < -OUTPUT_MAX) out = -OUTPUT_MAX;

    return (int)lrint(out);
}

#pragma once

#include <signal.h>
#include <stdbool.h>

struct gamepad;
struct axis_state;

struct device_ops {
    const char *name;
    int (*init)(void **ctx,
                struct gamepad *gp,
                struct axis_state *lx,
                struct axis_state *ly,
                struct axis_state *rx,
                struct axis_state *ry,
                bool verbose);
    bool (*poll)(void *ctx);
    void (*close)(void *ctx);
};

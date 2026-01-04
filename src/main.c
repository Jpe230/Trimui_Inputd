#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/input.h>

#include "calibration/calibration.h"
#include "devices/device.h"
#include "devices/brick/brick.h"
#include "devices/smart_pro/smart_pro.h"
#include "devices/smart_pro_s/smart_pro_s.h"
#include "gamepad/uinput.h"

static volatile sig_atomic_t g_running = 1;
static bool g_verbose = false;
static const useconds_t LOOP_US = 4000;

enum variant {
    VAR_SMART_PRO_S,
    VAR_SMART_PRO,
    VAR_BRICK
};

static enum variant detect_variant(void)
{
    if (access("/dev/ttyAS5", F_OK) == 0) return VAR_SMART_PRO_S;
    if (access("/dev/ttyS4", F_OK) == 0) return VAR_SMART_PRO;
    return VAR_BRICK;
}

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char **argv)
{

    (void)argc;
    (void)argv;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    enum variant variant = detect_variant();

    const struct gamepad_desc      *gp_desc = &SMART_PRO_GAMEPAD_DESC;
    if (variant == VAR_SMART_PRO_S) gp_desc = &SMART_PRO_S_GAMEPAD_DESC;
    else if (variant == VAR_BRICK)  gp_desc = &BRICK_GAMEPAD_DESC;

    struct gamepad *gp = gamepad_init(gp_desc);
    if (!gp) {
        fprintf(stderr, "Failed to init uinput\n");
        return 1;
    }

    struct axis_state lx, ly, rx, ry;

    cal_init(&lx);
    cal_init(&ly);
    cal_init(&rx);
    cal_init(&ry);

    const struct device_ops ops_table[] = {
        [VAR_SMART_PRO_S] = {.name = "Smart Pro S", .init = smart_pro_s_init, .poll = smart_pro_s_poll, .close = smart_pro_s_close},
        [VAR_SMART_PRO]   = {.name = "Smart Pro",   .init = smart_pro_init,   .poll = smart_pro_poll,   .close = smart_pro_close  },
        [VAR_BRICK]       = {.name = "Brick",       .init = brick_init,       .poll = brick_poll,       .close = brick_close      }
    };

    void *dev_ctx = NULL;
    if (ops_table[variant].init(&dev_ctx, gp, &lx, &ly, &rx, &ry, g_verbose) < 0) {
        gamepad_destroy(gp);
        return 1;
    }

    while (g_running) {
        if (!ops_table[variant].poll(dev_ctx)) {
            break;
        }
        usleep(LOOP_US);
    }

    ops_table[variant].close(dev_ctx);
    gamepad_destroy(gp);
    return 0;
}

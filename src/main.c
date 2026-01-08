#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <linux/input.h>

#include "calibration/calibration.h"
#include "devices/device.h"
#include "devices/brick/brick.h"
#include "devices/smart_pro/smart_pro.h"
#include "devices/smart_pro_s/smart_pro_s.h"
#include "devices/turbo.h"
#include "gamepad/uinput.h"

static volatile sig_atomic_t g_running = 1;
static bool g_verbose = false;
static const useconds_t LOOP_US = 16000;

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

struct flag_poll_args {
    enum variant variant;
    void *dev_ctx;
};

static const struct device_ops OPS_TABLE[] = {
    [VAR_SMART_PRO_S] = {.name = "Smart Pro S", .init = smart_pro_s_init, .poll = smart_pro_s_poll, .close = smart_pro_s_close},
    [VAR_SMART_PRO]   = {.name = "Smart Pro",   .init = smart_pro_init,   .poll = smart_pro_poll,   .close = smart_pro_close  },
    [VAR_BRICK]       = {.name = "Brick",       .init = brick_init,       .poll = brick_poll,       .close = brick_close      }
};

static void poll_variant_flags(const struct flag_poll_args *args)
{
    switch (args->variant) {
    case VAR_SMART_PRO: {
        struct smart_pro_device *dev = args->dev_ctx;
        if (dev) {
            turbo_refresh_flags(dev->turbo, dev->turbo_count);
        }
        break;
    }
    case VAR_SMART_PRO_S: {
        struct smart_pro_s_device *dev = args->dev_ctx;
        if (dev) {
            turbo_refresh_flags(dev->turbo, dev->turbo_count);
        }
        break;
    }
    case VAR_BRICK: {
        struct brick_state *st = args->dev_ctx;
        if (st) {
            brick_refresh_dpad_flags(st);
            turbo_refresh_flags(st->turbo, st->turbo_count);
        }
        break;
    }
    }
}

static void *flag_thread_fn(void *arg)
{
    struct flag_poll_args *args = arg;
    while (g_running) {
        poll_variant_flags(args);
        sleep(1);
    }
    return NULL;
}

static int daemonize_process(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() == -1) {
        perror("setsid");
        return -1;
    }

    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &sa, NULL);
    return 0;
}

static void install_signal_handlers(void)
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
}

static const struct gamepad_desc *select_gamepad_desc(enum variant variant)
{
    switch (variant) {
    case VAR_SMART_PRO_S: return &SMART_PRO_S_GAMEPAD_DESC;
    case VAR_BRICK:       return &BRICK_GAMEPAD_DESC;
    case VAR_SMART_PRO:
    default:              return &SMART_PRO_GAMEPAD_DESC;
    }
}

static void init_axes(struct axis_state *lx, struct axis_state *ly, struct axis_state *rx, struct axis_state *ry)
{
    lx->debug_id = 0;
    ly->debug_id = 1;
    rx->debug_id = 3;
    ry->debug_id = 4;

    cal_init(lx);
    cal_init(ly);
    cal_init(rx);
    cal_init(ry);
}

static int start_flag_thread(pthread_t *thread, struct flag_poll_args *args)
{
    if (pthread_create(thread, NULL, flag_thread_fn, args) != 0) {
        perror("pthread_create");
        return -1;
    }
    return 0;
}

static int init_device_for_variant(enum variant variant,
                                   void **dev_ctx,
                                   struct gamepad *gp,
                                   struct axis_state *lx,
                                   struct axis_state *ly,
                                   struct axis_state *rx,
                                   struct axis_state *ry)
{
    if (OPS_TABLE[variant].init(dev_ctx, gp, lx, ly, rx, ry, g_verbose) < 0) {
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{

    (void)argc;
    (void)argv;

    if (daemonize_process() < 0) {
        return EXIT_FAILURE;
    }

    install_signal_handlers();

    enum variant variant = detect_variant();

    const struct gamepad_desc *gp_desc = select_gamepad_desc(variant);

    struct gamepad *gp = gamepad_init(gp_desc);
    if (!gp) {
        fprintf(stderr, "Failed to init uinput\n");
        return 1;
    }

    struct axis_state lx, ly, rx, ry;

    init_axes(&lx, &ly, &rx, &ry);

    const struct device_ops *ops = &OPS_TABLE[variant];

    void *dev_ctx = NULL;
    if (init_device_for_variant(variant, &dev_ctx, gp, &lx, &ly, &rx, &ry) < 0) {
        gamepad_destroy(gp);
        return 1;
    }

    struct flag_poll_args flag_args = {.variant = variant, .dev_ctx = dev_ctx};
    poll_variant_flags(&flag_args);
    pthread_t flag_thread;
    if (start_flag_thread(&flag_thread, &flag_args) < 0) {
        ops->close(dev_ctx);
        gamepad_destroy(gp);
        return 1;
    }

    while (g_running) {
        if (!ops->poll(dev_ctx)) {
            break;
        }
        usleep(LOOP_US);
    }

    pthread_join(flag_thread, NULL);
    ops->close(dev_ctx);
    gamepad_destroy(gp);
    return 0;
}

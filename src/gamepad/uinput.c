#include "uinput.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

struct gamepad {
    int fd;
};

static void set_abs(int fd, const struct gamepad_abs_desc *axis)
{
    struct uinput_abs_setup abs = {
        .code = axis->code,
        .absinfo = {
            .minimum = axis->min,
            .maximum = axis->max,
            .fuzz = axis->fuzz,
            .flat = axis->flat,
            .resolution = axis->resolution,
        },
    };
    ioctl(fd, UI_SET_ABSBIT, axis->code);
    ioctl(fd, UI_ABS_SETUP, &abs);
}

struct gamepad *gamepad_init(const struct gamepad_desc *desc)
{
    if (!desc || !desc->name) {
        return NULL;
    }

    int fd = open("/dev/uinput", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open /dev/uinput");
        return NULL;
    }

    if (desc->key_count > 0) {
        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        for (size_t i = 0; i < desc->key_count; ++i) {
            ioctl(fd, UI_SET_KEYBIT, desc->keys[i]);
        }
    }

    if (desc->axis_count > 0) {
        ioctl(fd, UI_SET_EVBIT, EV_ABS);
        for (size_t i = 0; i < desc->axis_count; ++i) {
            set_abs(fd, &desc->axes[i]);
        }
    }

    if (desc->switch_count > 0) {
        ioctl(fd, UI_SET_EVBIT, EV_SW);
        for (size_t i = 0; i < desc->switch_count; ++i) {
            ioctl(fd, UI_SET_SWBIT, desc->switches[i]);
        }
    }

    if (desc->ff_effects_max > 0 && desc->enable_ff_rumble) {
        ioctl(fd, UI_SET_EVBIT, EV_FF);

        const unsigned short default_ff[] = {FF_RUMBLE};
        const unsigned short *ff_effects = desc->ff_effects ? desc->ff_effects : default_ff;
        size_t ff_count = (desc->ff_effects && desc->ff_effect_count > 0)
                              ? desc->ff_effect_count
                              : (sizeof(default_ff) / sizeof(default_ff[0]));

        for (size_t i = 0; i < ff_count; ++i) {
            ioctl(fd, UI_SET_FFBIT, ff_effects[i]);
        }
    }

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id = desc->id;
    setup.ff_effects_max = desc->ff_effects_max;
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "%s", desc->name);

    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0) {
        perror("UI_DEV_SETUP");
        close(fd);
        return NULL;
    }
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("UI_DEV_CREATE");
        close(fd);
        return NULL;
    }

    struct gamepad *gp = calloc(1, sizeof(*gp));
    if (!gp) {
        perror("calloc");
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
        return NULL;
    }
    gp->fd = fd;
    return gp;
}

static void emit_event(struct gamepad *gp, unsigned short type, unsigned short code, int value)
{
    if (!gp) {
        return;
    }
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, NULL);
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(gp->fd, &ev, sizeof(ev)) < 0) {
        perror("write uinput");
    }
}

void gamepad_emit_key(struct gamepad *gp, unsigned short code, int value)
{
    emit_event(gp, EV_KEY, code, value);
}

void gamepad_emit_abs(struct gamepad *gp, unsigned short code, int value)
{
    emit_event(gp, EV_ABS, code, value);
}

void gamepad_emit_sw(struct gamepad *gp, unsigned short code, int value)
{
    emit_event(gp, EV_SW, code, value);
}

void gamepad_sync(struct gamepad *gp)
{
    emit_event(gp, EV_SYN, SYN_REPORT, 0);
}

void gamepad_destroy(struct gamepad *gp)
{
    if (!gp) {
        return;
    }
    ioctl(gp->fd, UI_DEV_DESTROY);
    close(gp->fd);
    free(gp);
}

int gamepad_get_fd(struct gamepad *gp)
{
    if (!gp) {
        return -1;
    }
    return gp->fd;
}

int gamepad_read_event(struct gamepad *gp, struct input_event *ev)
{
    if (!gp || !ev) {
        return -1;
    }
    ssize_t r = read(gp->fd, ev, sizeof(*ev));
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        perror("read uinput");
        return -1;
    }
    if (r == 0) {
        return 0;
    }
    if (r != (ssize_t)sizeof(*ev)) {
        fprintf(stderr, "short read from uinput\n");
        return -1;
    }
    return 1;
}

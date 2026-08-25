#define _POSIX_C_SOURCE 200809L

#include "rumble_pwm_vibrator.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../../../devices/device_rumble.h"

#define PWM_VIBRATOR_EVENT "/dev/input/by-path/platform-soc@3000000:pwm_vibrator-event"

static int write_ff(struct rumble_pwm_vibrator_hw *hw, int value)
{
    struct input_event ev = {
        .type = EV_FF,
        .code = (unsigned short)hw->effect_id,
        .value = value,
    };
    return write(hw->fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev) ? 0 : -1;
}

int rumble_pwm_vibrator_init(struct rumble_pwm_vibrator_hw *hw)
{
    if (!hw) return -1;
    *hw = (struct rumble_pwm_vibrator_hw){.fd = -1, .effect_id = -1};

    hw->fd = open(PWM_VIBRATOR_EVENT, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (hw->fd < 0) {
        perror("open pwm-vibrator");
        return -1;
    }

    struct ff_effect effect;
    memset(&effect, 0, sizeof(effect));
    effect.type = FF_RUMBLE;
    effect.id = -1;
    effect.u.rumble.strong_magnitude = 0xffff;
    effect.u.rumble.weak_magnitude = 0xffff;
    effect.replay.length = 0;

    if (ioctl(hw->fd, EVIOCSFF, &effect) < 0) {
        perror("EVIOCSFF pwm-vibrator");
        close(hw->fd);
        hw->fd = -1;
        return -1;
    }

    hw->effect_id = effect.id;
    hw->initialized = true;
    return 0;
}

int rumble_pwm_vibrator_set(struct rumble_pwm_vibrator_hw *hw, bool on)
{
    if (!hw || !hw->initialized) return -1;
    if (hw->motor_on == on) return 0;
    if (write_ff(hw, on ? 1 : 0) < 0) {
        perror("write pwm-vibrator FF");
        return -1;
    }
    hw->motor_on = on;
    return 0;
}

void rumble_pwm_vibrator_close(struct rumble_pwm_vibrator_hw *hw)
{
    if (!hw) return;
    if (hw->initialized) {
        (void)rumble_pwm_vibrator_set(hw, false);
        if (hw->effect_id >= 0) {
            (void)ioctl(hw->fd, EVIOCRMFF, hw->effect_id);
        }
    }
    if (hw->fd >= 0) close(hw->fd);
    *hw = (struct rumble_pwm_vibrator_hw){.fd = -1, .effect_id = -1};
}

const struct device_rumble_driver *rumble_pwm_vibrator_driver(void)
{
    static const struct device_rumble_driver driver = {
        .name = "pwm-vibrator",
        .ctx_size = sizeof(struct rumble_pwm_vibrator_hw),
        .init = (int (*)(void *))rumble_pwm_vibrator_init,
        .set = (int (*)(void *, bool))rumble_pwm_vibrator_set,
        .close = (void (*)(void *))rumble_pwm_vibrator_close,
    };
    return &driver;
}

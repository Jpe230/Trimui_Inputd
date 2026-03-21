#include "rumble_a133.h"

#include <stdio.h>

#include "../../../devices/device_rumble.h"
#include "../../sunxi-gpio/sunxi-gpio.h"

int rumble_a133_init(struct rumble_a133_hw *hw)
{
    if (!hw) {
        return -1;
    }
    *hw = (struct rumble_a133_hw){.gpio_pin = RUMBLE_A133_GPIO_PIN, .initialized = false};
    if (sunxi_gpio_init() < 0 || sunxi_gpio_set_cfgpin((uint32_t)RUMBLE_A133_GPIO_PIN, SUNXI_GPIO_OUTPUT) < 0 || sunxi_gpio_output((uint32_t)RUMBLE_A133_GPIO_PIN, 0) < 0) {
        return -1;
    }
    hw->initialized = true;
    return 0;
}

int rumble_a133_set(struct rumble_a133_hw *hw, bool on)
{
    if (!hw || !hw->initialized) {
        return -1;
    }
    if (sunxi_gpio_output((uint32_t)hw->gpio_pin, on ? 1u : 0u) < 0) {
        perror("rumble_a133 sunxi_gpio_output");
        return -1;
    }
    return 0;
}

void rumble_a133_close(struct rumble_a133_hw *hw)
{
    if (!hw) {
        return;
    }
    if (hw->initialized) {
        sunxi_gpio_output((uint32_t)hw->gpio_pin, 0);
        sunxi_gpio_close();
    }
    *hw = (struct rumble_a133_hw){0};
}

const struct device_rumble_driver *rumble_a133_driver(void)
{
    static const struct device_rumble_driver drv = {
        .name = "a133-gpio",
        .ctx_size = sizeof(struct rumble_a133_hw),
        .init = (int (*)(void *))rumble_a133_init,
        .set = (int (*)(void *, bool))rumble_a133_set,
        .close = (void (*)(void *))rumble_a133_close,
    };
    return &drv;
}

#pragma once

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#include "../device_helpers.h"
#include "../device_rumble.h"
#include "../../drivers/serial/serial.h"

struct gamepad;
struct axis_state;

/* Shared serial/ring buffer and stick state for a Smart Pro half. */
struct sp_ctx_common {
    int fd;
    struct ring_buffer rb;
    struct gamepad *gp;
    struct axis_state *ax;
    struct axis_state *ay;
    volatile sig_atomic_t *running;
    bool verbose;
    uint8_t prev_buttons;
    int last_x;
    int last_y;
};

/* Runtime context for the Smart Pro left controller half. */
struct smart_pro_left_ctx {
    struct sp_ctx_common c;
    int last_z;
    int last_switch;
    struct device_hat_state hat;
};

/* Runtime context for the Smart Pro right controller half. */
struct smart_pro_right_ctx {
    struct sp_ctx_common c;
    int last_rz;
};

/* Maps a packed button mask bit to a uinput key code. */
struct sp_button_map_entry {
    uint8_t mask;
    unsigned short code;
};

#pragma pack(push, 1)
/* Bit-packed button payload for Smart Pro frames. */
union sp_a_buttons {
    uint8_t raw;
    struct {
        unsigned l1:1;
        unsigned l2:1;
        unsigned dpad_up:1;
        unsigned dpad_left:1;
        unsigned dpad_right:1;
        unsigned dpad_down:1;
        unsigned reserved:1;
        unsigned mode:1;
    } left;
    struct {
        unsigned r1:1;
        unsigned r2:1;
        unsigned y:1;
        unsigned x:1;
        unsigned b:1;
        unsigned a:1;
        unsigned select:1;
        unsigned start:1;
    } right;
};

/* On-wire frame layout for Smart Pro variant A reports. */
struct sp_frame_a {
    uint8_t start;
    uint8_t unknown;
    union sp_a_buttons buttons;
    uint8_t x_hi;
    uint8_t x_lo;
    uint8_t y_hi;
    uint8_t y_lo;
    uint8_t end;
};
#pragma pack(pop)

/* Owning container for both Smart Pro halves. */
struct smart_pro_device {
    struct smart_pro_left_ctx left;
    struct smart_pro_right_ctx right;
    struct device_dirty_state dirty;
    struct device_rumble_state rumble;
};

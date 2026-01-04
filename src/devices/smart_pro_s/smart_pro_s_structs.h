#pragma once

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#include "../device_helpers.h"
#include "../../drivers/serial/serial.h"

struct gamepad;
struct axis_state;

/* Shared serial/ring buffer and stick state for a Smart Pro S half. */
struct sp_s_ctx_common {
    int fd;
    struct ring_buffer rb;
    struct gamepad *gp;
    struct axis_state *ax;
    struct axis_state *ay;
    volatile sig_atomic_t *running;
    bool verbose;
    uint32_t prev_buttons;
    int last_x;
    int last_y;
};

/* Runtime context for the Smart Pro S left controller half. */
struct smart_pro_s_left_ctx {
    struct sp_s_ctx_common c;
    int last_z;
    int last_switch;
    struct device_hat_state hat;
};

/* Runtime context for the Smart Pro S right controller half. */
struct smart_pro_s_right_ctx {
    struct sp_s_ctx_common c;
    int last_rz;
};

/* Maps packed Smart Pro S button bits to uinput key codes. */
struct sp_s_button_map_entry {
    uint32_t mask;
    unsigned short code;
};

#pragma pack(push, 1)
/* Bit-packed button payload for Smart Pro S frames. */
union sp_b_buttons {
    uint32_t raw;
    struct {
        unsigned a:1;         /* bit0 */
        unsigned x:1;         /* bit1 */
        unsigned select:1;    /* bit2 */
        unsigned start:1;     /* bit3 */
        unsigned dpad_up:1;   /* bit4 */
        unsigned dpad_down:1; /* bit5 */
        unsigned dpad_left:1; /* bit6 */
        unsigned dpad_right:1;/* bit7 */
        unsigned b:1;         /* bit8 */
        unsigned y:1;         /* bit9 */
        unsigned l1:1;        /* bit10 */
        unsigned r1:1;        /* bit11 */
        unsigned l2:1;        /* bit12 */
        unsigned r2:1;        /* bit13 */
        unsigned l3:1;        /* bit14 */
        unsigned r3:1;        /* bit15 */
        unsigned mode:1;      /* bit16 */
        unsigned reserved:15;
    } bits;
};

/* On-wire frame layout for Smart Pro S reports. */
struct sp_frame_b {
    uint8_t start;
    uint8_t unknown;
    union sp_b_buttons buttons;
    uint16_t lx;
    uint16_t ly;
    uint16_t rx;
    uint16_t ry;
    uint8_t reserved[4];
    uint8_t end;
    uint8_t reserved2;
};
#pragma pack(pop)

/* Owning container for both Smart Pro S halves. */
struct smart_pro_s_device {
    struct smart_pro_s_left_ctx left;
    struct smart_pro_s_right_ctx right;
    struct device_dirty_state dirty;
};

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "../gamepad/uinput.h"
#include "device_helpers.h"

/* Common prefix for turbo flag files. */
#define TURBO_FLAG_PREFIX "/tmp/trimui_inputd/turbo_"

enum { TURBO_TOGGLE_PERIOD = 3 };

struct turbo_state {
    bool enabled;
    bool physical_down;
    bool virtual_down;
    int frame_counter;
};

struct turbo_binding_cfg {
    const char *flag_path;
    unsigned short code;
};

struct turbo_binding {
    const char *flag_path;
    unsigned short code;
    struct turbo_state state;
    bool last_output;
    bool was_active;
};

void turbo_init_bindings(struct turbo_binding *bindings, size_t count, const struct turbo_binding_cfg *cfgs);
bool turbo_manages_code(const struct turbo_binding *bindings, size_t count, unsigned short code);
void turbo_refresh_flags(struct turbo_binding *bindings, size_t count);
bool turbo_note_physical(struct turbo_binding *bindings, size_t count, unsigned short code, bool pressed);
void turbo_process_frame(struct turbo_binding *bindings, size_t count, struct gamepad *gp, struct device_dirty_state *dirty);

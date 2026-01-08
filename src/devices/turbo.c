#include "turbo.h"

#include <string.h>
#include <unistd.h>

static void turbo_reset_state(struct turbo_state *st)
{
    st->virtual_down = false;
    st->frame_counter = 0;
}

static struct turbo_binding *find_binding(struct turbo_binding *bindings, size_t count, unsigned short code)
{
    for (size_t i = 0; i < count; ++i) {
        if (bindings[i].code == code) {
            return &bindings[i];
        }
    }
    return NULL;
}

void turbo_init_bindings(struct turbo_binding *bindings, size_t count, const struct turbo_binding_cfg *cfgs)
{
    for (size_t i = 0; i < count; ++i) {
        bindings[i].flag_path = cfgs[i].flag_path;
        bindings[i].code = cfgs[i].code;
        memset(&bindings[i].state, 0, sizeof(bindings[i].state));
        bindings[i].last_output = false;
        bindings[i].was_active = false;
    }
}

bool turbo_manages_code(const struct turbo_binding *bindings, size_t count, unsigned short code)
{
    for (size_t i = 0; i < count; ++i) {
        if (bindings[i].code == code) {
            return true;
        }
    }
    return false;
}

void turbo_refresh_flags(struct turbo_binding *bindings, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        struct turbo_state *st = &bindings[i].state;
        bool was_enabled = st->enabled;
        st->enabled = access(bindings[i].flag_path, F_OK) == 0;
        if (was_enabled && !st->enabled) {
            turbo_reset_state(st);
            bindings[i].was_active = false;
        }
    }
}

bool turbo_note_physical(struct turbo_binding *bindings, size_t count, unsigned short code, bool pressed)
{
    struct turbo_binding *binding = find_binding(bindings, count, code);
    if (!binding) {
        return false;
    }
    struct turbo_state *st = &binding->state;
    bool prev_down = st->physical_down;
    st->physical_down = pressed;
    if (!pressed && prev_down) {
        turbo_reset_state(st);
        binding->was_active = false;
    }
    return true;
}

void turbo_process_frame(struct turbo_binding *bindings, size_t count, struct gamepad *gp, struct device_dirty_state *dirty)
{
    for (size_t i = 0; i < count; ++i) {
        struct turbo_binding *binding = &bindings[i];
        struct turbo_state *st = &binding->state;
        bool turbo_active = st->enabled && st->physical_down;
        bool desired_down = st->physical_down;

        if (!turbo_active) {
            binding->was_active = false;
        } else {
            if (!binding->was_active && !st->virtual_down) {
                st->virtual_down = true;
                st->frame_counter = 0;
            }
            st->frame_counter++;
            if (st->frame_counter >= TURBO_TOGGLE_PERIOD) {
                st->virtual_down = !st->virtual_down;
                st->frame_counter = 0;
            }
            desired_down = st->virtual_down;
            binding->was_active = true;
        }

        if (desired_down != binding->last_output) {
            gamepad_emit_key(gp, binding->code, desired_down ? 1 : 0);
            device_dirty_mark(dirty);
            binding->last_output = desired_down;
        }
    }
}

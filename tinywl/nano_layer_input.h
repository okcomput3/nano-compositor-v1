// nano_layer_input.h - Input event IPC definitions for layer wrapper communication
#ifndef NANO_LAYER_INPUT_H
#define NANO_LAYER_INPUT_H

#include <stdint.h>
#include <stdbool.h>

// Input event types (compositor -> wrapper)
typedef enum {
    IPC_INPUT_MOUSE_MOTION = 1,
    IPC_INPUT_MOUSE_MOTION_ABSOLUTE = 2,
    IPC_INPUT_MOUSE_BUTTON = 3,
    IPC_INPUT_MOUSE_AXIS = 4,
    IPC_INPUT_KEY = 5,
    IPC_INPUT_MODIFIERS = 6,
} ipc_input_type_t;

typedef struct {
    ipc_input_type_t type;
    uint32_t time_ms;
    union {
        struct { double dx; double dy; } motion;
        struct { double x; double y; uint32_t width; uint32_t height; } motion_abs;
        struct { uint32_t button; uint32_t state; } button;  // state: 0=released, 1=pressed
        struct { uint32_t axis; double value; } axis;        // axis: 0=vertical, 1=horizontal
        struct { uint32_t key; uint32_t state; } key;        // state: 0=released, 1=pressed
        struct { uint32_t mods_depressed; uint32_t mods_latched; 
                 uint32_t mods_locked; uint32_t group; } modifiers;
    } data;
} ipc_input_event_t;

// Helper functions for compositor to send input to layer
static inline bool layer_send_mouse_motion(int event_fd, double dx, double dy, uint32_t time_ms) {
    if (event_fd < 0) return false;
    ipc_input_event_t ev = {
        .type = IPC_INPUT_MOUSE_MOTION,
        .time_ms = time_ms,
        .data.motion = { .dx = dx, .dy = dy }
    };
    return write(event_fd, &ev, sizeof(ev)) == sizeof(ev);
}

static inline bool layer_send_mouse_motion_absolute(int event_fd, double x, double y, 
                                                     uint32_t width, uint32_t height, 
                                                     uint32_t time_ms) {
    if (event_fd < 0) return false;
    ipc_input_event_t ev = {
        .type = IPC_INPUT_MOUSE_MOTION_ABSOLUTE,
        .time_ms = time_ms,
        .data.motion_abs = { .x = x, .y = y, .width = width, .height = height }
    };
    return write(event_fd, &ev, sizeof(ev)) == sizeof(ev);
}

static inline bool layer_send_mouse_button(int event_fd, uint32_t button, bool pressed, uint32_t time_ms) {
    if (event_fd < 0) return false;
    ipc_input_event_t ev = {
        .type = IPC_INPUT_MOUSE_BUTTON,
        .time_ms = time_ms,
        .data.button = { .button = button, .state = pressed ? 1 : 0 }
    };
    return write(event_fd, &ev, sizeof(ev)) == sizeof(ev);
}

static inline bool layer_send_mouse_axis(int event_fd, uint32_t axis, double value, uint32_t time_ms) {
    if (event_fd < 0) return false;
    ipc_input_event_t ev = {
        .type = IPC_INPUT_MOUSE_AXIS,
        .time_ms = time_ms,
        .data.axis = { .axis = axis, .value = value }
    };
    return write(event_fd, &ev, sizeof(ev)) == sizeof(ev);
}

static inline bool layer_send_key(int event_fd, uint32_t key, bool pressed, uint32_t time_ms) {
    if (event_fd < 0) return false;
    ipc_input_event_t ev = {
        .type = IPC_INPUT_KEY,
        .time_ms = time_ms,
        .data.key = { .key = key, .state = pressed ? 1 : 0 }
    };
    return write(event_fd, &ev, sizeof(ev)) == sizeof(ev);
}

static inline bool layer_send_modifiers(int event_fd, uint32_t depressed, uint32_t latched,
                                         uint32_t locked, uint32_t group) {
    if (event_fd < 0) return false;
    ipc_input_event_t ev = {
        .type = IPC_INPUT_MODIFIERS,
        .time_ms = 0,
        .data.modifiers = { 
            .mods_depressed = depressed,
            .mods_latched = latched,
            .mods_locked = locked,
            .group = group
        }
    };
    return write(event_fd, &ev, sizeof(ev)) == sizeof(ev);
}

#endif // NANO_LAYER_INPUT_H
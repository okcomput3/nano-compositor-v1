// nano_event.c - Event system implementation
#include "nano_compositor.h"

// Event system is mostly implemented in nano_plugin.c
// This file can contain additional event utilities if needed

const char *nano_event_type_name(enum nano_event_type type) {
    switch (type) {
        case NANO_EVENT_SERVER_START: return "server-start";
        case NANO_EVENT_SERVER_STOP: return "server-stop";
        case NANO_EVENT_OUTPUT_ADD: return "output-add";
        case NANO_EVENT_OUTPUT_REMOVE: return "output-remove";
        case NANO_EVENT_VIEW_CREATE: return "view-create";
        case NANO_EVENT_VIEW_DESTROY: return "view-destroy";
        case NANO_EVENT_VIEW_MAP: return "view-map";
        case NANO_EVENT_VIEW_UNMAP: return "view-unmap";
        case NANO_EVENT_KEYBOARD_KEY: return "keyboard-key";
        case NANO_EVENT_POINTER_MOTION: return "pointer-motion";
        case NANO_EVENT_POINTER_BUTTON: return "pointer-button";
        case NANO_EVENT_BEFORE_RENDER: return "before-render";
        case NANO_EVENT_AFTER_RENDER: return "after-render";
        default: return "unknown";
    }
}
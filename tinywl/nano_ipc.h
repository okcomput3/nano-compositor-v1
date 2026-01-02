// nano_ipc.h - Updated with window move, focus notifications, and output resize support
#ifndef NANO_IPC_H
#define NANO_IPC_H

// =======================================================================================
// +++ THE FIX IS HERE +++
// This line MUST be included. It provides the full definition for the
// 'wlr_dmabuf_attributes' struct, which solves the "incomplete type" error.
#include <wlr/render/dmabuf.h>
// =======================================================================================

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// SHARED DEFINITIONS
// ============================================================================
// Define the dimensions and size of the shared memory buffer used for the
// plugin's final rendered output.
#define SHM_WIDTH 1920
#define SHM_HEIGHT 1080
#define SHM_BUFFER_SIZE (SHM_WIDTH * SHM_HEIGHT * 4) // 4 bytes per pixel (RGBA)

// ============================================================================
// DATA STRUCTURES FOR IPC MESSAGES (Compositor -> Plugin)
// ============================================================================
typedef enum {
    IPC_BUFFER_TYPE_SHM = 0,
    IPC_BUFFER_TYPE_DMABUF = 1
} ipc_buffer_type_t;

typedef struct {
    int id;
    double x, y;
    int width, height;
    char title[128];
    bool is_focused;
    bool is_fullscreen;      // NEW: Track fullscreen state
    ipc_buffer_type_t buffer_type;
    int fd;
    uint32_t format, stride;
    uint64_t modifier;
    int fd_index;
} ipc_window_info_t;

typedef struct {
    int count;
    ipc_window_info_t windows[32];
} ipc_window_list_t;

typedef struct {
    char name[64];
    int32_t x, y, width, height, refresh;
} ipc_output_info_t;

// Window click structure
typedef struct {
    int window_index;
    float x, y;           // Relative coordinates within the window
    uint32_t button;      // Which button was clicked
    uint32_t state;       // Pressed (1) or released (0)
} ipc_window_click_t;

// NEW: Output resize data structure
typedef struct {
    char name[64];
    uint32_t width, height;
    uint32_t old_width, old_height;  // Previous dimensions
    float scale_factor;              // Scale change ratio (new/old)
    bool is_fullscreen_transition;   // True if this is nested->fullscreen or vice versa
} ipc_output_resize_t;

// NEW: Output remove data structure
typedef struct {
    char name[64];
    uint32_t last_width, last_height;  // Dimensions before removal
} ipc_output_remove_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t stride;
    uint64_t modifier;
} ipc_full_output_info_t;

// ENHANCED: Event types with clear ordering
typedef enum {
    // Input events
    IPC_EVENT_TYPE_POINTER_MOTION = 0,
    IPC_EVENT_TYPE_POINTER_BUTTON = 1,
    IPC_EVENT_TYPE_WINDOW_CLICK = 2,
    IPC_EVENT_TYPE_RENDER_DMABUF      = 4,
    IPC_EVENT_TYPE_PLUGIN_DMABUF_INFO = 5,
    IPC_EVENT_TYPE_PLUGIN_FRAME_READY = 6,
    IPC_EVENT_TYPE_BUFFER_ALLOCATED = 7,
    IPC_EVENT_TYPE_GEOMETRY_UPDATE = 8, 
    IPC_EVENT_TYPE_TOGGLE_GRID_LAYOUT=9,

    // Window management events
    IPC_EVENT_TYPE_WINDOW_LIST_UPDATE = 10,
    IPC_EVENT_TYPE_WINDOW_MOVE = 11,
    IPC_EVENT_TYPE_WINDOW_RESIZE = 12,
    IPC_EVENT_TYPE_WINDOW_FOCUS = 13,
    IPC_EVENT_TYPE_WINDOW_FULLSCREEN = 14,  // NEW: Window fullscreen state change

    // Output/Desktop events
    IPC_EVENT_TYPE_OUTPUT_ADD = 20,
    IPC_EVENT_TYPE_OUTPUT_REMOVE = 21,
    IPC_EVENT_TYPE_OUTPUT_RESIZE = 22,      // Desktop size change (nested->fullscreen)
    IPC_EVENT_TYPE_OUTPUT_MODE_CHANGE = 23, // NEW: Resolution/refresh rate change
    IPC_EVENT_TYPE_FULL_OUTPUT_UPDATE = 24,

    // System events
    IPC_EVENT_TYPE_COMPOSITOR_READY = 30,   // NEW: Compositor fully initialized
    IPC_EVENT_TYPE_COMPOSITOR_SHUTDOWN = 31, // NEW: Compositor shutting down
} ipc_event_type_t;


// **FIX 1: This definition is moved BEFORE it is used.**
// This struct now holds all attributes for a single buffer.
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t stride;
    uint64_t modifier;
    uint32_t offset;
} ipc_dmabuf_info_t;

// Add this new struct to hold the geometry for a single window
typedef struct {
    float x;
    float y;
    // You could add width/height here too if needed, but for now we only need position
} ipc_window_geometry_t;

// A structure to hold information for multiple buffers (for double buffering).
typedef struct {
    uint32_t count;
    // **THE FIX IS HERE:** This now correctly uses the type defined above.
    ipc_dmabuf_info_t buffers[2];
} ipc_multi_buffer_info_t;

typedef union {
    ipc_full_output_info_t full_output;
    // ... your existing members like window_list ...
    struct {
        int count;
        ipc_window_geometry_t windows[32];
    } geometry_list; // <-- ADD THIS NEW MEMBER
} ipc_event_data_t;

// NEW: Window fullscreen event data
typedef struct {
    int window_index;
    bool is_fullscreen;
    uint32_t old_width, old_height;  // Size before fullscreen
    uint32_t new_width, new_height;  // Size after fullscreen
} ipc_window_fullscreen_t;

typedef struct {
    int window_index;
} ipc_window_raise_request_t;


// NEW: Output mode change event data
typedef struct {
    char name[64];
    uint32_t width, height;
    uint32_t refresh_rate;
    bool mode_changed;      // True if resolution changed
    bool refresh_changed;   // True if refresh rate changed
} ipc_output_mode_change_t;

// ENHANCED: Main IPC event structure with all event types
typedef struct {
    ipc_event_type_t type;
    uint64_t timestamp_ns;

    union {
        // Input events
        struct { double x, y; } motion;
        struct { double x, y; uint32_t button, state; } button;
        ipc_window_click_t window_click;

        // Window events
        ipc_window_list_t window_list;

        struct { int window_index; int x, y; } window_move;
        struct { int window_index; int width, height; } window_resize;
        struct { int window_index; } window_focus;
        ipc_window_fullscreen_t window_fullscreen;

        // Output events
        ipc_output_info_t output_add;
        ipc_output_remove_t output_remove;
        ipc_output_resize_t output_resize;
        ipc_output_mode_change_t output_mode_change;

        // ============================ THE FIX ============================
        // ADD THE NEW MEMBER HERE inside the correct union.
        ipc_full_output_info_t full_output;
        // =======================================================================

        ipc_dmabuf_info_t dmabuf_info;
        struct wlr_dmabuf_attributes buffer_info;

        ipc_multi_buffer_info_t multi_buffer_info;

        // System events (no additional data needed for ready/shutdown)
    } data;
} ipc_event_t;




// ... (rest of the file is unchanged) ...

// ============================================================================
// DATA STRUCTURES FOR NOTIFICATIONS (Plugin -> Compositor)
// ============================================================================
typedef enum {
    IPC_NOTIFICATION_TYPE_CRASHED = 0,
    IPC_NOTIFICATION_TYPE_FRAME_SUBMIT = 1,
    IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT = 2,

    // Window management notifications
    IPC_NOTIFICATION_TYPE_WINDOW_MOVE_REQUEST = 3,
    IPC_NOTIFICATION_TYPE_WINDOW_FOCUS_REQUEST = 4,
    IPC_NOTIFICATION_TYPE_WINDOW_RESIZE_REQUEST = 5,
    IPC_NOTIFICATION_TYPE_WINDOW_CLOSE_REQUEST = 6,     // NEW: Request to close window
    IPC_NOTIFICATION_TYPE_WINDOW_FULLSCREEN_REQUEST = 7, // NEW: Request fullscreen toggle
    IPC_NOTIFICATION_TYPE_WINDOW_RAISE_REQUEST = 8,

    // Desktop management notifications
    IPC_NOTIFICATION_TYPE_OUTPUT_SCALE_REQUEST = 10,    // NEW: Request output scaling
    IPC_NOTIFICATION_TYPE_PLUGIN_RESIZE_RESPONSE = 11,  // NEW: Plugin adapted to size change

    // Plugin lifecycle notifications
    IPC_NOTIFICATION_TYPE_PLUGIN_READY = 20,           // NEW: Plugin initialization complete
    IPC_NOTIFICATION_TYPE_PLUGIN_ERROR = 21,           // NEW: Plugin error occurred
} ipc_notification_type_t;

typedef struct {
    uint32_t width, height, format, stride;
    uint64_t modifier;
} ipc_frame_info_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t format;        // DRM format (e.g., DRM_FORMAT_ABGR8888)
    uint32_t stride;
    uint64_t modifier;      // DRM format modifier
    uint32_t offset;        // Offset into the buffer
} ipc_dmabuf_frame_info_t;

// Window management notification data
typedef struct {
    int window_index;
    int x, y;
} ipc_window_move_request_t;

typedef struct {
    int window_index;
} ipc_window_focus_request_t;

typedef struct {
    int window_index;
    int width, height;
} ipc_window_resize_request_t;

// NEW: Window close request
typedef struct {
    int window_index;
    bool force_close;  // True for immediate close, false for graceful
} ipc_window_close_request_t;

// NEW: Window fullscreen toggle request
typedef struct {
    int window_index;
    bool enable_fullscreen;
    char output_name[64];  // Preferred output for fullscreen (empty = any)
} ipc_window_fullscreen_request_t;

// NEW: Output scale request
typedef struct {
    char output_name[64];
    float scale_factor;  // Requested scale (1.0 = native, 2.0 = 2x, etc.)
} ipc_output_scale_request_t;

// NEW: Plugin resize response (tells compositor plugin adapted to new size)
typedef struct {
    uint32_t new_width, new_height;
    bool adaptation_successful;
    char error_message[128];  // If adaptation failed
} ipc_plugin_resize_response_t;

// NEW: Plugin ready notification
typedef struct {
    char plugin_name[64];
    char plugin_version[32];
    uint32_t api_version;
    bool supports_dmabuf;
    bool supports_window_management;
} ipc_plugin_ready_t;

// NEW: Plugin error notification
typedef struct {
    uint32_t error_code;
    char error_message[256];
    bool is_fatal;  // True if plugin needs to restart
} ipc_plugin_error_t;

// ENHANCED: Combined notification structure with all notification types
typedef struct {
    ipc_notification_type_t type;
    uint64_t timestamp_ns;  // NEW: Timestamp for ordering
    union {
        // Frame notifications
        ipc_frame_info_t frame_info;
        ipc_dmabuf_frame_info_t dmabuf_frame_info;

        // Window management notifications
        ipc_window_move_request_t window_move_request;
        ipc_window_focus_request_t window_focus_request;
        ipc_window_resize_request_t window_resize_request;
        ipc_window_close_request_t window_close_request;           // NEW
        ipc_window_fullscreen_request_t window_fullscreen_request; // NEW
        ipc_window_raise_request_t window_raise_request;

        // Desktop management notifications
        ipc_output_scale_request_t output_scale_request;           // NEW
        ipc_plugin_resize_response_t plugin_resize_response;       // NEW

        // Plugin lifecycle notifications
        ipc_plugin_ready_t plugin_ready;                          // NEW
        ipc_plugin_error_t plugin_error;                          // NEW
    } data;
} ipc_notification_t;

// ============================================================================
// UTILITY MACROS AND HELPER FUNCTIONS
// ============================================================================

// Macro to get current timestamp in nanoseconds
#define IPC_GET_TIMESTAMP_NS() ({ \
    struct timespec ts; \
    clock_gettime(CLOCK_MONOTONIC, &ts); \
    (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec; \
})

// Helper function declarations (implement in your plugin/compositor code)
static inline const char* ipc_event_type_to_string(ipc_event_type_t type) {
    switch (type) {
        case IPC_EVENT_TYPE_POINTER_MOTION: return "POINTER_MOTION";
        case IPC_EVENT_TYPE_POINTER_BUTTON: return "POINTER_BUTTON";
        case IPC_EVENT_TYPE_WINDOW_CLICK: return "WINDOW_CLICK";
        case IPC_EVENT_TYPE_WINDOW_LIST_UPDATE: return "WINDOW_LIST_UPDATE";
        case IPC_EVENT_TYPE_WINDOW_MOVE: return "WINDOW_MOVE";
        case IPC_EVENT_TYPE_WINDOW_RESIZE: return "WINDOW_RESIZE";
        case IPC_EVENT_TYPE_WINDOW_FOCUS: return "WINDOW_FOCUS";
        case IPC_EVENT_TYPE_WINDOW_FULLSCREEN: return "WINDOW_FULLSCREEN";
        case IPC_EVENT_TYPE_OUTPUT_ADD: return "OUTPUT_ADD";
        case IPC_EVENT_TYPE_OUTPUT_REMOVE: return "OUTPUT_REMOVE";
        case IPC_EVENT_TYPE_OUTPUT_RESIZE: return "OUTPUT_RESIZE";
        case IPC_EVENT_TYPE_OUTPUT_MODE_CHANGE: return "OUTPUT_MODE_CHANGE";
        case IPC_EVENT_TYPE_COMPOSITOR_READY: return "COMPOSITOR_READY";
        case IPC_EVENT_TYPE_COMPOSITOR_SHUTDOWN: return "COMPOSITOR_SHUTDOWN";
        case IPC_EVENT_TYPE_BUFFER_ALLOCATED: return "BUFFER_ALLOCATED";
        default: return "UNKNOWN";
    }
}

static inline const char* ipc_notification_type_to_string(ipc_notification_type_t type) {
    switch (type) {
        case IPC_NOTIFICATION_TYPE_CRASHED: return "CRASHED";
        case IPC_NOTIFICATION_TYPE_FRAME_SUBMIT: return "FRAME_SUBMIT";
        case IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT: return "DMABUF_FRAME_SUBMIT";
        case IPC_NOTIFICATION_TYPE_WINDOW_MOVE_REQUEST: return "WINDOW_MOVE_REQUEST";
        case IPC_NOTIFICATION_TYPE_WINDOW_FOCUS_REQUEST: return "WINDOW_FOCUS_REQUEST";
        case IPC_NOTIFICATION_TYPE_WINDOW_RESIZE_REQUEST: return "WINDOW_RESIZE_REQUEST";
        case IPC_NOTIFICATION_TYPE_WINDOW_CLOSE_REQUEST: return "WINDOW_CLOSE_REQUEST";
        case IPC_NOTIFICATION_TYPE_WINDOW_FULLSCREEN_REQUEST: return "WINDOW_FULLSCREEN_REQUEST";
        case IPC_NOTIFICATION_TYPE_WINDOW_RAISE_REQUEST: return "WINDOW_RAISE_REQUEST";
        case IPC_NOTIFICATION_TYPE_OUTPUT_SCALE_REQUEST: return "OUTPUT_SCALE_REQUEST";
        case IPC_NOTIFICATION_TYPE_PLUGIN_RESIZE_RESPONSE: return "PLUGIN_RESIZE_RESPONSE";
        case IPC_NOTIFICATION_TYPE_PLUGIN_READY: return "PLUGIN_READY";
        case IPC_NOTIFICATION_TYPE_PLUGIN_ERROR: return "PLUGIN_ERROR";
        default: return "UNKNOWN";
    }
}

#endif // NANO_IPC_H
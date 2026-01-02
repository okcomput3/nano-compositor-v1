// nano_compositor.h
#ifndef NANO_COMPOSITOR_H
#define NANO_COMPOSITOR_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/util/log.h>
#include <linux/input-event-codes.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_xdg_shell.h>


#include <wlr/types/wlr_xdg_decoration_v1.h>


#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <wlr/render/gles2.h>
#include <wlr/render/drm_format_set.h>
#include <wayland-server-protocol.h>
#include <drm_fourcc.h>
#include <string.h>
#include <EGL/egl.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/render/egl.h>


#include <wlr/render/gles2.h>     // For wlr_gles2_renderer_create_surfaceless, wlr_gles2_renderer_create_with_egl
#include <wlr/render/wlr_renderer.h> // For wlr_renderer_begin, wlr_renderer_end etc.
#include <wlr/render/egl.h>    
#include <GLES3/gl31.h> // For OpenGL ES 3.1 constants
#include <wlr/render/gles2.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h> // For wlr_matrix_project_box, also includes matrix.h
#include <wlr/util/log.h> // For WLR_ERRORARNING
 // For wlr_rdp_backend
#include <wlr/types/wlr_seat.h> // For wlr_seat_pointer_notify_*
#include <linux/input-event-codes.h> // For BTN_LEFT, BTN_RIGHT, BTN_MIDDLE
#include <wlr/render/wlr_renderer.h> // For renderer functions
//#include <wlr/types/wlr_matrix.h>   // For wlr_matrix_project_box
#include <wlr/types/wlr_scene.h>    // For scene graph functions
#include <wlr/render/gles2.h>       // For GLES2-specific functions
#include <wlr/render/pass.h>        // For wlr_render_pass API
#include <GLES3/gl31.h>
#include <time.h>
#include <string.h> // For memcpy
#include <math.h>   // For round
//#include <wlr/types/wlr_matrix.h>
// Helper (ensure it's defined or declared before these functions)
// static float get_monotonic_time_seconds_as_float() { /* ... */ }
#include <GLES2/gl2.h>
#include <wlr/render/gles2.h>
//#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/log.h>
#include <string.h>
#include <stdlib.h>

#include <wlr/render/egl.h>

#include <wlr/render/wlr_renderer.h> // For wlr_renderer functions
      // For wlr_matrix_project_box
#include <wlr/types/wlr_scene.h>    // For scene graph functions
#include <GLES3/gl31.h>             // For OpenGL ES 3.1

// Ensure you have these includes (or equivalent for your project structure)
#include <time.h>
#include <wlr/types/wlr_compositor.h> // For wlr_surface_send_frame_done
//#include <wlr/types/wlr_output_damage.h> // For wlr_output_damage_add_whole (optional but good)


#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
//#include <wlr/util/transform.h>
#include <wlr/util/log.h>
#include <wlr/render/wlr_renderer.h>


#include <pixman.h>
#include <time.h>
#include <pthread.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
//#include <wlr/types/wlr_matrix.h> 
//#include <wlr/types/wlr_matrix.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h> // THIS IS KEY for wlr_xdg_surface_get_geometry
// Add other wlr/types/ as needed: scene, output_layout, cursor, seat, etc.
#include <wlr/types/wlr_scene.h>


#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h> // THIS IS KEY for struct wlr_render_texture_options
#include <wlr/render/allocator.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <linux/input-event-codes.h>
#include <time.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>

#include <drm_fourcc.h>

// Core server structure (nano-kernel)
#include <pthread.h>
#include "nano_ipc.h" // <-- INCLUDE THE MASTER PROTOCOL FIRST
#include "concurrent_queue.h"
#include "libdmabuf_share/dmabuf_share.h"
#include <stdint.h>  // for uintptr_t
#include "nano_layer_compositor.h"

#define PROTOCOL_DECORATION_MODE_NONE 0
#define PROTOCOL_DECORATION_MODE_SERVER_SIDE 1
#define PROTOCOL_DECORATION_MODE_CLIENT_SIDE 2



// Forward declarations
struct nano_server;
struct nano_view;
struct nano_output;
struct nano_service;
struct nano_event;
struct nano_plugin_api;
struct concurrent_queue;


// Event types for plugin system
enum nano_event_type {
    NANO_EVENT_SERVER_START,
    NANO_EVENT_SERVER_STOP,
    NANO_EVENT_OUTPUT_ADD,
    NANO_EVENT_OUTPUT_REMOVE,
    NANO_EVENT_VIEW_CREATE,
    NANO_EVENT_VIEW_DESTROY,
    NANO_EVENT_VIEW_MAP,
    NANO_EVENT_VIEW_UNMAP,
    NANO_EVENT_KEYBOARD_KEY,
    NANO_EVENT_POINTER_MOTION,
    NANO_EVENT_POINTER_BUTTON,
    NANO_EVENT_BEFORE_RENDER,
    NANO_EVENT_AFTER_RENDER,
    
    // NEW: Full OpenGL render hooks
    NANO_EVENT_RENDER_START,      // Called when render pass starts (OpenGL context ready)
    NANO_EVENT_RENDER_BACKGROUND, // Called to render background layer with full OpenGL
    NANO_EVENT_RENDER_WINDOWS,    // Called to render window layer  
    NANO_EVENT_RENDER_OVERLAYS,   // Called to render overlay layer
    NANO_EVENT_RENDER_END,        // Called when render pass ends
    
    NANO_EVENT_MAX,

    NANO_EVENT_CUSTOM,  // For plugin-to-plugin communication
};

// Plugin capabilities - what a plugin can access
enum nano_capability {
    NANO_CAP_NONE           = 0,
    NANO_CAP_VIEW_MANAGE    = 1 << 0,  // Can move/resize views
    NANO_CAP_INPUT_HANDLE   = 1 << 1,  // Can process input events
    NANO_CAP_RENDER_HOOK    = 1 << 2,  // Can hook into render pipeline
    NANO_CAP_OUTPUT_MANAGE  = 1 << 3,  // Can manage outputs
    NANO_CAP_SCENE_ACCESS   = 1 << 4,  // Can modify scene graph
    NANO_CAP_PROTOCOL_EXT   = 1 << 5,  // Can add protocol extensions
    NANO_CAP_ALL           = 0xFF
};

// PASTE THE STRUCT DEFINITION HERE
struct plugin_ipc_connection {
    struct spawned_plugin *plugin;
    struct wl_event_source *event_source;
    struct wl_list link;
};

struct spawned_plugin {
    pid_t pid;
    char name[64];
    int ipc_fd;

    char shm_name[64];
    void* shm_ptr;

    struct wlr_texture *texture; 


    bool event_mask[NANO_EVENT_MAX];

    struct wl_list link;

    int new_frame_dma_fd;
    ipc_frame_info_t new_frame_info;
     int frame_age;
         // NEW: DMA-BUF support fields
    int dmabuf_fd;
    uint32_t dmabuf_width;
    uint32_t dmabuf_height;
    uint32_t dmabuf_format;
    uint32_t dmabuf_stride;
    uint64_t dmabuf_modifier;

     int pending_frame_fd; 

    struct wl_event_source *ipc_event_source;
    
struct wlr_buffer *compositor_buffer; 
    struct nano_server *server;  

    // --- State Flags ---
    bool is_async;       // TRUE for the new Python plugin, FALSE for legacy plugins
    bool has_new_frame;

    // --- ASYNC Double-Buffering Model (for Python plugin) ---
    struct wlr_buffer *compositor_buffers[2];   // The two pre-allocated buffers
    struct wlr_dmabuf_attributes dmabuf_attrs[2]; // Attributes for each
    int last_ready_buffer;  


        // --- NEW: IPC Listener Thread ---
    pthread_t ipc_thread;
    bool ipc_thread_running;   

        pthread_t process_monitor_thread;
    bool process_monitor_running;

    // Async laundering
    bool laundering_in_progress;
       // Performance tracking for smart frame skipping
    uint64_t last_frame_time;
    uint64_t last_processed_frame;
    uint64_t last_launder_duration;
    
    // Caching for smooth rendering
    struct wlr_texture *cached_texture;
    struct wlr_buffer *cached_buffer;
    bool scale_cached;
    int cached_x_offset, cached_y_offset;
    int cached_scaled_width, cached_scaled_height;
    int cached_output_width, cached_output_height;

};
struct nano_keyboard {
    struct wl_list link;
    struct nano_server *server;
    struct wlr_input_device *device;

    struct wl_listener modifiers;
    struct wl_listener key;
    struct wl_listener destroy;
};


struct ipc_queue_item {
    ipc_event_t event;
    int fds[32];
    int fd_count;
    struct wl_list link;
};

// FD Pool structures
struct fd_pool_entry {
    int fd;
    uint64_t buffer_id;  // Unique buffer identifier
    struct timespec last_used;
    bool in_use;
    struct wl_list link;
};

struct fd_pool {
    struct wl_list available;  // Available FDs
    struct wl_list in_use;     // Currently used FDs
    int max_size;
    int current_size;
    pthread_mutex_t mutex;
};


// Event data structures
struct nano_event {
    enum nano_event_type type;
    struct nano_server *server;
    void *data;
    bool handled;  // Plugin can set to true to stop propagation
    
    // Allow plugins to attach additional data
    void *plugin_data[4];  // Up to 4 plugins can attach data to an event
};

struct nano_event_view {
    struct nano_view *view;
};

struct nano_event_keyboard {
    uint32_t keycode;
    uint32_t modifiers;
    enum wl_keyboard_key_state state;
};

struct nano_event_pointer {
    double x, y;
    uint32_t button;
    enum wl_pointer_button_state state;
};


// Render pass data for plugins
struct nano_render_pass_data {
    struct wlr_output *output;
    struct wlr_renderer *renderer;
    struct wlr_render_pass *render_pass;  // The actual wlroots render pass
    int width, height;
    float transform_matrix[9];  // 3x3 transformation matrix
    bool should_continue_default_rendering;  // Plugin can set to false to override
    float clear_color[4];  // Background clear color RGBA
};

struct nano_window {
    struct wlr_surface *surface;
    int x, y, width, height;
    // other window properties
};

struct nano_surface_texture {
    struct wlr_surface *surface;
    GLuint texture_id;
    int x, y, width, height;
    bool is_mapped;
    char *title;
};

// Add this structure to nano_compositor.h
struct nano_scene_buffer_info {
    struct wlr_scene_buffer *scene_buffer;
    struct wlr_surface *surface;  // Associated surface (if available)
    int x, y;                     // Position
    int width, height;            // Dimensions
    char *title;                  // Window title (if available)
    bool is_mapped;               // Whether surface is mapped
    struct nano_view *view;       // Associated view (if available)
};

// Plugin API interface
struct nano_plugin_api {
    // Core server info
    struct nano_server *(*get_server)(void);
    struct wl_display *(*get_display)(void);
    struct wlr_renderer *(*get_renderer)(void);
    struct wlr_egl *(*get_egl)(void);

    GLuint (*extract_texture_during_render)(struct wlr_surface *surface);
    
    // NEW: Render pass access
    void (*begin_render_pass)(struct wlr_output *output);
    void (*end_render_pass)(struct wlr_output *output);
    bool (*is_render_pass_active)(void);
    
    // View management (requires NANO_CAP_VIEW_MANAGE)
    void (*view_move)(struct nano_view *view, double x, double y);
    void (*view_resize)(struct nano_view *view, int width, int height);
    void (*view_focus)(struct nano_view *view);
    void (*view_close)(struct nano_view *view);
    
    // Scene graph access (requires NANO_CAP_SCENE_ACCESS)
    struct wlr_scene_tree *(*get_scene_tree)(void);
    struct wlr_scene_node *(*view_get_scene_node)(struct nano_view *view);
    

    
    // Output management (requires NANO_CAP_OUTPUT_MANAGE)
    struct wlr_output_layout *(*get_output_layout)(void);
    
    // Service registry
    int (*register_service)(const char *name, void *service_api);
    void *(*get_service)(const char *name);
    
    // Configuration
    const char *(*get_config_string)(const char *section, const char *key, const char *default_val);
    int (*get_config_int)(const char *section, const char *key, int default_val);

    struct nano_window_list *(*get_window_list)(void);

    struct nano_surface_texture* (*get_surface_textures)(int *count);

        // Scene buffer access (for real rendered content)
    struct nano_scene_buffer_info* (*get_scene_buffers)(int *count);
    GLuint (*extract_texture_from_scene_buffer)(struct wlr_scene_buffer *scene_buffer);
//plugin discory


};



struct mouse_event_queue {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct nano_event_pointer events[256];
    int head, tail, count;
    bool running;
    pthread_t thread;
};

// ============================================================================
// Structure Definitions - MUST BE COMPLETE HERE
// ============================================================================

// A structure to hold a reusable buffer and its state
struct buffer_pool_entry {
    struct wlr_buffer *buffer;
    bool in_use;
    uint32_t width;
    uint32_t height;
    struct wl_list link;
};

enum {
    NANO_RESIZE_MODE_NONE,
    NANO_RESIZE_MODE_TOP,
    NANO_RESIZE_MODE_BOTTOM,
    NANO_RESIZE_MODE_LEFT,
    NANO_RESIZE_MODE_RIGHT,
};



struct nano_server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    
    struct wlr_compositor *compositor;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_output_layout *output_layout;
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;

    struct wl_list ipc_queue;
    struct wl_event_source *ipc_source;

     bool should_exit;

    struct wl_event_source *render_timer;   
    
    // EGL context for plugin OpenGL access
    struct wlr_egl *egl;
    
    // NEW: Render hook system
    bool render_hooks_enabled;  // Enable full OpenGL render hooks
    
    // Input devices
    struct wl_list keyboards;
    struct wl_list pointers;
    
    // Minimal core state
    struct wl_list outputs;
    struct wl_list views;
    
    // Plugin system
    struct wl_list plugins; // This list now holds 'struct spawned_plugin'
    struct nano_plugin_api plugin_api;
    
    pthread_mutex_t plugin_list_mutex;
    struct concurrent_queue *render_queue;
    
    
    // Event system
    struct wl_list event_listeners[NANO_EVENT_MAX];
    
    // Service registry
    struct wl_list services;


    layer_manager_t layer_manager;
    struct wl_list layers;  // List of compositor_layer_t    
    
    // Configuration
    void *config;
    
    // Core event listeners
    struct wl_listener new_output;
    struct wl_listener new_xdg_surface;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    
    // Cursor event listeners
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    // Frame timing optimization
    struct timespec last_frame_time;
    bool frame_pending;
    int frame_skip_counter;
    
    // Texture extraction throttling
    struct timespec last_texture_extraction;
    bool texture_extraction_pending;

    dmabuf_share_context_t dmabuf_ctx; 

    struct fd_pool fd_pool; 


struct mouse_event_queue mouse_queue;

 struct wl_list plugin_connections; 

    struct wl_list buffer_pool; 


    struct xkb_context *xkb_context; // <-- ADD THIS XKB CONTEXT

      // --- ADD ALL OF THESE FIELDS FOR DRAGGING AND RESIZING ---
    bool is_dragging;
    struct nano_view *grabbed_view;
    double grab_x, grab_y;

    // For dragging
    double view_start_x, view_start_y;

    // For resizing
    uint32_t resize_mode;
    double view_start_width, view_start_height;

    bool has_managed_windows; 


};

// View structure
struct nano_view {
    struct wl_list link;
    struct nano_server *server;
     struct wlr_xdg_surface *xdg_surface;  
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree *scene_tree;
        struct wl_listener surface_commit;      

        bool toplevel_listeners_added; 
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
    
    // Plugin-accessible data
    void *plugin_data;  // For plugins to attach data

    struct wlr_buffer *cached_linear_buffer;
    uint32_t last_buffer_seq;  // Track when buffer changes
    uint32_t last_buffer_hash; 
    struct wlr_buffer *last_buffer_ptr;      // Track buffer pointer
    uint32_t last_surface_commit;  
    int last_client_fd;

     // FD management
    uint64_t cached_buffer_id;
    int cached_fd;
    bool fd_valid;
    
    // Buffer change detection
    uint32_t last_buffer_sequence;
    struct timespec last_buffer_update;
        uint64_t cached_client_buffer_id;  
        bool linear_buffer_changed;

 // ADD THESE NEW FIELDS FOR THROTTLING:
    struct timespec last_fd_sent;     // When we last sent an FD
    int frames_since_fd_sent;         // Count frames since last FD
    
    int last_sent_fd; 
 
      struct wlr_buffer *last_client_buffer;   
      struct buffer_pool_entry *current_buffer_entry; // The buffer we are currently using from the pool
  bool dirty;  // Still needed to check for changes

   struct wlr_box saved_geometry;
       bool last_update_sent;
    int last_buffer_width, last_buffer_height;
    int last_x, last_y;
    uint64_t last_content_hash;  

};

// Output structure  
struct nano_output {
    struct wl_list link;
    struct nano_server *server;
    struct wlr_output *wlr_output;
    struct wlr_scene_output *scene_output;
    
    struct wl_listener frame;
    struct wl_listener destroy;
    struct wl_listener commit;   

        // ADD THESE LINES:
    uint32_t last_width;
    uint32_t last_height;
    bool size_changed;
  
    struct wl_listener request_state;  // Add this line
    struct wl_listener wl_configure;  

    // Add to struct nano_output
int prev_width;
int prev_height;
struct wl_listener mode;

struct wlr_buffer *composite_buffer;

};

// Service registry entry
struct nano_service {
    struct wl_list link;
    char *name;
    void *api;
    struct nano_plugin *provider;
};


// Public function prototypes for the new plugin manager
void nano_plugin_manager_init(struct nano_server *server);
void nano_plugin_spawn(struct nano_server *server, const char *path, const char* name);
void nano_plugin_spawn_python(struct nano_server *server, const char *path, const char* name);
void nano_plugin_spawn_javascript(struct nano_server *server, 
                                   const char *loader_path, 
                                   const char *script_path,
                                   const char *name);
void nano_plugin_kill(struct nano_server *server, struct spawned_plugin *plugin);

struct spawned_plugin *nano_plugin_get_by_pid(struct nano_server *server, pid_t pid);
// Plugin manager functions
//struct nano_plugin *nano_plugin_load(struct nano_server *server, const char *path);
//void nano_plugin_unload(struct nano_server *server, struct nano_plugin *plugin);
int nano_plugin_check_capabilities(struct nano_plugin *plugin, enum nano_capability caps);

// Event system
void nano_event_emit(struct nano_server *server, struct nano_event *event);

// Plugin API setup
void nano_server_setup_plugin_api(struct nano_server *server);

// Core functions
bool nano_server_init(struct nano_server *server);
void nano_server_destroy(struct nano_server *server);
int nano_server_run(struct nano_server *server);

// Event handler function declarations
static void output_frame(struct wl_listener *listener, void *data);
static void output_destroy(struct wl_listener *listener, void *data);
static void xdg_toplevel_map(struct wl_listener *listener, void *data);
static void xdg_toplevel_unmap(struct wl_listener *listener, void *data);
static void xdg_toplevel_destroy(struct wl_listener *listener, void *data);
static void xdg_toplevel_request_move(struct wl_listener *listener, void *data);
static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data);
static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data);
static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data);


struct wlr_egl {
    EGLDisplay display;
    EGLContext context;
    EGLDeviceEXT device; // may be EGL_NO_DEVICE_EXT
    struct gbm_device *gbm_device;

    struct {
        // Display extensions
        bool KHR_image_base;
        bool EXT_image_dma_buf_import;
        bool EXT_image_dma_buf_import_modifiers;
        bool IMG_context_priority;
        bool EXT_create_context_robustness;

        // Device extensions
        bool EXT_device_drm;
        bool EXT_device_drm_render_node;

        // Client extensions
        bool EXT_device_query;
        bool KHR_platform_gbm;
        bool EXT_platform_device;
        bool KHR_display_reference;
    } exts;

    struct {
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
        PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
        PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
        PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT;
        PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT;
        PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR;
        PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT;
        PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT;
        PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT;
        PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
        PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
        PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
        PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
    } procs;

    bool has_modifiers;
    struct wlr_drm_format_set dmabuf_texture_formats;
    struct wlr_drm_format_set dmabuf_render_formats;
};


#endif // NANO_COMPOSITOR_H
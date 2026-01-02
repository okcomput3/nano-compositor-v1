/*Copyright (c) 2025 Andrew Pliatsikas

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
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
#include "shader.h"

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
#include <wlr/types/wlr_matrix.h>   // For wlr_matrix_project_box
#include <wlr/types/wlr_scene.h>    // For scene graph functions
#include <wlr/render/gles2.h>       // For GLES2-specific functions
#include <wlr/render/pass.h>        // For wlr_render_pass API
#include <GLES3/gl31.h>
#include <time.h>
#include <string.h> // For memcpy
#include <math.h>   // For round
#include <wlr/types/wlr_matrix.h>
// Helper (ensure it's defined or declared before these functions)
// static float get_monotonic_time_seconds_as_float() { /* ... */ }
#include <GLES2/gl2.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_matrix.h>
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
#include <wlr/types/wlr_matrix.h> 
#include <wlr/types/wlr_matrix.h>

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

#include <drm_fourcc.h>

#define PROTOCOL_DECORATION_MODE_NONE 0
#define PROTOCOL_DECORATION_MODE_SERVER_SIDE 1
#define PROTOCOL_DECORATION_MODE_CLIENT_SIDE 2


#ifndef RENDER_EGL_H
#define RENDER_EGL_H


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

struct wlr_egl_context {
    EGLDisplay display;
    EGLContext context;
    EGLSurface draw_surface;
    EGLSurface read_surface;
};

/**
 * Initializes an EGL context for the given DRM FD.
 *
 * Will attempt to load all possibly required API functions.
 */
struct wlr_egl *wlr_egl_create_with_drm_fd(int drm_fd);

/**
 * Frees all related EGL resources, makes the context not-current and
 * unbinds a bound wayland display.
 */
void wlr_egl_destroy(struct wlr_egl *egl);

/**
 * Creates an EGL image from the given dmabuf attributes. Check usability
 * of the dmabuf with wlr_egl_check_import_dmabuf once first.
 */
EGLImageKHR wlr_egl_create_image_from_dmabuf(struct wlr_egl *egl,
    struct wlr_dmabuf_attributes *attributes, bool *external_only);

/**
 * Get DMA-BUF formats suitable for sampling usage.
 */
const struct wlr_drm_format_set *wlr_egl_get_dmabuf_texture_formats(
    struct wlr_egl *egl);
/**
 * Get DMA-BUF formats suitable for rendering usage.
 */
const struct wlr_drm_format_set *wlr_egl_get_dmabuf_render_formats(
    struct wlr_egl *egl);

/**
 * Destroys an EGL image created with the given wlr_egl.
 */
bool wlr_egl_destroy_image(struct wlr_egl *egl, EGLImageKHR image);

int wlr_egl_dup_drm_fd(struct wlr_egl *egl);

/**
 * Restore EGL context that was previously saved using wlr_egl_save_current().
 */
bool wlr_egl_restore_context(struct wlr_egl_context *context);

/**
 * Make the EGL context current.
 *
 * The old EGL context is saved. Callers are expected to clear the current
 * context when they are done by calling wlr_egl_restore_context().
 */
bool wlr_egl_make_current(struct wlr_egl *egl, struct wlr_egl_context *save_context);

bool wlr_egl_unset_current(struct wlr_egl *egl);

EGLSyncKHR wlr_egl_create_sync(struct wlr_egl *egl, int fence_fd);

void wlr_egl_destroy_sync(struct wlr_egl *egl, EGLSyncKHR sync);

int wlr_egl_dup_fence_fd(struct wlr_egl *egl, EGLSyncKHR sync);

bool wlr_egl_wait_sync(struct wlr_egl *egl, EGLSyncKHR sync);

#endif






/* Structures */
enum tinywl_cursor_mode {
    TINYWL_CURSOR_PASSTHROUGH,
    TINYWL_CURSOR_MOVE,
    TINYWL_CURSOR_RESIZE,
};



struct render_data {
    struct wlr_render_pass *pass;
    struct wlr_renderer *renderer;
    GLuint shader_program;
       GLint scale_uniform_loc; // Renamed to avoid conflict
    struct tinywl_server *server;
    struct wlr_output *output; // Add output for projection matrix
     int desktop_index; 
};

// First, modify your server struct to support multiple desktops:
struct desktop_fb {
    GLuint fbo;
    GLuint texture;
    GLuint rbo;  // if you end up using renderbuffers
    int width;
    int height;
};


int DestopGridSize ; // 2x2 grid for 4 desktops
float GLOBAL_vertical_offset;

float dockX_align =10.0;

// Add near the top of the file, with other struct definitions
#define MAX_DOCK_ICONS 10

struct dock_icon {
    struct wlr_box box;         // On-screen geometry, updated every frame
    const char *app_command;    // The base command to run, e.g., "alacritty"
    GLuint texture_id;          // Texture for this icon
};

struct tinywl_server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xdg_popup;
    struct wl_list toplevels;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

struct wl_listener pointer_motion;
    struct wlr_seat *seat;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    struct wl_list keyboards;
    enum tinywl_cursor_mode cursor_mode;
    struct tinywl_toplevel *grabbed_toplevel;
    double grab_x, grab_y;
    struct wlr_box grab_geobox;
    uint32_t resize_edges;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;
      EGLDisplay egl_display;
    EGLConfig egl_config;
    EGLContext egl_context;
   EGLSurface egl_surface;
 // For XDG clients
 // list of tinywl_output.link
    struct wl_list views;   // <<< THIS IS CRUCIAL: list of tinywl_view.link
    struct wl_list popups;        // List of tinywl_popup objects
    struct wl_listener new_popup; // Listener for new xdg_popup events

     GLuint shader_program; // Flame shader for buffers/animations
    GLint flame_shader_mvp_loc;
    GLint flame_shader_tex_loc;
    GLint flame_shader_time_loc;
    GLint flame_shader_res_loc;

    // New: Rectangle Shader
    GLuint rect_shader_program;
    GLint rect_shader_mvp_loc;
    GLint rect_shader_color_loc;
    GLint rect_shader_time_loc;
    GLint rect_shader_resolution_loc; // New: for iResolution
    GLint rect_shader_rect_res_loc;  

    // New: Panel Shader
    GLuint panel_shader_program;
    GLint panel_shader_mvp_loc;
    GLint panel_shader_time_loc;
    GLint panel_shader_base_color_loc; // u_panel_base_color
    GLint panel_shader_resolution_loc;


 // ADD THESE UNIFORM LOCATIONS FOR THE PANEL PREVIEW
    GLint panel_shader_preview_tex_loc;
    GLint panel_shader_is_preview_active_loc;
    GLint panel_shader_preview_rect_loc;
    GLint panel_shader_preview_tex_transform_loc;
    GLint panel_shader_rect_pixel_dimensions_loc;


// NEW: Shader for SSDs
    GLuint ssd_shader_program;
    GLint ssd_shader_mvp_loc;
    GLint ssd_shader_color_loc;
    GLint ssd_shader_time_loc;                  // NEW for SSD shader
    GLint ssd_shader_rect_pixel_dimensions_loc; // Was already there, now used as 
 GLint ssd_shader_resolution_loc;
    struct wlr_scene_node *main_background_node;

// NEW: 2nd Shader for SSDs    
    GLuint ssd2_shader_program;
    GLint ssd2_shader_mvp_loc;
    GLint ssd2_shader_color_loc;
    GLint ssd2_shader_time_loc;
    GLint ssd2_shader_resolution_loc;

    GLuint back_shader_program;
    GLint back_shader_mvp_loc;
    GLint back_shader_time_loc;
    GLint back_shader_base_color_loc; // u_panel_base_color
    GLint back_shader_resolution_loc;
    GLint back_shader_tex_loc ;
    GLint back_shader_res_loc;

    // Pointer to the top panel's scene node for easy identification
    struct wlr_scene_node *top_panel_node;

     // Custom shader program for animations
    GLint scale_uniform;  // Location of the scale uniform in the shader

    // For shared quad rendering
    GLuint quad_vao;
    GLuint quad_vbo;
    GLuint quad_ibo;



    // Fullscreen shader effect (as before)
    GLuint fullscreen_shader_program;
    GLint fullscreen_shader_mvp_loc;
     GLint fullscreen_shader_time_loc;
      // Keep if shader uses it for non-rotation anim
    GLint fullscreen_shader_scene_tex0_loc;
   GLint fullscreen_shader_scene_tex1_loc;
    GLint fullscreen_shader_scene_tex2_loc;
    GLint fullscreen_shader_scene_tex3_loc;

     GLint fullscreen_shader_scene_tex4_loc;
   GLint fullscreen_shader_scene_tex5_loc;
    GLint fullscreen_shader_scene_tex6_loc;
    GLint fullscreen_shader_scene_tex7_loc;

     GLint fullscreen_shader_scene_tex8_loc;
   GLint fullscreen_shader_scene_tex9_loc;
    GLint fullscreen_shader_scene_tex10_loc;
    GLint fullscreen_shader_scene_tex11_loc;

     GLint fullscreen_shader_scene_tex12_loc;
   GLint fullscreen_shader_scene_tex13_loc;
    GLint fullscreen_shader_scene_tex14_loc;
    GLint fullscreen_shader_scene_tex15_loc;

    GLint fullscreen_shader_desktop_grid_loc;
  
 GLint fullscreen_shader_resolution_loc;
    GLint fullscreen_switch_mode;
     GLint fullscreen_switchXY; 

   // GLint fullscreen_shader_current_quad_loc;   


// Add these new zoom uniform locations:
    GLint fullscreen_shader_zoom_loc;
    GLint fullscreen_shader_move_loc;
    GLint fullscreen_shader_zoom_center_loc;
    GLint fullscreen_shader_quadrant_loc;


    // cube shader effect (as before)
    GLuint cube_shader_program;
    GLint cube_shader_mvp_loc;
     GLint cube_shader_time_loc; // Keep if shader uses it for non-rotation anim
    GLint cube_shader_scene_tex0_loc;
   GLint cube_shader_scene_tex1_loc;
    GLint cube_shader_scene_tex2_loc;
    GLint cube_shader_scene_tex3_loc;
    GLint cube_shader_tex_matrix_loc;   // Location for texture matrix uniform
 GLint cube_shader_resolution_loc;


    GLint cube_shader_current_quad_loc;   


// Add these new zoom uniform locations:
    GLint cube_shader_zoom_loc;
    GLint cube_shader_zoom_center_loc;
    GLint cube_shader_quadrant_loc;
     GLint cube_shader_vertical_offset_loc;
     GLint cube_shader_global_vertical_offset_loc;







     struct wlr_buffer *scene_capture_buffer;    // Buffer to capture the scene (NEW/REPURPOSED)
    struct wlr_texture *scene_texture;          // Texture from the captured scene buffer (REPURPOSED)
    // GLuint scene_fbo; // REMOVE THIS


 // Variables to control the zoom effect at runtime
   // --- Animation variables for the fullscreen effect's geometric zoom ---
 bool expo_effect_active;
    float effect_zoom_factor_normal;
    float effect_zoom_factor_zoomed;
    
    bool  effect_is_target_zoomed;      // True if the *target* state is zoomed
    bool  effect_is_animating_zoom;
    float effect_anim_current_factor;   // Calculated in output_frame
    float effect_anim_start_factor;
    float effect_anim_target_factor;
    float effect_anim_start_time_sec;
    float effect_anim_duration_sec;
    
    float effect_zoom_center_x;
    float effect_zoom_center_y;

 bool cube_effect_active;
    float cube_zoom_factor_normal;
    float cube_zoom_factor_zoomed;
    
    bool  cube_is_target_zoomed;      // True if the *target* state is zoomed
    bool  cube_is_animating_zoom;
    float cube_anim_current_factor;   // Calculated in output_frame
    float cube_anim_start_factor;
    float cube_anim_target_factor;
    float cube_anim_start_time_sec;
    float cube_anim_duration_sec;
    
    float cube_zoom_center_x;
    float cube_zoom_center_y;

    // Add these to your server struct:
float start_vertical_offset;     
 float current_interpolated_vertical_offset;
 float target_vertical_offset;
 float vertical_offset_animation_start_time;
 bool vertical_offset_animating;


    struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
struct wl_listener xdg_decoration_new;

 // Instead of single desktop_fbos[0], use arrays or separate fields:
    GLuint desktop_fbos[16];        // Support up to 4 virtual desktops
    GLuint desktop_rbos[16];        // Render buffer objects if needed
int intermediate_width[16];
int intermediate_height[16];
GLuint intermediate_texture[16];
GLuint intermediate_rbo[16];

             
    int current_desktop;
     struct desktop_fb *desktops;
    int num_desktops;

     int pending_desktop_switch;

       GLuint desktop_background_shaders[16];
    GLint desktop_bg_shader_mvp_loc[16];
    GLint desktop_bg_shader_time_loc[16];
    GLint desktop_bg_shader_res_loc[16];
    GLint desktop_bg_shader_color_loc[16]; // In case your shaders use a base color

   // New cube geometry (add these)
    GLuint cube_vao, cube_vbo, cube_ibo;

    GLuint cube_background_shader_program;
    GLint cube_background_shader_time_loc;

     // <<< ADD THESE NEW MEMBERS FOR POST-PROCESSING >>>
    GLuint post_process_fbo;
    GLuint post_process_texture;
    int post_process_width;
    int post_process_height;

    GLuint post_process_rbo; 

    // Shader program for the final effect
    GLuint post_process_shader_program;
    GLint post_process_shader_tex_loc;
    GLint post_process_shader_time_loc; // Optional: for animated effects
    GLint post_process_shader_resolution_loc;

    // <<< ADD THESE MEMBERS >>>
     // --- TV EFFECT MEMBERS ---
    bool tv_effect_animating;
    float tv_effect_start_time;
    float tv_effect_duration; 
    bool tv_is_on; 

     GLuint passthrough_shader_program;
    GLint passthrough_shader_mvp_loc;
    GLint passthrough_shader_tex_loc;

     GLint passthrough_shader_res_loc;
    GLint passthrough_shader_cornerRadius_loc;
    GLint passthrough_shader_bevelColor_loc;
    GLint passthrough_shader_time_loc;


     GLuint passthrough_icons_shader_program;
    GLint passthrough_icons_shader_mvp_loc;
    GLint passthrough_icons_shader_tex_loc;

     GLint passthrough_icons_shader_res_loc;
    GLint passthrough_icons_shader_cornerRadius_loc;
    GLint passthrough_icons_shader_bevelColor_loc;
    GLint passthrough_icons_shader_time_loc;

    // NEW: Genie/Minimize Effect Shader
    GLuint genie_shader_program;
    GLint genie_shader_mvp_loc;
    GLint genie_shader_tex_loc;
    GLint genie_shader_progress_loc;
   
    GLint genie_shader_bend_factor_loc;
    struct tinywl_toplevel *animating_toplevel;
    
// NEW: Tessellated mesh for Genie Effect
    GLuint genie_vao;
    GLuint genie_vbo;
    GLuint genie_ibo;
    int genie_index_count;

    GLuint solid_shader_program;
    GLint solid_shader_mvp_loc;
    GLint solid_shader_color_loc;
    GLint solid_shader_time_loc;

    struct dock_icon dock_icons[MAX_DOCK_ICONS];
    int num_dock_icons;
    const char *socket_name;

    bool dock_anim_active;
    bool dock_mouse_at_edge; // Latch to prevent re-triggering
    float dock_anim_start_value;
    float dock_anim_target_value;
    float dock_anim_start_time;
    float dock_anim_duration;
};

static float current_rotation = 0.0f;
static float target_rotation = 0.0f;
static float animation_start_time = 0.0f;
static float animation_duration = 0.5f; // 500ms animation
static int last_quadrant = -1; // Initialize to -1 to guarantee first animation
static bool animating = false;
static float last_rendered_rotation = 0.0f;

/* Updated tinywl_output struct to store timer */
struct tinywl_output {
    struct wl_list link;
    struct tinywl_server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;
    struct wl_event_source *timer; /* Added for timer access */
    struct wlr_scene_output *scene_output;
    bool rendering; 
    struct wlr_texture *cached_texture;
};

enum tinywl_toplevel_type {
    TINYWL_TOPLEVEL_XDG,
    TINYWL_TOPLEVEL_CUSTOM,
};

struct tinywl_decoration { // Your struct for SSD scene rects
    struct tinywl_toplevel *toplevel;
    struct wlr_scene_rect *title_bar;
    struct wlr_scene_rect *border_top;
     bool enabled;
 
        struct wlr_scene_rect *border_left;
        struct wlr_scene_rect *border_right;
        struct wlr_scene_rect *border_bottom;

// --- ADD THESE TWO LINES ---
    struct wlr_scene_rect *minimize_button;
    struct wlr_scene_rect *maximize_button;
    struct wlr_scene_rect *close_button;
   

};

#define TITLE_BAR_HEIGHT 13
#define BORDER_WIDTH 10

struct tinywl_toplevel {
    struct wl_list link;
    struct tinywl_server *server;
    union {
        struct wlr_xdg_toplevel *xdg_toplevel;
        struct wlr_scene_rect *custom_rect;
    };

    // This 'scene_tree' should be the *main frame* for the window,
    // including decorations AND client content area.
    struct wlr_scene_tree *scene_tree; // Overall window frame

    // This will point to the scene tree specifically for the client's content
    // (i.e., the tree returned by wlr_scene_xdg_surface_create for the base xdg_surface)
    // NEW: A dedicated tree for our server-side decorations
   

    struct wlr_scene_tree *client_xdg_scene_tree; // Tree for xdg_toplevel->base// Tree for xdg_toplevel->base

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
    struct wlr_texture *cached_texture;
    uint32_t last_commit_seq;
    bool needs_redraw;
    struct wlr_texture *custom_texture;
bool mapped;
    // Animation state
    bool is_animating;
    float scale;          // Current scale factor (1.0 = normal size)
    float target_scale;   // Target scale for animation (e.g., 1.0 for zoom in, 0.0 for minimize)
    float animation_start; // Animation start time (in seconds)
    float animation_duration; // Duration in seconds (e.g., 0.3 for 300ms)
 bool pending_destroy;
enum tinywl_toplevel_type type;

struct wlr_xdg_toplevel_decoration_v1 *decoration;
    struct wl_listener decoration_destroy;
    struct wl_listener decoration_request_mode;

     struct tinywl_decoration ssd; 

     struct wl_listener decoration_destroy_listener;
     struct wl_listener decoration_request_mode_listener;
   

     // Decoration fields
     bool ssd_pending_enable;

     int desktop;

     struct wlr_box restored_geom; // Last known geometry when not minimized

     // NEW: Fields for minimizing via taskbar
  
    bool is_minimizing; // True during the genie animation
    bool minimizing_to_dock; // True if minimizing, false if restoring
    float minimize_start_time;
    float minimize_duration;
    struct wlr_box minimize_start_geom;
    struct wlr_box minimize_target_geom;
    

     // NEW: Fields for minimizing via taskbar
    bool minimized;
    struct wlr_box panel_preview_box;

    // Fields for maximizing animation
    bool is_maximizing;
    bool maximized;
    float maximize_start_time;
    float maximize_duration;
    struct wlr_box maximize_start_geom;
    struct wlr_box maximize_target_geom;

    struct wl_list animation_link;
    
};





struct tinywl_keyboard {
    struct wl_list link;
    struct tinywl_server *server;
    struct wlr_keyboard *wlr_keyboard;
    struct wl_listener modifiers;
    struct wl_listener key;
    struct wl_listener destroy;
};

struct tinywl_view {
    struct tinywl_server *server;
    struct wlr_xdg_surface *xdg_surface; // Or wlr_layer_surface_v1, etc.
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    // You also need a commit listener on xdg_surface->surface->events.commit
    // to know when to ask for a frame render.
    struct wl_listener surface_commit;

    bool mapped;
    int x, y; // Position on the output
    struct wl_list link; // For server.views
};

struct tinywl_popup {
    struct wl_listener map;       // Listener for surface map events
    struct wl_listener unmap;     // Listener for surface unmap events
    struct wl_listener destroy;   // Listener for popup destroy
    struct wl_listener commit;    // Listener for surface commit
    struct wlr_xdg_popup *xdg_popup; // The underlying xdg_popup
    struct tinywl_server *server;    // Reference to the compositor
    struct wlr_scene_tree *scene_tree; // Scene node for rendering
    struct wl_list link;          // Link for list management
};

struct shader_uniform_spec {
    const char *name;       // Name of the uniform in GLSL
    GLint *location_ptr;  // Pointer to where the GLint location should be stored
};

   int switch_mode=0;
    int switchXY=0;


/* Function declarations */
//struct wlr_backend *wlr_RDP_backend_create(struct wl_display *display, struct wlr_egl *egl);
//struct wlr_renderer *wlr_gles2_renderer_create_surfaceless(void);
struct wlr_allocator *wlr_rdp_allocator_create(struct wlr_renderer *renderer);
//void rdp_transmit_surface(struct wlr_buffer *buffer);
//freerdp_peer *get_global_rdp_peer(void);
struct wlr_egl *setup_surfaceless_egl(struct tinywl_server *server) ;
void cleanup_egl(struct tinywl_server *server);
static void process_cursor_motion(struct tinywl_server *server, uint32_t time);
void check_scene_bypass_issue(struct wlr_scene_output *scene_output, struct wlr_output *output) ;
void debug_scene_rendering(struct wlr_scene *scene, struct wlr_output *output) ;
void debug_scene_tree(struct wlr_scene *scene, struct wlr_output *output);
void desktop_background(struct tinywl_server *server) ;

 float get_monotonic_time_seconds_as_float(void) ;
static void scene_buffer_iterator(struct wlr_scene_buffer *scene_buffer, int sx, int sy, void *user_data); // If not already declared

// Forward declarations (ensure these are before server_new_xdg_decoration)
static void decoration_handle_destroy(struct wl_listener *listener, void *data);
static void decoration_handle_request_mode(struct wl_listener *listener, void *data);
// Helper function to update decoration geometry
static void update_decoration_geometry(struct tinywl_toplevel *toplevel);
 void ensure_ssd_enabled(struct tinywl_toplevel *toplevel);
 static bool setup_desktop_framebuffer(struct tinywl_server *server, int desktop_idx, int width, int height);
 void update_toplevel_visibility(struct tinywl_server *server) ;
 void update_compositor_state(struct tinywl_server *server);
 static void begin_interactive(struct tinywl_toplevel *toplevel, enum tinywl_cursor_mode mode, uint32_t edges);
static void set_decorations_visible(struct tinywl_toplevel *toplevel, bool visible);

// Add this new function with other function implementations
static void launch_application(struct tinywl_server *server, const char *command) {
    if (fork() == 0) {
        // Child process
        setsid(); // Create a new session to detach from the compositor
        
        // Set environment variables for the new process
        setenv("WAYLAND_DISPLAY", server->socket_name, 1);
        // This command is an example; your original code had this, so I've kept it.
        // It forces software rendering, which can be useful for certain apps under a custom compositor.
        setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
        
        // Execute the command using the shell
        execl("/bin/sh", "/bin/sh", "-c", command, (void *)NULL);
        
        // If execl returns, it's an error
        wlr_log(WLR_ERROR, "execl failed for command: %s", command);
        _exit(127); // Exit child process
    }
}
 // Function to compile a shader
static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log_buf[512];
        glGetShaderInfoLog(shader, sizeof(log_buf), NULL, log_buf);
        wlr_log(WLR_ERROR, "Shader compilation failed: %s", log_buf);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}


/*
static bool create_generic_shader_program(
    struct wlr_renderer *renderer,
    const char *shader_name_for_log,
    const char *vertex_src,
    const char *fragment_src,
    GLuint *program_id_out,
    struct shader_uniform_spec *uniforms_to_get,
    int num_uniforms_to_get
) {
    if (!renderer || !program_id_out || !vertex_src || !fragment_src) {
        wlr_log(WLR_ERROR, "Invalid NULL arguments to create_generic_shader_program for %s", shader_name_for_log);
        if (program_id_out) *program_id_out = 0;
        return false;
    }
    if (num_uniforms_to_get > 0 && !uniforms_to_get) {
        wlr_log(WLR_ERROR, "uniforms_to_get is NULL but num_uniforms_to_get is > 0 for %s", shader_name_for_log);
        *program_id_out = 0;
        return false;
    }

    *program_id_out = 0; // Initialize output

    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(renderer);
    if (!egl) {
        wlr_log(WLR_ERROR, "Failed to get EGL context for %s shader", shader_name_for_log);
        return false;
    }

    struct wlr_egl_context saved_egl_ctx;
    // Save the current EGL context and make ours current
    if (!wlr_egl_make_current(egl, &saved_egl_ctx)) {
        wlr_log(WLR_ERROR, "Failed to make EGL context current for %s shader", shader_name_for_log);
        return false;
    }

    GLenum gl_err;
    while ((gl_err = glGetError()) != GL_NO_ERROR) {
        wlr_log(WLR_DEBUG, "%s: Clearing pre-existing GL error: 0x%x", shader_name_for_log, gl_err);
    }

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (!vs) {
        wlr_log(WLR_ERROR, "Vertex shader compilation failed for %s", shader_name_for_log);
        wlr_egl_restore_context(&saved_egl_ctx);
        return false;
    }

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) {
        wlr_log(WLR_ERROR, "Fragment shader compilation failed for %s", shader_name_for_log);
        glDeleteShader(vs);
        wlr_egl_restore_context(&saved_egl_ctx);
        return false;
    }

    GLuint program = glCreateProgram();
    if (program == 0) {
        wlr_log(WLR_ERROR, "glCreateProgram failed for %s: GL error 0x%x", shader_name_for_log, glGetError());
        glDeleteShader(vs);
        glDeleteShader(fs);
        wlr_egl_restore_context(&saved_egl_ctx);
        return false;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Shaders can be deleted after linking
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (!link_status) {
        char log_buf[1024];
        glGetProgramInfoLog(program, sizeof(log_buf), NULL, log_buf);
        wlr_log(WLR_ERROR, "%s shader program linking failed: %s", shader_name_for_log, log_buf);
        glDeleteProgram(program);
        wlr_egl_restore_context(&saved_egl_ctx);
        return false;
    }

    wlr_log(WLR_INFO, "%s shader program created successfully: ID=%u", shader_name_for_log, program);
    *program_id_out = program;

    // Get uniform locations if requested
    if (num_uniforms_to_get > 0) {
        glUseProgram(program); // Program must be active to query uniform locations
        for (int i = 0; i < num_uniforms_to_get; ++i) {
            if (uniforms_to_get[i].name && uniforms_to_get[i].location_ptr) {
                *(uniforms_to_get[i].location_ptr) = glGetUniformLocation(program, uniforms_to_get[i].name);
                if (*(uniforms_to_get[i].location_ptr) == -1) {
                    wlr_log(WLR_ERROR, "%s shader: Uniform '%s' not found (or optimized out). Location: %d",
                            shader_name_for_log, uniforms_to_get[i].name, *(uniforms_to_get[i].location_ptr));
                } else {
                    wlr_log(WLR_DEBUG, "%s shader: Uniform '%s' found at location %d.",
                            shader_name_for_log, uniforms_to_get[i].name, *(uniforms_to_get[i].location_ptr));
                }
            } else {
                wlr_log(WLR_ERROR, "%s shader: Invalid uniform_spec at index %d (null name or ptr).", shader_name_for_log, i);
            }
        }
        glUseProgram(0); // Unbind program after getting locations
    }

    // Restore the previous EGL context
    if (!wlr_egl_restore_context(&saved_egl_ctx)) {
        wlr_log(WLR_ERROR, "Failed to restore previous EGL context after %s shader creation.", shader_name_for_log);
        // This isn't necessarily fatal for the shader creation itself, but good to know.
    }

    return true;
}*/

bool create_generic_shader_program(struct wlr_renderer *renderer, const char *name,
                                  const char *vertex_shader_src, const char *fragment_shader_src,
                                  GLuint *program, struct shader_uniform_spec *uniforms, size_t uniform_count) {
    if (!renderer || !program || !vertex_shader_src || !fragment_shader_src) {
        wlr_log(WLR_ERROR, "Invalid NULL arguments to create_generic_shader_program for %s", name);
        if (program) *program = 0;
        return false;
    }
    if (uniform_count > 0 && !uniforms) {
        wlr_log(WLR_ERROR, "uniforms is NULL but uniform_count is > 0 for %s", name);
        *program = 0;
        return false;
    }

    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(renderer);
    if (!egl) {
        wlr_log(WLR_ERROR, "Failed to get EGL context for %s", name);
        *program = 0;
        return false;
    }

    // Make EGL context current
    if (!wlr_egl_make_current(egl, NULL)) {
        wlr_log(WLR_ERROR, "Failed to make EGL context current for %s", name);
        *program = 0;
        return false;
    }

    // Clear any pre-existing GL errors
    GLenum gl_err;
    while ((gl_err = glGetError()) != GL_NO_ERROR) {
        wlr_log(WLR_DEBUG, "%s: Clearing pre-existing GL error: 0x%x", name, gl_err);
    }

    // Create and compile vertex shader
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_src, NULL);
    glCompileShader(vertex_shader);
    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vertex_shader, sizeof(log), NULL, log);
        wlr_log(WLR_ERROR, "%s vertex shader compilation failed: %s", name, log);
        glDeleteShader(vertex_shader);
        return false;
    }

    // Create and compile fragment shader
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_src, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fragment_shader, sizeof(log), NULL, log);
        wlr_log(WLR_ERROR, "%s fragment shader compilation failed: %s", name, log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }

    // Link program
    *program = glCreateProgram();
    if (*program == 0) {
        wlr_log(WLR_ERROR, "glCreateProgram failed for %s: GL error 0x%x", name, glGetError());
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return false;
    }
    glAttachShader(*program, vertex_shader);
    glAttachShader(*program, fragment_shader);
    glLinkProgram(*program);
    glGetProgramiv(*program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(*program, sizeof(log), NULL, log);
        wlr_log(WLR_ERROR, "%s program linking failed: %s", name, log);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(*program);
        return false;
    }

    // Clean up shaders
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // Set uniform locations
    glUseProgram(*program);
    for (size_t i = 0; i < uniform_count; i++) {
        if (uniforms[i].name && uniforms[i].location_ptr) {
            GLint loc = glGetUniformLocation(*program, uniforms[i].name);
            *(uniforms[i].location_ptr) = loc;
            if (loc == -1) {
                wlr_log(WLR_ERROR, "%s shader: Uniform '%s' not found or optimized out (location: %d)",
                        name, uniforms[i].name, loc);
            } else {
                wlr_log(WLR_INFO, "%s shader: Uniform '%s' found at location %d", name, uniforms[i].name, loc);
            }
        } else {
            wlr_log(WLR_ERROR, "%s shader: Invalid uniform_spec at index %zu (null name or ptr)", name, i);
        }
    }
    glUseProgram(0);

    wlr_log(WLR_INFO, "%s shader program created successfully: ID=%u", name, *program);

    // Do not unset the EGL context here; let the caller manage it
    return true;
}

/*
// Function to start rotation animation
void start_quadrant_animation(int new_quadrant, float current_time) {
    if (new_quadrant != last_quadrant) {
        // Calculate target rotation (-90 degrees per quadrant)
        float new_target = (float)new_quadrant * -M_PI / 2.0f;
        
        // --- THE FIX ---
        // Capture the current visual rotation (including any wobble)
        // as the true starting point for our new animation.
        current_rotation = last_rendered_rotation;
        
        // Handle wrap-around for shortest path (this logic now uses the captured angle)
        float diff = new_target - current_rotation;
        if (diff > M_PI) {
            current_rotation += 2.0f * M_PI;
        } else if (diff < -M_PI) {
            current_rotation -= 2.0f * M_PI;
        }
        
        target_rotation = new_target;
        animation_start_time = current_time;
        last_quadrant = new_quadrant;
        animating = true;
        wlr_log(WLR_INFO, "Starting anim to quad %d. From %.2frad to %.2frad", new_quadrant, current_rotation, target_rotation);
    }
}*/

// Function to start rotation animation
void start_quadrant_animation(int new_quadrant, float current_time) {
    if (new_quadrant != last_quadrant) {
        // Calculate target rotation (-90 degrees per quadrant)
        float new_target = (float)new_quadrant * -M_PI / 2.0f;
        
        // --- THE FIX ---
        // Capture the current visual rotation (including any wobble)
        // as the true starting point for our new animation.
        current_rotation = last_rendered_rotation;
        
        // Handle wrap-around for shortest path (this logic now uses the captured angle)
        float diff = new_target - current_rotation;
        if (diff > M_PI) {
            current_rotation += 2.0f * M_PI;
        } else if (diff < -M_PI) {
            current_rotation -= 2.0f * M_PI;
        }
        
        target_rotation = new_target;
        animation_start_time = current_time;
        last_quadrant = new_quadrant;
        animating = true;
        wlr_log(WLR_INFO, "Starting anim to quad %d. From %.2frad to %.2frad", new_quadrant, current_rotation, target_rotation);
    }
}
/*
// Function to update rotation animation
float update_rotation_animation(float current_time) {
    if (animating) {
        float elapsed = current_time - animation_start_time;
        float progress = elapsed / animation_duration;
        
        if (progress >= 1.0f) {
            animating = false;
            current_rotation = target_rotation;
            return target_rotation;
        }
        
        // Smooth easing function (ease-in-out)
        float eased_progress = progress < 0.5f 
                             ? 4.0f * progress * progress * progress
                             : 1.0f - pow(-2.0f * progress + 2.0f, 3.0f) / 2.0f;
        
        // Interpolate from the rotation at the start of the animation
        return current_rotation + (target_rotation - current_rotation) * eased_progress;
    }
    
    return current_rotation;
}*/

#include <math.h> // Make sure you have this include for powf

// ... (keep the rest of your code the same) ...
// In update_rotation_animation:

float update_rotation_animation(struct tinywl_server *server, float current_time) {
    float base_rotation_this_frame;

    // --- Part 1: Determine the Base Rotation ---
    // This is either the result of the main animation or the stable target angle.
    if (animating) {
        float elapsed = current_time - animation_start_time;
        float progress = elapsed / animation_duration;

        if (progress >= 1.0f) {
            // The main animation has just finished.
            animating = false;
            current_rotation = target_rotation; // Lock in the new base angle for the *next* animation.
            base_rotation_this_frame = target_rotation;
        } else {
            // The animation is in progress. Calculate the eased angle.
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            float eased_progress = 1.0f + c3 * powf(progress - 1.0f, 3.0f) + c1 * powf(progress - 1.0f, 2.0f);
            base_rotation_this_frame = current_rotation + (target_rotation - current_rotation) * eased_progress;
        }
    } else {
        // Not animating, so the base rotation is the stable target angle.
        base_rotation_this_frame = target_rotation;
    }

    // --- Part 2: Calculate the Additive Wobble Offset ---
    // This is calculated regardless of the animation state but is only non-zero if zoomed.
    float wobble_offset = 0.0f;
    float zoom_diff = fabs(server->cube_anim_current_factor - server->cube_zoom_factor_normal);

    if (zoom_diff > 0.1f) {
        float wobble_speed = 4.0f;
        float wobble_amplitude = 0.5f; // A subtle amplitude is usually best
        float fade_factor = fminf(1.0f, zoom_diff / 1.0f);
        fade_factor = fade_factor * fade_factor * (3.0f - 2.0f * fade_factor); // Smoothstep
        
        // The wobble is just a small, sinusoidal offset from the base.
        wobble_offset = sin(current_time * wobble_speed) * wobble_amplitude * fade_factor;
    }

    // --- Part 3: Combine and Return the Final Angle ---
    // The final visual angle is the base rotation plus the wobble offset.
    float final_rotation = base_rotation_this_frame + wobble_offset;

    // Always update the last_rendered_rotation for seamless animation starts.
    last_rendered_rotation = final_rotation;
    
    return final_rotation;
}


/* Function implementations */
static void server_destroy(struct tinywl_server *server) {
    /* Remove all listeners */
    wl_list_remove(&server->new_xdg_toplevel.link);
    wl_list_remove(&server->new_xdg_popup.link);
    wl_list_remove(&server->cursor_motion.link);
    wl_list_remove(&server->cursor_motion_absolute.link);
    wl_list_remove(&server->cursor_button.link);
    wl_list_remove(&server->cursor_axis.link);
    wl_list_remove(&server->cursor_frame.link);
    wl_list_remove(&server->new_input.link);
    wl_list_remove(&server->request_cursor.link);
    wl_list_remove(&server->request_set_selection.link);
    wl_list_remove(&server->new_output.link);
    if (server->xdg_decoration_manager) { // Check if it was created
        wl_list_remove(&server->xdg_decoration_new.link);
    }


    /* Clean up toplevels */
    struct tinywl_toplevel *toplevel, *toplevel_tmp;
    wl_list_for_each_safe(toplevel, toplevel_tmp, &server->toplevels, link) {
        // xdg_toplevel_destroy will remove listeners and free toplevel
        // For safety, ensure destroy is called if not already
        if (toplevel->type == TINYWL_TOPLEVEL_XDG && toplevel->xdg_toplevel) {
             // xdg_toplevel_destroy is called via signal, no need to call manually here
        } else {
            wl_list_remove(&toplevel->link);
            // remove other specific listeners if it's a custom toplevel type
            free(toplevel);
        }
    }

    /* Clean up outputs */
    struct tinywl_output *output, *output_tmp;
    wl_list_for_each_safe(output, output_tmp, &server->outputs, link) {
        // output_destroy is called via signal
    }

    /* Clean up keyboards */
    struct tinywl_keyboard *kb, *kb_tmp;
    wl_list_for_each_safe(kb, kb_tmp, &server->keyboards, link) {
        // keyboard_handle_destroy is called via signal
    }
    
    /* Clean up popups */
    struct tinywl_popup *popup, *popup_tmp;
    wl_list_for_each_safe(popup, popup_tmp, &server->popups, link) {
        // popup_destroy is called via signal
    }


    /* Destroy resources */
    if (server->scene) { // scene_layout is part of scene
        wlr_scene_node_destroy(&server->scene->tree.node); // This destroys scene_layout too
        server->scene = NULL;
        server->scene_layout = NULL;
    }
     if (server->output_layout) { // Destroy after scene
        wlr_output_layout_destroy(server->output_layout);
        server->output_layout = NULL;
    }
    if (server->xdg_shell) { // Destroy after toplevels/popups
        // wlr_xdg_shell_destroy(server->xdg_shell); // This is usually handled by wl_display_destroy_clients
        server->xdg_shell = NULL;
    }
    if (server->xdg_decoration_manager) {
        // wlr_xdg_decoration_manager_v1_destroy(server->xdg_decoration_manager); // Also usually handled by wl_display_destroy_clients
        server->xdg_decoration_manager = NULL;
    }


    if (server->cursor_mgr) {
        wlr_xcursor_manager_destroy(server->cursor_mgr);
        server->cursor_mgr = NULL;
    }
    if (server->cursor) { // Destroy after output_layout
        wlr_cursor_destroy(server->cursor);
        server->cursor = NULL;
    }

    struct wlr_egl *egl_destroy = NULL;
    if (server->renderer) { 
        egl_destroy = wlr_gles2_renderer_get_egl(server->renderer);
    }

   
        

    if (egl_destroy && wlr_egl_make_current(egl_destroy, NULL)) {
        if (server->shader_program) glDeleteProgram(server->shader_program);
        if (server->passthrough_shader_program) glDeleteProgram(server->passthrough_shader_program);
         if (server->passthrough_icons_shader_program) glDeleteProgram(server->passthrough_icons_shader_program);
        if (server->rect_shader_program) glDeleteProgram(server->rect_shader_program);
        if (server->panel_shader_program) glDeleteProgram(server->panel_shader_program);
        if (server->ssd_shader_program) glDeleteProgram(server->ssd_shader_program);
        if (server->back_shader_program) glDeleteProgram(server->back_shader_program);
        if (server->fullscreen_shader_program) glDeleteProgram(server->fullscreen_shader_program); // NEW
    if (server->cube_shader_program) glDeleteProgram(server->cube_shader_program); 
        if (server->quad_vao) glDeleteVertexArrays(1, &server->quad_vao);
        if (server->quad_vbo) glDeleteBuffers(1, &server->quad_vbo);
        if (server->quad_ibo) glDeleteBuffers(1, &server->quad_ibo);
        if (server->genie_shader_program) glDeleteProgram(server->genie_shader_program);
        if (server->genie_vao) glDeleteVertexArrays(1, &server->genie_vao);
        if (server->genie_vbo) glDeleteBuffers(1, &server->genie_vbo);
        if (server->genie_ibo) glDeleteBuffers(1, &server->genie_ibo);
        // ...

        // Also add the new members to the zero-out list:
        server->genie_shader_program = 0;
        server->genie_vao = 0;
        server->genie_vbo = 0;
        server->genie_ibo = 0;
        
        server->shader_program = 0;
        server->rect_shader_program = 0;
        server->panel_shader_program = 0;
        server->ssd_shader_program = 0;
        server->back_shader_program = 0;
 
        server->fullscreen_shader_program = 0; // NEW
        server->cube_shader_program = 0; // NEW
        server->quad_vao = 0;
        server->quad_vbo = 0;
        server->quad_ibo = 0;

        wlr_egl_unset_current(egl_destroy);
    } else if (server->shader_program || server->rect_shader_program || server->panel_shader_program ||
               server->ssd_shader_program || server->back_shader_program ||
               server->fullscreen_shader_program || // NEW
                server->cube_shader_program || // NEW
               server->quad_vao || server->quad_vbo || server->quad_ibo) {
        wlr_log(WLR_ERROR, "Could not make EGL context current to delete GL resources in server_destroy");
    }
    
    if (server->allocator) { // Destroy before renderer
        wlr_allocator_destroy(server->allocator);
        server->allocator = NULL;
    }
    if (server->renderer) { 
         wlr_renderer_destroy(server->renderer);
         server->renderer = NULL;
    }
    if (server->backend) { // Destroy after renderer
        wlr_backend_destroy(server->backend);
        server->backend = NULL;
    }
    if (server->seat) { // Destroy after backend (input devices)
        wlr_seat_destroy(server->seat);
        server->seat = NULL;
    }
    if (server->wl_display) {
        wl_display_destroy_clients(server->wl_display); // Ensure clients are gone before display
        wl_display_destroy(server->wl_display);
        server->wl_display = NULL;
    }
}




static void focus_toplevel(struct tinywl_toplevel *toplevel) {
    if (toplevel == NULL) {
        wlr_log(WLR_ERROR, "focus_toplevel: No toplevel to focus");
        return;
    }
    struct tinywl_server *server = toplevel->server;
    if (!server) { // Defensive
        wlr_log(WLR_ERROR, "focus_toplevel: toplevel->server is NULL!");
        return;
    }
    struct wlr_seat *seat = server->seat;
    if (!seat) { // Defensive
        wlr_log(WLR_ERROR, "focus_toplevel: server->seat is NULL!");
        return;
    }

    // Ensure xdg_toplevel and its base surface are valid before dereferencing
    if (!toplevel->xdg_toplevel || !toplevel->xdg_toplevel->base || !toplevel->xdg_toplevel->base->surface) {
        wlr_log(WLR_ERROR, "focus_toplevel: toplevel xdg structures or surface are NULL! xdg_toplevel=%p", (void*)toplevel->xdg_toplevel);
        if (toplevel->xdg_toplevel && !toplevel->xdg_toplevel->base) wlr_log(WLR_ERROR, "  base is NULL");
        else if (toplevel->xdg_toplevel && toplevel->xdg_toplevel->base && !toplevel->xdg_toplevel->base->surface) wlr_log(WLR_ERROR, "  surface is NULL");
        return;
    }
    struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;

    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;

    if (prev_surface == surface) {
        wlr_log(WLR_DEBUG, "Surface %p already focused, skipping", surface); // Changed to DEBUG
        return;
    }

    if (prev_surface) {
        struct wlr_xdg_toplevel *prev_toplevel =
            wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
        if (prev_toplevel != NULL) {
            wlr_xdg_toplevel_set_activated(prev_toplevel, false);
            wlr_log(WLR_DEBUG, "Deactivated previous toplevel %p", prev_toplevel); // Changed to DEBUG
        }
    }

    if (!toplevel->scene_tree) { // Defensive
        wlr_log(WLR_ERROR, "focus_toplevel: toplevel->scene_tree is NULL!");
        return;
    }
    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
    wl_list_remove(&toplevel->link);
    wl_list_insert(&server->toplevels, &toplevel->link);
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
    wlr_log(WLR_INFO, "Focused toplevel %p, title='%s'", toplevel, // Changed to INFO
            toplevel->xdg_toplevel->title ? toplevel->xdg_toplevel->title : "(null)");

    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    // CRITICAL CHECK: Ensure keyboard exists before trying to use it for notify_enter
    if (keyboard) {
        if (surface) { // Surface should be valid from checks above
            wlr_log(WLR_INFO, "focus_toplevel: Attempting to notify seat of keyboard focus on surface %p", surface);
            wlr_seat_keyboard_notify_enter(seat, surface,
                keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
            wlr_log(WLR_INFO, "focus_toplevel: Notified seat of keyboard focus on surface %p", surface);
            if (server->wl_display) { // Defensive
                 wl_display_flush_clients(server->wl_display);
            } else {
                 wlr_log(WLR_ERROR, "focus_toplevel: server->wl_display is NULL during flush_clients!");
            }
        } else {
             wlr_log(WLR_ERROR, "focus_toplevel: Surface became NULL before notify_enter (should not happen).");
        }
    } else {
        wlr_log(WLR_ERROR, "focus_toplevel: No keyboard attached to seat %s. Cannot notify keyboard enter.", seat->name);
        // If there's no keyboard, we might still want to set pointer focus if applicable,
        // but for keyboard focus, we can't proceed.
        // Consider if wlr_seat_keyboard_clear_focus(seat); is appropriate if keyboard becomes NULL
        // or if focus should only be set if a keyboard is present.
        // For now, just logging is fine to confirm if this is the path.
    }
}

// Original interruption code - REMOVE THIS ENTIRE BLOCK to allow multiple animations:
/*
if (toplevel->server->animating_toplevel && toplevel->server->animating_toplevel != toplevel) {
    struct tinywl_toplevel *other = toplevel->server->animating_toplevel;
    wlr_log(WLR_INFO, "Interrupting animation for '%s' to start one for '%s'.",
            other->xdg_toplevel->title, toplevel->xdg_toplevel->title);
            
    // End the old animation instantly
    other->is_minimizing = false;
    other->is_maximizing = false; // Also handle maximizing if you add it
            
    if (other->minimizing_to_dock) { // It was minimizing
        other->minimized = true;
        if (other->scene_tree) {
            wlr_scene_node_set_enabled(&other->scene_tree->node, false);
        }
    } else { // It was restoring
        other->minimized = false;
    }
}
*/

// Add this function near toggle_minimize_toplevel
/*
static void toggle_maximize_toplevel(struct tinywl_toplevel *toplevel, bool maximized) {
    if (toplevel->maximized == maximized) {
        return;
    }

    // NEW: Hide decorations when starting the animation.
    set_decorations_visible(toplevel, false);

    struct tinywl_output *output;
    // Find the first enabled output to base our geometry on
    wl_list_for_each(output, &toplevel->server->outputs, link) {
        if (output->wlr_output && output->wlr_output->enabled) {
            
            // Set up animation parameters
            toplevel->is_maximizing = true;
            toplevel->maximize_start_time = get_monotonic_time_seconds_as_float();
            toplevel->maximize_duration = 0.35f; // A quick, snappy animation

            if (maximized) { // Animating TO maximized state
                toplevel->maximize_start_geom = toplevel->restored_geom;
                
                // Target is the full output geometry, minus panel height if you have one
                toplevel->maximize_target_geom.x = 0;
                toplevel->maximize_target_geom.y = 0; // Assuming panel is at the bottom
                toplevel->maximize_target_geom.width = output->wlr_output->width;
                // Adjust for your panel height. If no panel, use output->wlr_output->height
                toplevel->maximize_target_geom.height = output->wlr_output->height - TITLE_BAR_HEIGHT; 
            } else { // Animating FROM maximized TO restored state
                // The start geometry is the current (maximized) geometry
                toplevel->maximize_start_geom.x = toplevel->scene_tree->node.x;
                toplevel->maximize_start_geom.y = toplevel->scene_tree->node.y;
                
                // === THIS IS THE FIX ===
                // Get the current content geometry directly from the struct member.
                // No function call is needed in modern wlroots.
                toplevel->maximize_start_geom.width = toplevel->xdg_toplevel->base->geometry.width;
                toplevel->maximize_start_geom.height = toplevel->xdg_toplevel->base->geometry.height;
                // =======================

                toplevel->maximize_target_geom = toplevel->restored_geom;
            }

            // Tell the client its state is changing
            wlr_xdg_toplevel_set_maximized(toplevel->xdg_toplevel, maximized);
            // Schedule a frame to start the animation
            wlr_output_schedule_frame(output->wlr_output);
            
            // We only need to set this up for one output, so break the loop
            break;
        }
    }
}*/
static void toggle_minimize_toplevel(struct tinywl_toplevel *toplevel, bool minimized) {
    if (toplevel->minimized == minimized && !toplevel->is_minimizing) {
        if (!minimized) focus_toplevel(toplevel);
        return;
    }

    wlr_log(WLR_INFO, "Toggling minimize for '%s' to %d",
            toplevel->xdg_toplevel->title, minimized);

    set_decorations_visible(toplevel, false);

    toplevel->is_minimizing = true;
    toplevel->minimizing_to_dock = minimized;
    toplevel->minimize_start_time = get_monotonic_time_seconds_as_float();
    toplevel->minimize_duration = 2.5f;

    if (minimized) { // Minimizing TO the dock
        toplevel->minimized = false;
        toplevel->minimize_start_geom = toplevel->restored_geom;
        toplevel->minimize_target_geom = toplevel->panel_preview_box;
        wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
        if (toplevel->server->seat->keyboard_state.focused_surface == toplevel->xdg_toplevel->base->surface) {
            wlr_seat_keyboard_clear_focus(toplevel->server->seat);
        }
    } else { // Restoring FROM the dock
        toplevel->minimized = false;
        toplevel->minimize_start_geom = toplevel->panel_preview_box;
        toplevel->minimize_target_geom = toplevel->restored_geom;
        wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);

        // *** THE FIX - PART 1 ***
        // When restoring, we must re-enable the client's content so it becomes visible.
        if (toplevel->client_xdg_scene_tree) {
            wlr_scene_node_set_enabled(&toplevel->client_xdg_scene_tree->node, true);
        }
        
        focus_toplevel(toplevel);
    }

    struct tinywl_output *output;
    wl_list_for_each(output, &toplevel->server->outputs, link) {
        wlr_output_schedule_frame(output->wlr_output);
    }
}
/*
static void toggle_minimize_toplevel(struct tinywl_toplevel *toplevel, bool minimized) {
	// This check now primarily handles restoring a window or re-focusing an active one.
	if (toplevel->minimized == minimized) {
		if (!minimized) focus_toplevel(toplevel);
		return;
	}

	wlr_log(WLR_INFO, "Toggling 'minimize' for '%s' to %d",
			toplevel->xdg_toplevel->title, minimized);
	
	// Set up animation parameters
	toplevel->is_minimizing = true;
	toplevel->minimizing_to_dock = minimized; // This still controls the animation's direction
	toplevel->minimize_start_time = get_monotonic_time_seconds_as_float();
	toplevel->minimize_duration = 1.0f; // 1-second animation
	
	if (minimized) { // This is the case for animating TO the dock
		toplevel->minimize_start_geom = toplevel->restored_geom;
		toplevel->minimize_target_geom = toplevel->panel_preview_box;
		
		// The scene node must be enabled for the animation to be visible.
		wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
		
		// We can still clear keyboard focus to make the window feel "inactive".
		if (toplevel->server->seat->keyboard_state.focused_surface == toplevel->xdg_toplevel->base->surface) {
			wlr_seat_keyboard_clear_focus(toplevel->server->seat);
		}

		// *** THE KEY CHANGE IS HERE ***
		// We DO NOT set `toplevel->minimized = true;`.
		// By omitting this state change, the render loop will not hide the window's
		// scene node when the animation finishes. The window will animate to the
		// panel preview box and remain there as a tiny, visible version.
		wlr_log(WLR_INFO, "Playing minimize animation, but the window will remain visible.");

	} else { // This case handles restoring FROM the dock
		toplevel->minimized = false; // Set the state back to normal.
		toplevel->minimize_start_geom = toplevel->panel_preview_box;
		toplevel->minimize_target_geom = toplevel->restored_geom;
		wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
		focus_toplevel(toplevel);
	}

	// A redraw is needed to start the animation.
	struct tinywl_output *output;
	wl_list_for_each(output, &toplevel->server->outputs, link) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}*/
static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    struct tinywl_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
        &keyboard->wlr_keyboard->modifiers);
}

static bool handle_keybinding(struct tinywl_server *server, xkb_keysym_t sym) {
    switch (sym) {
    case XKB_KEY_Escape:
        wl_display_terminate(server->wl_display);
        break;
    case XKB_KEY_F1:
        if (wl_list_length(&server->toplevels) < 2) {
            break;
        }
        struct tinywl_toplevel *next_toplevel =
            wl_container_of(server->toplevels.prev, next_toplevel, link);
        focus_toplevel(next_toplevel);
        break;
    default:
        return false;
    }
    return true;
}

/**
 * Handles all keyboard key events.
 *
 * This function processes key presses in the following order of priority:
 * 1. Alt+Shift+[1-4]: Move the focused window to the specified desktop.
 * 2. P (no modifiers): Toggle the "expo" overview effect.
 * 3. O (no modifiers): Cycle to the next virtual desktop.
 * 4. Alt+[Key] (e.g., F1, Escape): Handle compositor-level shortcuts like focus cycling or exit.
 *
 * If a key combination is not handled by the compositor, it is forwarded to the
 * currently focused client application.
 */
static void keyboard_handle_key(struct wl_listener *listener, void *data) {
    struct tinywl_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct tinywl_server *server = keyboard->server;
    struct wlr_keyboard_key_event *event = data;
    struct wlr_seat *seat = server->seat;

    // Duplicate event detection
    static uint32_t last_keycode = 0, last_state = 0, last_time = 0;
    if (last_keycode == event->keycode && last_state == event->state && 
        event->time_msec > 0 && last_time > 0 && 
        abs((int)(event->time_msec - last_time)) < 5) {
        wlr_log(WLR_DEBUG, "Ignoring duplicate key event: keycode=%u", event->keycode);
        return;
    }
    last_keycode = event->keycode;
    last_state = event->state;
    last_time = event->time_msec;

    // Phantom release detection
    if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED && !seat->keyboard_state.focused_surface) {
        xkb_keycode_t xkb_keycode = event->keycode + 8;
        if (xkb_state_key_get_level(keyboard->wlr_keyboard->xkb_state, xkb_keycode, 0) == 0) {
            wlr_log(WLR_DEBUG, "Ignoring phantom release for keycode=%u", event->keycode);
            return;
        }
    }

    uint32_t keycode_xkb = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode_xkb, &syms);
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
    bool handled_by_compositor = false, needs_redraw = false;

    // Move window to desktop (Alt + Shift + [1-4])
    if ((modifiers & (WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT)) == (WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT) && 
        event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        
        struct wlr_surface *focused = seat->keyboard_state.focused_surface;
        if (focused) {
            struct wlr_xdg_toplevel *xdg_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(focused);
            if (xdg_toplevel && xdg_toplevel->base && xdg_toplevel->base->data) {
                struct tinywl_toplevel *toplevel = xdg_toplevel->base->data;
                
                for (int i = 0; i < nsyms; i++) {
                    if (syms[i] >= XKB_KEY_1 && syms[i] <= XKB_KEY_4) {
                        int target = syms[i] - XKB_KEY_1;
                        if (target < server->num_desktops) {
                            wlr_log(WLR_INFO, "Moving toplevel '%s' to desktop %d", 
                                    toplevel->xdg_toplevel->title, target);
                            toplevel->desktop = target;
                            update_toplevel_visibility(server);
                            handled_by_compositor = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Handle key press bindings (no modifiers) - Your new consolidated logic
    if (!handled_by_compositor && event->state == WL_KEYBOARD_KEY_STATE_PRESSED && modifiers == 0) {
        int target_desktop = -1;
        int current_desktop = server->current_desktop;
        
        for (int i = 0; i < nsyms; i++) {
            if (syms[i] == XKB_KEY_O || syms[i] == XKB_KEY_o) {
                int group_start = (current_desktop / 4) * 4;
                target_desktop = current_desktop + 1;
                if (target_desktop >= group_start + 4 || target_desktop >= server->num_desktops) {
                    target_desktop = group_start;
                }
                break;
            }
            if (syms[i] == XKB_KEY_I || syms[i] == XKB_KEY_i) {
                int group_start = (current_desktop / 4) * 4;
                target_desktop = current_desktop - 1;
                if (target_desktop < group_start) {
                    target_desktop = group_start + 3;
                }
                break;
            }
            if (syms[i] == XKB_KEY_w || syms[i] == XKB_KEY_W) {
                target_desktop = current_desktop - DestopGridSize;
                if (target_desktop >= 0) 
                    GLOBAL_vertical_offset -= 1.2;
                break;
            }
            if (syms[i] == XKB_KEY_S || syms[i] == XKB_KEY_s) {
                target_desktop = current_desktop + DestopGridSize;
                if (target_desktop < server->num_desktops) 
                    GLOBAL_vertical_offset += 1.2;
                break;
            }
        }
        
        // If a valid desktop switch was triggered...
        if (target_desktop != -1 && target_desktop >= 0 && target_desktop < server->num_desktops) {
            wlr_log(WLR_INFO, "Switching from desktop %d to %d", server->current_desktop, target_desktop);
            server->current_desktop = target_desktop;
            server->pending_desktop_switch = target_desktop; // For animations
            // Call the one true function to fix everything.
            update_compositor_state(server);
            handled_by_compositor = true;
        }
        
        // Handle other key bindings that don't involve desktop switching
        if (!handled_by_compositor) {
            for (int i = 0; i < nsyms && !handled_by_compositor; i++) {
                switch (syms[i]) {
                    case XKB_KEY_a: case XKB_KEY_A:
                        switch_mode = (switch_mode + 1) % 2;
                        wlr_log(WLR_INFO, "Expo effect mode: %s", switch_mode == 0 ? "Zoom" : "Slide");
                        handled_by_compositor = true;
                        break;
                        
                    case XKB_KEY_q: case XKB_KEY_Q:
                        switchXY = 1 - switchXY;
                        wlr_log(WLR_INFO, "Expo slide direction: %s", switchXY == 0 ? "Horizontal" : "Vertical");
                        handled_by_compositor = true;
                        break;
                        
                    case XKB_KEY_p: case XKB_KEY_P:
                        if (!server->expo_effect_active && !server->cube_effect_active) {
                            server->expo_effect_active = true;
                            wlr_log(WLR_INFO, "Expo effect ENABLED");
                            server->effect_is_target_zoomed = true;
                            server->effect_anim_start_factor = server->effect_zoom_factor_normal;
                            server->effect_anim_target_factor = server->effect_zoom_factor_zoomed;
                        } else {
                            server->effect_is_target_zoomed = !server->effect_is_target_zoomed;
                            server->effect_anim_start_factor = server->effect_anim_current_factor;
                            server->effect_anim_target_factor = server->effect_is_target_zoomed ?
                                server->effect_zoom_factor_zoomed : server->effect_zoom_factor_normal;
                        }
                        
                        if (fabs(server->effect_anim_start_factor - server->effect_anim_target_factor) > 1e-4f) {
                            server->effect_anim_start_time_sec = get_monotonic_time_seconds_as_float();
                            server->effect_is_animating_zoom = true;
                        }
                        update_toplevel_visibility(server);
                        handled_by_compositor = true;
                        break;
                        
                    case XKB_KEY_l: case XKB_KEY_L:
                        if (!server->cube_effect_active) {
                            server->cube_effect_active = true;
                            wlr_log(WLR_INFO, "Cube effect ENABLED");
                            server->cube_is_target_zoomed = true;
                            server->cube_anim_start_factor = server->cube_zoom_factor_normal;
                            server->cube_anim_target_factor = server->cube_zoom_factor_zoomed;
                        } else {
                            server->cube_is_target_zoomed = !server->cube_is_target_zoomed;
                            server->cube_anim_start_factor = server->cube_anim_current_factor;
                            server->cube_anim_target_factor = server->cube_is_target_zoomed ?
                                server->cube_zoom_factor_zoomed : server->cube_zoom_factor_normal;
                        }
                        
                        if (fabs(server->cube_anim_start_factor - server->cube_anim_target_factor) > 1e-4f) {
                            server->cube_anim_start_time_sec = get_monotonic_time_seconds_as_float();
                            server->cube_is_animating_zoom = true;
                        }
                        update_toplevel_visibility(server);
                        handled_by_compositor = true;
                        break;
                        
                    case XKB_KEY_k: case XKB_KEY_K:
                        if (!server->tv_effect_animating) {
                            wlr_log(WLR_INFO, "TV Animation triggered");
                            server->tv_effect_animating = true;
                            server->tv_effect_start_time = get_monotonic_time_seconds_as_float();
                            
                            struct tinywl_output *output;
                            wl_list_for_each(output, &server->outputs, link) {
                                if (output->wlr_output && output->wlr_output->enabled) {
                                    wlr_output_schedule_frame(output->wlr_output);
                                }
                            }
                        }
                        handled_by_compositor = true;
                        break;
                }
            }
        }
    }

    // Alt + key bindings (no Shift)
    if (!handled_by_compositor && (modifiers & WLR_MODIFIER_ALT) && !(modifiers & WLR_MODIFIER_SHIFT)) {
        for (int i = 0; i < nsyms; i++) {
            if (handle_keybinding(server, syms[i])) {
                handled_by_compositor = true;
                needs_redraw = true;
                break;
            }
        }
    }

    if (needs_redraw) {
        struct tinywl_output *output;
        wl_list_for_each(output, &server->outputs, link) {
            if (output->wlr_output && output->wlr_output->enabled) {
                wlr_output_schedule_frame(output->wlr_output);
            }
        }
    }

    // Forward to client if not handled
    if (!handled_by_compositor) {
        static struct wlr_keyboard *last_keyboard = NULL;
        if (last_keyboard != keyboard->wlr_keyboard) {
            wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
            last_keyboard = keyboard->wlr_keyboard;
        }
        if (seat->keyboard_state.focused_surface) {
            wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
        }
    }
}
static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
    struct tinywl_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    free(keyboard);
}

static void server_new_keyboard(struct tinywl_server *server, struct wlr_input_device *device) {
    struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
    if (!wlr_keyboard) {
        wlr_log(WLR_ERROR, "Failed to get wlr_keyboard from input device %p", device);
        return;
    }

    struct tinywl_keyboard *keyboard = calloc(1, sizeof(*keyboard));
    if (!keyboard) {
        wlr_log(WLR_ERROR, "Failed to allocate tinywl_keyboard");
        return;
    }
    keyboard->server = server;
    keyboard->wlr_keyboard = wlr_keyboard;

    // Initialize XKB context and keymap
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!context) {
        wlr_log(WLR_ERROR, "Failed to create XKB context");
        free(keyboard);
        return;
    }

    // Use a more specific keymap to align with RDP client expectations
    struct xkb_rule_names rules = {0};
    rules.rules = "evdev";
    rules.model = "pc105";
    rules.layout = "gb";
    rules.variant = "";
    rules.options = "";
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        wlr_log(WLR_ERROR, "Failed to create XKB keymap");
        xkb_context_unref(context);
        free(keyboard);
        return;
    }

    if (!wlr_keyboard_set_keymap(wlr_keyboard, keymap)) {
        wlr_log(WLR_ERROR, "Failed to set keymap on wlr_keyboard");
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        free(keyboard);
        return;
    }
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);

    // Set repeat info
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);
    wlr_log(WLR_ERROR, "Keyboard %p initialized with keymap and repeat info", wlr_keyboard);

    // Add event listeners
    keyboard->modifiers.notify = keyboard_handle_modifiers;
    wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
    keyboard->key.notify = keyboard_handle_key;
    wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
    keyboard->destroy.notify = keyboard_handle_destroy;
    wl_signal_add(&device->events.destroy, &keyboard->destroy);

    // Set keyboard on seat
    wlr_seat_set_keyboard(server->seat, wlr_keyboard);
    wlr_log(WLR_ERROR, "Keyboard %p set on seat %s", wlr_keyboard, server->seat->name);

    wl_list_insert(&server->keyboards, &keyboard->link);
    wlr_log(WLR_ERROR, "Keyboard %p added to server keyboards list", wlr_keyboard);

    // Update seat capabilities
    wlr_seat_set_capabilities(server->seat, WL_SEAT_CAPABILITY_KEYBOARD);
    wlr_log(WLR_ERROR, "Set seat capabilities: WL_SEAT_CAPABILITY_KEYBOARD");
}

static void server_pointer_motion(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, pointer_motion); 
    struct wlr_pointer_motion_event *event = data;

    if (!server->cursor || !server->output_layout) {
        wlr_log(WLR_ERROR, "Cursor or output layout is null during motion event");
        return;
    }

    wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
    wlr_log(WLR_DEBUG, "Pointer motion: cursor moved to (%f, %f)", server->cursor->x, server->cursor->y);
    process_cursor_motion(server, event->time_msec);
}

static void server_new_pointer(struct tinywl_server *server, struct wlr_input_device *device) {
    if (device->type != WLR_INPUT_DEVICE_POINTER) {
        wlr_log(WLR_ERROR, "Device %p is not a pointer", device);
        return;
    }
    wlr_cursor_attach_input_device(server->cursor, device); // This is good, adds device to cursor

    // The wlr_cursor itself will emit motion events that aggregate all attached inputs.
    // You are already listening to server->cursor->events.motion in main().
    // So, you might not even need to listen to individual pointer->events.motion here.

    // If you *do* want to listen to individual raw pointer events:
    struct wlr_pointer *pointer = wlr_pointer_from_input_device(device);
    if (pointer) {
        // If you absolutely need a per-device listener that calls server_pointer_motion,
        // then server_pointer_motion CANNOT use wl_container_of with server.pointer_motion.
        // It would need a different way to get the 'server' instance.

        // For now, let's assume the listener on server.cursor->events.motion in main()
        // is sufficient for cursor movement. We can remove the direct listener on
        // pointer->events.motion to avoid the conflict if server.cursor handles aggregation.

        wlr_log(WLR_DEBUG, "Pointer device %p attached to cursor. Cursor will aggregate its events.", device);
        wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default"); // Set cursor image for this device if needed
    } else {
        wlr_log(WLR_ERROR, "Failed to get wlr_pointer from device %p", device);
    }
}


static void server_new_input(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        server_new_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        server_new_pointer(server, device);
        break;
    default:
        break;
    }
    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards)) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, request_cursor);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wlr_seat_client *focused_client = server->seat->pointer_state.focused_client;
    if (focused_client == event->seat_client) {
        wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
    }
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}
/*
static struct tinywl_toplevel *find_toplevel_at(struct tinywl_server *server,
        double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
    
    // Iterate through our toplevels list FROM TOP TO BOTTOM (front of the list).
    struct tinywl_toplevel *toplevel;
    wl_list_for_each(toplevel, &server->toplevels, link) {
        // Condition 1: Must be an XDG toplevel with a valid scene tree.
        if (toplevel->type != TINYWL_TOPLEVEL_XDG || !toplevel->scene_tree) {
            continue;
        }

        // Condition 2: The scene node must be enabled.
        if (!toplevel->scene_tree->node.enabled) {
            continue;
        }
         if (toplevel->minimized || toplevel->is_minimizing) {
            continue;
        }
        // Condition 3: Must be on the currently active desktop.
        // This is the key logic that makes windows on other desktops non-interactive.
        if (toplevel->desktop != server->current_desktop) {
            continue;
        }

        // Condition 4: The cursor must be within the toplevel's bounding box.
        struct wlr_box toplevel_box = {
            .x = toplevel->scene_tree->node.x,
            .y = toplevel->scene_tree->node.y,
        };

        // --- FIX 1: Use direct struct access instead of the old function ---
        struct wlr_box content_geo = {0};
        if (toplevel->xdg_toplevel && toplevel->xdg_toplevel->base) {
             content_geo = toplevel->xdg_toplevel->base->geometry;
        }
        
        if (toplevel->ssd.enabled) {
            toplevel_box.width = content_geo.width + 2 * BORDER_WIDTH;
            toplevel_box.height = content_geo.height + TITLE_BAR_HEIGHT + BORDER_WIDTH;
        } else {
            toplevel_box.width = content_geo.width;
            toplevel_box.height = content_geo.height;
        }
        
        if (wlr_box_contains_point(&toplevel_box, lx, ly)) {
            // It's a match! Now, find the specific surface and local coords.
            
            // --- FIX 2 & 3: Correct the variable declaration and typo ---
            // Now that we know we are inside this toplevel's box, we can use
            // wlr_scene_node_at to find the specific component under the cursor.
            struct wlr_scene_node *node_at = wlr_scene_node_at(
                &toplevel->scene_tree->node, lx, ly, sx, sy);
            
            if (node_at && node_at->type == WLR_SCENE_NODE_BUFFER) {
                // We are over a client surface.
                struct wlr_scene_buffer *sbuf = wlr_scene_buffer_from_node(node_at);
                struct wlr_scene_surface *ssurf = wlr_scene_surface_try_from_buffer(sbuf);
                if (ssurf) {
                    *surface = ssurf->surface;
                    return toplevel; // Return the toplevel and the surface.
                }
            }
            
            // We are over a decoration or empty space within the toplevel's frame.
            // Return the toplevel, but indicate no specific surface was hit.
            *surface = NULL;
            return toplevel;
        }
    }

    // No interactive window was found at these coordinates.
    return NULL;
}*/

// You can replace or augment desktop_toplevel_at.
// This new function tries to find the toplevel owner of any scene node.
static struct tinywl_toplevel *get_toplevel_from_scene_node(struct wlr_scene_node *node) {
    if (!node) {
        return NULL;
    }
    // Traverse up the scene graph to find a node that has a tinywl_toplevel in its data field
    // and ensure that node is the toplevel->scene_tree.
    struct wlr_scene_tree *tree = node->parent;
    while (tree != NULL) {
        if (tree->node.data != NULL) {
            struct tinywl_toplevel *toplevel_candidate = tree->node.data;
            // Verify this is indeed the main scene_tree of the toplevel
            if (toplevel_candidate->scene_tree == tree) {
                return toplevel_candidate;
            }
        }
        tree = tree->node.parent;
    }
    // If the node itself is a toplevel's scene_tree
    if (node->type == WLR_SCENE_NODE_TREE && node->data != NULL) {
        struct tinywl_toplevel *toplevel_candidate = node->data;
        if (toplevel_candidate->scene_tree == (struct wlr_scene_tree *)node) {
            return toplevel_candidate;
        }
    }
    return NULL;
}

static struct tinywl_toplevel *desktop_toplevel_at(
        struct tinywl_server *server, double lx, double ly,
        struct wlr_surface **surface, double *sx, double *sy) {

    // Iterate through our toplevels list FROM TOP TO BOTTOM.
    // The first one we hit that is on the correct desktop and under the
    // cursor is the one we want.
    struct tinywl_toplevel *toplevel;
    wl_list_for_each(toplevel, &server->toplevels, link) {
        // Condition 1: Must be an XDG toplevel with a valid scene tree.
        if (toplevel->type != TINYWL_TOPLEVEL_XDG || !toplevel->scene_tree) {
            continue;
        }

        // Condition 2: The scene node must be enabled.
        if (!toplevel->scene_tree->node.enabled) {
            continue;
        }

        // Condition 3: Must be on the currently active desktop,
        // unless an overview effect is active.
        if (!server->expo_effect_active && !server->cube_effect_active &&
            toplevel->desktop != server->current_desktop) {
            continue;
        }

        // Condition 4: The cursor must be within the node's bounding box.
        // We get the node's position and size to check this.
        int node_x = toplevel->scene_tree->node.x;
        int node_y = toplevel->scene_tree->node.y;
        
        // --- THIS IS THE FIX ---
        // Get the geometry directly from the struct member.
        struct wlr_box geo = {0};
        if (toplevel->xdg_toplevel && toplevel->xdg_toplevel->base) {
       //     geo = toplevel->xdg_toplevel->base->geometry;
        }

        struct wlr_box toplevel_box = {
            .x = node_x,
            .y = node_y,
            .width = geo.width,
            .height = geo.height,
        };
        
        if (wlr_box_contains_point(&toplevel_box, lx, ly)) {
            // It's a match! Now, find the specific surface and local coords.
            // We can now safely use wlr_scene_node_at on this specific tree.
            struct wlr_scene_node *node_inside = wlr_scene_node_at(
                &toplevel->scene_tree->node, lx, ly, sx, sy);
            
            if (node_inside && node_inside->type == WLR_SCENE_NODE_BUFFER) {
                struct wlr_scene_buffer *sbuf = wlr_scene_buffer_from_node(node_inside);
                struct wlr_scene_surface *ssurf = wlr_scene_surface_try_from_buffer(sbuf);
                if (ssurf) {
                    *surface = ssurf->surface;
                    return toplevel; // Success!
                }
            }
            // If we are inside the box but not over a buffer (e.g., on the SSD border),
            // we still found our target toplevel, even if the surface is null.
            *surface = NULL;
            return toplevel;
        }
    }

    // If we get here, no interactive window was found.
    return NULL;
}
///////////////////////////////////////////////////


static void process_cursor_move(struct tinywl_server *server) { // This is called when server->cursor_mode == TINYWL_CURSOR_MOVE
    struct tinywl_toplevel *toplevel = server->grabbed_toplevel;
    if (!toplevel) return; // Should not happen in move mode

    // New position for the toplevel's scene_tree
    int new_x = server->cursor->x - server->grab_x;
    int new_y = server->cursor->y - server->grab_y;

    wlr_scene_node_set_position(&toplevel->scene_tree->node, new_x, new_y);
    wlr_log(WLR_DEBUG, "Moving toplevel to (%d, %d)", new_x, new_y);
}

// --- Modified process_cursor_resize ---
static void process_cursor_resize(struct tinywl_server *server) {
    struct tinywl_toplevel *toplevel = server->grabbed_toplevel;
    if (!toplevel || toplevel->type != TINYWL_TOPLEVEL_XDG || !toplevel->xdg_toplevel) {
        return;
    }

    // border_x and border_y are the new target screen coordinates for the
    // *content edges* being dragged, based on initial grab offset.
    double border_x_target = server->cursor->x - server->grab_x;
    double border_y_target = server->cursor->y - server->grab_y;

    // Initialize new content dimensions/positions with the initial grab_geobox (content screen geo)
    int new_content_left_screen = server->grab_geobox.x;
    int new_content_top_screen = server->grab_geobox.y;
    int new_content_width = server->grab_geobox.width;
    int new_content_height = server->grab_geobox.height;

    if (server->resize_edges & WLR_EDGE_TOP) {
        new_content_top_screen = border_y_target;
        new_content_height = (server->grab_geobox.y + server->grab_geobox.height) - new_content_top_screen;
    } else if (server->resize_edges & WLR_EDGE_BOTTOM) {
        new_content_height = border_y_target - new_content_top_screen;
    }

    if (server->resize_edges & WLR_EDGE_LEFT) {
        new_content_left_screen = border_x_target;
        new_content_width = (server->grab_geobox.x + server->grab_geobox.width) - new_content_left_screen;
    } else if (server->resize_edges & WLR_EDGE_RIGHT) {
        new_content_width = border_x_target - new_content_left_screen;
    }

    // Add minimum size constraints for content
    int min_width = 50;  // Minimum content width
    int min_height = 30; // Minimum content height
    if (new_content_width < min_width) {
        if (server->resize_edges & WLR_EDGE_LEFT) {
            new_content_left_screen -= (min_width - new_content_width);
        }
        new_content_width = min_width;
    }
    if (new_content_height < min_height) {
         if (server->resize_edges & WLR_EDGE_TOP) {
            new_content_top_screen -= (min_height - new_content_height);
        }
        new_content_height = min_height;
    }

    // Determine new position for the scene_tree (top-left of entire decorated window)
    int scene_tree_new_x = new_content_left_screen;
    int scene_tree_new_y = new_content_top_screen;

    if (toplevel->ssd.enabled) {
        scene_tree_new_x -= BORDER_WIDTH;
        scene_tree_new_y -= TITLE_BAR_HEIGHT;
    }
    // else: scene_tree_new_x/y are already correct as content_left/top if no SSD

    wlr_scene_node_set_position(&toplevel->scene_tree->node, scene_tree_new_x, scene_tree_new_y);
    wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_content_width, new_content_height);

    // update_decoration_geometry will be called on the next commit from the client
    // which will correctly size and position SSDs around the new content size,
    // and position client_xdg_scene_tree within the main scene_tree.
}


// Add this helper function to improve popup handling
static void ensure_popup_responsiveness(struct tinywl_server *server, struct wlr_surface *surface) {
    if (!surface) return;
    
    struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface);
    if (!xdg_surface || xdg_surface->role != WLR_XDG_SURFACE_ROLE_POPUP) {
        return;
    }
    
    // Ensure popup gets frame callbacks
    if (surface->current.buffer) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        wlr_surface_send_frame_done(surface, &now);
    }
    
    // Schedule configure if needed
    if (!xdg_surface->configured) {
        wlr_xdg_surface_schedule_configure(xdg_surface);
    }
}
/////////////////////////////////////////////////////////////
// Replace this function
static void process_cursor_motion(struct tinywl_server *server, uint32_t time) {
    /* NEW: Dock animation trigger logic */
    if (server->cursor->x <= 0 && !server->dock_mouse_at_edge) {
        // Mouse just hit the left edge, and the animation isn't already running
        server->dock_mouse_at_edge = true; // Set the latch

        // Start a new animation
        server->dock_anim_active = true;
        server->dock_anim_start_time = get_monotonic_time_seconds_as_float();
        server->dock_anim_start_value = dockX_align;

        // Toggle the target value
        if (server->dock_anim_target_value == 90.0f) {
            server->dock_anim_target_value = 10.0f;
        } else {
            server->dock_anim_target_value = 90.0f;
        }
        wlr_log(WLR_INFO, "Dock animation triggered. Target: %.1f", server->dock_anim_target_value);

    } else if (server->cursor->x > 0) {
        // Mouse has moved away from the edge, reset the latch
        server->dock_mouse_at_edge = false;
    }


    /* The rest of the function remains the same */
    if (server->cursor_mode == TINYWL_CURSOR_MOVE) {
        process_cursor_move(server);
        return;
    } else if (server->cursor_mode == TINYWL_CURSOR_RESIZE) {
        process_cursor_resize(server);
        return;
    }

    double sx, sy;
    struct wlr_seat *seat = server->seat;
    struct wlr_surface *surface = NULL;
    
  //  struct tinywl_toplevel *toplevel = find_toplevel_at(server,
    //        server->cursor->x, server->cursor->y, &surface, &sx, &sy);

    if (!surface) {
        wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
        wlr_seat_pointer_clear_focus(seat);
        return;
    }

    wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
    // THIS listener *IS* server.cursor_motion from the struct tinywl_server
    struct tinywl_server *server = wl_container_of(listener, server, cursor_motion); // Correct use of server.cursor_motion
    struct wlr_pointer_motion_event *event = data;

    // The 'dev' field in wlr_pointer_motion_event from wlr_cursor might be NULL
    // if the motion was synthetic (e.g. warp). Use server->cursor->active_pointer_device if needed.
    // For wlr_cursor_move, the device argument is optional if you just want to move based on deltas.
    wlr_cursor_move(server->cursor, NULL /* or &event->pointer->base if from a specific device */,
                    event->delta_x, event->delta_y);
    process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
    process_cursor_motion(server, event->time_msec);
}



// The complete, updated server_cursor_button function
static void server_cursor_button(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, cursor_button);
    struct wlr_pointer_button_event *event = data;
    struct wlr_seat *seat = server->seat;


 // NEW: Check for dock icon clicks first
    if (event->button == BTN_LEFT && event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        for (int i = 0; i < server->num_dock_icons; i++) {
            // Check if the cursor's current position is inside the icon's box
            if (wlr_box_contains_point(&server->dock_icons[i].box, server->cursor->x, server->cursor->y)) {
                wlr_log(WLR_INFO, "Clicked on dock icon %d, launching: %s", i, server->dock_icons[i].app_command);
                
                // Call the new launch function
                launch_application(server, server->dock_icons[i].app_command);
                
                // Notify the seat that a button was pressed and then return,
                // as this event has been fully handled by the compositor.
                wlr_seat_pointer_notify_button(seat, event->time_msec, event->button, event->state);
                return;
            }
        }
    }

    // --- 1. Check for clicks inside the panel area first ---
    if (server->top_panel_node && server->top_panel_node->enabled) {
        struct wlr_scene_rect *panel_srect = wlr_scene_rect_from_node(server->top_panel_node);
        struct wlr_box panel_box = {
            .x = server->top_panel_node->x,
            .y = server->top_panel_node->y,
            .width = panel_srect->width,
            .height = panel_srect->height,
        };

        if (event->button == BTN_LEFT && event->state == WL_POINTER_BUTTON_STATE_PRESSED &&
            wlr_box_contains_point(&panel_box, server->cursor->x, server->cursor->y)) {
            
            // The click was inside the panel. Check if it hit a toplevel preview.
            struct tinywl_toplevel *toplevel_iter;
            wl_list_for_each(toplevel_iter, &server->toplevels, link) {
                // We only care about previews on the current desktop.
                if (toplevel_iter->desktop == server->current_desktop &&
                    wlr_box_contains_point(&toplevel_iter->panel_preview_box, server->cursor->x, server->cursor->y)) {
                    
                    // Clicked on this toplevel's preview icon.
                    // Toggle its minimized state.
                    toggle_minimize_toplevel(toplevel_iter, !toplevel_iter->minimized);
                    
                    // The event is handled, notify the seat and return.
                    wlr_seat_pointer_notify_button(seat, event->time_msec, event->button, event->state);
                    return;
                }
            }
        }
    }

    // --- 2. Find which toplevel (if any) is under the cursor ---
    struct wlr_surface *surface_at_cursor = NULL;
    double sx, sy;
   // struct tinywl_toplevel *toplevel = find_toplevel_at(server,
     //   server->cursor->x, server->cursor->y, &surface_at_cursor, &sx, &sy);

    // --- 3. Handle button release ---
    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        // If we are in a move or resize mode, releasing the button ends it.
        if (server->cursor_mode != TINYWL_CURSOR_PASSTHROUGH) {
            server->cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
            server->grabbed_toplevel = NULL;
        }
    } 
    // --- 4. Handle button press ---
    else { 
        bool handled_by_compositor = false;
/*
        // Only interact with a toplevel if one was found and it's not minimized
        if (toplevel && !toplevel->minimized) {
            focus_toplevel(toplevel); // Bring window to front and give it focus

            // Check if decorations are enabled and the click was the left mouse button
            if (toplevel->ssd.enabled && event->button == BTN_LEFT) {
                double dummy_sx, dummy_sy;
                // Find the specific scene node (title bar, button, border) under the cursor
                struct wlr_scene_node *node_at = wlr_scene_node_at(
                    &toplevel->scene_tree->node, server->cursor->x, server->cursor->y, &dummy_sx, &dummy_sy);

                // Check which decoration element was clicked
                if (node_at == &toplevel->ssd.close_button->node) {
                    wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
                    handled_by_compositor = true;

                } else if (node_at == &toplevel->ssd.maximize_button->node) {
                //    toggle_maximize_toplevel(toplevel, !toplevel->maximized);
                    handled_by_compositor = true;

                } else if (node_at == &toplevel->ssd.minimize_button->node) { // NEW
                    // Minimize the window. The panel click logic will handle restoring it.
                    toggle_minimize_toplevel(toplevel, true); 
                    handled_by_compositor = true;    

                } else if (node_at == &toplevel->ssd.title_bar->node) {
                    begin_interactive(toplevel, TINYWL_CURSOR_MOVE, 0);
                    handled_by_compositor = true;

                } else {
                    // Check for border resize clicks
                    uint32_t resize_edges = 0;
                    if (node_at == &toplevel->ssd.border_left->node) resize_edges = WLR_EDGE_LEFT;
                    else if (node_at == &toplevel->ssd.border_right->node) resize_edges = WLR_EDGE_RIGHT;
                    else if (node_at == &toplevel->ssd.border_bottom->node) resize_edges = WLR_EDGE_BOTTOM;
                    
                    if (resize_edges != 0) {
                        begin_interactive(toplevel, TINYWL_CURSOR_RESIZE, resize_edges);
                        handled_by_compositor = true;
                    }
                }
            }
        } else {
            // If the user clicked on the empty desktop, clear keyboard focus.
            wlr_seat_keyboard_clear_focus(seat);
        }
  */      
        // If the click wasn't handled by our compositor (e.g., it was on the client's content area),
        // make sure the keyboard is ready to be passed to the client.
        if (!handled_by_compositor) {
            // This ensures the correct keyboard is associated with the seat for the client.
            struct tinywl_keyboard *keyboard;
            if (!wl_list_empty(&server->keyboards)) {
                 keyboard = wl_container_of(server->keyboards.next, keyboard, link);
                 wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
            }
        }
    }
    
    // --- 5. Forward the button event ---
    // This is crucial. Even if we handle the click, the seat needs to be notified.
    // If we didn't handle it, this forwards the click to the focused client application.
    wlr_seat_pointer_notify_button(seat, event->time_msec, event->button, event->state);
}
static void server_cursor_axis(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, cursor_axis);
    struct wlr_pointer_axis_event *event = data;
   // wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation,
     //   event->delta, event->delta_discrete, event->source, event->relative_direction);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}


static void popup_map(struct wl_listener *listener, void *data) {
    struct tinywl_popup *popup = wl_container_of(listener, popup, map);
    
    // Enable the popup in the scene
    wlr_scene_node_set_enabled(&popup->scene_tree->node, true);
    
    // Send the ack_configure immediately to complete the handshake
    if (popup->xdg_popup && popup->xdg_popup->base) {
        struct wlr_xdg_surface *xdg_surface = popup->xdg_popup->base;
        if (xdg_surface->configured) {
            // Send ack to complete popup configuration
            wlr_xdg_surface_schedule_configure(xdg_surface);
        }
    }
    
    // Schedule frames on all outputs to ensure popup is displayed
    struct tinywl_server *server = popup->server;
    if (server) {
        struct tinywl_output *output;
        wl_list_for_each(output, &server->outputs, link) {
            if (output->wlr_output && output->wlr_output->enabled) {
                wlr_output_schedule_frame(output->wlr_output);
            }
        }
    }
    
    wlr_log(WLR_DEBUG, "Popup mapped and configured: %p", popup->xdg_popup);
}

static void popup_unmap(struct wl_listener *listener, void *data) {
    struct tinywl_popup *popup = wl_container_of(listener, popup, unmap);
    
    // Disable the popup in the scene
    wlr_scene_node_set_enabled(&popup->scene_tree->node, false);
    
    // Schedule frames to update display
    struct tinywl_server *server = popup->server;
    if (server) {
        struct tinywl_output *output;
        wl_list_for_each(output, &server->outputs, link) {
            if (output->wlr_output && output->wlr_output->enabled) {
                wlr_output_schedule_frame(output->wlr_output);
            }
        }
    }
    
    wlr_log(WLR_DEBUG, "Popup unmapped: %p", popup->xdg_popup);
}

static void popup_destroy(struct wl_listener *listener, void *data) {
    struct tinywl_popup *popup = wl_container_of(listener, popup, destroy);
    
    wlr_log(WLR_DEBUG, "Popup destroyed: %p", popup->xdg_popup);
    
    // Clean up listeners
    wl_list_remove(&popup->map.link);
    wl_list_remove(&popup->unmap.link);
    wl_list_remove(&popup->destroy.link);
    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->link);
    
    // Destroy scene node
    if (popup->scene_tree) {
        wlr_scene_node_destroy(&popup->scene_tree->node);
    }
    
    // Schedule frames to update display after popup destruction
    struct tinywl_server *server = popup->server;
    if (server) {
        struct tinywl_output *output;
        wl_list_for_each(output, &server->outputs, link) {
            if (output->wlr_output && output->wlr_output->enabled) {
                wlr_output_schedule_frame(output->wlr_output);
            }
        }
    }
    
    free(popup);
}




static void popup_commit(struct wl_listener *listener, void *data) {
    struct tinywl_popup *popup = wl_container_of(listener, popup, commit);
    
    if (!popup->xdg_popup || !popup->xdg_popup->base) {
        return;
    }

    struct wlr_xdg_surface *xdg_surface = popup->xdg_popup->base;
    struct wlr_surface *surface = xdg_surface->surface;
    
    wlr_log(WLR_DEBUG, "Popup commit: %p (initial: %d, configured: %d, mapped: %d)", 
            popup->xdg_popup, xdg_surface->initial_commit, xdg_surface->configured, surface ? surface->mapped : 0);
    
    // Handle configuration properly with immediate response for GTK
    if (xdg_surface->initial_commit) {
        // For initial commit, configure immediately
        wlr_xdg_surface_schedule_configure(xdg_surface);
        wlr_log(WLR_DEBUG, "Scheduled initial configure for popup: %p", popup->xdg_popup);
    } else if (!xdg_surface->configured) {
        // Configure unconfigured popups immediately
        wlr_xdg_surface_schedule_configure(xdg_surface);
        wlr_log(WLR_DEBUG, "Scheduled configure for unconfigured popup: %p", popup->xdg_popup);
    }
    
    // CRITICAL: Always send frame_done for popup commits to prevent GTK freezing
    if (surface && surface->current.buffer) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        wlr_surface_send_frame_done(surface, &now);
    }
    
    // Schedule frames on all outputs
    struct tinywl_server *server = popup->server;
    if (server) {
        struct tinywl_output *output;
        wl_list_for_each(output, &server->outputs, link) {
            if (output->wlr_output && output->wlr_output->enabled) {
                wlr_output_schedule_frame(output->wlr_output);
            }
        }
    }
    
    // If this is a commit after configure, the popup should be ready
    if (xdg_surface->configured && surface && surface->mapped) {
        wlr_log(WLR_DEBUG, "Popup ready and mapped: %p", popup->xdg_popup);
    }
}








float get_monotonic_time_seconds_as_float(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        wlr_log_errno(WLR_ERROR, "clock_gettime failed");
        return 0.0f;
    }
    return (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f;
}









void desktop_background(struct tinywl_server *server) {
    wlr_log(WLR_INFO, "=== DESKTOP BACKGROUND (MELT SHADER) ===");
    float bg_color[4] = {1.0f, 0.0f, 0.0f, 1.0f}; // Base color for the Melt shader
    struct wlr_scene_rect *bg_rect = wlr_scene_rect_create(&server->scene->tree, 1024, 768, bg_color);
    if (bg_rect) {
        server->main_background_node = &bg_rect->node; // Store the pointer
        wlr_scene_node_set_position(&bg_rect->node, 0, 0);
        wlr_scene_node_set_enabled(&bg_rect->node, true);
        wlr_log(WLR_INFO, "Main background rect created at (0,0), node: %p", (void*)server->main_background_node);
    } else {
        server->main_background_node = NULL;
        wlr_log(WLR_ERROR, "Failed to create main background rect.");
    }
}


void desktop_panel(struct tinywl_server *server) {
    int panel_height=60;
    wlr_log(WLR_INFO, "=== DESKTOP PANEL TEST ===");
    float panel_tint_color[4] = {0.2f, 0.8f, 0.3f, 0.9f}; // Give it a distinct tint
    struct wlr_scene_rect *panel_srect = wlr_scene_rect_create(
        &server->scene->tree,
        1024, // Assuming output width
        panel_height,   // Panel height
        panel_tint_color
    );
    if (panel_srect) {
        server->top_panel_node = &panel_srect->node; // CRITICAL: Store the node
        wlr_scene_node_set_position(server->top_panel_node, 0, 768-panel_height); // Position at top
        wlr_scene_node_set_enabled(server->top_panel_node, true);
        wlr_scene_node_raise_to_top(server->top_panel_node); // Ensure it's on top
        wlr_log(WLR_INFO, "Desktop panel rect created and assigned to server->top_panel_node: %p at (0,0)", server->top_panel_node);
    } else {
        wlr_log(WLR_ERROR, "Failed to create desktop_panel rect.");
        server->top_panel_node = NULL; // Ensure it's NULL if creation fails
    }
}


// ISSUE 5: Debug logging to verify scene tree state
void debug_scene_tree(struct wlr_scene *scene, struct wlr_output *output) {
    wlr_log(WLR_INFO, "[DEBUG] Scene tree state for output %s:", output->name);
    
    struct wlr_scene_node *node;
    int node_count = 0;
    
    wl_list_for_each(node, &scene->tree.children, link) {
        node_count++;
        const char *type_name = "UNKNOWN";
        
        // Use correct enum values for wlroots 0.19
        switch (node->type) {
            case WLR_SCENE_NODE_TREE:
                type_name = "TREE";
                break;
            case WLR_SCENE_NODE_RECT:
                type_name = "RECT";
                break;
            case WLR_SCENE_NODE_BUFFER:
                type_name = "BUFFER";
                break;
            default:
                type_name = "UNKNOWN";
                break;
        }
        
        struct tinywl_toplevel *tl = node->data;
        const char *custom_type = "NONE";
        if (tl) {
            custom_type = (tl->type == TINYWL_TOPLEVEL_CUSTOM) ? "CUSTOM" : "XDG";
        }
        
        wlr_log(WLR_INFO, "[DEBUG] Node %d: type=%s, enabled=%d, custom_type=%s, pos=(%d,%d)",
                node_count, type_name, node->enabled, custom_type, node->x, node->y);
        
        // If it's a rect node, show color info
        if (node->type == WLR_SCENE_NODE_RECT) {
            struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
            wlr_log(WLR_INFO, "[DEBUG] Rect: %dx%d, color=(%.2f,%.2f,%.2f,%.2f)",
                    rect->width, rect->height, 
                    rect->color[0], rect->color[1], rect->color[2], rect->color[3]);
        }
    }
    
    wlr_log(WLR_INFO, "[DEBUG] Total nodes: %d", node_count);
}


// Texture cache entry
struct texture_cache_entry {
    struct tinywl_toplevel *toplevel;
    struct wlr_texture *texture;
};

// Texture cache (static array for simplicity)
#define MAX_CACHED_TEXTURES 16
static struct texture_cache_entry texture_cache[MAX_CACHED_TEXTURES] = {0};

static void update_texture_cache(struct tinywl_toplevel *toplevel, struct wlr_texture *texture) {
    // Find existing entry or first free slot
    int free_slot = -1;
    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        if (texture_cache[i].toplevel == toplevel) {
            // Update existing entry
            if (texture_cache[i].texture) {
                wlr_texture_destroy(texture_cache[i].texture);
            }
            texture_cache[i].texture = texture;
            return;
        }
        if (!texture_cache[i].toplevel && free_slot == -1) {
            free_slot = i;
        }
    }
    
    // Use free slot if available
    if (free_slot != -1 && texture) {
        texture_cache[free_slot].toplevel = toplevel;
        texture_cache[free_slot].texture = texture;
    }
}

static struct wlr_texture *get_cached_texture(struct tinywl_toplevel *toplevel) {
    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        if (texture_cache[i].toplevel == toplevel) {
            return texture_cache[i].texture;
        }
    }
    return NULL;
}

 // Function to load texture from file (you'll need to implement or include this)
GLuint load_texture_from_file(const char* filename) {
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // You'll need to implement image loading here using a library like stb_image
    // This is a placeholder - replace with actual image loading code
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);
    
    if (data) {
        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        wlr_log(WLR_INFO, "Loaded texture: %s (%dx%d, %d channels)", filename, width, height, channels);
    } else {
        wlr_log(WLR_ERROR, "Failed to load texture: %s", filename);
        glDeleteTextures(1, &texture_id);
        return 0;
    }
    
    return texture_id;
}

static void scene_buffer_iterator(struct wlr_scene_buffer *scene_buffer,
                                 int sx, int sy,
                                 void *user_data) {
    struct render_data *rdata = user_data;
    struct wlr_renderer *renderer = rdata->renderer;
    struct wlr_output *output = rdata->output;
    struct tinywl_server *server = rdata->server;
    struct wlr_gles2_texture_attribs tex_attribs;
    const char *output_name_log = output ? output->name : "UNKNOWN_OUTPUT";

    // Basic validation
    if (!rdata || !server || !renderer || !output) {
        wlr_log(WLR_ERROR, "[SCENE_INV:%s] Fatal: Invalid render_data, server, renderer, or output.", output_name_log);
        return;
    }
    if (!scene_buffer) {
        wlr_log(WLR_ERROR, "[SCENE_INV:%s] Fatal: scene_buffer is NULL.", output_name_log);
        return;
    }
    if (!scene_buffer->node.enabled) {
        wlr_log(WLR_DEBUG, "[SCENE_INV:%s] scene_buffer %p node not enabled, skipping.", output_name_log, (void*)scene_buffer);
        return;
    }
    if (output->width == 0 || output->height == 0) {
        wlr_log(WLR_ERROR, "[SCENE_INV:%s] Output has zero width/height.", output_name_log);
        return;
    }
    if (!scene_buffer->buffer) {
        wlr_log(WLR_DEBUG, "[SCENE_INV:%s] scene_buffer %p has NULL wlr_buffer.", output_name_log, (void*)scene_buffer);
        return;
    }

    // Toplevel identification
    struct tinywl_toplevel *toplevel = NULL;
    struct wlr_scene_node *node_iter = &scene_buffer->node;
    int depth = 0;
    const int MAX_DEPTH = 10;
    
    while (node_iter && depth < MAX_DEPTH) {
        if (node_iter->data) {
            struct tinywl_toplevel *temp_ptl = node_iter->data;
            if (temp_ptl && temp_ptl->server == server && temp_ptl->scene_tree == (struct wlr_scene_tree*)node_iter) {
                toplevel = temp_ptl;
                break;
            }
        }
        if (!node_iter->parent) break;
        node_iter = &node_iter->parent->node;
        depth++;
    }

    // Projection matrix
    float projection[9];
    enum wl_output_transform transform = wlr_output_transform_invert(output->transform);
    wlr_matrix_identity(projection);
    wlr_matrix_scale(projection, 2.0f / output->width, 2.0f / output->height);
    wlr_matrix_translate(projection, -1.0f, -1.0f);
    float transform_matrix[9];
    wlr_matrix_identity(transform_matrix);
    wlr_matrix_transform(transform_matrix, transform);
    wlr_matrix_multiply(projection, transform_matrix, projection);

    static int active_animation_count = 0;
    wlr_log(WLR_DEBUG, "[DEBUG] Animation check for toplevel %p", (void*)toplevel);

    // Genie animation path
    if (toplevel && (toplevel->is_minimizing || toplevel->is_maximizing)) {
        const int MAX_ANIMATIONS = 5;
        if (active_animation_count >= MAX_ANIMATIONS) {
            wlr_log(WLR_ERROR, "[ANIM] Too many active animations (%d), skipping", active_animation_count);
            return;
        }
        
        active_animation_count++;
        
        // Create texture from surface or fallback to scene buffer or cache
        struct wlr_texture *texture = NULL;
        if (toplevel->xdg_toplevel && toplevel->xdg_toplevel->base && toplevel->xdg_toplevel->base->surface) {
            struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
            if (surface->buffer) {
                texture = wlr_texture_from_buffer(renderer, &surface->buffer->base);
            }
        }
        if (!texture) {
            texture = wlr_texture_from_buffer(renderer, scene_buffer->buffer);
        }
        if (!texture) {
            texture = get_cached_texture(toplevel);
            if (texture) {
                wlr_log(WLR_DEBUG, "[GENIE_ANIM:%s] Using cached texture for toplevel %p", output_name_log, (void*)toplevel);
            }
        }
        
        if (!texture) {
            wlr_log(WLR_ERROR, "[GENIE_ANIM:%s] Failed to create or find cached texture for genie animation.", output_name_log);
            active_animation_count--;
            return;
        }

        // Update cache with new texture if created
        if (texture != get_cached_texture(toplevel)) {
            update_texture_cache(toplevel, texture);
        }

        wlr_gles2_texture_get_attribs(texture, &tex_attribs);
        
        if (!tex_attribs.tex) {
            wlr_log(WLR_ERROR, "[GENIE_ANIM:%s] Invalid texture attributes.", output_name_log);
            if (texture != get_cached_texture(toplevel)) {
                wlr_texture_destroy(texture);
            }
            active_animation_count--;
            return;
        }

        // Animation progress
        float elapsed, progress;
        struct wlr_box start_box = {0}, target_box = {0};
        bool is_minimize_anim = toplevel->is_minimizing;
        
        if (is_minimize_anim) {
            elapsed = get_monotonic_time_seconds_as_float() - toplevel->minimize_start_time;
            progress = (toplevel->minimize_duration > 1e-5f)
                     ? fminf(1.0f, elapsed / toplevel->minimize_duration)
                     : 1.5f;
            start_box = toplevel->minimize_start_geom;
            target_box = toplevel->minimize_target_geom;
        } else {
            elapsed = get_monotonic_time_seconds_as_float() - toplevel->maximize_start_time;
            progress = (toplevel->maximize_duration > 1e-5f)
                     ? fminf(1.0f, elapsed / toplevel->maximize_duration)
                     : 1.5f;
            start_box = toplevel->maximize_start_geom;
            target_box = toplevel->maximize_target_geom;
        }

        // Validate geometry
        if (start_box.width <= 0 || start_box.height <= 0 || 
            target_box.width <= 0 || target_box.height <= 0) {
            wlr_log(WLR_ERROR, "[GENIE_ANIM] Invalid geometry: start %dx%d, target %dx%d", 
                    start_box.width, start_box.height, target_box.width, target_box.height);
            if (texture != get_cached_texture(toplevel)) {
                wlr_texture_destroy(texture);
            }
            active_animation_count--;
            return;
        }

        // Check animation completion
         // --- MODIFIED SECTION: Check animation completion ---
        if (progress >= 1.0f) {
            if (is_minimize_anim) {
                toplevel->is_minimizing = false;
                toplevel->server->animating_toplevel = NULL;
                if (toplevel->minimizing_to_dock) {
                    toplevel->minimized = true;
                    wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
                    // Decorations remain hidden since window is not visible
                } else {
                    // Restoring FROM dock, so make decorations visible again.
                    set_decorations_visible(toplevel, true);
                }
            } else { // is_maximizing
                toplevel->is_maximizing = false;
                toplevel->server->animating_toplevel = NULL;
                toplevel->maximized = toplevel->xdg_toplevel->current.maximized; // Use the final state
                // Animation is finished, make decorations visible again.
                set_decorations_visible(toplevel, true);
            }
            
            if (texture != get_cached_texture(toplevel)) {
                wlr_texture_destroy(texture);
            }
            active_animation_count--;
            // keep this commented out to render the final state
            // return; 
        }

        // Schedule next frame
        wlr_output_schedule_frame(output);

        // Interpolated render box
        struct wlr_box render_box = {0};
        render_box.x = (int)round((double)start_box.x * (1.0 - progress) + (double)target_box.x * progress);
        render_box.y = (int)round((double)start_box.y * (1.0 - progress) + (double)target_box.y * progress);
        render_box.width = (int)round((double)start_box.width * (1.0 - progress) + (double)target_box.width * progress);
        render_box.height = (int)round((double)start_box.height * (1.0 - progress) + (double)target_box.height * progress);
        
        if (render_box.width < 1) render_box.width = 1;
        if (render_box.height < 1) render_box.height = 1;

        // Account for decoration size
        int decoration_left = BORDER_WIDTH;
        int decoration_right = BORDER_WIDTH;
        int decoration_top = TITLE_BAR_HEIGHT;
        int decoration_bottom = BORDER_WIDTH;
        
        struct wlr_box content_area = {
            .x = render_box.x + decoration_left,
            .y = render_box.y + decoration_top,
            .width = fmaxf(1, render_box.width - decoration_left - decoration_right),
            .height = fmaxf(1, render_box.height - decoration_top - decoration_bottom)
        };

        // Render genie animation
        if (render_box.width > 0 && render_box.height > 0 && 
            render_box.width <= 8192 && render_box.height <= 8192) {
            
            GLint original_fbo = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &original_fbo);
            
            // Pass 1: Render passthrough shader to intermediate texture
            GLuint intermediate_fbo, intermediate_texture;
            glGenTextures(1, &intermediate_texture);
            glBindTexture(GL_TEXTURE_2D, intermediate_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, render_box.width, render_box.height, 
                        0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            glGenFramebuffers(1, &intermediate_fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, intermediate_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, intermediate_texture, 0);
            
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                wlr_log(WLR_ERROR, "[GENIE_ANIM:%s] Framebuffer not complete: %d", 
                        output_name_log, glCheckFramebufferStatus(GL_FRAMEBUFFER));
                glBindFramebuffer(GL_FRAMEBUFFER, original_fbo);
                glDeleteTextures(1, &intermediate_texture);
                glDeleteFramebuffers(1, &intermediate_fbo);
                if (texture != get_cached_texture(toplevel)) {
                    wlr_texture_destroy(texture);
                }
                active_animation_count--;
                return;
            }
            
            glViewport(0, 0, render_box.width, render_box.height);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            glUseProgram(server->passthrough_shader_program);
            glBindVertexArray(server->quad_vao);
            
            // Passthrough MVP matrix
            float passthrough_mvp[9] = {
                2.0f, 0.0f, 0.0f,
                0.0f, -2.0f, 0.0f,
                -1.0f, 1.0f, 1.0f
            };
            
            if (output->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
                float temp_mvp[9];
                memcpy(temp_mvp, passthrough_mvp, sizeof(passthrough_mvp));
                float transform_matrix[9];
                wlr_matrix_transform(transform_matrix, output->transform);
                wlr_matrix_multiply(passthrough_mvp, transform_matrix, temp_mvp);
            }
            if (scene_buffer->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
                float temp_mvp_buffer[9];
                memcpy(temp_mvp_buffer, passthrough_mvp, sizeof(passthrough_mvp));
                float buffer_matrix[9];
                wlr_matrix_transform(buffer_matrix, scene_buffer->transform);
                wlr_matrix_multiply(passthrough_mvp, temp_mvp_buffer, buffer_matrix);
            }
            
            GLint mvp_loc_pass = glGetUniformLocation(server->passthrough_shader_program, "mvp");
            if (mvp_loc_pass != -1) {
                glUniformMatrix3fv(mvp_loc_pass, 1, GL_FALSE, passthrough_mvp);
            }
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(tex_attribs.target, tex_attribs.tex);
            glTexParameteri(tex_attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(tex_attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(tex_attribs.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(tex_attribs.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            GLint tex_loc_pass = glGetUniformLocation(server->passthrough_shader_program, "u_texture");
            if (tex_loc_pass != -1) {
                glUniform1i(tex_loc_pass, 0);
            }
            
            GLint time_loc_pass = glGetUniformLocation(server->passthrough_shader_program, "time");
            if (time_loc_pass != -1) {
                glUniform1f(time_loc_pass, get_monotonic_time_seconds_as_float());
            }
            
            GLint res_loc_pass = glGetUniformLocation(server->passthrough_shader_program, "iResolution");
            if (res_loc_pass != -1) {
                float resolution[2] = {(float)render_box.width, (float)render_box.height};
                glUniform2fv(res_loc_pass, 1, resolution);
            }
            
            if (server->passthrough_shader_res_loc != -1) {
                glUniform2f(server->passthrough_shader_res_loc, (float)render_box.width, (float)render_box.height);
            }
            
            if (server->passthrough_shader_cornerRadius_loc != -1) {
                glUniform1f(server->passthrough_shader_cornerRadius_loc, 12.0f);
            }
            
            // Cycling bevel color
            static const float color_palette[][4] = {
                {1.0f, 0.2f, 0.2f, 0.7f}, // Red
                {1.0f, 1.0f, 0.2f, 0.7f}, // Yellow
                {0.2f, 1.0f, 0.2f, 0.7f}, // Green
                {0.2f, 1.0f, 1.0f, 0.7f}, // Cyan
                {0.2f, 0.2f, 1.0f, 0.7f}, // Blue
                {1.0f, 0.2f, 1.0f, 0.7f}  // Magenta
            };
            const int num_colors = sizeof(color_palette) / sizeof(color_palette[0]);
            
            float time_sec = get_monotonic_time_seconds_as_float();
            float time_per_color = 2.0f;
            float total_cycle_duration = (float)num_colors * time_per_color;
            float time_in_cycle = fmod(time_sec, total_cycle_duration);
            int current_color_idx = (int)floor(time_in_cycle / time_per_color);
            int next_color_idx = (current_color_idx + 1) % num_colors;
            float time_in_transition = fmod(time_in_cycle, time_per_color);
            float mix_factor = time_in_transition / time_per_color;
            float eased_mix_factor = mix_factor * mix_factor * (3.0f - 2.0f * mix_factor);
            
            float final_bevel_color[4];
            for (int i = 0; i < 4; ++i) {
                final_bevel_color[i] = 
                    color_palette[current_color_idx][i] * (1.0f - eased_mix_factor) +
                    color_palette[next_color_idx][i] * eased_mix_factor;
            }
            
            if (server->passthrough_shader_bevelColor_loc != -1) {
                glUniform4fv(server->passthrough_shader_bevelColor_loc, 1, final_bevel_color);
            }
            
            if (server->passthrough_shader_time_loc != -1) {
                glUniform1f(server->passthrough_shader_time_loc, get_monotonic_time_seconds_as_float());
            }
            
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            
            // Pass 2: Render genie shader with tessellated mesh
            glBindFramebuffer(GL_FRAMEBUFFER, original_fbo);
            glViewport(0, 0, output->width, output->height);
            
            glUseProgram(server->genie_shader_program);
            if (!server->genie_vao) {
                wlr_log(WLR_ERROR, "[GENIE_ANIM:%s] Genie VAO not initialized.", output_name_log);
                glDeleteTextures(1, &intermediate_texture);
                glDeleteFramebuffers(1, &intermediate_fbo);
                if (texture != get_cached_texture(toplevel)) {
                    wlr_texture_destroy(texture);
                }
                active_animation_count--;
                return;
            }
            glBindVertexArray(server->genie_vao);

            // MVP matrix for genie shader
            float genie_mvp[9];
            float box_scale_x = (float)content_area.width * (2.0f / output->width);
            float box_scale_y = (float)content_area.height * (-2.0f / output->height);  
            float box_translate_x = ((float)content_area.x / output->width) * 2.0f - 1.0f;
            float box_translate_y = ((float)content_area.y / output->height) * -2.0f + 1.0f;
            
            float model_view[9] = {
                box_scale_x, 0.0f, 0.0f,
                0.0f, box_scale_y, 0.0f,
                box_translate_x, box_translate_y, 1.0f
            };
            memcpy(genie_mvp, model_view, sizeof(model_view));

            if (output->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
                float temp_mvp[9];
                memcpy(temp_mvp, genie_mvp, sizeof(genie_mvp));
                float transform_matrix[9];
                wlr_matrix_transform(transform_matrix, output->transform);
                wlr_matrix_multiply(genie_mvp, transform_matrix, temp_mvp);
            }
            
            if (scene_buffer->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
                float temp_mvp_buffer[9];
                memcpy(temp_mvp_buffer, genie_mvp, sizeof(genie_mvp));
                float buffer_matrix[9];
                wlr_matrix_transform(buffer_matrix, scene_buffer->transform);
                wlr_matrix_multiply(genie_mvp, temp_mvp_buffer, buffer_matrix);
            }
            
            if (server->genie_shader_mvp_loc != -1) {
                glUniformMatrix3fv(server->genie_shader_mvp_loc, 1, GL_FALSE, genie_mvp);
            }

            // Bind intermediate texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, intermediate_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            if (server->genie_shader_tex_loc != -1) {
                glUniform1i(server->genie_shader_tex_loc, 0);
            }

            // Texture coordinate scaling/offset for decorations
            GLint tex_scale_loc = glGetUniformLocation(server->genie_shader_program, "texScale");
            GLint tex_offset_loc = glGetUniformLocation(server->genie_shader_program, "texOffset");
            if (tex_scale_loc != -1) {
                float tex_scale[2] = {
                    (float)content_area.width / (float)render_box.width,
                    (float)content_area.height / (float)render_box.height
                };
                glUniform2fv(tex_scale_loc, 1, tex_scale);
            }
            if (tex_offset_loc != -1) {
                float tex_offset[2] = {
                    (float)decoration_left / (float)render_box.width,
                    (float)decoration_top / (float)render_box.height
                };
                glUniform2fv(tex_offset_loc, 1, tex_offset);
            }

            // Animation uniforms
            float anim_progress = is_minimize_anim ? (toplevel->minimizing_to_dock ? progress : 1.0f - progress) : (1.0f - progress);
            if (server->genie_shader_progress_loc != -1) {
                glUniform1f(server->genie_shader_progress_loc, anim_progress);
            }
            if (server->genie_shader_bend_factor_loc != -1) {
                glUniform1f(server->genie_shader_bend_factor_loc, 0.3f);
            }

            GLint time_loc = glGetUniformLocation(server->genie_shader_program, "time");
            if (time_loc != -1) {
                glUniform1f(time_loc, time_sec);
            }

            GLint resolution_loc = glGetUniformLocation(server->genie_shader_program, "iResolution");
            if (resolution_loc != -1) {
                float resolution[2] = {(float)content_area.width, (float)content_area.height};
                glUniform2fv(resolution_loc, 1, resolution);
            }

            float alpha = is_minimize_anim ? (1.0f - progress * 0.3f) : (0.7f + 0.3f * progress);
            GLint alpha_loc = glGetUniformLocation(server->genie_shader_program, "alpha");
            if (alpha_loc != -1) {
                glUniform1f(alpha_loc, alpha);
            }

            // Draw
            glDrawElements(GL_TRIANGLES, server->genie_index_count, GL_UNSIGNED_INT, 0);
            
            wlr_log(WLR_DEBUG, "[GENIE_ANIM:%s] Rendered genie effect, progress: %.3f, alpha: %.3f", 
                    output_name_log, anim_progress, alpha);

            // Clean up
            glBindVertexArray(0);
            glUseProgram(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, original_fbo);
            glDeleteTextures(1, &intermediate_texture);
            glDeleteFramebuffers(1, &intermediate_fbo);
            glDisable(GL_BLEND);
        }
        
        if (texture != get_cached_texture(toplevel)) {
            wlr_texture_destroy(texture);
        }
        active_animation_count--;
        return;
    }

    // Normal rendering
    wlr_log(WLR_DEBUG, "[NORMAL] Normal rendering for scene_buffer %p", (void*)scene_buffer);
    
    struct wlr_texture *texture = wlr_texture_from_buffer(renderer, scene_buffer->buffer);
    if (!texture && toplevel) {
        texture = get_cached_texture(toplevel);
        if (texture) {
            wlr_log(WLR_DEBUG, "[NORMAL:%s] Using cached texture for toplevel %p", output_name_log, (void*)toplevel);
        }
    }
    if (!texture) {
        wlr_log(WLR_ERROR, "[SCENE_INV:%s] Failed to create or find cached wlr_texture from buffer %p.", output_name_log, (void*)scene_buffer->buffer);
        return;
    }

    // Update cache with new texture if created
    if (toplevel && texture != get_cached_texture(toplevel)) {
        update_texture_cache(toplevel, texture);
    }

    struct wlr_scene_surface *current_scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
    struct wlr_surface *surface_to_render = current_scene_surface ? current_scene_surface->surface : NULL;
    if (!surface_to_render) {
        if (texture != get_cached_texture(toplevel)) {
            wlr_texture_destroy(texture);
        }
        return;
    }

    wlr_gles2_texture_get_attribs(texture, &tex_attribs);

    char title_buffer[128] = "NO_TL_ITER";
    const char* tl_title_for_log = "UNKNOWN_OR_NOT_TOPLEVEL_CONTENT";

    if (toplevel) {
        if (toplevel->xdg_toplevel && toplevel->xdg_toplevel->title) {
            snprintf(title_buffer, sizeof(title_buffer), "%s", toplevel->xdg_toplevel->title);
            tl_title_for_log = title_buffer;
        } else {
            snprintf(title_buffer, sizeof(title_buffer), "Ptr:%p (XDG maybe NULL)", toplevel);
            tl_title_for_log = "TOPLEVEL_NO_TITLE";
        }

        wlr_log(WLR_DEBUG, "[ITERATOR:%s] Processing TL ('%s' Ptr:%p). Anim:%d, Scale:%.3f, Target:%.3f, StartT:%.3f, PendingD:%d, NodeEn:%d",
               output_name_log, title_buffer, toplevel,
               toplevel->is_animating, toplevel->scale, toplevel->target_scale,
               toplevel->animation_start, toplevel->pending_destroy,
               (toplevel->scene_tree ? toplevel->scene_tree->node.enabled : -1));
    }

    // Scaling animation
    float anim_scale_factor = 1.0f;
    if (toplevel && toplevel->is_animating) {
        float elapsed = get_monotonic_time_seconds_as_float() - toplevel->animation_start;
        if (elapsed < 0.0f) elapsed = 0.0f;
        
        float t = 0.0f;
        if (toplevel->animation_duration > 1e-5f) {
            t = elapsed / toplevel->animation_duration;
        } else if (elapsed > 0) {
            t = 1.0f;
        }

        if (t >= 1.0f) {
            t = 1.0f;
            anim_scale_factor = toplevel->target_scale;
            if (toplevel->target_scale == 0.0f && !toplevel->pending_destroy) {
                toplevel->scale = 0.0f;
                wlr_log(WLR_DEBUG, "[ITERATOR:%s] '%s' close anim reached t>=1. Scale=0.0.", output_name_log, title_buffer);
                wlr_output_schedule_frame(output);
            } else {
                toplevel->is_animating = false;
                toplevel->scale = toplevel->target_scale;
            }
        } else {
            anim_scale_factor = toplevel->scale + (toplevel->target_scale - toplevel->scale) * t;
        }

        if (toplevel->is_animating) {
            wlr_output_schedule_frame(output);
        }
        wlr_log(WLR_DEBUG, "[ITERATOR:%s] '%s': t=%.3f, anim_scale_factor=%.3f",
               output_name_log, title_buffer, t, anim_scale_factor);
    } else if (toplevel) {
        anim_scale_factor = toplevel->scale;
    }

    if (anim_scale_factor < 0.001f && !(toplevel && toplevel->target_scale == 0.0f && !toplevel->pending_destroy)) {
        anim_scale_factor = 0.001f;
    }

    // Main window rendering
    struct wlr_box main_render_box = {
        .x = (int)round((double)sx + (double)texture->width * (1.0 - (double)anim_scale_factor) / 2.0),
        .y = (int)round((double)sy + (double)texture->height * (1.0 - (double)anim_scale_factor) / 2.0),
        .width = (int)round((double)texture->width * (double)anim_scale_factor),
        .height = (int)round((double)texture->height * (double)anim_scale_factor)
    };

    if (main_render_box.width <= 0 || main_render_box.height <= 0) {
        if (toplevel && !toplevel->pending_destroy && toplevel->target_scale == 0.0f) {
            wlr_log(WLR_DEBUG, "[DEBUG:%s] '%s' scaled to zero width/height.", output_name_log, title_buffer);
            wlr_output_schedule_frame(output);
        }
        if (texture != get_cached_texture(toplevel)) {
            wlr_texture_destroy(texture);
        }
        return;
    }

    // Calculate MVP matrix
    float main_mvp[9];
    wlr_matrix_identity(main_mvp);
    float box_scale_x = (float)main_render_box.width * (2.0f / output->width);
    float box_scale_y = (float)main_render_box.height * (-2.0f / output->height);
    float box_translate_x = ((float)main_render_box.x / output->width) * 2.0f - 1.0f;
    float box_translate_y = ((float)main_render_box.y / output->height) * -2.0f + 1.0f;

    if (output->transform == WL_OUTPUT_TRANSFORM_FLIPPED_180) {
        main_mvp[0] = -box_scale_x; main_mvp[4] = -box_scale_y; main_mvp[6] = -box_translate_x; main_mvp[7] = -box_translate_y;
        main_mvp[1] = main_mvp[2] = main_mvp[3] = main_mvp[5] = 0.0f; main_mvp[8] = 1.0f;
    } else if (output->transform == WL_OUTPUT_TRANSFORM_NORMAL) {
        main_mvp[0] = box_scale_x; main_mvp[4] = box_scale_y; main_mvp[6] = box_translate_x; main_mvp[7] = box_translate_y;
        main_mvp[1] = main_mvp[2] = main_mvp[3] = main_mvp[5] = 0.0f; main_mvp[8] = 1.0f;
    } else {
        float temp_mvp[9] = {box_scale_x, 0.0f, 0.0f, 0.0f, box_scale_y, 0.0f, box_translate_x, box_translate_y, 1.0f};
        float transform_matrix[9];
        wlr_matrix_transform(transform_matrix, output->transform);
        wlr_matrix_multiply(main_mvp, transform_matrix, temp_mvp);
    }

    if (scene_buffer->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        float temp_mvp_buffer[9];
        memcpy(temp_mvp_buffer, main_mvp, sizeof(main_mvp));
        float buffer_matrix[9];
        wlr_matrix_transform(buffer_matrix, scene_buffer->transform);
        wlr_matrix_multiply(main_mvp, temp_mvp_buffer, buffer_matrix);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(server->passthrough_shader_program);
    
    GLint mvp_loc = glGetUniformLocation(server->passthrough_shader_program, "mvp");
    if (mvp_loc != -1) {
        glUniformMatrix3fv(mvp_loc, 1, GL_FALSE, main_mvp);
    }

    GLint tex_loc = glGetUniformLocation(server->passthrough_shader_program, "u_texture");
    if (tex_loc != -1) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(tex_attribs.target, tex_attribs.tex);
        glTexParameteri(tex_attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(tex_attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(tex_attribs.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(tex_attribs.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glUniform1i(tex_loc, 0);
    }

    // Set time uniform
    struct timespec now_shader_time_iter_draw;
    clock_gettime(CLOCK_MONOTONIC, &now_shader_time_iter_draw);
    float time_value_iter_draw = now_shader_time_iter_draw.tv_sec + now_shader_time_iter_draw.tv_nsec / 1e9f;
    GLint time_loc_iter_draw = glGetUniformLocation(server->passthrough_shader_program, "time");
    if (time_loc_iter_draw != -1) glUniform1f(time_loc_iter_draw, time_value_iter_draw);

    // Set resolution uniform
    float resolution_iter_draw[2] = {(float)output->width, (float)output->height};
    GLint resolution_loc_iter_draw = glGetUniformLocation(server->passthrough_shader_program, "iResolution");
    if (resolution_loc_iter_draw != -1) glUniform2fv(resolution_loc_iter_draw, 1, resolution_iter_draw);

    // Set new uniforms
    if (server->passthrough_shader_res_loc != -1) {
        glUniform2f(server->passthrough_shader_res_loc, (float)main_render_box.width, (float)main_render_box.height);
    }

    if (server->passthrough_shader_cornerRadius_loc != -1) {
        glUniform1f(server->passthrough_shader_cornerRadius_loc, 12.0f);
    }

    // Cycling bevel color
    static const float color_palette[][4] = {
        {1.0f, 0.2f, 0.2f, 0.7f}, // Red
        {1.0f, 1.0f, 0.2f, 0.7f}, // Yellow
        {0.2f, 1.0f, 0.2f, 0.7f}, // Green
        {0.2f, 1.0f, 1.0f, 0.7f}, // Cyan
        {0.2f, 0.2f, 1.0f, 0.7f}, // Blue
        {1.0f, 0.2f, 1.0f, 0.7f}  // Magenta
    };
    const int num_colors = sizeof(color_palette) / sizeof(color_palette[0]);
    
    float time_sec = get_monotonic_time_seconds_as_float();
    float time_per_color = 2.0f;
    float total_cycle_duration = (float)num_colors * time_per_color;
    float time_in_cycle = fmod(time_sec, total_cycle_duration);
    int current_color_idx = (int)floor(time_in_cycle / time_per_color);
    int next_color_idx = (current_color_idx + 1) % num_colors;
    float time_in_transition = fmod(time_in_cycle, time_per_color);
    float mix_factor = time_in_transition / time_per_color;
    float eased_mix_factor = mix_factor * mix_factor * (3.0f - 2.0f * mix_factor);

    float final_bevel_color[4];
    for (int i = 0; i < 4; ++i) {
        final_bevel_color[i] = 
            color_palette[current_color_idx][i] * (1.0f - eased_mix_factor) +
            color_palette[next_color_idx][i] * eased_mix_factor;
    }

    if (server->passthrough_shader_bevelColor_loc != -1) {
        glUniform4fv(server->passthrough_shader_bevelColor_loc, 1, final_bevel_color);
    }
    if (server->passthrough_shader_time_loc != -1) {
        glUniform1f(server->passthrough_shader_time_loc, get_monotonic_time_seconds_as_float());
    }

    glBindVertexArray(server->quad_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    wlr_log(WLR_DEBUG, "[DEBUG:%s] Drew main window for '%s'", output_name_log, tl_title_for_log);

float p_box_scale_x, p_box_scale_y , p_box_translate_x,  p_box_translate_y;
    // Preview rendering
    if (toplevel && server->top_panel_node && server->top_panel_node->enabled) {
        struct wlr_scene_rect *panel_srect = wlr_scene_rect_from_node(server->top_panel_node);
        if (panel_srect) {
            glUseProgram(server->shader_program);
            glBindVertexArray(server->quad_vao);

            const float PREVIEW_PADDING = 5.0f, PANEL_LEFT_MARGIN = 20.0f, MAX_PREVIEW_WIDTH = 120.0f;
            float available_panel_width = (float)panel_srect->width - PANEL_LEFT_MARGIN * 2.0f;
            float preview_y_padding = PREVIEW_PADDING;
            float preview_height = (float)panel_srect->height - 2.0f * preview_y_padding;
            
            int desktop_window_count = 10;
            struct tinywl_toplevel *iter_tl;
            wl_list_for_each(iter_tl, &server->toplevels, link) {
                if (iter_tl->desktop == rdata->desktop_index) desktop_window_count++;
            }
            if (desktop_window_count == 0) {
                wlr_log(WLR_DEBUG, "[PREVIEW:%s] No windows on desktop %d.", output_name_log, rdata->desktop_index);
                if (texture != get_cached_texture(toplevel)) {
                    wlr_texture_destroy(texture);
                }
                return;
            }
            
            float ideal_preview_width = fminf(MAX_PREVIEW_WIDTH, (available_panel_width - (desktop_window_count - 1) * PREVIEW_PADDING) / desktop_window_count);
            
            int per_desktop_index = 0;
            wl_list_for_each(iter_tl, &server->toplevels, link) {
                if (iter_tl->desktop == rdata->desktop_index) {
                    if (iter_tl == toplevel) break;
                    per_desktop_index++;
                }
            }

            float slot_x_offset = PANEL_LEFT_MARGIN + (per_desktop_index * (ideal_preview_width + PREVIEW_PADDING));
            float aspect_ratio = (texture->width > 0 && texture->height > 0) ? (float)texture->width / (float)texture->height : 1.0f;
            float preview_width = fminf(ideal_preview_width, preview_height * aspect_ratio);
            float preview_height_adjusted = preview_width / aspect_ratio;
            float preview_x_offset = slot_x_offset + (ideal_preview_width - preview_width) / 2.0f;
            float y_offset = preview_y_padding + (preview_height - preview_height_adjusted) / 2.0f;

            struct wlr_box preview_box = {
                .x = (int)round((double)server->top_panel_node->x + preview_x_offset),
                .y = (int)round((double)server->top_panel_node->y + y_offset),
                .width = (int)round(preview_width),
                .height = (int)round(preview_height_adjusted)
            };
            
            toplevel->panel_preview_box = preview_box;

            if (preview_box.width > 0 && preview_box.height > 0) {
                float preview_mvp[9];
                 p_box_scale_x = (float)preview_box.width * (2.0f / output->width);
                 p_box_scale_y = (float)preview_box.height * (-2.0f / output->height);
                 p_box_translate_x = ((float)preview_box.x / output->width) * 2.0f - 1.0f;
                 p_box_translate_y = ((float)preview_box.y / output->height) * -2.0f + 1.0f;
                float  p_model_view[] = {
                    p_box_scale_x, 0.0f, 0.0f,
                    0.0f, p_box_scale_y, 0.0f,
                    p_box_translate_x, p_box_translate_y, 1.0f
                };
                memcpy(preview_mvp, p_model_view, sizeof(preview_mvp));
                
                if (server->flame_shader_mvp_loc != -1) {
                    glUniformMatrix3fv(server->flame_shader_mvp_loc, 1, GL_FALSE, preview_mvp);
                }

                if (tex_attribs.tex) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(tex_attribs.target, tex_attribs.tex);
                    if (server->flame_shader_tex_loc != -1) {
                        glUniform1i(server->flame_shader_tex_loc, 0);
                    }
                }

                if (server->flame_shader_time_loc != -1) {
                    glUniform1f(server->flame_shader_time_loc, time_sec);
                }
                if (server->flame_shader_res_loc != -1) {
                    float preview_res[2] = {(float)preview_box.width, (float)preview_box.height};
                    glUniform2fv(server->flame_shader_res_loc, 1, preview_res);
                }

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                wlr_log(WLR_DEBUG, "[PREVIEW:%s] Drew preview for %s", output_name_log, tl_title_for_log);
            }




// Modified rendering code
{
  static GLuint textures[10] = {0};
    static bool textures_loaded = false;
    
    if (!textures_loaded) {
        textures_loaded = true;
        for (int t = 0; t < 10; t++) {
            char filename[32];
            snprintf(filename, sizeof(filename), "texture%d.png", t + 1);
            textures[t] = load_texture_from_file(filename);
        }
    }

    // 1. **CRITICAL FIX**: Explicitly set the correct shader program for the dock.
    glUseProgram(server->passthrough_shader_program);
    
    // 2. Bind the standard quad VAO.
    glBindVertexArray(server->quad_vao);
    
    // Define placeholder dimensions and layout
    float empty_width = 50;
    float empty_height = 50;
    float empty_x = PANEL_LEFT_MARGIN-dockX_align;
    int num_placeholders =  server->num_dock_icons;;
    float placeholder_spacing = 10;

    // Calculate total content dimensions
    float total_content_height = (num_placeholders * empty_height) + ((num_placeholders - 1) * placeholder_spacing);
    float total_content_width = empty_width; // Single column width
    
    // Calculate dock background dimensions (slightly larger than content)
    float dock_padding = 15; // Padding around the content
    float dock_width = total_content_width + (dock_padding * 2);
    float dock_height = total_content_height + (dock_padding * 2);
    
    // Calculate starting Y position for content (same as texture calculation)
    float start_y = (output->height - total_content_height) / 2.0f;
    
    // Calculate dock background position (centered around the content)
    float dock_x = empty_x - dock_padding;
    float dock_y = start_y - dock_padding;

    // RENDER BACKGROUND PLACEHOLDER (dock-sized empty placeholder)
    {
        float dock_mvp[9];
        float dock_scale_x = (float)dock_width * (2.0f / output->width);
        float dock_scale_y = (float)dock_height * (-2.0f / output->height);
        
        // Fix positioning: use same coordinate system as the placeholders
        float dock_translate_x = ((float)dock_x / output->width) * 2.0f - 1.0f;
        float dock_translate_y = ((float)dock_y / output->height) * -2.0f + 1.0f;
        
        float dock_model_view[] = {
            dock_scale_x, 0.0f, 0.0f,
            0.0f, dock_scale_y, 0.0f,
            dock_translate_x, dock_translate_y, 1.0f
        };
        memcpy(dock_mvp, dock_model_view, sizeof(dock_mvp));
        
               
        
        if (server->flame_shader_mvp_loc != -1) {
            glUniformMatrix3fv(server->passthrough_shader_mvp_loc, 1, GL_FALSE, dock_mvp);
        }

        // Render empty background (no texture)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0); // No texture for background
        
        if (server->passthrough_shader_tex_loc != -1) {
            glUniform1i(server->passthrough_shader_tex_loc, 0);
        }

        if (server->passthrough_shader_time_loc != -1) {
            glUniform1f(server->passthrough_shader_time_loc, time_sec);
        }
        
        if (server->passthrough_shader_res_loc != -1) {
            float dock_res[2] = {dock_width, dock_height};
            glUniform2fv(server->passthrough_shader_res_loc, 1, dock_res);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
       
        wlr_log(WLR_DEBUG, "[PREVIEW:%s] Rendered dock background placeholder (%.1fx%.1f) at (%.1f,%.1f)", 
                output_name_log, dock_width, dock_height, dock_x, dock_y);
    }
  

    glUseProgram(server->passthrough_icons_shader_program);
    // RENDER TEXTURED PLACEHOLDERS (existing code, unchanged)
    // Calculate starting Y position to center vertically (reuse the same calculation)
   for (int i = 0; i < server->num_dock_icons; i++) {


    
            float current_x = empty_x;
            float current_y = start_y + (i * (empty_height + placeholder_spacing));

            // CRITICAL: Update the stored bounding box for this icon for click detection.
            server->dock_icons[i].box.x = (int)current_x;
            server->dock_icons[i].box.y = (int)current_y;
            server->dock_icons[i].box.width = (int)empty_width;
            server->dock_icons[i].box.height = (int)empty_height;

            float preview_mvp[9];
            float p_box_scale_x = empty_width * (2.0f / output->width);
            float p_box_scale_y = empty_height * (-2.0f / output->height);
            float p_box_translate_x = (current_x / output->width) * 2.0f - 1.0f;
            float p_box_translate_y = (current_y / output->height) * -2.0f + 1.0f;

            float p_model_view[] = {
                p_box_scale_x, 0.0f, 0.0f,
                0.0f, p_box_scale_y, 0.0f,
                p_box_translate_x, p_box_translate_y, 1.0f
            };
            memcpy(preview_mvp, p_model_view, sizeof(preview_mvp));

    if (server->passthrough_icons_shader_bevelColor_loc != -1) {
        glUniform4fv(server->passthrough_icons_shader_bevelColor_loc, 1, final_bevel_color);
    }

            if (server->passthrough_icons_shader_mvp_loc != -1) {
                glUniformMatrix3fv(server->passthrough_icons_shader_mvp_loc, 1, GL_FALSE, preview_mvp);
            }

         
            
         // Bind the loaded texture instead of unbinding
        glActiveTexture(GL_TEXTURE0);
        if (textures[i] != 0) {
            glBindTexture(GL_TEXTURE_2D, textures[i]);
            wlr_log(WLR_DEBUG, "[PREVIEW:%s] Using texture%d.png for placeholder %d", output_name_log, i+1, i);
        } else {
            glBindTexture(GL_TEXTURE_2D, 0); // Fallback to no texture if loading failed
            wlr_log(WLR_DEBUG, "[PREVIEW:%s] Texture%d.png not loaded, drawing empty placeholder %d", output_name_log, i+1, i);
        }
        // Tell the 'u_texture' sampler to use texture unit 0.
        glUniform1i(server->passthrough_icons_shader_tex_loc, 0);

        // Set the other uniforms required by this specific shader.
        glUniform2f(server->passthrough_icons_shader_res_loc, empty_width, empty_height);
        glUniform1f(server->passthrough_icons_shader_cornerRadius_loc, 8.0f);
        glUniform1f(server->passthrough_icons_shader_time_loc, get_monotonic_time_seconds_as_float()*(float)(i+3)/(float)(i+2));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    
}
        }
    } else if (toplevel) {
        toplevel->panel_preview_box = (struct wlr_box){0, 0, 0, 0};
    }

    // Final cleanup
    glBindVertexArray(0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex_attribs.target, 0);
    glDisable(GL_BLEND);
    
    if (texture != get_cached_texture(toplevel)) {
        wlr_texture_destroy(texture);
    }
}////////////////////////////////////////////////////////////////////////
static pthread_mutex_t rdp_transmit_mutex = PTHREAD_MUTEX_INITIALIZER;


struct scene_diagnostics_data {
    struct wlr_output *output;
    int buffer_count;
};






// STEP 2: Add this debug function to check scene state during rendering
void debug_scene_rendering(struct wlr_scene *scene, struct wlr_output *output) {
    wlr_log(WLR_INFO, "[RENDER_DEBUG:%s] === SCENE RENDERING DEBUG ===", output->name);
    
    int total_nodes = 0, enabled_nodes = 0, rect_nodes = 0;
    struct wlr_scene_node *node;
    
    wl_list_for_each(node, &scene->tree.children, link) {
        total_nodes++;
        if (node->enabled) enabled_nodes++;
        
        const char *type_str = "UNKNOWN";
        switch (node->type) {
            case WLR_SCENE_NODE_TREE: type_str = "TREE"; break;
            case WLR_SCENE_NODE_RECT: 
                type_str = "RECT"; 
                rect_nodes++;
                // Detailed rect info
                struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
                wlr_log(WLR_INFO, "[RENDER_DEBUG:%s] RECT found: %dx%d at (%d,%d), enabled=%d, color=(%.2f,%.2f,%.2f,%.2f)",
                        output->name, rect->width, rect->height, node->x, node->y, node->enabled,
                        rect->color[0], rect->color[1], rect->color[2], rect->color[3]);
                break;
            case WLR_SCENE_NODE_BUFFER: type_str = "BUFFER"; break;
        }
        
        wlr_log(WLR_INFO, "[RENDER_DEBUG:%s] Node: type=%s, pos=(%d,%d), enabled=%d", 
                output->name, type_str, node->x, node->y, node->enabled);
    }
    
    wlr_log(WLR_INFO, "[RENDER_DEBUG:%s] Summary: total=%d, enabled=%d, rects=%d", 
            output->name, total_nodes, enabled_nodes, rect_nodes);
    wlr_log(WLR_INFO, "[RENDER_DEBUG:%s] === SCENE RENDERING DEBUG END ===", output->name);
}

// STEP 3: Check if you're bypassing scene rendering entirely
// Add this to your output_frame function RIGHT AFTER the scene_output check:
void check_scene_bypass_issue(struct wlr_scene_output *scene_output, struct wlr_output *output) {
    wlr_log(WLR_INFO, "[BYPASS_CHECK:%s] Scene output: %p", output->name, (void*)scene_output);
    
    // Check if you're using wlr_scene_output_commit properly
    // This is CRITICAL - if you're not using this, scene rects won't render
    
    // In your output_frame, you should be doing:
    // bool scene_commit_result = wlr_scene_output_commit(scene_output, NULL);
    // wlr_log(WLR_INFO, "[BYPASS_CHECK:%s] Scene commit result: %d", output->name, scene_commit_result);
    
    wlr_log(WLR_INFO, "[BYPASS_CHECK:%s] If you see this but no rectangles, you might be bypassing scene rendering", output->name);
}

// Forward declaration
static void render_rect_node(struct wlr_scene_node *node, void *user_data);

// Helper function to check if a node is a descendant of a parent tree
static bool is_node_descendant(struct wlr_scene_node *node, struct wlr_scene_tree *parent_tree) {
    if (!node || !parent_tree) return false;
    
    struct wlr_scene_node *current = node;
    while (current && current->parent) {
        if (current->parent == parent_tree) return true;
        current = &current->parent->node;
    }
    return false;
}

// Helper function to find and render all rect nodes in a scene tree
static void find_and_render_all_rects(struct wlr_scene_node *node, void *user_data) {
    if (!node) return;

    // Process current node if it's a rect
    if (node->type == WLR_SCENE_NODE_RECT) {
        render_rect_node(node, user_data);
    }

    // Recursively process all children in tree nodes
    if (node->type == WLR_SCENE_NODE_TREE) {
        struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
        struct wlr_scene_node *child;
        wl_list_for_each(child, &tree->children, link) {
            find_and_render_all_rects(child, user_data);
        }
    }
}

static void render_rect_node(struct wlr_scene_node *node, void *user_data) {
    struct render_data *rdata = user_data;
    struct wlr_output *output = rdata->output;
    struct tinywl_server *server = rdata->server;

    if (!node || node->type != WLR_SCENE_NODE_RECT || !node->enabled) {
        wlr_log(WLR_DEBUG, "Node %p: invalid (type: %d, enabled: %d)",
                (void*)node, node ? node->type : -1, node ? node->enabled : 0);
        return;
    }

    struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
    int render_sx, render_sy;
    if (!wlr_scene_node_coords(node, &render_sx, &render_sy)) {
        wlr_log(WLR_DEBUG, "Node %p: invalid coords.", (void*)node);
        return;
    }

    if (output->width == 0 || output->height == 0) {
        wlr_log(WLR_DEBUG, "Node %p: invalid output size (%dx%d).",
                (void*)node, output->width, output->height);
        return;
    }

    GLuint program_to_use = 0;
    GLint mvp_loc = -1, color_loc = -1, time_loc = -1, res_loc = -1;
    const float *color_to_use = rect->color;
    bool is_ssd = false;
    bool needs_alpha_blending = false;
    struct tinywl_toplevel *toplevel = NULL;

    // Check if this is a special node (main background or top panel)
    if (server->main_background_node == node) {
        int desktop_idx = rdata->desktop_index;
        if (desktop_idx < 0 || desktop_idx >= server->num_desktops) {
            desktop_idx = 0;
            wlr_log(WLR_DEBUG, "Invalid desktop index %d, using 0.", rdata->desktop_index);
        }
        program_to_use = server->desktop_background_shaders[desktop_idx];
        mvp_loc = server->desktop_bg_shader_mvp_loc[desktop_idx];
        color_loc = server->desktop_bg_shader_color_loc[desktop_idx];
        time_loc = server->desktop_bg_shader_time_loc[desktop_idx];
        res_loc = server->desktop_bg_shader_res_loc[desktop_idx];
        if (desktop_idx != 0) {
            static const float neutral_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
            color_to_use = neutral_color;
        }
        wlr_log(WLR_DEBUG, "Node %p: main background, shader %u.", (void*)node, program_to_use);
    } else if (server->top_panel_node == node) {
        wlr_log(WLR_DEBUG, "Node %p: top panel, skipping.", (void*)node);
        return;
    } else {
        // Try to find toplevel by checking node data and parent hierarchy
        if (node->data) {
            toplevel = node->data;
            if (toplevel && toplevel->xdg_toplevel) {
                is_ssd = true;
                needs_alpha_blending = true;  // SSD elements need alpha blending
                wlr_log(WLR_DEBUG, "Node %p: direct toplevel %p found.", (void*)node, (void*)toplevel);
            }
        }

        // If no direct toplevel, traverse up the scene graph
        if (!toplevel) {
            struct wlr_scene_node *current_node = node;
            int traversal_depth = 0;
            const int max_traversal_depth = 20;

            while (current_node && current_node->parent && traversal_depth < max_traversal_depth) {
                current_node = &current_node->parent->node;
                if (current_node->data) {
                    struct tinywl_toplevel *potential_toplevel = current_node->data;
                    if (potential_toplevel && potential_toplevel->xdg_toplevel) {
                        toplevel = potential_toplevel;
                        is_ssd = true;
                        needs_alpha_blending = true;  // SSD elements need alpha blending
                        wlr_log(WLR_DEBUG, "Node %p: found toplevel %p at depth %d for SSD.",
                                (void*)node, (void*)toplevel, traversal_depth);
                        break;
                    }
                }
                traversal_depth++;
            }
        }

        // If still no toplevel, check server's toplevel list
        if (!toplevel && !wl_list_empty(&server->toplevels)) {
            struct tinywl_toplevel *potential_toplevel;
            wl_list_for_each(potential_toplevel, &server->toplevels, link) {
                if (potential_toplevel->scene_tree &&
                    is_node_descendant(node, potential_toplevel->scene_tree)) {
                    toplevel = potential_toplevel;
                    is_ssd = true;
                    needs_alpha_blending = true;  // SSD elements need alpha blending
                    wlr_log(WLR_DEBUG, "Node %p: found toplevel %p via server toplevels list.",
                            (void*)node, (void*)toplevel);
                    break;
                }
            }
        }

        // Apply appropriate shader for SSD or fallback
        if (toplevel && (node == &toplevel->ssd.close_button->node || node == &toplevel->ssd.maximize_button->node || node == &toplevel->ssd.minimize_button->node)) {
            // This is a button. Use the simple solid color shader.
            program_to_use = server->solid_shader_program;
            mvp_loc = server->solid_shader_mvp_loc;
            color_loc = server->solid_shader_color_loc;
            time_loc = server->solid_shader_time_loc; 
            // time_loc and res_loc remain -1, which is fine.
            wlr_log(WLR_DEBUG, "Rendering node %p as a BUTTON.", (void*)node);
        } else if (toplevel && toplevel->xdg_toplevel) {
            const char *app_id = toplevel->xdg_toplevel->app_id ? toplevel->xdg_toplevel->app_id : "null";
            const char *title = toplevel->xdg_toplevel->title ? toplevel->xdg_toplevel->title : "null";
            wlr_log(WLR_DEBUG, "SSD node %p: app_id=%s, title=%s (window %p)",
                    (void*)node, app_id, title, (void*)toplevel->xdg_toplevel);

            bool is_alacritty = (app_id && strcasestr(app_id, "alacritty")) ||
                                (title && strcasestr(title, "alacritty"));

            program_to_use = is_alacritty ? server->ssd2_shader_program : server->ssd_shader_program;
            mvp_loc = is_alacritty ? server->ssd2_shader_mvp_loc : server->ssd_shader_mvp_loc;
            color_loc = is_alacritty ? server->ssd2_shader_color_loc : server->ssd_shader_color_loc;
            time_loc = is_alacritty ? server->ssd2_shader_time_loc : server->ssd_shader_time_loc;
            res_loc = is_alacritty ? server->ssd2_shader_resolution_loc : server->ssd_shader_resolution_loc;
        } else {
            // Fallback for unassociated rect nodes
            program_to_use = server->ssd_shader_program;
            mvp_loc = server->ssd_shader_mvp_loc;
            color_loc = server->ssd_shader_color_loc;
            time_loc = server->ssd_shader_time_loc;
            res_loc = server->ssd_shader_resolution_loc;
            needs_alpha_blending = true;  // Even fallback SSD needs alpha
            wlr_log(WLR_DEBUG, "Rect node %p: no associated toplevel, using default SSD shader.", (void*)node);
        }
    }

    if (program_to_use == 0) {
        wlr_log(WLR_ERROR, "Node %p: invalid shader program.", (void*)node);
        return;
    }

    // This logic is slightly adjusted to be correct for all cases
    if (rect->color[3] < 1.0f) {
        needs_alpha_blending = true;
    }

    // Enable alpha blending for SSD elements
    if (needs_alpha_blending) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        wlr_log(WLR_DEBUG, "Node %p: alpha blending enabled for SSD.", (void*)node);
    }

    glUseProgram(program_to_use);

    GLint current_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    if (current_program != (GLint)program_to_use) {
        wlr_log(WLR_ERROR, "Node %p: failed to bind program %u (current: %d).",
                (void*)node, program_to_use, current_program);
        if (needs_alpha_blending) glDisable(GL_BLEND);
        return;
    }

    // CRITICAL FIX: Ensure VAO is bound before rendering
    // Use the existing quad_vao from your server struct
    if (server->quad_vao == 0) {
        wlr_log(WLR_ERROR, "Node %p: server quad VAO not initialized.", (void*)node);
        if (needs_alpha_blending) glDisable(GL_BLEND);
        return;
    }
    
    glBindVertexArray(server->quad_vao);
    
    // Verify VAO is now bound
    GLint vao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    if (vao == 0) {
        wlr_log(WLR_ERROR, "Node %p: failed to bind VAO %u.", (void*)node, server->quad_vao);
        if (needs_alpha_blending) glDisable(GL_BLEND);
        return;
    }

    float mvp[9];
    float box_scale_x = (float)rect->width * (2.0f / output->width);
    float box_scale_y = (float)rect->height * (-2.0f / output->height);
    float box_translate_x = (float)render_sx / output->width * 2.0f - 1.0f;
    float box_translate_y = (float)render_sy / output->height * -2.0f + 1.0f;
    float base_mvp[9] = {box_scale_x, 0, 0, 0, box_scale_y, 0, box_translate_x, box_translate_y, 1};

    if (output->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        float transform_matrix[9];
        wlr_matrix_transform(transform_matrix, output->transform);
        wlr_matrix_multiply(mvp, transform_matrix, base_mvp);
    } else {
        memcpy(mvp, base_mvp, sizeof(mvp));
    }

    // For SSD elements, use the original color (which should include alpha)
    // but ensure we're respecting the alpha channel from the rect->color
    float final_color[4];
    if (is_ssd && rect->color[3] < 1.0f) {
        // Use the original alpha from the rect
        memcpy(final_color, rect->color, sizeof(final_color));
        color_to_use = final_color;
        wlr_log(WLR_DEBUG, "Node %p: using original alpha %.2f for SSD.", (void*)node, rect->color[3]);
    }

    if (mvp_loc != -1) glUniformMatrix3fv(mvp_loc, 1, GL_FALSE, mvp);
    if (color_loc != -1) glUniform4fv(color_loc, 1, color_to_use);
    if (time_loc != -1) glUniform1f(time_loc, get_monotonic_time_seconds_as_float());
    if (res_loc != -1) glUniform2f(res_loc, (float)rect->width, (float)rect->height);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        wlr_log(WLR_ERROR, "Node %p: pre-draw GL error %d.", (void*)node, err);
        if (needs_alpha_blending) glDisable(GL_BLEND);
        return;
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    err = glGetError();
    if (err != GL_NO_ERROR) {
        wlr_log(WLR_ERROR, "Node %p: post-draw GL error %d.", (void*)node, err);
    } else {
        wlr_log(WLR_DEBUG, "Node %p: rendered successfully%s%s.",
                (void*)node, is_ssd ? " (SSD)" : "", 
                needs_alpha_blending ? " with alpha" : "");
    }

    // Clean up: disable blending if we enabled it
    if (needs_alpha_blending) {
        glDisable(GL_BLEND);
    }
}
static void render_panel_node(struct wlr_scene_node *node, void *user_data) {
    struct render_data *rdata = user_data;
    struct wlr_output *output = rdata->output;
    struct tinywl_server *server = rdata->server;

    if (node->type != WLR_SCENE_NODE_RECT || !node->enabled) {
        return;
    }
    struct wlr_scene_rect *panel_srect = wlr_scene_rect_from_node(node);

    if (output->width == 0 || output->height == 0) {
        static struct wlr_output *last_logged_zero_dim_output = NULL;
        if (last_logged_zero_dim_output != output) {
            wlr_log(WLR_ERROR, "[render_panel_node] Output %s has zero width/height.", output->name);
            last_logged_zero_dim_output = output;
        }
        return;
    }
    if (server->panel_shader_program == 0) {
        static bool panel_shader_is_zero_logged = false;
        if (!panel_shader_is_zero_logged) {
            wlr_log(WLR_ERROR, "[render_panel_node] Panel shader program is 0.");
            panel_shader_is_zero_logged = true;
        }
        return;
    }

    glUseProgram(server->panel_shader_program);
    GLenum err_after_use_program = glGetError();
    if (err_after_use_program != GL_NO_ERROR) {
        wlr_log(WLR_ERROR, "[render_panel_node] GL error AFTER glUseProgram(panel_shader: %u): 0x%x",
                server->panel_shader_program, err_after_use_program);
        return;
    }

    if (server->panel_shader_rect_pixel_dimensions_loc != -1) {
        // Get the dimensions directly from the wlr_scene_rect struct.
        float panel_dims[2] = { (float)panel_srect->width, (float)panel_srect->height };
        glUniform2fv(server->panel_shader_rect_pixel_dimensions_loc, 1, panel_dims);
    }

    // --- Standard Panel Uniforms (MVP, time, iResolution for panel's own shader effects) ---
    float mvp_for_panel_geometry[9];
    int sx = node->x;
    int sy = node->y;
    float box_scale_x = (float)panel_srect->width * (2.0f / output->width);
    float box_scale_y = (float)panel_srect->height * (-2.0f / output->height);
    float box_translate_x = ((float)sx / output->width) * 2.0f - 1.0f;
    float box_translate_y = ((float)sy / output->height) * -2.0f + 1.0f;

    float base_mvp[9] = {
        box_scale_x, 0.0f, 0.0f,
        0.0f, box_scale_y, 0.0f,
        box_translate_x, box_translate_y, 1.0f
    };
    if (output->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        float output_transform_matrix[9];
        wlr_matrix_identity(output_transform_matrix);
        wlr_matrix_transform(output_transform_matrix, output->transform);
        wlr_matrix_multiply(mvp_for_panel_geometry, output_transform_matrix, base_mvp);
    } else {
        memcpy(mvp_for_panel_geometry, base_mvp, sizeof(base_mvp));
    }

    if (server->panel_shader_mvp_loc != -1) {
        glUniformMatrix3fv(server->panel_shader_mvp_loc, 1, GL_FALSE, mvp_for_panel_geometry);
    } else {
         wlr_log(WLR_ERROR, "[render_panel_node] Panel shader 'mvp' uniform location is -1.");
    }

    if (server->panel_shader_time_loc != -1) {
        glUniform1f(server->panel_shader_time_loc, get_monotonic_time_seconds_as_float());
    } else {
        static bool panel_time_loc_missing_logged = false;
        if (!panel_time_loc_missing_logged) {
            wlr_log(WLR_INFO, "[render_panel_node] Panel shader 'time' uniform location is -1 (or shader doesn't use it).");
            panel_time_loc_missing_logged = true;
        }
    }

    if (server->panel_shader_resolution_loc != -1) {
        float panel_resolution_vec[2] = {(float)output->width, (float)output->height};
        glUniform2fv(server->panel_shader_resolution_loc, 1, panel_resolution_vec);
    } else {
        static bool panel_res_loc_missing_logged = false;
        if (!panel_res_loc_missing_logged) {
            wlr_log(WLR_INFO, "[render_panel_node] Panel shader 'iResolution' uniform location is -1 (or shader doesn't use it).");
            panel_res_loc_missing_logged = true;
        }
    }
    // --- End Standard Panel Uniforms ---

    // --- Preview-Specific Uniform Setup (using cached_texture strategy) ---
    struct tinywl_toplevel *preview_toplevel_candidate = NULL;
    struct wlr_texture *texture_for_preview = NULL;
    struct wlr_gles2_texture_attribs preview_gl_tex_attribs = {0}; // Initialize
    bool is_preview_active_flag = false; // Default to false

    float calculated_norm_x = 0.f, calculated_norm_y = 0.f, calculated_norm_w = 0.f, calculated_norm_h = 0.f;
    float calculated_preview_tex_transform_matrix[9];
    wlr_matrix_identity(calculated_preview_tex_transform_matrix); // Initialize to identity

    wlr_log(WLR_DEBUG, "[PNODE_PREVIEW] Finding preview toplevel to use its cached_texture. Total toplevels: %d", wl_list_length(&server->toplevels));

    if (!wl_list_empty(&server->toplevels)) {
        struct tinywl_toplevel *iter_tl;
        wl_list_for_each_reverse(iter_tl, &server->toplevels, link) {
            const char *title = (iter_tl->xdg_toplevel && iter_tl->xdg_toplevel->title) ? iter_tl->xdg_toplevel->title : "N/A";
            wlr_log(WLR_DEBUG, "[PNODE_PREVIEW_ITER] Checking TL: %p (Title: %s) for cached_texture: %p",
                    (void*)iter_tl, title, (void*)iter_tl->cached_texture);

            if (iter_tl->xdg_toplevel &&
                iter_tl->scene_tree && iter_tl->scene_tree->node.enabled &&
                iter_tl->scale > 0.01f && !iter_tl->pending_destroy &&
                iter_tl->cached_texture != NULL) {
                
                wlr_log(WLR_INFO, "[PNODE_PREVIEW_ITER] TL '%s': Using its cached_texture %p (Size: %dx%d).",
                        title, (void*)iter_tl->cached_texture,
                        iter_tl->cached_texture->width, iter_tl->cached_texture->height);
                preview_toplevel_candidate = iter_tl;
                texture_for_preview = iter_tl->cached_texture;
                break; 
            }
        }
    }

    if (preview_toplevel_candidate && texture_for_preview) {
        wlr_log(WLR_INFO, "[PNODE_PREVIEW] Preparing to use TL '%s's cached_texture %p for preview.",
            preview_toplevel_candidate->xdg_toplevel->title, (void*)texture_for_preview);

        is_preview_active_flag = true; 
        wlr_gles2_texture_get_attribs(texture_for_preview, &preview_gl_tex_attribs);

        glActiveTexture(GL_TEXTURE1); 
        glBindTexture(preview_gl_tex_attribs.target, preview_gl_tex_attribs.tex);
        glTexParameteri(preview_gl_tex_attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(preview_gl_tex_attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(preview_gl_tex_attribs.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(preview_gl_tex_attribs.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLenum gl_err_texbind = glGetError();
        if(gl_err_texbind != GL_NO_ERROR) {
            wlr_log(WLR_ERROR, "[PNODE_PREVIEW_GL_ERROR] Error after binding/setting params for preview texture unit 1: 0x%x", gl_err_texbind);
            is_preview_active_flag = false;
        }

        if (is_preview_active_flag && server->panel_shader_preview_tex_loc != -1) {
            glUniform1i(server->panel_shader_preview_tex_loc, 1); 
             wlr_log(WLR_INFO, "[PNODE_PREVIEW] Set u_previewTexture (loc %d) to unit 1. GL Tex ID: %u",
                    server->panel_shader_preview_tex_loc, preview_gl_tex_attribs.tex);
        } else if(is_preview_active_flag) { 
             wlr_log(WLR_ERROR, "[PNODE_PREVIEW] u_previewTexture location is -1! Cannot set sampler.");
             is_preview_active_flag = false;
        }

        if (is_preview_active_flag) { 
            float preview_pixel_height = (float)panel_srect->height - 10.0f; 
            if (preview_pixel_height < 1.0f) preview_pixel_height = 1.0f;
            float aspect = (texture_for_preview->width > 0 && texture_for_preview->height > 0) ?
                           (float)texture_for_preview->width / (float)texture_for_preview->height : 1.0f;
            float preview_pixel_width = preview_pixel_height * aspect;
            if (preview_pixel_width < 1.0f) preview_pixel_width = 1.0f;
            float preview_pixel_x_start = 20.0f;

            if (panel_srect->width > 0 && panel_srect->height > 0) {
                calculated_norm_x = preview_pixel_x_start / (float)panel_srect->width;
                calculated_norm_y = 5.0f / (float)panel_srect->height; 
                calculated_norm_w = preview_pixel_width / (float)panel_srect->width;
                calculated_norm_h = preview_pixel_height / (float)panel_srect->height;
                calculated_norm_w = fmaxf(0.0f, fminf(calculated_norm_w, 1.0f - calculated_norm_x)); 
                calculated_norm_h = fmaxf(0.0f, fminf(calculated_norm_h, 1.0f - calculated_norm_y)); 
                
                if (calculated_norm_w > 0.001f && calculated_norm_h > 0.001f && server->panel_shader_preview_rect_loc != -1) {
                     // Value setting will happen below, after all is_preview_active_flag checks
                } else {
                    wlr_log(WLR_ERROR, "[PNODE_PREVIEW] Cannot set u_previewRect or rect too small. norm_w=%.3f, norm_h=%.3f, loc=%d. Panel Dims: %dx%d",
                            calculated_norm_w, calculated_norm_h, server->panel_shader_preview_rect_loc, panel_srect->width, panel_srect->height);
                    is_preview_active_flag = false; 
                }
            } else {
                 wlr_log(WLR_ERROR, "[PNODE_PREVIEW] Panel has zero dimensions (%dx%d), cannot calculate u_previewRect.", panel_srect->width, panel_srect->height);
                 is_preview_active_flag = false;
            }
        }
            
        if (is_preview_active_flag) { 
            calculated_preview_tex_transform_matrix[4] = -1.0f; 
            calculated_preview_tex_transform_matrix[7] =  1.0f; 
            if (server->panel_shader_preview_tex_transform_loc == -1) {
                 wlr_log(WLR_ERROR, "[PNODE_PREVIEW] u_previewTexTransform location is -1! Cannot set tex transform.");
                 // Potentially set is_preview_active_flag = false; if this transform is critical
            }
        }
    } else { 
        wlr_log(WLR_DEBUG, "[PNODE_PREVIEW] No suitable toplevel with a cached_texture found, or texture_for_preview is NULL.");
        is_preview_active_flag = false;
    }

    // Set u_isPreviewActive now that all conditions have been checked
    if (server->panel_shader_is_preview_active_loc != -1) {
        glUniform1i(server->panel_shader_is_preview_active_loc, is_preview_active_flag ? 1 : 0);
        wlr_log(WLR_INFO, "[PNODE_PREVIEW] FINAL u_isPreviewActive (loc %d) set to: %s",
                server->panel_shader_is_preview_active_loc, is_preview_active_flag ? "TRUE (1)" : "FALSE (0)");
    } else {
        wlr_log(WLR_ERROR, "[PNODE_PREVIEW] u_isPreviewActive uniform location is -1!");
    }

    // Set the other preview uniforms ONLY IF preview is active
    if (is_preview_active_flag) {
        if (server->panel_shader_preview_rect_loc != -1) {
            float rect_data[4] = {calculated_norm_x, calculated_norm_y, calculated_norm_w, calculated_norm_h};
            glUniform4fv(server->panel_shader_preview_rect_loc, 1, rect_data);
            wlr_log(WLR_INFO, "[PNODE_PREVIEW] Actually set u_previewRect (loc %d) to: (%.3f, %.3f, %.3f, %.3f)",
                    server->panel_shader_preview_rect_loc, calculated_norm_x, calculated_norm_y, calculated_norm_w, calculated_norm_h);
        }
        if (server->panel_shader_preview_tex_transform_loc != -1) {
            glUniformMatrix3fv(server->panel_shader_preview_tex_transform_loc, 1, GL_FALSE, calculated_preview_tex_transform_matrix);
            wlr_log(WLR_INFO, "[PNODE_PREVIEW] Actually set u_previewTexTransform (loc %d).", server->panel_shader_preview_tex_transform_loc);
        }
    }


    GLenum err_before_draw = glGetError();
    if (err_before_draw != GL_NO_ERROR) {
        wlr_log(WLR_ERROR, "[PNODE_PRE_DRAW_GL_ERROR] GL error BEFORE panel draw: 0x%x", err_before_draw);
    }

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    GLenum err_after_draw = glGetError();
    if (err_after_draw != GL_NO_ERROR) {
        wlr_log(WLR_ERROR, "[PNODE_POST_DRAW_GL_ERROR] GL error AFTER panel draw: 0x%x", err_after_draw);
    }

    if (is_preview_active_flag && texture_for_preview) { 
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(preview_gl_tex_attribs.target, 0); 
        glActiveTexture(GL_TEXTURE0); 
        // DO NOT destroy `texture_for_preview` here (it's cached)
    }
}
/*
static void render_desktop_content(struct tinywl_server *server,
                                   struct wlr_output *wlr_output,
                                   struct render_data *current_rdata,
                                   int desktop_to_render) {
  

    if (server->quad_vao == 0) return;
    glBindVertexArray(server->quad_vao);

    // --- Render Desktop-Specific Chrome (Background, Panel, etc.) ---
    // We'll render these only when drawing desktop 0 to avoid duplication.
    if (desktop_to_render == 0) {
        // Render the main background rectangle
        if (server->main_background_node && server->main_background_node->enabled) {
            render_rect_node(server->main_background_node, current_rdata);
        }
        
    }

if (desktop_to_render == 1) {
        // Render the main background rectangle
        if (server->main_background_node && server->main_background_node->enabled) {
            render_rect_node(server->main_background_node, current_rdata);
        }
        
    }

if (desktop_to_render == 2) {
        // Render the main background rectangle
        if (server->main_background_node && server->main_background_node->enabled) {
            render_rect_node(server->main_background_node, current_rdata);
        }
        
    }
    
if (desktop_to_render == 3) {
        // Render the main background rectangle
        if (server->main_background_node && server->main_background_node->enabled) {
            render_rect_node(server->main_background_node, current_rdata);
        }
        
    }    

    // Render the top panel
        if (server->top_panel_node && server->top_panel_node->enabled) {
            render_panel_node(server->top_panel_node, current_rdata);
        }

    // --- Render Windows (Surface + Decorations) ---
    struct wlr_scene_node *iter_node_lvl1;
    wl_list_for_each(iter_node_lvl1, &server->scene->tree.children, link) {
        // Skip nodes that are disabled or are part of the desktop chrome we handled above
        if (!iter_node_lvl1->enabled || iter_node_lvl1 == server->main_background_node || iter_node_lvl1 == server->top_panel_node) {
            continue;
        }

        if (iter_node_lvl1->type == WLR_SCENE_NODE_TREE) {
            struct tinywl_toplevel *toplevel_ptr = iter_node_lvl1->data;

            // THIS IS THE KEY CHECK: Only render toplevels belonging to the target desktop
            if (toplevel_ptr && toplevel_ptr->type == TINYWL_TOPLEVEL_XDG && toplevel_ptr->desktop == desktop_to_render) {
                struct wlr_scene_tree *toplevel_s_tree = wlr_scene_tree_from_node(iter_node_lvl1);

                // First render SSD decorations if enabled
                if (toplevel_ptr->ssd.enabled) {
                    struct wlr_scene_node *ssd_node_candidate;
                    wl_list_for_each(ssd_node_candidate, &toplevel_s_tree->children, link) {
                        if (ssd_node_candidate->enabled && ssd_node_candidate->type == WLR_SCENE_NODE_RECT) {
                            render_rect_node(ssd_node_candidate, current_rdata);
                        }
                    }
                }

                // Then render the window surface/buffers
                if (server->shader_program != 0) {
                    glUseProgram(server->shader_program);
                    wlr_scene_node_for_each_buffer(&toplevel_s_tree->node, scene_buffer_iterator, current_rdata);
                }
            }
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);
}
*/
/*
static void render_desktop_content(struct tinywl_server *server,
                                   struct wlr_output *wlr_output,
                                   struct render_data *current_rdata,
                                   int desktop_to_render) {
    if (server->quad_vao == 0) return;
    glBindVertexArray(server->quad_vao);

    // --- Render Desktop-Specific Chrome (Background, Panel, etc.) ---

    // *** THE FIX IS HERE: Render the background on EVERY desktop. ***
    // The specific shader for this desktop is chosen inside render_rect_node
    // based on the `desktop_to_render` index passed in `current_rdata`.
    if (server->main_background_node && server->main_background_node->enabled) {
        render_rect_node(server->main_background_node, current_rdata);
    }

    // Render the top panel on every desktop. This part was already correct.
    if (server->top_panel_node && server->top_panel_node->enabled) {
        render_panel_node(server->top_panel_node, current_rdata);
    }

    // --- Render Windows (Surface + Decorations) ---
    struct wlr_scene_node *iter_node_lvl1;
    wl_list_for_each(iter_node_lvl1, &server->scene->tree.children, link) {
        // Skip nodes that are disabled or are part of the desktop chrome we handled above
        if (!iter_node_lvl1->enabled || iter_node_lvl1 == server->main_background_node || iter_node_lvl1 == server->top_panel_node) {
            continue;
        }

        if (iter_node_lvl1->type == WLR_SCENE_NODE_TREE) {
            struct tinywl_toplevel *toplevel_ptr = iter_node_lvl1->data;

            // THIS IS THE KEY CHECK: Only render toplevels belonging to the target desktop
            if (toplevel_ptr && toplevel_ptr->type == TINYWL_TOPLEVEL_XDG && toplevel_ptr->desktop == desktop_to_render) {
                struct wlr_scene_tree *toplevel_s_tree = wlr_scene_tree_from_node(iter_node_lvl1);

                // First render SSD decorations if enabled
                if (toplevel_ptr->ssd.enabled) {
                    struct wlr_scene_node *ssd_node_candidate;
                    wl_list_for_each(ssd_node_candidate, &toplevel_s_tree->children, link) {
                        if (ssd_node_candidate->enabled && ssd_node_candidate->type == WLR_SCENE_NODE_RECT) {
                            render_rect_node(ssd_node_candidate, current_rdata);
                        }
                    }
                }

                // Then render the window surface/buffers
                if (server->shader_program != 0) {
                    glUseProgram(server->shader_program);
                    wlr_scene_node_for_each_buffer(&toplevel_s_tree->node, scene_buffer_iterator, current_rdata);
                }
            }
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);
}*/

// MODIFIED: This is the final, correct version using the wlr_render_pass API.
static void render_desktop_content(struct tinywl_server *server,
                                   struct wlr_output *wlr_output,
                                   struct render_data *current_rdata,
                                   int desktop_to_render) {
    if (server->quad_vao == 0) return;
    glBindVertexArray(server->quad_vao);

    // --- Render Desktop Chrome (Background and Panel Base) ---
    if (server->main_background_node && server->main_background_node->enabled) {
        render_rect_node(server->main_background_node, current_rdata);
    }
    if (server->top_panel_node && server->top_panel_node->enabled) {
        render_panel_node(server->top_panel_node, current_rdata);
    }

    // --- LOOP 1: Render MAIN WINDOWS (non-minimized only) ---
    struct wlr_scene_node *scene_node;
    wl_list_for_each(scene_node, &server->scene->tree.children, link) {
        // Skip nodes that are disabled (minimized) or are desktop chrome
        if (!scene_node->enabled || scene_node == server->main_background_node || scene_node == server->top_panel_node) {
            continue;
        }

        if (scene_node->type == WLR_SCENE_NODE_TREE) {
            struct tinywl_toplevel *toplevel = scene_node->data;

            // Skip rendering the main window if it's minimized
            if (toplevel && toplevel->desktop == desktop_to_render && !toplevel->minimized) {
                if (toplevel->ssd.enabled) {
                    struct wlr_scene_node *ssd_node;
                    wl_list_for_each(ssd_node, &toplevel->scene_tree->children, link) {
                        if (ssd_node->type == WLR_SCENE_NODE_RECT) render_rect_node(ssd_node, current_rdata);
                    }
                }
                // The scene_buffer_iterator now correctly handles its own rendering.
                wlr_scene_node_for_each_buffer(scene_node, scene_buffer_iterator, current_rdata);
            }
        }
    }

    // --- LOOP 2: Render ALL panel previews for this desktop ---
    if (server->top_panel_node && server->top_panel_node->enabled && current_rdata->pass) {
        struct wlr_scene_rect *panel_srect = wlr_scene_rect_from_node(server->top_panel_node);
        int desktop_window_count = 0;
        struct tinywl_toplevel *iter_tl;
        wl_list_for_each(iter_tl, &server->toplevels, link) {
            if (iter_tl->desktop == desktop_to_render) desktop_window_count++;
        }

        if (desktop_window_count > 0) {
            const float PREVIEW_PADDING = 5.0f, PANEL_LEFT_MARGIN = 20.0f, MAX_PREVIEW_WIDTH = 120.0f;
            float available_width = panel_srect->width - PANEL_LEFT_MARGIN * 2;
            float ideal_width = fminf(MAX_PREVIEW_WIDTH, (available_width - (desktop_window_count - 1) * PREVIEW_PADDING) / desktop_window_count);
            float preview_y_padding = PREVIEW_PADDING;
            float preview_height = panel_srect->height - 2.0f * preview_y_padding;

            int per_desktop_index = 0;
            wl_list_for_each(iter_tl, &server->toplevels, link) {
                if (iter_tl->desktop != desktop_to_render || !iter_tl->cached_texture) {
                    continue;
                }

                struct wlr_texture *texture = iter_tl->cached_texture;
                float aspect_ratio = (texture->width > 0) ? (float)texture->width / (float)texture->height : 1.0f;
                float preview_width = fminf(ideal_width, preview_height * aspect_ratio);
                float preview_height_adjusted = preview_width / aspect_ratio;
                float slot_x = PANEL_LEFT_MARGIN + per_desktop_index * (ideal_width + PREVIEW_PADDING);
                float preview_x = slot_x + (ideal_width - preview_width) / 2.0f;
                float preview_y = preview_y_padding + (preview_height - preview_height_adjusted) / 2.0f;

                struct wlr_box box = {
                    .x = (int)round(server->top_panel_node->x + preview_x),
                    .y = (int)round(server->top_panel_node->y + preview_y),
                    .width = (int)round(preview_width),
                    .height = (int)round(preview_height_adjusted)
                };
                iter_tl->panel_preview_box = box;

                if (box.width > 0 && box.height > 0) {
                    // ***** THIS IS THE FINAL, CORRECT API USAGE *****
                    // We construct the options struct and pass its address.
                    // The renderer and render pass handle all matrix transformations.
                    struct wlr_render_texture_options options = {
                        .texture = texture,
                        .dst_box = box,
                        // No need for transform here as dst_box is already in output-local coords.
                    };
                    wlr_render_pass_add_texture(current_rdata->pass, &options);
                }
                per_desktop_index++;
            }
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

// Helper function to manually create a texture transform matrix (if wlr_matrix_texture_transform is missing)
// This is a simplified version. A full one would handle all 8 transforms.
// tex_coords are typically (0,0) top-left, (1,1) bottom-right.
// Output geometry is (0,0) top-left for NORMAL.
static void manual_texture_transform(float mat[static 9], enum wl_output_transform transform) {
    wlr_matrix_identity(mat);
    switch (transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
            // mat is already identity
            break;
        case WL_OUTPUT_TRANSFORM_90:
            // Rotate 90 deg clockwise around (0.5, 0.5)
            // Tex (u,v) -> (v, 1-u)
            // x' = 0*u + 1*v + 0
            // y' = -1*u + 0*v + 1
            // z' = 0*u + 0*v + 1
            mat[0] = 0; mat[1] = -1; mat[2] = 0;
            mat[3] = 1; mat[4] = 0;  mat[5] = 0;
            mat[6] = 0; mat[7] = 1;  mat[8] = 1;
            break;
        case WL_OUTPUT_TRANSFORM_180:
            // Tex (u,v) -> (1-u, 1-v)
            mat[0] = -1; mat[1] = 0; mat[2] = 0;
            mat[3] = 0;  mat[4] = -1;mat[5] = 0;
            mat[6] = 1;  mat[7] = 1; mat[8] = 1;
            break;
        case WL_OUTPUT_TRANSFORM_270:
            // Tex (u,v) -> (1-v, u)
            mat[0] = 0; mat[1] = 1; mat[2] = 0;
            mat[3] = -1;mat[4] = 0; mat[5] = 0;
            mat[6] = 1; mat[7] = 0; mat[8] = 1;
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            // Tex (u,v) -> (1-u, v)
            mat[0] = -1; mat[1] = 0; mat[2] = 0;
            mat[3] = 0;  mat[4] = 1; mat[5] = 0;
            mat[6] = 1;  mat[7] = 0; mat[8] = 1;
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            // Tex (u,v) -> (v, u) -- Check this one, flipped then 90
            // Flipped: (1-u, v)
            // Rot 90 on that: (v, 1-(1-u)) = (v, u)
            mat[0] = 0; mat[1] = 1; mat[2] = 0;
            mat[3] = 1; mat[4] = 0; mat[5] = 0;
            mat[6] = 0; mat[7] = 0; mat[8] = 1;
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            // Tex (u,v) -> (u, 1-v)
            mat[0] = 1; mat[1] = 0; mat[2] = 0;
            mat[3] = 0; mat[4] = -1;mat[5] = 0;
            mat[6] = 0; mat[7] = 1; mat[8] = 1;
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            // Tex (u,v) -> (1-v, 1-u) -- Check this one
            // Flipped: (1-u, v)
            // Rot 270 on that: (1-v, 1-(1-u)) = (1-v, u) -> NO, (1-v, 1-(1-u)) = (1-v, u)
            // Rot 270 is (y, 1-x). So (v, 1-(1-u)) = (v, u) ... no this is wrong
            // Flipped: (1-u, v)
            // 270: (1-v', u') -> (1-v, 1-u)
            mat[0] = 0; mat[1] = -1; mat[2] = 0;
            mat[3] = -1;mat[4] = 0;  mat[5] = 0;
            mat[6] = 1; mat[7] = 1;  mat[8] = 1;
            break;
    }
}


static bool setup_intermediate_framebuffer(struct tinywl_server *server, int width, int height, int idx);

static bool setup_intermediate_framebuffer(struct tinywl_server *server, int width, int height, int idx) {
    if (idx < 0 || idx >= 16) {
        wlr_log(WLR_ERROR, "Invalid desktop index %d for FBO setup.", idx);
        return false;
    }

    // If dimensions match and FBO exists, reuse it
    if (server->intermediate_width[idx] == width && 
        server->intermediate_height[idx] == height && 
        server->desktop_fbos[idx] != 0) {
        return true;
    }

    // Delete existing FBO if it exists
    if (server->desktop_fbos[idx] != 0) {
        glDeleteFramebuffers(1, &server->desktop_fbos[idx]);
        server->desktop_fbos[idx] = 0;
    }
    if (server->intermediate_texture[idx] != 0) {
        glDeleteTextures(1, &server->intermediate_texture[idx]);
        server->intermediate_texture[idx] = 0;
    }
    if (server->intermediate_rbo[idx] != 0) {
        glDeleteRenderbuffers(1, &server->intermediate_rbo[idx]);
        server->intermediate_rbo[idx] = 0;
    }

    // Create color texture
    glGenTextures(1, &server->intermediate_texture[idx]);
    glBindTexture(GL_TEXTURE_2D, server->intermediate_texture[idx]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create depth renderbuffer (optional, but good for 3D)
    glGenRenderbuffers(1, &server->intermediate_rbo[idx]);
    glBindRenderbuffer(GL_RENDERBUFFER, server->intermediate_rbo[idx]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create FBO and attach
    glGenFramebuffers(1, &server->desktop_fbos[idx]);
    glBindFramebuffer(GL_FRAMEBUFFER, server->desktop_fbos[idx]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, server->intermediate_texture[idx], 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, server->intermediate_rbo[idx]);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        wlr_log(WLR_ERROR, "Framebuffer incomplete for desktop %d: status 0x%x", idx, status);
        // Cleanup partial creation
        glDeleteFramebuffers(1, &server->desktop_fbos[idx]);
        glDeleteTextures(1, &server->intermediate_texture[idx]);
        glDeleteRenderbuffers(1, &server->intermediate_rbo[idx]);
        server->desktop_fbos[idx] = 0;
        server->intermediate_texture[idx] = 0;
        server->intermediate_rbo[idx] = 0;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    server->intermediate_width[idx] = width;
    server->intermediate_height[idx] = height;

    wlr_log(WLR_INFO, "Setup FBO %d for desktop %d: %dx%d", server->desktop_fbos[idx], idx, width, height);

    return true;
}

/*
static void output_frame(struct wl_listener *listener, void *data) {
    struct tinywl_output *output_wrapper = wl_container_of(listener, output_wrapper, frame);
    struct wlr_output *wlr_output = output_wrapper->wlr_output;
    struct tinywl_server *server = output_wrapper->server;
    struct wlr_scene *scene = server->scene;
    struct wlr_renderer *renderer = server->renderer;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct wlr_output_state output_state_direct;
    struct wlr_output_state output_state_effect;
    struct wlr_render_pass *current_screen_pass = NULL;
    float time_for_shader;
    if (!wlr_output || !wlr_output->enabled || !renderer || !server->allocator) {
        wlr_log(WLR_ERROR, "[OUTPUT_FRAME:%s] Output not ready (output_ptr=%p, enabled=%d, renderer_ptr=%p, allocator=%p).",
                wlr_output ? wlr_output->name : "none",
                wlr_output, wlr_output ? wlr_output->enabled : 0, renderer, server->allocator);
        if (scene && wlr_output) {
            struct wlr_scene_output *output_early_exit = wlr_scene_get_scene_output(scene, wlr_output);
            if (output_early_exit) wlr_scene_output_send_frame_done(output_early_exit, &now);
        }
        return;
    }
    if (!scene) {
        wlr_log(WLR_ERROR, "[%s] Output_frame: no scene", wlr_output->name);
        return;
    }
    struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, wlr_output);
    if (!scene_output) {
        wlr_log(WLR_ERROR, "[%s] Output_frame: no scene_output", wlr_output->name);
        return;
    }
    // NEW: Dock animation logic 
    if (server->dock_anim_active) {
        float elapsed = get_monotonic_time_seconds_as_float() - server->dock_anim_start_time;
        float progress = (elapsed / server->dock_anim_duration);
        if (progress >= 1.0f) {
            // Animation finished
            dockX_align = (float)server->dock_anim_target_value;
            server->dock_anim_active = false;
        } else {
            // Animation in progress: use an ease-in-out curve for smoothness
            float eased_progress = progress * progress * (3.0f - 2.0f * progress);
            // Interpolate the value
            dockX_align = (float)server->dock_anim_start_value +
                          (server->dock_anim_target_value - server->dock_anim_start_value) * eased_progress;
        }
        // Schedule another frame to continue the animation
        wlr_output_schedule_frame(wlr_output);
    }
    // Make EGL context current
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(renderer);
    if (!egl || !wlr_egl_make_current(egl, NULL)) {
        wlr_log(WLR_ERROR, "[%s] Failed to make EGL context current for output_frame", wlr_output->name);
        wlr_scene_output_send_frame_done(scene_output, &now);
        return;
    }
    // Clear any existing GL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        wlr_log(WLR_DEBUG, "[%s] Clearing pre-existing GL error: 0x%x", wlr_output->name, err);
    }
    // Attach render buffer
    int buffer_age = -1;
    if (!wlr_output_attach_render(wlr_output, &buffer_age)) {
        wlr_log(WLR_ERROR, "[%s] Failed to attach render", wlr_output->name);
        wlr_egl_unset_current(egl);
        wlr_scene_output_send_frame_done(scene_output, &now);
        return;
    }
    GLint screen_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screen_fbo);
    // Clear to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // Damage detection
    wlr_output_state_init(&output_state_direct);
    struct wlr_scene_output_state_options opts = {0};
    bool has_damage = wlr_scene_output_build_state(scene_output, &output_state_direct, &opts);
    bool effects_frame_fbo_path = server->expo_effect_active || server->cube_effect_active;
    // Cube effect animation state
    if (!server->cube_effect_active) {
        if (last_quadrant != server->current_desktop) {
            // This is the first frame the cube is active after a potential desktop change.
            // Force the animation system to accept the new reality.
            wlr_log(WLR_INFO, "CUBE SYNC: Current desktop is %d, but last cube quadrant was %d. Synchronizing.",
                    server->current_desktop, last_quadrant);
            last_quadrant = server->current_desktop;
          
            // Set the cube's rotation directly to the correct angle without animation.
            // This prevents the jarring rotation from the old desktop to the new one.
            target_rotation = (float)server->current_desktop * -M_PI / 2.0f;
            current_rotation = target_rotation;
            last_rendered_rotation = target_rotation;
            animating = false; // Ensure no old rotation animation is running.
        }
    }
    // IMPROVED: Expo effect animation state (from second version)
    if (server->expo_effect_active) {
        if (server->effect_is_animating_zoom) {
            float now_sec = get_monotonic_time_seconds_as_float();
            float elapsed_sec = now_sec - server->effect_anim_start_time_sec;
            float t = (server->effect_anim_duration_sec > 1e-5f) ? (elapsed_sec / server->effect_anim_duration_sec) : 1.0f;
            if (t >= 1.0f) {
                // IMPROVED: Proper state reset when animation finishes
                server->effect_anim_start_factor = server->effect_zoom_factor_normal;
                server->effect_anim_current_factor = server->effect_anim_target_factor;
                server->effect_is_animating_zoom = false;
                // This code block runs ONLY when the zoom-out animation finishes.
                if (!server->effect_is_target_zoomed) {
                    server->expo_effect_active = false;
                    wlr_log(WLR_INFO, "[%s] Expo Effect OFF.", wlr_output->name);
                    update_compositor_state(server); // <-- THE FIX
                }
            } else {
                server->effect_anim_current_factor = server->effect_anim_start_factor +
                                                    (server->effect_anim_target_factor - server->effect_anim_start_factor) * t;
            }
            wlr_output_schedule_frame(wlr_output); // Schedule next frame for animation
        } else {
            server->effect_anim_current_factor = server->effect_is_target_zoomed ?
                                               server->effect_zoom_factor_zoomed : server->effect_zoom_factor_normal;
        }
    } else {
        server->effect_is_animating_zoom = false;
        server->effect_anim_current_factor = server->effect_zoom_factor_normal;
    }
    // IMPROVED: TV effect animation state (from second version)
    if (server->tv_effect_animating) {
        float elapsed = get_monotonic_time_seconds_as_float() - server->tv_effect_start_time;
        float animation_duration = server->tv_effect_duration / 2.0f;
      
        if (elapsed >= animation_duration) {
            // Animation is over, stop animating and toggle state
            server->tv_effect_animating = false;
          
            if (server->tv_effect_duration / 2.0f == 2.5f) {
                // Was at 2.5, now store 5.0
                server->tv_effect_duration = 10.0f; // So duration/2 = 5.0
                time_for_shader = 5.0f;
            } else {
                // Was at 5.0, now store 2.5
                server->tv_effect_duration = 5.0f; // So duration/2 = 2.5
                time_for_shader = 2.5f;
            }
        } else {
            // Animation is in progress - determine start and end values
            float start_value = server->tv_effect_duration / 2.0f;
            float end_value = (start_value == 2.5f) ? 5.0f : 2.5f;
          
            // Interpolate between start and end
            float progress = elapsed / animation_duration;
            time_for_shader = start_value + (end_value - start_value) * progress;
        }
    } else {
        // Not animating, so send the stored stable state
        time_for_shader = server->tv_effect_duration / 2.0f;
    }
    // Stage 1: Prepare textures for multi-desktop effects
    if (effects_frame_fbo_path) {
        struct tinywl_toplevel *toplevel;
        wl_list_for_each(toplevel, &server->toplevels, link) {
            if (toplevel->scene_tree) {
                wlr_scene_node_set_enabled(&toplevel->scene_tree->node, toplevel->scale > 0.01);
            }
        }
        // IMPROVED: Better conditional rendering logic from second version
        if (server->effect_anim_current_factor < 2.0 || switch_mode == 1) {
            // Bind all desktop textures
            for (int i = 0; i < server->num_desktops; ++i) {
                if (!setup_intermediate_framebuffer(server, wlr_output->width, wlr_output->height, i)) {
                    wlr_log(WLR_ERROR, "Failed to setup desktop FBO %d. Skipping.", i);
                    continue;
                }
                glBindFramebuffer(GL_FRAMEBUFFER, server->desktop_fbos[i]);
                glViewport(0, 0, server->intermediate_width[i], server->intermediate_height[i]);
              
                // FIX 8: Clear with transparent black for individual desktop FBOs
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent, not opaque
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
              
                // FIX 9: Ensure blending is enabled for desktop content rendering
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
              
                struct render_data rdata = {
                    .renderer = renderer,
                    .server = server,
                    .output = wlr_output,
                    .pass = NULL,
                    .desktop_index = i
                };
                render_desktop_content(server, wlr_output, &rdata, i);
                glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo);
            }
        } else if (server->effect_anim_current_factor == 2.0 && switch_mode == 0) {
            // IMPROVED: Single desktop rendering optimization from second version
            int target_quadrant = server->pending_desktop_switch != -1 ?
                                server->pending_desktop_switch : server->current_desktop;
               
            if (!setup_intermediate_framebuffer(server, wlr_output->width, wlr_output->height, target_quadrant)) {
                wlr_log(WLR_ERROR, "Failed to setup desktop FBO %d. Aborting frame.", target_quadrant);
                wlr_output_commit(wlr_output);
                wlr_egl_unset_current(egl);
                wlr_scene_output_send_frame_done(scene_output, &now);
                return;
            }
            glBindFramebuffer(GL_FRAMEBUFFER, server->desktop_fbos[target_quadrant]);
            glViewport(0, 0, server->intermediate_width[target_quadrant], server->intermediate_height[target_quadrant]);
          
            // Clear with transparent black for individual desktop FBOs
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent, not opaque
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          
            // Ensure blending is enabled for desktop content rendering
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          
            struct render_data rdata = {
                .renderer = renderer,
                .server = server,
                .output = wlr_output,
                .pass = NULL,
                .desktop_index = target_quadrant
            };
            render_desktop_content(server, wlr_output, &rdata, target_quadrant);
            glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo);
        }
        wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);
        // Render expo effect using GL (since we have GL textures)
        glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo); // Bind default FBO for screen
        glViewport(0, 0, wlr_output->width, wlr_output->height);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(server->quad_vao);
        glUseProgram(server->fullscreen_shader_program);
        glUniform1f(server->fullscreen_shader_zoom_loc, server->effect_anim_current_factor);
        glUniform1f(server->fullscreen_shader_move_loc, server->effect_anim_current_factor);
        int target_quadrant = server->pending_desktop_switch != -1 ? server->pending_desktop_switch : server->current_desktop;
        glUniform1i(server->fullscreen_shader_quadrant_loc, target_quadrant);
        glUniform1i(server->fullscreen_switch_mode, switch_mode);
        glUniform1i(server->fullscreen_switchXY, switchXY);
        // IMPROVED: Better texture binding logic
        if (server->effect_anim_current_factor < 2.0 || switch_mode == 1) {
            // Bind all desktop textures
            for (int i = 0; i < server->num_desktops; ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, server->intermediate_texture[i]);
                glUniform1i(server->fullscreen_shader_scene_tex0_loc + i, i);
            }
        } else if (server->effect_anim_current_factor == 2.0 && switch_mode == 0) {
            // Bind only the target desktop texture
            glActiveTexture(GL_TEXTURE0 + target_quadrant);
            glBindTexture(GL_TEXTURE_2D, server->intermediate_texture[target_quadrant]);
            glUniform1i(server->fullscreen_shader_scene_tex0_loc + target_quadrant, target_quadrant);
        }
        glUniform1i(server->fullscreen_shader_desktop_grid_loc, DestopGridSize);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glUseProgram(0);
        glBindVertexArray(0);
        wlr_renderer_end(renderer);
        // Commit after direct GL rendering in effects path
        wlr_output_commit(wlr_output);
        wlr_egl_unset_current(egl);
        wlr_scene_output_send_frame_done(scene_output, &now);
        return;
    } else {
        // Begin render pass for normal rendering
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        pixman_region32_t damage;
        pixman_region32_init_rect(&damage, 0, 0, wlr_output->width, wlr_output->height);
        wlr_output_state_set_damage(&state, &damage);
        pixman_region32_fini(&damage);
        struct wlr_render_pass *pass = wlr_output_begin_render_pass(wlr_output, &state, &buffer_age, NULL);
        if (!pass) {
            wlr_log(WLR_ERROR, "[%s] Failed to begin render pass", wlr_output->name);
            wlr_output_rollback(wlr_output);
            wlr_egl_unset_current(egl);
            wlr_scene_output_send_frame_done(scene_output, &now);
            return;
        }
        // Normal rendering to pass
        struct render_data rdata = {
            .renderer = renderer,
            .server = server,
            .output = wlr_output,
            .pass = pass,
            .desktop_index = server->current_desktop
        };
        render_desktop_content(server, wlr_output, &rdata, server->current_desktop);
        // Submit pass and commit
        wlr_render_pass_submit(pass);
        wlr_output_commit(wlr_output);
    }
    wlr_egl_unset_current(egl);
    wlr_scene_output_send_frame_done(scene_output, &now);
    // Schedule next frame
    wlr_output_schedule_frame(wlr_output);
}*/

static void output_frame(struct wl_listener *listener, void *data) {
    struct tinywl_output *output_wrapper = wl_container_of(listener, output_wrapper, frame);
    struct wlr_output *wlr_output = output_wrapper->wlr_output;
    struct tinywl_server *server = output_wrapper->server;
    struct wlr_scene *scene = server->scene;
    struct wlr_renderer *renderer = server->renderer;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct wlr_output_state output_state_direct;
    struct wlr_output_state output_state_effect;
    struct wlr_render_pass *current_screen_pass = NULL;
    float time_for_shader;
    if (!wlr_output || !wlr_output->enabled || !renderer || !server->allocator) {
        wlr_log(WLR_ERROR, "[OUTPUT_FRAME:%s] Output not ready (output_ptr=%p, enabled=%d, renderer_ptr=%p, allocator=%p).",
                wlr_output ? wlr_output->name : "none",
                wlr_output, wlr_output ? wlr_output->enabled : 0, renderer, server->allocator);
        if (scene && wlr_output) {
            struct wlr_scene_output *output_early_exit = wlr_scene_get_scene_output(scene, wlr_output);
            if (output_early_exit) wlr_scene_output_send_frame_done(output_early_exit, &now);
        }
        return;
    }
    if (!scene) {
        wlr_log(WLR_ERROR, "[%s] Output_frame: no scene", wlr_output->name);
        return;
    }
    struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, wlr_output);
    if (!scene_output) {
        wlr_log(WLR_ERROR, "[%s] Output_frame: no scene_output", wlr_output->name);
        return;
    }
    /* NEW: Dock animation logic */
    if (server->dock_anim_active) {
        float elapsed = get_monotonic_time_seconds_as_float() - server->dock_anim_start_time;
        float progress = (elapsed / server->dock_anim_duration);
        if (progress >= 1.0f) {
            // Animation finished
            dockX_align = (float)server->dock_anim_target_value;
            server->dock_anim_active = false;
        } else {
            // Animation in progress: use an ease-in-out curve for smoothness
            float eased_progress = progress * progress * (3.0f - 2.0f * progress);
            // Interpolate the value
            dockX_align = (float)server->dock_anim_start_value +
                          (server->dock_anim_target_value - server->dock_anim_start_value) * eased_progress;
        }
        // Schedule another frame to continue the animation
        wlr_output_schedule_frame(wlr_output);
    }
    // Make EGL context current
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(renderer);
    if (!egl || !wlr_egl_make_current(egl, NULL)) {
        wlr_log(WLR_ERROR, "[%s] Failed to make EGL context current for output_frame", wlr_output->name);
        wlr_scene_output_send_frame_done(scene_output, &now);
        return;
    }
    // Clear any existing GL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        wlr_log(WLR_DEBUG, "[%s] Clearing pre-existing GL error: 0x%x", wlr_output->name, err);
    }
    // Attach render buffer
    int buffer_age = -1;
    if (!wlr_output_attach_render(wlr_output, &buffer_age)) {
        wlr_log(WLR_ERROR, "[%s] Failed to attach render", wlr_output->name);
        wlr_egl_unset_current(egl);
        wlr_scene_output_send_frame_done(scene_output, &now);
        return;
    }
    GLint screen_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &screen_fbo);
    // Clear to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // Damage detection
    wlr_output_state_init(&output_state_direct);
    struct wlr_scene_output_state_options opts = {0};
    bool has_damage = wlr_scene_output_build_state(scene_output, &output_state_direct, &opts);
    bool effects_frame_fbo_path = server->expo_effect_active || server->cube_effect_active;
    // Cube effect animation state
    if (!server->cube_effect_active) {
        if (last_quadrant != server->current_desktop) {
            // This is the first frame the cube is active after a potential desktop change.
            // Force the animation system to accept the new reality.
            wlr_log(WLR_INFO, "CUBE SYNC: Current desktop is %d, but last cube quadrant was %d. Synchronizing.",
                    server->current_desktop, last_quadrant);
            last_quadrant = server->current_desktop;
          
            // Set the cube's rotation directly to the correct angle without animation.
            // This prevents the jarring rotation from the old desktop to the new one.
            target_rotation = (float)server->current_desktop * -M_PI / 2.0f;
            current_rotation = target_rotation;
            last_rendered_rotation = target_rotation;
            animating = false; // Ensure no old rotation animation is running.
        }
    }

    // --- Cube Effect Animation State Calculation (Added from second version) ---
    if (server->cube_effect_active) {
        if (server->cube_is_animating_zoom) {
            float now_sec = get_monotonic_time_seconds_as_float();
            float elapsed_sec = now_sec - server->cube_anim_start_time_sec;
            float t = (server->cube_anim_duration_sec > 1e-5f) ? (elapsed_sec / server->cube_anim_duration_sec) : 1.0f;
            server->effect_is_animating_zoom = false;
            server->expo_effect_active = false;
            server->effect_is_target_zoomed = false;
            if (t >= 1.0f) {
                server->cube_anim_current_factor = server->cube_anim_target_factor;
                server->cube_is_animating_zoom = false;
                if (!server->cube_is_target_zoomed) {
                    server->cube_effect_active = false;
                    wlr_log(WLR_INFO, "[%s] Cube Effect OFF.", wlr_output->name);
                    
                    // 1. Apply the deferred desktop switch if there is one.
                    if (server->pending_desktop_switch != -1) {
                        server->current_desktop = server->pending_desktop_switch;
                        server->pending_desktop_switch = -1;
                    }

                    update_compositor_state(server); // <-- THE FIX
                }
            } else {
                server->cube_anim_current_factor = server->cube_anim_start_factor + (server->cube_anim_target_factor - server->cube_anim_start_factor) * t;
            }
        } else {
            server->cube_anim_current_factor = server->cube_is_target_zoomed ? server->cube_zoom_factor_zoomed : server->cube_zoom_factor_normal;
        }
    } else {
        server->cube_is_animating_zoom = false;
        server->cube_anim_current_factor = server->cube_zoom_factor_normal;
    }

    // IMPROVED: Expo effect animation state (from second version)
    if (server->expo_effect_active) {
        if (server->effect_is_animating_zoom) {
            float now_sec = get_monotonic_time_seconds_as_float();
            float elapsed_sec = now_sec - server->effect_anim_start_time_sec;
            float t = (server->effect_anim_duration_sec > 1e-5f) ? (elapsed_sec / server->effect_anim_duration_sec) : 1.0f;
            if (t >= 1.0f) {
                // IMPROVED: Proper state reset when animation finishes
                server->effect_anim_start_factor = server->effect_zoom_factor_normal;
                server->effect_anim_current_factor = server->effect_anim_target_factor;
                server->effect_is_animating_zoom = false;
                // This code block runs ONLY when the zoom-out animation finishes.
                if (!server->effect_is_target_zoomed) {
                    server->expo_effect_active = false;
                    wlr_log(WLR_INFO, "[%s] Expo Effect OFF.", wlr_output->name);
                    update_compositor_state(server); // <-- THE FIX
                }
            } else {
                server->effect_anim_current_factor = server->effect_anim_start_factor +
                                                    (server->effect_anim_target_factor - server->effect_anim_start_factor) * t;
            }
            wlr_output_schedule_frame(wlr_output); // Schedule next frame for animation
        } else {
            server->effect_anim_current_factor = server->effect_is_target_zoomed ?
                                               server->effect_zoom_factor_zoomed : server->effect_zoom_factor_normal;
        }
    } else {
        server->effect_is_animating_zoom = false;
        server->effect_anim_current_factor = server->effect_zoom_factor_normal;
    }

    // IMPROVED: TV effect animation state (from second version)
    if (server->tv_effect_animating) {
        float elapsed = get_monotonic_time_seconds_as_float() - server->tv_effect_start_time;
        float animation_duration = server->tv_effect_duration / 2.0f;
      
        if (elapsed >= animation_duration) {
            // Animation is over, stop animating and toggle state
            server->tv_effect_animating = false;
          
            if (server->tv_effect_duration / 2.0f == 2.5f) {
                // Was at 2.5, now store 5.0
                server->tv_effect_duration = 10.0f; // So duration/2 = 5.0
                time_for_shader = 5.0f;
            } else {
                // Was at 5.0, now store 2.5
                server->tv_effect_duration = 5.0f; // So duration/2 = 2.5
                time_for_shader = 2.5f;
            }
        } else {
            // Animation is in progress - determine start and end values
            float start_value = server->tv_effect_duration / 2.0f;
            float end_value = (start_value == 2.5f) ? 5.0f : 2.5f;
          
            // Interpolate between start and end
            float progress = elapsed / animation_duration;
            time_for_shader = start_value + (end_value - start_value) * progress;
        }
    } else {
        // Not animating, so send the stored stable state
        time_for_shader = server->tv_effect_duration / 2.0f;
    }

    // Mutual exclusion logic between cube and expo effects (added from second version)
    if (server->cube_effect_active) {
        // If the cube is on, the expo MUST be off.
        if (server->expo_effect_active) {
            server->expo_effect_active = false;
            // No need to handle focus here; the cube logic does that when it deactivates.
        }
    } else {
        // If the cube is off, the expo MUST be on (our default state).
        if (!server->expo_effect_active) {
            server->expo_effect_active = true;
        }
    }

    // Stage 1: Prepare textures for multi-desktop effects
    if (effects_frame_fbo_path) {
        struct tinywl_toplevel *toplevel;
        wl_list_for_each(toplevel, &server->toplevels, link) {
            if (toplevel->scene_tree) {
                wlr_scene_node_set_enabled(&toplevel->scene_tree->node, toplevel->scale > 0.01);
            }
        }
        // IMPROVED: Better conditional rendering logic from second version
        if (server->effect_anim_current_factor < 2.0 || switch_mode == 1) {
            // Bind all desktop textures
            for (int i = 0; i < server->num_desktops; ++i) {
                if (!setup_intermediate_framebuffer(server, wlr_output->width, wlr_output->height, i)) {
                    wlr_log(WLR_ERROR, "Failed to setup desktop FBO %d. Skipping.", i);
                    continue;
                }
                glBindFramebuffer(GL_FRAMEBUFFER, server->desktop_fbos[i]);
                glViewport(0, 0, server->intermediate_width[i], server->intermediate_height[i]);
              
                // FIX 8: Clear with transparent black for individual desktop FBOs
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent, not opaque
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
              
                // FIX 9: Ensure blending is enabled for desktop content rendering
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
              
                struct render_data rdata = {
                    .renderer = renderer,
                    .server = server,
                    .output = wlr_output,
                    .pass = NULL,
                    .desktop_index = i
                };
                render_desktop_content(server, wlr_output, &rdata, i);
                glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo);
            }
        } else if (server->effect_anim_current_factor == 2.0 && switch_mode == 0) {
            // IMPROVED: Single desktop rendering optimization from second version
            int target_quadrant = server->pending_desktop_switch != -1 ?
                                server->pending_desktop_switch : server->current_desktop;
               
            if (!setup_intermediate_framebuffer(server, wlr_output->width, wlr_output->height, target_quadrant)) {
                wlr_log(WLR_ERROR, "Failed to setup desktop FBO %d. Aborting frame.", target_quadrant);
                wlr_output_commit(wlr_output);
                wlr_egl_unset_current(egl);
                wlr_scene_output_send_frame_done(scene_output, &now);
                return;
            }
            glBindFramebuffer(GL_FRAMEBUFFER, server->desktop_fbos[target_quadrant]);
            glViewport(0, 0, server->intermediate_width[target_quadrant], server->intermediate_height[target_quadrant]);
          
            // Clear with transparent black for individual desktop FBOs
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent, not opaque
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          
            // Ensure blending is enabled for desktop content rendering
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          
            struct render_data rdata = {
                .renderer = renderer,
                .server = server,
                .output = wlr_output,
                .pass = NULL,
                .desktop_index = target_quadrant
            };
            render_desktop_content(server, wlr_output, &rdata, target_quadrant);
            glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo);
        }
        wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);
        // Render expo effect using GL (since we have GL textures)
        glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo); // Bind default FBO for screen
        glViewport(0, 0, wlr_output->width, wlr_output->height);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Render effects based on which one is active
        if (server->expo_effect_active) {
            glBindVertexArray(server->quad_vao);
            glUseProgram(server->fullscreen_shader_program);
            glUniform1f(server->fullscreen_shader_zoom_loc, server->effect_anim_current_factor);
            glUniform1f(server->fullscreen_shader_move_loc, server->effect_anim_current_factor);
            int target_quadrant = server->pending_desktop_switch != -1 ? server->pending_desktop_switch : server->current_desktop;
            glUniform1i(server->fullscreen_shader_quadrant_loc, target_quadrant);
            glUniform1i(server->fullscreen_switch_mode, switch_mode);
            glUniform1i(server->fullscreen_switchXY, switchXY);
            // IMPROVED: Better texture binding logic
            if (server->effect_anim_current_factor < 2.0 || switch_mode == 1) {
                // Bind all desktop textures
                for (int i = 0; i < server->num_desktops; ++i) {
                    glActiveTexture(GL_TEXTURE0 + i);
                    glBindTexture(GL_TEXTURE_2D, server->intermediate_texture[i]);
                    glUniform1i(server->fullscreen_shader_scene_tex0_loc + i, i);
                }
            } else if (server->effect_anim_current_factor == 2.0 && switch_mode == 0) {
                // Bind only the target desktop texture
                glActiveTexture(GL_TEXTURE0 + target_quadrant);
                glBindTexture(GL_TEXTURE_2D, server->intermediate_texture[target_quadrant]);
                glUniform1i(server->fullscreen_shader_scene_tex0_loc + target_quadrant, target_quadrant);
            }
            glUniform1i(server->fullscreen_shader_desktop_grid_loc, DestopGridSize);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glUseProgram(0);
            glBindVertexArray(0);
        } else if (server->cube_effect_active) {
            // Cube effect rendering (added from second version)
            float now_float_sec = get_monotonic_time_seconds_as_float();
            start_quadrant_animation(server->current_desktop, now_float_sec);
            float animated_rotation = update_rotation_animation(server, now_float_sec);
            last_rendered_rotation = animated_rotation;

            // Interpolate vertical offset
            if (fabs(GLOBAL_vertical_offset - server->target_vertical_offset) > 0.01f) {
                // A new target has been set. Start a new animation.
                // The animation begins from wherever the cube *currently is visually*.
                server->start_vertical_offset = server->current_interpolated_vertical_offset;
                
                // Lock in the new destination.
                server->target_vertical_offset = GLOBAL_vertical_offset;
                
                // Reset the timer and flag that we are animating.
                server->vertical_offset_animation_start_time = now_float_sec;
                server->vertical_offset_animating = true;
            }

            // If an animation is in progress, calculate its current state.
            if (server->vertical_offset_animating) {
                float animation_duration = 0.4f; // A nice, smooth duration
                float elapsed = now_float_sec - server->vertical_offset_animation_start_time;
                float t = elapsed / animation_duration;

                if (t >= 1.0f) {
                    // Animation is finished. Snap to the final position.
                    server->current_interpolated_vertical_offset = server->target_vertical_offset;
                    server->vertical_offset_animating = false;
                } else {
                    // Animation is ongoing. Use a smooth easing curve for a professional feel.
                    float eased_t = 1.0f - pow(1.0f - t, 3.0f); // Ease-out cubic

                    // Interpolate from the fixed start point to the fixed target.
                    server->current_interpolated_vertical_offset =
                        server->start_vertical_offset + (server->target_vertical_offset - server->start_vertical_offset) * eased_t;
                }
            }

            // Background rendering with proper alpha
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);  // Ensure blending for background
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            glUseProgram(server->cube_background_shader_program);
            glUniform1f(server->cube_background_shader_time_loc, now_float_sec);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            // Cube rendering with depth and alpha
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_GREATER);  
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);     // Cull back faces
            glFrontFace(GL_CW);     // Counter-clockwise is front (OpenGL default) 

            // Keep blending enabled for cube faces
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            glBindVertexArray(server->cube_vao);
            glUseProgram(server->cube_shader_program);

            glUniform1f(server->cube_shader_time_loc, animated_rotation);
            glUniform1f(server->cube_shader_zoom_loc, server->cube_anim_current_factor);
            glUniform1i(server->cube_shader_quadrant_loc, server->current_desktop);

            if (server->cube_shader_resolution_loc != -1) {
                glUniform2f(server->cube_shader_resolution_loc, (float)wlr_output->width, (float)wlr_output->width);
            }

             // Loop in reverse order - draw from back to front for proper depth
            for(int loop = 3; loop >= 0; --loop) {
                for(int i = 0; i < server->num_desktops/4; ++i) {
                    glActiveTexture(GL_TEXTURE0 + i);
                    glBindTexture(GL_TEXTURE_2D, server->intermediate_texture[i+(loop*4)]);
                    glUniform1i(server->cube_shader_scene_tex0_loc + i, i);
                }

                float vertical_offset = (float)loop * -1.2f; // Local offset for each cube
                glUniform1f(server->cube_shader_vertical_offset_loc, vertical_offset);
                
                // Use interpolated global vertical offset for smooth animation
                glUniform1f(server->cube_shader_global_vertical_offset_loc, server->current_interpolated_vertical_offset);
                
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
            // Schedule next frame if still animating
            if (server->vertical_offset_animating) {
                struct tinywl_output *output;
                wl_list_for_each(output, &server->outputs, link) {
                    if (output->wlr_output && output->wlr_output->enabled) {
                        wlr_output_schedule_frame(output->wlr_output);
                    }
                }
            }
        }
        
        wlr_renderer_end(renderer);
        // Commit after direct GL rendering in effects path
        wlr_output_commit(wlr_output);
        wlr_egl_unset_current(egl);
        wlr_scene_output_send_frame_done(scene_output, &now);
        return;
    } else {
        // Begin render pass for normal rendering
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        pixman_region32_t damage;
        pixman_region32_init_rect(&damage, 0, 0, wlr_output->width, wlr_output->height);
        wlr_output_state_set_damage(&state, &damage);
        pixman_region32_fini(&damage);
        struct wlr_render_pass *pass = wlr_output_begin_render_pass(wlr_output, &state, &buffer_age, NULL);
        if (!pass) {
            wlr_log(WLR_ERROR, "[%s] Failed to begin render pass", wlr_output->name);
            wlr_output_rollback(wlr_output);
            wlr_egl_unset_current(egl);
            wlr_scene_output_send_frame_done(scene_output, &now);
            return;
        }
        // Normal rendering to pass
        struct render_data rdata = {
            .renderer = renderer,
            .server = server,
            .output = wlr_output,
            .pass = pass,
            .desktop_index = server->current_desktop
        };
        render_desktop_content(server, wlr_output, &rdata, server->current_desktop);
        // Submit pass and commit
        wlr_render_pass_submit(pass);
        wlr_output_commit(wlr_output);
    }
    wlr_egl_unset_current(egl);
    wlr_scene_output_send_frame_done(scene_output, &now);
    // Schedule next frame
    wlr_output_schedule_frame(wlr_output);
}

static void output_request_state(struct wl_listener *listener, void *data) {
    struct tinywl_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    wlr_output_commit_state(output->wlr_output, event->state);
}

static void output_destroy(struct wl_listener *listener, void *data) {
    struct tinywl_output *output = wl_container_of(listener, output, destroy);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    free(output);
}

// Helper function to disable/destroy SSDs
static void ensure_ssd_disabled(struct tinywl_toplevel *toplevel) {
    const char *title = (toplevel->xdg_toplevel && toplevel->xdg_toplevel->title) ? toplevel->xdg_toplevel->title : "N/A";
    wlr_log(WLR_INFO, "[SSD_HELPER] Enter ensure_ssd_disabled for toplevel %s. Current ssd.enabled: %d",
            title, toplevel->ssd.enabled);

    if (!toplevel->ssd.enabled) {
        wlr_log(WLR_DEBUG, "[SSD_HELPER] SSDs already disabled for %s", title);
        return;
    }
    wlr_log(WLR_INFO, "[SSD_HELPER] Disabling and destroying SSDs for %s.", title);

    if (toplevel->ssd.title_bar) { wlr_scene_node_destroy(&toplevel->ssd.title_bar->node); toplevel->ssd.title_bar = NULL; }
    if (toplevel->ssd.border_left) { wlr_scene_node_destroy(&toplevel->ssd.border_left->node); toplevel->ssd.border_left = NULL; }
    if (toplevel->ssd.border_right) { wlr_scene_node_destroy(&toplevel->ssd.border_right->node); toplevel->ssd.border_right = NULL; }
    if (toplevel->ssd.border_bottom) { wlr_scene_node_destroy(&toplevel->ssd.border_bottom->node); toplevel->ssd.border_bottom = NULL; }
    
    toplevel->ssd.enabled = false;

    if (toplevel->client_xdg_scene_tree) {
        wlr_log(WLR_INFO, "[SSD_HELPER] Resetting client_xdg_scene_tree position for %s to (0,0).", title);
        wlr_scene_node_set_position(&toplevel->client_xdg_scene_tree->node, 0, 0);
    }
}

// Add these constants near the top of your file if you haven't already
#define BUTTON_SIZE 18
#define BUTTON_PADDING 5

static void update_decoration_geometry(struct tinywl_toplevel *toplevel) {
    const char *title = (toplevel && toplevel->xdg_toplevel && toplevel->xdg_toplevel->title) ?
                        toplevel->xdg_toplevel->title : "N/A";

    if (!toplevel || !toplevel->client_xdg_scene_tree ||
        !toplevel->xdg_toplevel || !toplevel->xdg_toplevel->base ||
        !toplevel->xdg_toplevel->base->surface) {
        wlr_log(WLR_DEBUG, "[UPDATE_DECO:%s] Pre-conditions not met.", title);
        return;
    }

    if (!toplevel->ssd.enabled) {
        wlr_log(WLR_DEBUG, "[UPDATE_DECO:%s] SSDs not enabled. Client content at (0,0).", title);
        wlr_scene_node_set_position(&toplevel->client_xdg_scene_tree->node, 0, 0);
        if (toplevel->ssd.title_bar) wlr_scene_node_set_enabled(&toplevel->ssd.title_bar->node, false);
        if (toplevel->ssd.border_left) wlr_scene_node_set_enabled(&toplevel->ssd.border_left->node, false);
        if (toplevel->ssd.border_right) wlr_scene_node_set_enabled(&toplevel->ssd.border_right->node, false);
        if (toplevel->ssd.border_bottom) wlr_scene_node_set_enabled(&toplevel->ssd.border_bottom->node, false);
        if (toplevel->ssd.close_button) wlr_scene_node_set_enabled(&toplevel->ssd.close_button->node, false);
        if (toplevel->ssd.maximize_button) wlr_scene_node_set_enabled(&toplevel->ssd.maximize_button->node, false);
        return;
    }

    struct wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel;
    int actual_content_width = 0;
    int actual_content_height = 0;
/*
    if (xdg_toplevel->base->geometry.width > 0 && xdg_toplevel->base->geometry.height > 0) {
        actual_content_width = xdg_toplevel->base->geometry.width;
        actual_content_height = xdg_toplevel->base->geometry.height;
    } else {
        actual_content_width = xdg_toplevel->current.width;
        actual_content_height = xdg_toplevel->current.height;
    }
*/
    if (actual_content_width <= 0 || actual_content_height <= 0) {
        wlr_log(WLR_DEBUG, "[UPDATE_DECO:%s] Final effective content size is %dx%d. Hiding SSDs.",
                title, actual_content_width, actual_content_height);
        if (toplevel->ssd.title_bar) wlr_scene_node_set_enabled(&toplevel->ssd.title_bar->node, false);
        if (toplevel->ssd.border_left) wlr_scene_node_set_enabled(&toplevel->ssd.border_left->node, false);
        if (toplevel->ssd.border_right) wlr_scene_node_set_enabled(&toplevel->ssd.border_right->node, false);
        if (toplevel->ssd.border_bottom) wlr_scene_node_set_enabled(&toplevel->ssd.border_bottom->node, false);
        if (toplevel->ssd.close_button) wlr_scene_node_set_enabled(&toplevel->ssd.close_button->node, false);
        if (toplevel->ssd.maximize_button) wlr_scene_node_set_enabled(&toplevel->ssd.maximize_button->node, false);
        wlr_scene_node_set_position(&toplevel->client_xdg_scene_tree->node, 0, 0);
        return;
    }

    wlr_log(WLR_INFO, "[UPDATE_DECO:%s] Sizing SSDs around content of %dx%d",
            title, actual_content_width, actual_content_height);

    if (!toplevel->ssd.title_bar || !toplevel->ssd.border_left ||
        !toplevel->ssd.border_right || !toplevel->ssd.border_bottom ||
        !toplevel->ssd.close_button || !toplevel->ssd.maximize_button) {
        wlr_log(WLR_ERROR, "[UPDATE_DECO:%s] SSD rects are NULL even though SSD is enabled. Re-creating.", title);
        ensure_ssd_enabled(toplevel);
        return;
    }
    
    wlr_scene_node_set_enabled(&toplevel->ssd.title_bar->node, true);
    wlr_scene_node_set_enabled(&toplevel->ssd.border_left->node, true);
    wlr_scene_node_set_enabled(&toplevel->ssd.border_right->node, true);
    wlr_scene_node_set_enabled(&toplevel->ssd.border_bottom->node, true);

    int ssd_total_width = actual_content_width + 2 * BORDER_WIDTH;

    // --- BORDERS AND TITLE BAR ---
    wlr_scene_rect_set_size(toplevel->ssd.title_bar, ssd_total_width, TITLE_BAR_HEIGHT);
    wlr_scene_node_set_position(&toplevel->ssd.title_bar->node, 0, 0);

    wlr_scene_rect_set_size(toplevel->ssd.border_left, BORDER_WIDTH, actual_content_height);
    wlr_scene_node_set_position(&toplevel->ssd.border_left->node, 0, TITLE_BAR_HEIGHT);

    wlr_scene_rect_set_size(toplevel->ssd.border_right, BORDER_WIDTH, actual_content_height);
    wlr_scene_node_set_position(&toplevel->ssd.border_right->node, BORDER_WIDTH + actual_content_width, TITLE_BAR_HEIGHT);

    wlr_scene_rect_set_size(toplevel->ssd.border_bottom, ssd_total_width, BORDER_WIDTH);
    wlr_scene_node_set_position(&toplevel->ssd.border_bottom->node, 0, TITLE_BAR_HEIGHT + actual_content_height);

     // --- BUTTON POSITIONING ---
    int button_y = (TITLE_BAR_HEIGHT - BUTTON_SIZE) / 2;
    int close_button_x = ssd_total_width - BORDER_WIDTH - BUTTON_PADDING - BUTTON_SIZE;
    wlr_scene_rect_set_size(toplevel->ssd.close_button, BUTTON_SIZE, BUTTON_SIZE);
    wlr_scene_node_set_position(&toplevel->ssd.close_button->node, close_button_x, button_y);
    wlr_scene_node_set_enabled(&toplevel->ssd.close_button->node, true);


    
    int maximize_button_x = close_button_x - BUTTON_PADDING - BUTTON_SIZE;
    wlr_scene_rect_set_size(toplevel->ssd.maximize_button, BUTTON_SIZE, BUTTON_SIZE);
    wlr_scene_node_set_position(&toplevel->ssd.maximize_button->node, maximize_button_x, button_y);
    wlr_scene_node_set_enabled(&toplevel->ssd.maximize_button->node, true);

// Minimize button (to the left of maximize) - NEW
    int minimize_button_x = maximize_button_x - BUTTON_PADDING - BUTTON_SIZE;
    wlr_scene_rect_set_size(toplevel->ssd.minimize_button, BUTTON_SIZE, BUTTON_SIZE);
    wlr_scene_node_set_position(&toplevel->ssd.minimize_button->node, minimize_button_x, button_y);
    wlr_scene_node_set_enabled(&toplevel->ssd.minimize_button->node, true);


    // --- CLIENT CONTENT POSITIONING ---
    wlr_scene_node_set_position(&toplevel->client_xdg_scene_tree->node, BORDER_WIDTH, TITLE_BAR_HEIGHT);

    // --- Z-ORDERING (THE REAL FIX) ---
    // 1. Lower the client content to the bottom of the stack. It will be drawn first.
    wlr_scene_node_lower_to_bottom(&toplevel->client_xdg_scene_tree->node);
    
    // 2. Now, raise the buttons so they are drawn on top of everything else (including the title bar).
    wlr_scene_node_raise_to_top(&toplevel->ssd.minimize_button->node);
    wlr_scene_node_raise_to_top(&toplevel->ssd.maximize_button->node);
    wlr_scene_node_raise_to_top(&toplevel->ssd.close_button->node);
}
/* Updated server_new_output without damage */

static void server_new_output(struct wl_listener *listener, void *data) {
    struct tinywl_server *server =
        wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    struct tinywl_output *output_wrapper_iter;
    wl_list_for_each(output_wrapper_iter, &server->outputs, link) {
        if (output_wrapper_iter->wlr_output == wlr_output) {
            wlr_log(WLR_INFO, "server_new_output: Output %s (ptr %p) already processed, skipping.",
                    wlr_output->name ? wlr_output->name : "(null)", (void*)wlr_output);
            return;
        }
    }

    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: About to set transform. WL_OUTPUT_TRANSFORM_FLIPPED_180 is defined as: %d.", WL_OUTPUT_TRANSFORM_FLIPPED_180);
  //  wlr_output_state_set_transform(&state, WL_OUTPUT_TRANSFORM_FLIPPED_180);
wlr_output_state_set_transform(&state, WL_OUTPUT_TRANSFORM_NORMAL);
wlr_output_set_custom_mode(wlr_output, 1024.0, 768,0);
    if (!wlr_output_commit_state(wlr_output, &state)) {
        wlr_log(WLR_ERROR, "Failed to commit output state for %s", wlr_output->name);
        wlr_output_state_finish(&state);
        return;
    }
    wlr_output_state_finish(&state);

    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: After commit, actual wlr_output->transform is: %d.", wlr_output->transform);
    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: WL_OUTPUT_TRANSFORM_NORMAL is %d", WL_OUTPUT_TRANSFORM_NORMAL);
    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: WL_OUTPUT_TRANSFORM_90 is %d", WL_OUTPUT_TRANSFORM_90);
    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: WL_OUTPUT_TRANSFORM_180 is %d", WL_OUTPUT_TRANSFORM_180);
    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: WL_OUTPUT_TRANSFORM_270 is %d", WL_OUTPUT_TRANSFORM_270);
    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: WL_OUTPUT_TRANSFORM_FLIPPED is %d", WL_OUTPUT_TRANSFORM_FLIPPED);
    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: WL_OUTPUT_TRANSFORM_FLIPPED_90 is %d", WL_OUTPUT_TRANSFORM_FLIPPED_90);
    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: WL_OUTPUT_TRANSFORM_FLIPPED_180 is %d (This is the one we used for set_transform)", WL_OUTPUT_TRANSFORM_FLIPPED_180);
    wlr_log(WLR_ERROR, "SERVER_NEW_OUTPUT: WL_OUTPUT_TRANSFORM_FLIPPED_270 is %d", WL_OUTPUT_TRANSFORM_FLIPPED_270);


    struct tinywl_output *output = calloc(1, sizeof(*output));
    if (!output) {
        wlr_log(WLR_ERROR, "Failed to allocate tinywl_output for %s", wlr_output->name);
        return;
    }
    output->wlr_output = wlr_output;
    output->server = server;

    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);

    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wl_list_insert(&server->outputs, &output->link);

    struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout,
        wlr_output);
    if (!l_output) {
         wlr_log(WLR_ERROR, "Failed to add output %s to layout", wlr_output->name);
         wl_list_remove(&output->frame.link);
         wl_list_remove(&output->request_state.link);
         wl_list_remove(&output->destroy.link);
         wl_list_remove(&output->link);
         free(output);
         return;
    }

    struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
    if (!scene_output) {
        wlr_log(WLR_ERROR, "Failed to create scene output for %s", wlr_output->name);
        wlr_output_layout_remove(server->output_layout, wlr_output);
        wl_list_remove(&output->frame.link);
        wl_list_remove(&output->request_state.link);
        wl_list_remove(&output->destroy.link);
        wl_list_remove(&output->link);
        free(output);
        return;
    }
    output->scene_output = scene_output;

    wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);
    
    wlr_output_schedule_frame(output->wlr_output);
}


static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, map);
    struct wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel;
    const char *title = (xdg_toplevel->title) ? xdg_toplevel->title : "N/A";

    
    wlr_log(WLR_INFO, "[MAP:%s] Toplevel mapped. Starting geometry negotiation.", title);
    toplevel->mapped = true;

    // NEW: Explicitly set the initial state to not minimized.
    toplevel->mapped = true;
    toplevel->minimized = false; // <<< THIS IS THE KEY LINE
    toplevel->panel_preview_box = (struct wlr_box){0, 0, 0, 0};

    // Default content size if client provides no hints.
    int target_content_width = 800;
    int target_content_height = 600;
    bool size_from_client_geom = false;

    // Check if the client provided a preferred geometry.
  /*  struct wlr_box client_geom = xdg_toplevel->base->geometry;
    if (client_geom.width > 0 && client_geom.height > 0) {
        target_content_width = client_geom.width;
        target_content_height = client_geom.height;
        size_from_client_geom = true;
        wlr_log(WLR_DEBUG, "[MAP:%s] Client suggested initial geometry: %dx%d", title, target_content_width, target_content_height);
    }
*/
    // --- CRITICAL FIX: PREDICT IF SSDs WILL BE USED ---
    bool intend_server_decorations = true; // By default, we want to provide SSDs.

    // If a decoration object already exists and the client explicitly requested CSD, then we don't intend to draw SSDs.
    if (toplevel->decoration &&
        (toplevel->decoration->current.mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE ||
         toplevel->decoration->pending.mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE)) {
        intend_server_decorations = false;
    }

    // If we got a size from the client AND we intend to draw decorations,
    // we assume the client might have given us the *total window size*.
    // We derive the *content size* from that.
  /*  if (size_from_client_geom && intend_server_decorations) {
        int derived_content_width = client_geom.width - (2 * BORDER_WIDTH);
        int derived_content_height = client_geom.height - TITLE_BAR_HEIGHT - BORDER_WIDTH;

        // Sanity check: If the derived size is tiny, the client probably
        // meant the content size, not the total size.
        if (derived_content_width >= 50 && derived_content_height >= 50) {
            target_content_width = derived_content_width;
            target_content_height = derived_content_height;
            wlr_log(WLR_INFO, "[MAP:%s] Client suggested total %dx%d. Server intends SSDs. Derived target CONTENT size: %dx%d.",
                    title, client_geom.width, client_geom.height, target_content_width, target_content_height);
        } else {
             wlr_log(WLR_INFO, "[MAP:%s] Derived content size %dx%d was too small. Assuming client's suggestion was for content.",
                    title, derived_content_width, derived_content_height);
        }
    }
*/
    // Now, set the final CONTENT size. The subsequent commit will handle creating
    // the decoration visuals around this correctly-sized content area.
    wlr_log(WLR_INFO, "[MAP:%s] Setting final XDG toplevel CONTENT size to %dx%d.", title, target_content_width, target_content_height);
    wlr_xdg_toplevel_set_size(xdg_toplevel, target_content_width, target_content_height);

    focus_toplevel(toplevel);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);
    const char *title = (toplevel->xdg_toplevel && toplevel->xdg_toplevel->title) ? toplevel->xdg_toplevel->title : "N/A";
    
    // Mark as unmapped internally
    toplevel->mapped = false;
    
    // --- THIS IS THE FIX ---
    // If the window is unmapped in the middle of a minimize or maximize
    // animation, we should NOT immediately hide it. Let the animation
    // continue to its end. The animation code is responsible for setting
    // the final visibility (e.g., hiding the node after a minimize anim).
    if (toplevel->is_minimizing || toplevel->is_maximizing) {
        wlr_log(WLR_INFO, "[UNMAP:%s] Unmapped during animation. Letting animation complete.", title);
        // By returning here, we keep the scene node enabled, allowing the
        // animation in scene_buffer_iterator to finish smoothly.
        return;
    }

    // --- Original logic for a standard, non-animated unmap ---
    wlr_log(WLR_INFO, "[UNMAP:%s] Standard unmap. Hiding scene node.", title);

    // Stop any other types of animations if you have them
    toplevel->is_animating = false;
    
    // Hide the window from the scene graph since it's not animating
    if (toplevel->scene_tree) {
        wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
    }
    
    // Schedule a frame to ensure the screen updates to the hidden state
    struct tinywl_output *output;
    wl_list_for_each(output, &toplevel->server->outputs, link) {
        if (output->wlr_output && output->wlr_output->enabled) {
            wlr_output_schedule_frame(output->wlr_output);
        }
    }
}





// Helper function to create a scene rect
// Modify the function signature to accept the toplevel
static struct wlr_scene_rect *create_decoration_rect(
    struct wlr_scene_tree *parent,
    struct tinywl_toplevel *toplevel, // <<< ADD THIS ARGUMENT
    int width, int height, int x, int y, float color[4]) {
    
    struct wlr_scene_rect *rect = wlr_scene_rect_create(parent, width, height, color);
    if (rect) {
        wlr_scene_node_set_position(&rect->node, x, y);
        wlr_scene_node_set_enabled(&rect->node, true);
        
        // --- THIS IS THE CRITICAL FIX ---
        // Store a pointer to the parent toplevel in the node's data field.
        rect->node.data = toplevel;
        wlr_log(WLR_DEBUG, "Created decoration rect %p, set its node->data to toplevel %p", (void*)rect, (void*)toplevel);

    } else {
        wlr_log(WLR_ERROR, "Failed to create decoration rect");
    }
    return rect;
}





static void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, commit);
    struct wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel;
    struct wlr_xdg_surface *xdg_surface = xdg_toplevel->base;
    struct wlr_surface *surface = xdg_surface->surface;
    struct tinywl_server *server = toplevel->server;
    const char *title = (xdg_toplevel->title) ? xdg_toplevel->title : "N/A";

    if (!surface) return;

    wlr_log(WLR_INFO, "[COMMIT:%s] START - Mapped:%d, Init:%d, XDGConf:%d, SSD_EN:%d, SSD_PEND:%d. DecoCurr:%d, DecoPend:%d. Buf:%p(%dx%d), Seq:%u. XDGCurrentSize:%dx%d",
            title, surface->mapped, xdg_surface->initialized, xdg_surface->configured,
            toplevel->ssd.enabled, toplevel->ssd_pending_enable,
            toplevel->decoration ? toplevel->decoration->current.mode : -1,
            toplevel->decoration ? toplevel->decoration->pending.mode : -1,
            (void*)surface->current.buffer, surface->current.buffer ? surface->current.buffer->width : 0, surface->current.buffer ? surface->current.buffer->height : 0,
            surface->current.seq, xdg_toplevel->current.width, xdg_toplevel->current.height);

    bool needs_surface_configure = false;
    enum wlr_xdg_toplevel_decoration_v1_mode server_preferred_mode =
        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;

    // If a decoration request is pending, now is the time to handle it.
    if (toplevel->decoration && toplevel->ssd_pending_enable) {
        // We only negotiate when the surface is ready (initialized or on its first commit).
        if (xdg_surface->initialized || xdg_surface->initial_commit) {
            wlr_log(WLR_INFO, "[COMMIT:%s] ssd_pending_enable=true. Setting preferred mode: %d.", title, server_preferred_mode);
            wlr_xdg_toplevel_decoration_v1_set_mode(toplevel->decoration, server_preferred_mode);
            needs_surface_configure = true; // MUST configure after setting mode.
            toplevel->ssd_pending_enable = false;
        }
    }

    // Now, make a decision based on the current, stable decoration mode.
    enum wlr_xdg_toplevel_decoration_v1_mode effective_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE;
    if (toplevel->decoration) {
        effective_mode = toplevel->decoration->current.mode;
    }

    if (effective_mode == server_preferred_mode) {
        // The protocol state is SERVER_SIDE. Check if our internal state matches.
        if (!toplevel->ssd.enabled) {
            wlr_log(WLR_INFO, "[COMMIT:%s] Effective mode is SERVER_SIDE. Enabling SSDs.", title);
            ensure_ssd_enabled(toplevel);
            // This geometry change requires a configure.
            needs_surface_configure = true;
        }
    } else {
        // The protocol state is CLIENT_SIDE or NONE.
        if (toplevel->ssd.enabled) {
            wlr_log(WLR_INFO, "[COMMIT:%s] Effective mode is %d (NOT SERVER_SIDE). Disabling SSDs.", title, effective_mode);
            ensure_ssd_disabled(toplevel);
            // This geometry change requires a configure.
            needs_surface_configure = true;
        }
    }

    if (toplevel->ssd.enabled) {
        update_decoration_geometry(toplevel);
    }

    // If we determined a configure is needed, or if the surface is still unconfigured, schedule it now.
    if (needs_surface_configure || (xdg_surface->initialized && !xdg_surface->configured)) {
        wlr_log(WLR_INFO, "[COMMIT:%s] Scheduling configure for xdg_surface.", title);
        wlr_xdg_surface_schedule_configure(xdg_surface);
    }

    // Caching and redrawing logic.
    if (surface->current.buffer) {
        // A new, valid buffer was committed. Update the cache.
        if (!(toplevel->cached_texture && toplevel->last_commit_seq == surface->current.seq)) {
            if (toplevel->cached_texture) {
                wlr_texture_destroy(toplevel->cached_texture);
            }
            toplevel->cached_texture = wlr_texture_from_buffer(server->renderer, surface->current.buffer);
            toplevel->last_commit_seq = surface->current.seq;
        }
    } else {
        // A NULL buffer was committed. This happens on minimize.
        // *** THE FIX - PART 2 ***
        // We only destroy the texture if the window is neither minimized nor in the process of minimizing.
        if (toplevel->cached_texture && !toplevel->minimized && !toplevel->is_minimizing) {
            wlr_log(WLR_INFO, "[COMMIT:%s] Surface has NULL buffer and is not minimized/minimizing. Destroying cached texture.", title);
            wlr_texture_destroy(toplevel->cached_texture);
            toplevel->cached_texture = NULL;
        }
        toplevel->last_commit_seq = surface->current.seq;
    }
   if (surface->mapped && !toplevel->minimized && !toplevel->is_minimizing) {
        toplevel->restored_geom.x = toplevel->scene_tree->node.x;
        toplevel->restored_geom.y = toplevel->scene_tree->node.y;

        // --- THE FIX IS HERE ---
        // Get geometry directly from the struct member instead of the old function.
      /*  struct wlr_box content_geom = xdg_surface->geometry;

        if (toplevel->ssd.enabled) {
            toplevel->restored_geom.width = content_geom.width + 2 * BORDER_WIDTH;
            toplevel->restored_geom.height = content_geom.height + TITLE_BAR_HEIGHT + BORDER_WIDTH;
        } else {
            toplevel->restored_geom.width = content_geom.width;
            toplevel->restored_geom.height = content_geom.height;
        }*/
    }

    if (surface->mapped) {
        struct tinywl_output *output_iter;
        wl_list_for_each(output_iter, &server->outputs, link) {
            if (output_iter->wlr_output && output_iter->wlr_output->enabled) {
                wlr_output_schedule_frame(output_iter->wlr_output);
            }
        }
    }
}
static void decoration_handle_destroy(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel =
        wl_container_of(listener, toplevel, decoration_destroy_listener);

    if (!toplevel) return;
    struct wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel;

    wlr_log(WLR_INFO, "[DECO_OBJ_DESTROY] For toplevel %s, decoration %p is being destroyed.",
            xdg_toplevel->title ? xdg_toplevel->title : "N/A",
            (void*)toplevel->decoration);

    wl_list_remove(&toplevel->decoration_destroy_listener.link);
    if (toplevel->decoration_request_mode_listener.link.next) {
        wl_list_remove(&toplevel->decoration_request_mode_listener.link);
    }
    toplevel->decoration = NULL;
    toplevel->ssd_pending_enable = false;

    ensure_ssd_disabled(toplevel);

    if (xdg_toplevel->base->initialized) {
        wlr_log(WLR_INFO, "[DECO_OBJ_DESTROY] Toplevel %s: Scheduling configure for main xdg_surface.",
                xdg_toplevel->title);
        wlr_xdg_surface_schedule_configure(xdg_toplevel->base);
    }
}

// Modified xdg_toplevel_destroy to clean up SSD
static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);
    const char *title = toplevel->xdg_toplevel && toplevel->xdg_toplevel->title ? toplevel->xdg_toplevel->title : "N/A";

    wlr_log(WLR_INFO, "[DESTROY:%s] Destroying toplevel %p", title, (void*)toplevel);

    // Clean up cached texture
    if (toplevel->cached_texture) {
        wlr_texture_destroy(toplevel->cached_texture);
        toplevel->cached_texture = NULL;
    }

    // Clean up decoration listeners
    if (toplevel->decoration) {
        wl_list_remove(&toplevel->decoration_destroy_listener.link);
        wl_list_remove(&toplevel->decoration_request_mode_listener.link);
        toplevel->decoration = NULL;
    }

    // Remove other listeners
    wl_list_remove(&toplevel->map.link);
    wl_list_remove(&toplevel->unmap.link);
    wl_list_remove(&toplevel->commit.link);
    wl_list_remove(&toplevel->destroy.link);
    wl_list_remove(&toplevel->request_move.link);
    wl_list_remove(&toplevel->request_resize.link);
    wl_list_remove(&toplevel->request_maximize.link);
    wl_list_remove(&toplevel->request_fullscreen.link);

    // Destroy scene tree (includes SSD elements)
    if (toplevel->scene_tree) {
        wlr_scene_node_destroy(&toplevel->scene_tree->node);
        toplevel->scene_tree = NULL;
        toplevel->client_xdg_scene_tree = NULL;
        toplevel->ssd.title_bar = NULL;
        toplevel->ssd.border_left = NULL;
        toplevel->ssd.border_right = NULL;
        toplevel->ssd.border_bottom = NULL;
    }

    wl_list_remove(&toplevel->link);
    free(toplevel);
}

static void begin_interactive(struct tinywl_toplevel *toplevel, enum tinywl_cursor_mode mode, uint32_t edges) {
    struct tinywl_server *server = toplevel->server;
    server->grabbed_toplevel = toplevel;
    server->cursor_mode = mode;

    if (mode == TINYWL_CURSOR_MOVE) {
        server->grab_x = server->cursor->x - toplevel->scene_tree->node.x;
        server->grab_y = server->cursor->y - toplevel->scene_tree->node.y;
    } else {
 /*       struct wlr_box *geo_box = &toplevel->xdg_toplevel->base->geometry;
        double border_x = (toplevel->scene_tree->node.x + geo_box->x) +
            ((edges & WLR_EDGE_RIGHT) ? geo_box->width : 0);
        double border_y = (toplevel->scene_tree->node.y + geo_box->y) +
            ((edges & WLR_EDGE_BOTTOM) ? geo_box->height : 0);
        server->grab_x = server->cursor->x - border_x;
        server->grab_y = server->cursor->y - border_y;
        server->grab_geobox = *geo_box;
        server->grab_geobox.x += toplevel->scene_tree->node.x;
        server->grab_geobox.y += toplevel->scene_tree->node.y;
        server->resize_edges = edges;
   */ }
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_move);
    begin_interactive(toplevel, TINYWL_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_resize_event *event = data;
    struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
    begin_interactive(toplevel, TINYWL_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_maximize);
    // The event tells us the new desired state.
    bool new_maximized_state = toplevel->xdg_toplevel->requested.maximized;
   // toggle_maximize_toplevel(toplevel, new_maximized_state);
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_fullscreen);
    if (toplevel->xdg_toplevel->base->initialized) {
        wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
    }
}



static void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, new_xdg_toplevel);
    struct wlr_xdg_toplevel *xdg_toplevel = data;

    wlr_log(WLR_INFO, "New XDG Toplevel (app_id: %s, title: %s): %p",
            xdg_toplevel->app_id ? xdg_toplevel->app_id : "N/A",
            xdg_toplevel->title ? xdg_toplevel->title : "N/A",
            (void*)xdg_toplevel);

    if (!xdg_toplevel || !xdg_toplevel->base || !xdg_toplevel->base->surface) {
        wlr_log(WLR_ERROR, "server_new_xdg_toplevel: Invalid xdg_toplevel or base components.");
        if (xdg_toplevel && xdg_toplevel->resource) {
            wl_resource_post_no_memory(xdg_toplevel->resource);
        }
        return;
    }

    struct tinywl_toplevel *toplevel = calloc(1, sizeof(struct tinywl_toplevel));
    if (!toplevel) {
        wlr_log(WLR_ERROR, "server_new_xdg_toplevel: Failed to allocate tinywl_toplevel");
        wl_resource_post_no_memory(xdg_toplevel->resource);
        return;
    }

    toplevel->desktop = server->current_desktop; // Assign to current desktop
    wlr_log(WLR_INFO, "New toplevel created on desktop %d", toplevel->desktop);

    toplevel->server = server;
    toplevel->xdg_toplevel = xdg_toplevel;
    wl_list_init(&toplevel->link);
    toplevel->scale = 1.0f;
    toplevel->target_scale = 1.0f;
    toplevel->is_animating = false;

    toplevel->decoration = NULL;
    toplevel->ssd.enabled = false;
    toplevel->ssd_pending_enable = false; // Initialize to false
    toplevel->ssd.title_bar = NULL;
    toplevel->ssd.border_left = NULL;
    toplevel->ssd.border_right = NULL;
    toplevel->ssd.border_bottom = NULL;

    xdg_toplevel->base->data = toplevel;

    toplevel->scene_tree = wlr_scene_tree_create(&server->scene->tree);
    if (!toplevel->scene_tree) {
        wlr_log(WLR_ERROR, "Failed to create scene_tree for %p", (void*)xdg_toplevel);
        free(toplevel);
        xdg_toplevel->base->data = NULL;
        wl_resource_post_no_memory(xdg_toplevel->resource);
        return;
    }
   toplevel->scene_tree->node.data = toplevel;

   

    // Create the client content tree as a sibling to the decoration tree
    toplevel->client_xdg_scene_tree = wlr_scene_xdg_surface_create(
        toplevel->scene_tree, xdg_toplevel->base);

   
    if (!toplevel->client_xdg_scene_tree) {
        wlr_log(WLR_ERROR, "Failed to create client_xdg_scene_tree for %p", (void*)xdg_toplevel);
        wlr_scene_node_destroy(&toplevel->scene_tree->node);
        free(toplevel);
        xdg_toplevel->base->data = NULL;
        wl_resource_post_no_memory(xdg_toplevel->resource);
        return;
    }

    toplevel->map.notify = xdg_toplevel_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
    toplevel->unmap.notify = xdg_toplevel_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
    toplevel->commit.notify = xdg_toplevel_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);
    toplevel->destroy.notify = xdg_toplevel_destroy;
  //  wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);
  //  toplevel->request_move.notify = xdg_toplevel_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
    toplevel->request_resize.notify = xdg_toplevel_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
    toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
    toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);

    wl_list_init(&toplevel->decoration_destroy_listener.link);
    wl_list_init(&toplevel->decoration_request_mode_listener.link);

    wlr_log(WLR_INFO, "Toplevel %p (title: %s) wrapper created. SSDs NOT enabled. Waiting for xdg-decoration.",
            (void*)toplevel, xdg_toplevel->title ? xdg_toplevel->title : "N/A");
}
/*
void update_toplevel_visibility(struct tinywl_server *server) {
    wlr_log(WLR_DEBUG, "Updating toplevel visibility. Current desktop: %d, Expo active: %d",
            server->current_desktop, server->expo_effect_active);

    struct tinywl_toplevel *toplevel;
    wl_list_for_each(toplevel, &server->toplevels, link) {
        if (toplevel->scene_tree) {
            // A toplevel is visible if expo is on, OR if its desktop matches the current one.
            bool visible = server->expo_effect_active || (toplevel->desktop == server->current_desktop);
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, visible);
            wlr_log(WLR_DEBUG, "  Toplevel on desktop %d is now %s.",
                    toplevel->desktop, visible ? "ENABLED" : "DISABLED");
        }
    }

    // Schedule a frame on all outputs to reflect the change
    struct tinywl_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->wlr_output && output->wlr_output->enabled) {
            wlr_output_schedule_frame(output->wlr_output);
        }
    }
}*/
void update_toplevel_visibility(struct tinywl_server *server) {
    wlr_log(WLR_DEBUG, "Updating toplevel visibility. Current desktop: %d, Cube: %d, Expo: %d",
            server->current_desktop, server->cube_effect_active, server->expo_effect_active);

    struct tinywl_toplevel *toplevel;
    wl_list_for_each(toplevel, &server->toplevels, link) {
        if (toplevel->scene_tree) {
            bool should_be_enabled;

            // --- THIS IS THE CRITICAL LOGIC ---
            if (server->expo_effect_active || server->cube_effect_active) {
                // If any multi-desktop effect is active, a window is visible
                // as long as it's not minimized. Its desktop property determines
                // *where* it's rendered, not *if* it's enabled.
                should_be_enabled = (toplevel->scale > 0.01f);
            } else {
                // If no effect is active, only show windows on the current desktop.
                should_be_enabled = (toplevel->desktop == server->current_desktop);
            }

            // Apply the calculated state.
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, should_be_enabled);
        }
    }

    // Schedule a frame on all outputs to reflect the change.
    struct tinywl_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->wlr_output && output->wlr_output->enabled) {
            wlr_output_schedule_frame(output->wlr_output);
        }
    }
}


// NEW: Centralized function to manage all state changes.
/**
 * The single source of truth for updating the compositor's state after a
 * major change, like switching desktops.
 */
void update_compositor_state(struct tinywl_server *server) {
    wlr_log(WLR_INFO, "--- Updating compositor state for desktop %d ---", server->current_desktop);

    // 1. Update which toplevels are visible in the scene graph.
    update_toplevel_visibility(server);

    // 2. Clear all focus from the seat. This is the crucial reset step.
    if (server->seat->keyboard_state.focused_surface) {
        wlr_seat_keyboard_clear_focus(server->seat);
    }
    wlr_seat_pointer_clear_focus(server->seat);

    // 3. Re-evaluate what the pointer is over. This function will now find
    // the correct topmost window on the new desktop and grant it pointer focus.
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint32_t time_msec = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    process_cursor_motion(server, time_msec);

    // 4. Schedule a frame on all outputs to render the changes.
    struct tinywl_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->wlr_output && output->wlr_output->enabled) {
            wlr_output_schedule_frame(output->wlr_output);
        }
    }
}

static void server_new_xdg_popup(struct wl_listener *listener, void *data) {
    if (!listener || !data) {
        wlr_log(WLR_ERROR, "Invalid listener=%p or data=%p in server_new_xdg_popup", listener, data);
        return;
    }

    struct tinywl_server *server = wl_container_of(listener, server, new_xdg_popup);
    if (!server || !server->wl_display || !server->xdg_shell || !server->scene) {
        wlr_log(WLR_ERROR, "Invalid server state: server=%p, display=%p, xdg_shell=%p, scene=%p",
                server, server ? server->wl_display : NULL, server ? server->xdg_shell : NULL, server ? server->scene : NULL);
        return;
    }

    struct wlr_xdg_popup *xdg_popup = data;
    wlr_log(WLR_INFO, "New XDG popup: %p, parent=%p, surface=%p, initial_commit=%d",
            xdg_popup, xdg_popup->parent, xdg_popup->base ? xdg_popup->base->surface : NULL,
            xdg_popup->base ? xdg_popup->base->initial_commit : 0);

    // Validate popup surface and role
    if (!xdg_popup->base || !xdg_popup->base->surface || xdg_popup->base->role != WLR_XDG_SURFACE_ROLE_POPUP) {
        wlr_log(WLR_ERROR, "Invalid popup: base=%p, surface=%p, role=%d",
                xdg_popup->base, xdg_popup->base ? xdg_popup->base->surface : NULL,
                xdg_popup->base ? (int)xdg_popup->base->role : -1);
        return;
    }

    // Validate parent surface
    struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
    if (!parent || !parent->surface) {
        wlr_log(WLR_ERROR, "No valid parent XDG surface: parent=%p, parent->surface=%p",
                parent, parent ? parent->surface : NULL);
        return;
    }

    // Ensure parent is a toplevel (popups cannot have popup parents)
    if (parent->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        wlr_log(WLR_ERROR, "Parent surface is not a toplevel: role=%d", parent->role);
        return;
    }

    // Validate parent scene tree
    struct wlr_scene_tree *parent_tree = parent->data;
    if (!parent_tree || parent_tree->node.type != WLR_SCENE_NODE_TREE) {
        wlr_log(WLR_ERROR, "Invalid parent scene tree: parent_tree=%p, type=%d",
                parent_tree, parent_tree ? (int)parent_tree->node.type : -1);
        return;
    }

    // Check parent surface state
    if (!parent->surface->mapped || !wlr_surface_has_buffer(parent->surface)) {
        wlr_log(WLR_ERROR, "Parent surface not ready: mapped=%d, has_buffer=%d",
                parent->surface->mapped, wlr_surface_has_buffer(parent->surface));
        return; // Defer until parent is ready
    }

    // Allocate popup structure
    struct tinywl_popup *popup = calloc(1, sizeof(struct tinywl_popup));
    if (!popup) {
        wlr_log(WLR_ERROR, "Failed to allocate tinywl_popup");
        return;
    }

    popup->xdg_popup = xdg_popup;
    popup->server = server;

    // Create scene tree for popup
    popup->scene_tree = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);
    if (!popup->scene_tree) {
        wlr_log(WLR_ERROR, "Failed to create scene tree for popup: xdg_surface=%p", xdg_popup->base);
        free(popup);
        return;
    }

    // Set data pointers
    popup->scene_tree->node.data = popup;
    xdg_popup->base->data = popup; // Store popup for consistency with toplevels

    // Set up event listeners
    popup->map.notify = popup_map;
    wl_signal_add(&xdg_popup->base->surface->events.map, &popup->map);
    popup->unmap.notify = popup_unmap;
    wl_signal_add(&xdg_popup->base->surface->events.unmap, &popup->unmap);
    popup->destroy.notify = popup_destroy;
   // wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
    //popup->commit.notify = popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

    // Add to server's popup list
    wl_list_insert(&server->popups, &popup->link);
    wlr_log(WLR_DEBUG, "Popup created: %p, parent=%p, scene_tree=%p, xdg_surface=%p",
            xdg_popup, parent->surface, popup->scene_tree, xdg_popup->base);

    // Schedule configure if needed
    if (xdg_popup->base->initial_commit) {
        wlr_xdg_surface_schedule_configure(xdg_popup->base);
        wlr_log(WLR_DEBUG, "Scheduled configure for popup %p", xdg_popup);
    }

    // Schedule frame
    struct tinywl_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->wlr_output && output->wlr_output->enabled) {
            wlr_output_schedule_frame(output->wlr_output);
        }
    }
}


#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif
#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif


void cleanup_egl(struct tinywl_server *server) {
    printf("Cleaning up EGL resources\n");
    if (server->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(server->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (server->egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(server->egl_display, server->egl_surface);
            server->egl_surface = EGL_NO_SURFACE;
        }
        if (server->egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(server->egl_display, server->egl_context);
            server->egl_context = EGL_NO_CONTEXT;
        }
        eglTerminate(server->egl_display);
        server->egl_display = EGL_NO_DISPLAY;
    }
}


void ensure_ssd_enabled(struct tinywl_toplevel *toplevel) {
    const char *title = (toplevel->xdg_toplevel && toplevel->xdg_toplevel->title) ? toplevel->xdg_toplevel->title : "N/A";
    wlr_log(WLR_INFO, "[SSD_HELPER] Enter ensure_ssd_enabled for toplevel %s. Current ssd.enabled: %d",
            title, toplevel->ssd.enabled);

    if (toplevel->ssd.enabled) {
        wlr_log(WLR_DEBUG, "[SSD_HELPER] SSDs already enabled for toplevel %s. Ensuring geometry update.", title);
        update_decoration_geometry(toplevel); // Call update to be sure
        return;
    }

    wlr_log(WLR_INFO, "[SSD_HELPER] Enabling SSDs and creating decoration rects for toplevel %s.", title);

    // Ensure old ones are cleared if somehow they exist while enabled is false
    if (toplevel->ssd.title_bar) { wlr_scene_node_destroy(&toplevel->ssd.title_bar->node); toplevel->ssd.title_bar = NULL; }
    if (toplevel->ssd.border_left) { wlr_scene_node_destroy(&toplevel->ssd.border_left->node); toplevel->ssd.border_left = NULL; }
    if (toplevel->ssd.border_right) { wlr_scene_node_destroy(&toplevel->ssd.border_right->node); toplevel->ssd.border_right = NULL; }
    if (toplevel->ssd.border_bottom) { wlr_scene_node_destroy(&toplevel->ssd.border_bottom->node); toplevel->ssd.border_bottom = NULL; }

 float title_bar_color[4] = {0.0f, 0.0f, 0.8f, 0.7f};
    float border_color[4]    = {0.1f, 0.1f, 0.1f, 0.7f};

     // NEW: Define button colors
     float minimize_button_color[4] = {1.0f, 1.0f, 0.6f, 1.0f}; // Yellow
     float maximize_button_color[4] = {0.6f, 1.0f, 0.6f, 1.0f}; // Green
     float close_button_color[4] = {1.0f, 0.6f, 0.6f, 1.0f}; // Red
   
    
// NEW: Create the button rectangles and associate them with the toplevel
   // Pass the toplevel pointer to the helper function
    toplevel->ssd.title_bar       = create_decoration_rect(toplevel->scene_tree, toplevel, 0, 0, 0, 0, title_bar_color);
    toplevel->ssd.border_left     = create_decoration_rect(toplevel->scene_tree, toplevel, 0, 0, 0, 0, border_color);
    toplevel->ssd.border_right    = create_decoration_rect(toplevel->scene_tree, toplevel, 0, 0, 0, 0, border_color);
    toplevel->ssd.border_bottom   = create_decoration_rect(toplevel->scene_tree, toplevel, 0, 0, 0, 0, border_color);
    toplevel->ssd.minimize_button = create_decoration_rect(toplevel->scene_tree, toplevel, 0, 0, 0, 0, minimize_button_color); 
    toplevel->ssd.close_button    = create_decoration_rect(toplevel->scene_tree, toplevel, 0, 0, 0, 0, close_button_color);
    toplevel->ssd.maximize_button = create_decoration_rect(toplevel->scene_tree, toplevel, 0, 0, 0, 0, maximize_button_color);

    // MODIFIED: Add minimize_button to the check
    if (!toplevel->ssd.title_bar || !toplevel->ssd.border_left ||
        !toplevel->ssd.border_right || !toplevel->ssd.border_bottom ||
        !toplevel->ssd.minimize_button || !toplevel->ssd.maximize_button || !toplevel->ssd.close_button) {
        
        wlr_log(WLR_ERROR, "[SSD_HELPER] FAILED TO CREATE SOME SSD ELEMENTS for %s", title);
        // Clean up any successfully created ones
        if (toplevel->ssd.title_bar) { wlr_scene_node_destroy(&toplevel->ssd.title_bar->node); toplevel->ssd.title_bar = NULL; }
        if (toplevel->ssd.border_left) { wlr_scene_node_destroy(&toplevel->ssd.border_left->node); toplevel->ssd.border_left = NULL; }
        if (toplevel->ssd.border_right) { wlr_scene_node_destroy(&toplevel->ssd.border_right->node); toplevel->ssd.border_right = NULL; }
        if (toplevel->ssd.border_bottom) { wlr_scene_node_destroy(&toplevel->ssd.border_bottom->node); toplevel->ssd.border_bottom = NULL; }
        if (toplevel->ssd.minimize_button) { wlr_scene_node_destroy(&toplevel->ssd.minimize_button->node); toplevel->ssd.minimize_button = NULL; } // NEW
        if (toplevel->ssd.maximize_button) { wlr_scene_node_destroy(&toplevel->ssd.maximize_button->node); toplevel->ssd.maximize_button = NULL; }
        if (toplevel->ssd.close_button) { wlr_scene_node_destroy(&toplevel->ssd.close_button->node); toplevel->ssd.close_button = NULL; }
        
        toplevel->ssd.enabled = false;
    } else {
        toplevel->ssd.enabled = true;
        wlr_log(WLR_INFO, "[SSD_HELPER] SSDs successfully created for %s. title_bar: %p, left: %p, right: %p, bottom: %p",
                title, (void*)toplevel->ssd.title_bar, (void*)toplevel->ssd.border_left,
                (void*)toplevel->ssd.border_right, (void*)toplevel->ssd.border_bottom);

        // Initial positioning of client content within the scene_tree (which is the overall frame)
        if (toplevel->client_xdg_scene_tree) {
            wlr_log(WLR_INFO, "[SSD_HELPER] Positioning client_xdg_scene_tree for %s to (%d, %d) relative to scene_tree.",
                    title, BORDER_WIDTH, TITLE_BAR_HEIGHT);
            wlr_scene_node_set_position(&toplevel->client_xdg_scene_tree->node, BORDER_WIDTH, TITLE_BAR_HEIGHT);
        }
        update_decoration_geometry(toplevel); // This will size/position everything correctly
    }
}





static void server_new_xdg_decoration(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, xdg_decoration_new);
    struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
    struct wlr_xdg_toplevel *xdg_toplevel = decoration->toplevel;
    struct tinywl_toplevel *toplevel = xdg_toplevel->base->data;

    const char *title_for_log = xdg_toplevel->title ? xdg_toplevel->title : "N/A";

    wlr_log(WLR_INFO, "[DECO_MGR] New decoration object %p for toplevel '%s'.",
            (void*)decoration, title_for_log);

    if (!toplevel) {
        wlr_log(WLR_ERROR, "[DECO_MGR] No tinywl_toplevel for new decoration on '%s'.", title_for_log);
        return;
    }

    // Clean up old listeners if a decoration is being replaced
    if (toplevel->decoration) {
        wl_list_remove(&toplevel->decoration_destroy_listener.link);
        wl_list_remove(&toplevel->decoration_request_mode_listener.link);
    }

    toplevel->decoration = decoration;
    toplevel->decoration_destroy_listener.notify = decoration_handle_destroy;
    wl_signal_add(&decoration->events.destroy, &toplevel->decoration_destroy_listener);
    toplevel->decoration_request_mode_listener.notify = decoration_handle_request_mode;
    wl_signal_add(&decoration->events.request_mode, &toplevel->decoration_request_mode_listener);

    // --- THE FIX ---
    // Instead of setting the mode here, just flag that we need to handle it.
    toplevel->ssd_pending_enable = true;
    wlr_log(WLR_INFO, "[DECO_MGR] Toplevel '%s': Marked ssd_pending_enable=true. Mode set will occur on next commit.", title_for_log);

    // If the surface is already up and running, we need to trigger a commit/configure
    // cycle to apply the new decoration state.
    if (xdg_toplevel->base->initialized) {
        wlr_log(WLR_DEBUG, "[DECO_MGR] Surface for '%s' is already initialized. Scheduling configure to process pending decoration.", title_for_log);
        wlr_xdg_surface_schedule_configure(xdg_toplevel->base);
    }
}

static void decoration_handle_request_mode(struct wl_listener *listener, void *data) {
    struct tinywl_toplevel *toplevel =
        wl_container_of(listener, toplevel, decoration_request_mode_listener);
    struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
    struct wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel;
    struct wlr_xdg_surface *xdg_surface = xdg_toplevel->base;

    const char *title = (xdg_toplevel && xdg_toplevel->title) ?
                        xdg_toplevel->title : "N/A";

    wlr_log(WLR_INFO, "[DECO_REQUEST_MODE:%s] Client requested mode: %d. Surface initialized: %d",
            title, decoration->requested_mode, xdg_surface->initialized);

    // CRITICAL CHANGE: Do NOT call wlr_xdg_toplevel_decoration_v1_set_mode() here directly
    // if the surface might not be initialized.
    // Set the flag, and let the commit handler call set_mode.
    toplevel->ssd_pending_enable = true;
    wlr_log(WLR_INFO, "[DECO_REQUEST_MODE:%s] Marked ssd_pending_enable = true. Mode set will be handled in commit.", title);

    // If the surface IS ALREADY initialized, schedule a configure for it.
    // The commit handler will then see ssd_pending_enable and call set_mode.
    if (xdg_surface->initialized) {
        wlr_log(WLR_DEBUG, "[DECO_REQUEST_MODE:%s] Surface is initialized. Scheduling configure for xdg_surface to process pending decoration.", title);
        wlr_xdg_surface_schedule_configure(xdg_surface);
    }
}








static void handle_xdg_decoration_new(struct wl_listener *listener, void *data) {
    struct tinywl_server *server = wl_container_of(listener, server, xdg_decoration_new);
    struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
    
    // Force client-side decorations (no server decorations)
    wlr_xdg_toplevel_decoration_v1_set_mode(decoration, 
        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

/**
 * A helper function to set the visibility of all server-side decoration
 * scene nodes for a given toplevel.
 */
static void set_decorations_visible(struct tinywl_toplevel *toplevel, bool visible) {
    if (!toplevel || !toplevel->ssd.enabled) {
        return;
    }

    wlr_log(WLR_INFO, "Setting decorations for '%s' to visible: %d",
            toplevel->xdg_toplevel->title ? toplevel->xdg_toplevel->title : "N/A", visible);

    // Check each decoration component before trying to set its visibility
    if (toplevel->ssd.title_bar) {
        wlr_scene_node_set_enabled(&toplevel->ssd.title_bar->node, visible);
    }
    if (toplevel->ssd.border_left) {
        wlr_scene_node_set_enabled(&toplevel->ssd.border_left->node, visible);
    }
    if (toplevel->ssd.border_right) {
        wlr_scene_node_set_enabled(&toplevel->ssd.border_right->node, visible);
    }
    if (toplevel->ssd.border_bottom) {
        wlr_scene_node_set_enabled(&toplevel->ssd.border_bottom->node, visible);
    }
    if (toplevel->ssd.close_button) {
        wlr_scene_node_set_enabled(&toplevel->ssd.close_button->node, visible);
    }
    if (toplevel->ssd.maximize_button) {
        wlr_scene_node_set_enabled(&toplevel->ssd.maximize_button->node, visible);
    }
    if (toplevel->ssd.minimize_button) {
        wlr_scene_node_set_enabled(&toplevel->ssd.minimize_button->node, visible);
    }
}



/* Updated main function */
int main(int argc, char *argv[]) {
    



    wlr_log_init(WLR_DEBUG, NULL);
    char *startup_cmd = NULL;

    int c;
    while ((c = getopt(argc, argv, "s:h")) != -1) {
        switch (c) {
        case 's':
            startup_cmd = optarg;
            break;
        default:
            printf("Usage: %s [-s startup command]\n", argv[0]);
            return 0;
        }
    }
    if (optind < argc) {
        printf("Usage: %s [-s startup command]\n", argv[0]);
        return 0;
    }

        struct tinywl_server server = {0};


        server.dock_anim_active = true;
        server.dock_mouse_at_edge = false;
        server.dock_anim_duration = 0.8f; 
        server.animating_toplevel = NULL; 

        server.tv_is_on = false; // Start with the TV on
           server.tv_effect_animating = false; // The effect is off by default
        server.tv_effect_start_time = 0.0f;
        server.tv_effect_duration = 5.0f; // This is our 0 -> 5 second timeline
        server.expo_effect_active = false;
    server.effect_is_target_zoomed=false;
    server.effect_anim_current_factor=1.0;
     // <<< ADD THIS INITIALIZATION >>>
           server.tv_effect_animating = false; // Start with the animation off
        server.tv_effect_start_time = 0.0f;
        server.tv_effect_duration = 5.0f; // The animation lasts 5 seconds
    // Initialize all lists
        wl_list_init(&server.toplevels);
        wl_list_init(&server.outputs);
        wl_list_init(&server.keyboards);
        wl_list_init(&server.popups);

    

        /* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
         * can also specify a renderer using the WLR_RENDERER env var.
         * The renderer is responsible for defining the various pixel formats it
         * supports for shared memory, this configures that for clients. */
      // After creating the renderer
server.wl_display = wl_display_create();
    if (!server.wl_display) {
        wlr_log(WLR_ERROR, "Cannot create Wayland display");
        return 1;
    }

    // Create seat
    server.seat = wlr_seat_create(server.wl_display, "seat0");
    if (!server.seat) {
        wlr_log(WLR_ERROR, "Failed to create seat");
        wl_display_destroy(server.wl_display);
        return 1;
    }
    server.request_cursor.notify = seat_request_cursor;
    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);
    server.request_set_selection.notify = seat_request_set_selection;
    wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);
    wlr_log(WLR_INFO, "Compositor seat 'seat0' created: %p", (void*)server.seat);

    // Create backend
    server.backend = wlr_backend_autocreate(server.wl_display, NULL);
    if (server.backend == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_backend");
        wl_display_destroy(server.wl_display);
        return 1;
    }

    // Create renderer
    server.renderer = wlr_renderer_autocreate(server.backend);
    if (server.renderer == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_renderer");
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.wl_display);
        return 1;
    }

    // Make EGL context current
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(server.renderer);
    if (!egl || !wlr_egl_make_current(egl, NULL)) {
        wlr_log(WLR_ERROR, "Failed to make EGL context current for initialization");
        wlr_renderer_destroy(server.renderer);
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.wl_display);
        return 1;
    }

    // Query OpenGL information
    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    const char *shading_lang = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    wlr_log(WLR_INFO, "GL_VENDOR: %s", vendor ? vendor : "(null)");
    wlr_log(WLR_INFO, "GL_RENDERER: %s", renderer ? renderer : "(null)");
    wlr_log(WLR_INFO, "GL_VERSION: %s", version ? version : "(null)");
    wlr_log(WLR_INFO, "GL_SHADING_LANGUAGE_VERSION: %s", shading_lang ? shading_lang : "(null)");



     // --- Create Flame Shader ---
        struct shader_uniform_spec flame_uniforms[] = {
            {"mvp", &server.flame_shader_mvp_loc},
            {"texture_sampler_uniform", &server.flame_shader_tex_loc},
            {"time", &server.flame_shader_time_loc},
            {"iResolution", &server.flame_shader_res_loc}
        };
        if (!create_generic_shader_program(server.renderer, "FlameShader",
                                         vertex_shader_src, fragment_shader_src, // Use your actual flame shader sources
                                         &server.shader_program,
                                         flame_uniforms, sizeof(flame_uniforms) / sizeof(flame_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create flame shader program.");
            server_destroy(&server); return 1;
        }

        // --- Create "70s Melt" Rectangle Shader ---
        struct shader_uniform_spec melt_rect_uniforms[] = {
            {"mvp", &server.rect_shader_mvp_loc},
            {"u_color", &server.rect_shader_color_loc},
            {"time", &server.rect_shader_time_loc},
            {"iResolution", &server.rect_shader_resolution_loc},
            {"u_rectResolution", &server.rect_shader_rect_res_loc} 
        };
         if (!create_generic_shader_program(server.renderer, "MeltRectShader",
                                         rect_vertex_shader_src,
                                         desktop_0_fs_src, // Use the modified one from above
                                         &server.rect_shader_program,
                                         melt_rect_uniforms, sizeof(melt_rect_uniforms) / sizeof(melt_rect_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create 'Melt' rect shader program.");
            server_destroy(&server); return 1;
        }

        // --- Create Panel Shader (Stripes) ---
        struct shader_uniform_spec panel_uniforms[] = {
            {"mvp", &server.panel_shader_mvp_loc},                       // For mat3
            {"time", &server.panel_shader_time_loc},                     // For float
            {"iResolution", &server.panel_shader_resolution_loc},        // For vec2
             {"panelDimensions", &server.panel_shader_rect_pixel_dimensions_loc},
            {"u_previewTexture", &server.panel_shader_preview_tex_loc},  // For sampler2D (int)
            {"u_isPreviewActive", &server.panel_shader_is_preview_active_loc}, // For bool (int)
            {"u_previewRect", &server.panel_shader_preview_rect_loc},          // For vec4
            {"u_previewTexTransform", &server.panel_shader_preview_tex_transform_loc} // For mat3
        };
        if (!create_generic_shader_program(server.renderer, "PanelStripeShader",
                                         panel_vertex_shader_src, panel_fragment_shader_src,
                                         &server.panel_shader_program,
                                         panel_uniforms, sizeof(panel_uniforms) / sizeof(panel_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create panel stripe shader program.");
            server_destroy(&server); return 1;
        }

      

         struct shader_uniform_spec back_uniforms[] = {
            {"mvp",                       &server.back_shader_mvp_loc},
            {"texture_sampler_uniform",   &server.back_shader_tex_loc}, // If FS uses it
            {"time",                      &server.back_shader_time_loc},      // If FS uses it
            {"iResolution",               &server.back_shader_res_loc}       // If FS uses it
            // Add any other uniforms your back_fragment_shader_src DEFINES
            // e.g., {"u_some_color", &server.back_shader_some_other_uniform_loc}
        };

        if (!create_generic_shader_program(server.renderer, "backgroundShader", // Log name
                                         back_vertex_shader_src,  // Your chosen VS for window backgrounds
                                         back_fragment_shader_src, // Your chosen FS for window backgrounds
                                         &server.back_shader_program, // Store program ID here
                                         back_uniforms,
                                         sizeof(back_uniforms) / sizeof(back_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create Window Background shader program.");
            server_destroy(&server); return 1;
        }


    // In main()
     // --- Create SSD Shader (Checkerboard) ---
        struct shader_uniform_spec ssd_shader_uniforms[] = {
            {"mvp", &server.ssd_shader_mvp_loc},
            {"u_color", &server.ssd_shader_color_loc},
            // These are not used by the checkerboard shader, so their locations will be -1.
            // This is fine as render_rect_node doesn't try to set them for SSDs.
            {"time", &server.ssd_shader_time_loc},
             {"u_rectPixelDimensions", &server.ssd_shader_rect_pixel_dimensions_loc},
             {"iResolution", &server.ssd_shader_resolution_loc},
        };

        // Use the correctly named global shader sources
        if (!create_generic_shader_program(server.renderer, "SSDShader", // Updated log name
                                         ssd_vertex_shader_src,  // Use the renamed global VS
                                         ssd_fragment_shader_src, // Use the renamed global FS
                                         &server.ssd_shader_program,
                                         ssd_shader_uniforms,
                                         sizeof(ssd_shader_uniforms) / sizeof(ssd_shader_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create SSD shader program.");
            server_destroy(&server); return 1;
        }

        
       struct shader_uniform_spec ssd2_shader_uniforms[] = {
            {"mvp", &server.ssd2_shader_mvp_loc},
            {"u_color", &server.ssd2_shader_color_loc},
            // These are not used by the checkerboard shader, so their locations will be -1.
            // This is fine as render_rect_node doesn't try to set them for SSDs.
            {"time", &server.ssd2_shader_time_loc},
             {"u_rectPixelDimensions", &server.ssd_shader_rect_pixel_dimensions_loc},
             {"iResolution", &server.ssd2_shader_resolution_loc},
        };

        // Use the correctly named global shader sources
        if (!create_generic_shader_program(server.renderer, "SSDShader2", // Updated log name
                                         ssd_vertex_shader_src,  // Use the renamed global VS
                                         ssd2_fragment_shader_src, // Use the renamed global FS
                                         &server.ssd2_shader_program,
                                         ssd2_shader_uniforms,
                                         sizeof(ssd2_shader_uniforms) / sizeof(ssd2_shader_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create SSD2 shader program.");
            server_destroy(&server); return 1;
        }



    // /server.scale_down_effect_active = false; // Or true if you want it on by default

        // ... (creation of your other shaders: flame, rect, panel, ssd, back) ...

        // --- Create Fullscreen Shader (for Scaled Down Scene View) ---
        struct shader_uniform_spec scaled_scene_uniforms[] = {
        {"mvp", &server.fullscreen_shader_mvp_loc},
        {"u_scene_texture0", &server.fullscreen_shader_scene_tex0_loc},
        {"u_scene_texture1", &server.fullscreen_shader_scene_tex1_loc},
        {"u_scene_texture2", &server.fullscreen_shader_scene_tex2_loc},
        {"u_scene_texture3", &server.fullscreen_shader_scene_tex3_loc},
        {"u_scene_texture4", &server.fullscreen_shader_scene_tex4_loc},
        {"u_scene_texture5", &server.fullscreen_shader_scene_tex5_loc},
        {"u_scene_texture6", &server.fullscreen_shader_scene_tex6_loc},
        {"u_scene_texture7", &server.fullscreen_shader_scene_tex7_loc},
        {"u_scene_texture8", &server.fullscreen_shader_scene_tex8_loc},
        {"u_scene_texture9", &server.fullscreen_shader_scene_tex9_loc},
        {"u_scene_texture10", &server.fullscreen_shader_scene_tex10_loc},
        {"u_scene_texture11", &server.fullscreen_shader_scene_tex11_loc},
        {"u_scene_texture12", &server.fullscreen_shader_scene_tex12_loc},
        {"u_scene_texture13", &server.fullscreen_shader_scene_tex13_loc},
        {"u_scene_texture14", &server.fullscreen_shader_scene_tex14_loc},
        {"u_scene_texture15", &server.fullscreen_shader_scene_tex15_loc},
        {"DesktopGrid", &server.fullscreen_shader_desktop_grid_loc},
        {"u_zoom", &server.fullscreen_shader_zoom_loc},
        {"u_move", &server.fullscreen_shader_move_loc},
        {"u_zoom_center", &server.fullscreen_shader_zoom_center_loc},
        {"u_quadrant", &server.fullscreen_shader_quadrant_loc},
        {"switch_mode",&server.fullscreen_switch_mode},
        {"u_switchXY",&server.fullscreen_switchXY} // Add this if using the uniform version
    // {"u_current_quad", &server.fullscreen_shader_current_quad_loc} 
    };

    if (!create_generic_shader_program(server.renderer, "ScaledSceneViewShader",
                                     fullscreen_vertex_shader_src,
                                     expo_fragment_shader_src,
                                     &server.fullscreen_shader_program,
                                     scaled_scene_uniforms,
                                     sizeof(scaled_scene_uniforms) / sizeof(scaled_scene_uniforms[0]))) {
        wlr_log(WLR_ERROR, "Failed to create scaled scene view shader program.");
        server_destroy(&server); 
        return 1;
    }


        struct shader_uniform_spec cube_scene_uniforms[] = {
            {"mvp", &server.cube_shader_mvp_loc},
            {"u_scene_texture0", &server.cube_shader_scene_tex0_loc},
            {"u_scene_texture1", &server.cube_shader_scene_tex1_loc},
            {"u_scene_texture2", &server.cube_shader_scene_tex2_loc},
            {"u_scene_texture3", &server.cube_shader_scene_tex3_loc},
            {"u_zoom", &server.cube_shader_zoom_loc},
            {"u_zoom_center", &server.cube_shader_zoom_center_loc},
             {"u_zoom", &server.cube_shader_zoom_loc},
            {"u_rotation_y", &server.cube_shader_time_loc}, // RENAMED in shader, but C variable is the same
            {"u_quadrant", &server.cube_shader_quadrant_loc},
            {"u_vertical_offset", &server.cube_shader_vertical_offset_loc}, // Not used, but defined for consistency
             {"GLOBAL_u_vertical_offset", &server.cube_shader_global_vertical_offset_loc} ,
              {"iResolution", &server.cube_shader_resolution_loc}
        };

    if (!create_generic_shader_program(server.renderer, "CubeShader",
                                     cube_vertex_shader_src,
                                     cube_fragment_shader_src,
                                     &server.cube_shader_program,
                                     cube_scene_uniforms, // <-- USE THE CORRECT STRUCT HERE
                                     sizeof(cube_scene_uniforms) / sizeof(cube_scene_uniforms[0]))) {
        wlr_log(WLR_ERROR, "Failed to create cube effect shader program.");
        server_destroy(&server); 
        return 1;
    }



    // --- Create Cube Background Shader ---
    struct shader_uniform_spec cube_bg_uniforms[] = {
        {"u_time", &server.cube_background_shader_time_loc},
    };
    if (!create_generic_shader_program(server.renderer, "CubeBackgroundShader",
                                     background_vertex_shader_src,
                                     background_fragment_shader_src,
                                     &server.cube_background_shader_program,
                                     cube_bg_uniforms,
                                     sizeof(cube_bg_uniforms) / sizeof(cube_bg_uniforms[0]))) {
        wlr_log(WLR_ERROR, "Failed to create cube background shader program.");
        server_destroy(&server);
        return 1;
    }


    // In main(), with your other shader creations
    struct shader_uniform_spec solid_uniforms[] = {
        {"mvp", &server.solid_shader_mvp_loc},
        {"u_color", &server.solid_shader_color_loc},
        {"time", &server.solid_shader_time_loc}, 
    };
    if (!create_generic_shader_program(server.renderer, "SolidColorShader",
                                     solid_vertex_shader_src,
                                     solid_fragment_shader_src,
                                     &server.solid_shader_program,
                                     solid_uniforms,
                                     sizeof(solid_uniforms) / sizeof(solid_uniforms[0]))) {
        wlr_log(WLR_ERROR, "Failed to create solid color shader program.");
        server_destroy(&server);
        return 1;
    }

    const char *desktop_fs_sources[] = {
        desktop_0_fs_src, // Desktop 0 uses the "Melt" shader
        desktop_1_fs_src,         // Desktop 1 uses "Starfield"
        desktop_2_fs_src,         // Desktop 2 uses "Tunnel"
        desktop_3_fs_src,          // Desktop 3 uses "Noise"
        desktop_4_fs_src, // Desktop 0 uses the "Melt" shader
        desktop_5_fs_src,         // Desktop 1 uses "Starfield"
        desktop_6_fs_src,         // Desktop 2 uses "Tunnel"
        desktop_7_fs_src,          // Desktop 3 uses "Noise"
        desktop_8_fs_src, // Desktop 0 uses the "Melt" shader
        desktop_9_fs_src,         // Desktop 1 uses "Starfield"
        desktop_10_fs_src,         // Desktop 2 uses "Tunnel"
        desktop_11_fs_src,          // Desktop 3 uses "Noise"
        desktop_12_fs_src, // Desktop 0 uses the "Melt" shader
        desktop_13_fs_src,         // Desktop 1 uses "Starfield"
        desktop_14_fs_src,         // Desktop 2 uses "Tunnel"
        desktop_15_fs_src          // Desktop 3 uses "Noise"
        
    };

    for (int i = 0; i < 16; ++i) {
        char log_name[64];
        snprintf(log_name, sizeof(log_name), "DesktopBGShader%d", i);

        struct shader_uniform_spec bg_uniforms[] = {
            {"mvp", &server.desktop_bg_shader_mvp_loc[i]},
            {"time", &server.desktop_bg_shader_time_loc[i]},
            {"iResolution", &server.desktop_bg_shader_res_loc[i]},
            {"u_color", &server.desktop_bg_shader_color_loc[i]}
        };

        if (!create_generic_shader_program(server.renderer, log_name,
                                         rect_vertex_shader_src, // All use the same vertex shader
                                         desktop_fs_sources[i],
                                         &server.desktop_background_shaders[i],
                                         bg_uniforms, sizeof(bg_uniforms) / sizeof(bg_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create desktop background shader %d.", i);
            server_destroy(&server);
            return 1;
        }
    }

      struct shader_uniform_spec post_process_uniforms[] = {
            // The texture containing the entire pre-rendered scene.
            // The shader expects this on texture unit 0, which we set with glUniform1i.
            {"u_scene_texture", &server.post_process_shader_tex_loc},
            // A time value (in seconds) for creating animated effects.
            {"u_time", &server.post_process_shader_time_loc},
            {"iResolution",     &server.post_process_shader_resolution_loc}
        };

        // 2. Call your generic function to create the shader program.
        //    It will populate server.post_process_shader_program and the uniform locations.
        if (!create_generic_shader_program(server.renderer, "PostProcessShader",
                                         post_process_vert, post_process_frag,
                                         &server.post_process_shader_program,
                                         post_process_uniforms, 
                                         sizeof(post_process_uniforms) / sizeof(post_process_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create the final post-processing shader program.");
            // Your error handling here, e.g.:
            server_destroy(&server); 
            return 1;
        }


         // --- Create Pass-Through Shader ---
        struct shader_uniform_spec passthrough_uniforms[] = {
            {"mvp", &server.passthrough_shader_mvp_loc},
            {"u_texture", &server.passthrough_shader_tex_loc},
            {"iResolution", &server.passthrough_shader_res_loc},
            {"cornerRadius", &server.passthrough_shader_cornerRadius_loc},
            {"bevelColor", &server.passthrough_shader_bevelColor_loc},
            {"time", &server.passthrough_shader_time_loc} // <<< ADD THIS
        };
        if (!create_generic_shader_program(server.renderer, "PassThroughShader",
                                         passthrough_vertex_shader_src, 
                                         passthrough_fragment_shader_src,
                                         &server.passthrough_shader_program,
                                         passthrough_uniforms, 
                                         sizeof(passthrough_uniforms) / sizeof(passthrough_uniforms[0]))) {
            // ... error handling ...
        }

           // --- Create Pass-Through Shader ---
        struct shader_uniform_spec passthrough_icons_uniforms[] = {
            {"mvp", &server.passthrough_icons_shader_mvp_loc},
            {"u_texture", &server.passthrough_icons_shader_tex_loc},
            {"iResolution", &server.passthrough_icons_shader_res_loc},
            {"cornerRadius", &server.passthrough_icons_shader_cornerRadius_loc},
            {"bevelColor", &server.passthrough_icons_shader_bevelColor_loc},
            {"time", &server.passthrough_icons_shader_time_loc} // <<< ADD THIS
        };
        if (!create_generic_shader_program(server.renderer, "PassThroughShaderIcons",
                                         passthrough_vertex_shader_src, 
                                         passthrough_icons_fragment_shader_src,
                                         &server.passthrough_icons_shader_program,
                                         passthrough_icons_uniforms, 
                                         sizeof(passthrough_icons_uniforms) / sizeof(passthrough_icons_uniforms[0]))) {
            // ... error handling ...
        }



    // In main(), inside the shader setup section
        // ... (after your other create_generic_shader_program calls)
        // --- Create Genie/Minimize Shader ---
        struct shader_uniform_spec genie_uniforms[] = {
            {"mvp", &server.genie_shader_mvp_loc},
            {"u_texture", &server.genie_shader_tex_loc},
            {"progress", &server.genie_shader_progress_loc},
            {"u_bend_factor", &server.genie_shader_bend_factor_loc}
        };
        if (!create_generic_shader_program(server.renderer, "GenieShader",
                                         genie_vertex_shader_src,
                                         genie_fragment_shader_src,
                                         &server.genie_shader_program,
                                         genie_uniforms,
                                         sizeof(genie_uniforms) / sizeof(genie_uniforms[0]))) {
            wlr_log(WLR_ERROR, "Failed to create genie effect shader program.");
            server_destroy(&server);
            return 1;
        }

        server.current_desktop = 0;
        DestopGridSize = 4; // Assuming a 4x4 grid for 16 desktops
        server.num_desktops = DestopGridSize* DestopGridSize; // Assuming 4 desktops for the example
       
        server.effect_zoom_factor_normal = 2.0f;
        server.effect_zoom_factor_zoomed = 1.0f;
        
        server.effect_is_target_zoomed = true;
        server.effect_is_animating_zoom = false;
        server.effect_anim_current_factor = server.effect_zoom_factor_normal;
        server.effect_anim_start_factor = server.effect_zoom_factor_normal;
        server.effect_anim_target_factor = server.effect_zoom_factor_normal;
        server.effect_anim_duration_sec = 0.3f; 

        // For the current shader, u_zoom_center should be (0,0) to scale around the quadrant's local center
        server.effect_zoom_center_x = 0.0f;
        server.effect_zoom_center_y = 1.0f;

        // --- Initialize Cube effect variables ---
        server.cube_effect_active = false;
        server.cube_zoom_factor_normal = 2.0f; // Start zoomed out
        server.cube_zoom_factor_zoomed = 1.0f; // Zoomed in
        
        server.cube_is_target_zoomed = true; // The desired state is not zoomed
        server.cube_is_animating_zoom = false;
        server.cube_anim_current_factor = server.cube_zoom_factor_normal;
        server.cube_anim_start_factor = server.cube_zoom_factor_normal;
        server.cube_anim_target_factor = server.cube_zoom_factor_normal;
        server.cube_anim_duration_sec = 0.3f; // A slightly different duration
        
        // These centers likely won't be used if your shader doesn't have a u_zoom_center, but good to init
        server.cube_zoom_center_x = 0.0f; 
        server.cube_zoom_center_y = 1.0f;
     /* wlr_log(WLR_INFO, "Fullscreen Shader Locations: mvp=%d, scene_tex=%d, zoom=%d, zoom_center=%d, quadrant=%d",
                server.fullscreen_shader_mvp_loc,
                server.fullscreen_shader_scene_tex0_loc,
                server.fullscreen_shader_zoom_loc,
                server.fullscreen_shader_zoom_center_loc,
                server.fullscreen_shader_quadrant_loc); // Log the new location too

        wlr_log(WLR_INFO, "ScaledSceneViewShader (fullscreen_shader_program) created. MVP@%d, SceneTex@%d",
                server.fullscreen_shader_mvp_loc, server.fullscreen_shader_scene_tex_loc);
    */
        // Updated log message to be more accurate for the checkerboard shader
        wlr_log(WLR_INFO, "SSDShader created (ID: %u). MVP@%d, Color@%d. (Unused: Time@%d, iResolution@%d)",
                server.ssd_shader_program,
                server.ssd_shader_mvp_loc,
                server.ssd_shader_color_loc,
                server.ssd_shader_time_loc, // This will likely be -1
                server.ssd_shader_rect_pixel_dimensions_loc); 
    wlr_log(WLR_INFO, "SSDOctagramShader created (ID: %u). MVP@%d, Color@%d, Time@%d, RectDims(iRes)@%d",
            server.ssd_shader_program,
            server.ssd_shader_mvp_loc,
            server.ssd_shader_color_loc,
            server.ssd_shader_time_loc,
            server.ssd_shader_rect_pixel_dimensions_loc);
    wlr_log(WLR_INFO, "SSDMultiColorRoundedShader created (ID: %u). MVP@%d, Color@%d, RectDims@%d",
            server.ssd_shader_program, server.ssd_shader_mvp_loc, server.ssd_shader_color_loc, server.ssd_shader_rect_pixel_dimensions_loc);
    wlr_log(WLR_INFO, "SSDColorShader created (ID: %u). MVP@%d, Color@%d",
            server.ssd_shader_program, server.ssd_shader_mvp_loc, server.ssd_shader_color_loc);
        // Add a log to confirm locations (optional but helpful)
        wlr_log(WLR_INFO, "backgroundShader created (ID: %u). MVP@%d, Tex@%d, Time@%d, iRes@%d",
                server.back_shader_program,
                server.back_shader_mvp_loc,
                server.back_shader_tex_loc,
                server.back_shader_time_loc,
                server.back_shader_res_loc);
        // --- VAO Setup ---
        // Ensure EGL context is current for VAO operations.
        // create_generic_shader_program restores the context, so make it current again.
        struct wlr_egl *egl_main = wlr_gles2_renderer_get_egl(server.renderer);
    if (wlr_egl_make_current(egl_main, NULL)) {
        GLenum gl_error;
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main A (after make_current): 0x%x", gl_error); }


     // --- NEW: SETUP TESSELLATED MESH FOR GENIE EFFECT ---
            const int TESSELLATION_X = 100;
            const int TESSELLATION_Y = 100;
            const int VERTEX_COUNT = (TESSELLATION_X + 1) * (TESSELLATION_Y + 1);
            server.genie_index_count = TESSELLATION_X * TESSELLATION_Y * 6;

            GLfloat genie_vertices[VERTEX_COUNT * 4]; // 2 for pos, 2 for texcoord
            GLuint genie_indices[server.genie_index_count];

            for (int y = 0; y <= TESSELLATION_Y; ++y) {
                for (int x = 0; x <= TESSELLATION_X; ++x) {
                    int idx = (y * (TESSELLATION_X + 1) + x) * 4;
                    genie_vertices[idx + 0] = (float)x / TESSELLATION_X; // pos.x
                    genie_vertices[idx + 1] = (float)y / TESSELLATION_Y; // pos.y
                    genie_vertices[idx + 2] = (float)x / TESSELLATION_X; // tex.u
                    genie_vertices[idx + 3] = (float)y / TESSELLATION_Y; // tex.v
                }
            }

            int i = 0;
            for (int y = 0; y < TESSELLATION_Y; ++y) {
                for (int x = 0; x < TESSELLATION_X; ++x) {
                    int top_left = y * (TESSELLATION_X + 1) + x;
                    genie_indices[i++] = top_left;
                    genie_indices[i++] = top_left + 1;
                    genie_indices[i++] = top_left + TESSELLATION_X + 1;
                    genie_indices[i++] = top_left + 1;
                    genie_indices[i++] = top_left + TESSELLATION_X + 2;
                    genie_indices[i++] = top_left + TESSELLATION_X + 1;
                }
            }

            glGenVertexArrays(1, &server.genie_vao);
            glBindVertexArray(server.genie_vao);
            
            glGenBuffers(1, &server.genie_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, server.genie_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(genie_vertices), genie_vertices, GL_STATIC_DRAW);

            glGenBuffers(1, &server.genie_ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, server.genie_ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(genie_indices), genie_indices, GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
            
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            wlr_log(WLR_INFO, "Created tessellated genie mesh with %d indices.", server.genie_index_count);

        // EXISTING 2D QUAD DATA (keep this for your other effects)
        const GLfloat verts[] = {
    0.0f, 0.0f, 0.0f, 1.0f,  // Top-left: tex y=1 (top of texture)
    1.0f, 0.0f, 1.0f, 1.0f,  // Top-right: tex y=1
    0.0f, 1.0f, 0.0f, 0.0f,  // Bottom-left: tex y=0 (bottom of texture)
    1.0f, 1.0f, 1.0f, 0.0f,  // Bottom-right: tex y=0
};
        const GLuint indices[] = {0, 1, 2, 1, 3, 2}; 

        // NEW CUBE DATA (for the rotating cube effect)
const GLfloat cube_verts[] = {
// Front face (z = 0.5)
-0.5f, -0.5f,  0.5f,   0.0f, 1.0f,   0.0f,  // 0: Bottom-left, v=1
0.5f, -0.5f,  0.5f,   1.0f, 1.0f,   0.0f,  // 1: Bottom-right, v=1
0.5f,  0.5f,  0.5f,   1.0f, 0.0f,   0.0f,  // 2: Top-right, v=0
-0.5f,  0.5f,  0.5f,   0.0f, 0.0f,   0.0f,  // 3: Top-left, v=0
// Back face (z = -0.5) - Adjusted u to 0-1 left to right, flipped v
-0.5f, -0.5f, -0.5f,   0.0f, 1.0f,   1.0f,  // 4: Bottom-left, u=0, v=1
-0.5f,  0.5f, -0.5f,   0.0f, 0.0f,   1.0f,  // 5: Top-left, u=0, v=0
0.5f,  0.5f, -0.5f,   1.0f, 0.0f,   1.0f,  // 6: Top-right, u=1, v=0
0.5f, -0.5f, -0.5f,   1.0f, 1.0f,   1.0f,  // 7: Bottom-right, u=1, v=1
// Left face (x = -0.5) - Flipped v
-0.5f, -0.5f, -0.5f,   0.0f, 1.0f,   2.0f,  // 8: Bottom-back, v=1
-0.5f, -0.5f,  0.5f,   1.0f, 1.0f,   2.0f,  // 9: Bottom-front, v=1
-0.5f,  0.5f,  0.5f,   1.0f, 0.0f,   2.0f,  // 10: Top-front, v=0
-0.5f,  0.5f, -0.5f,   0.0f, 0.0f,   2.0f,  // 11: Top-back, v=0
// Right face (x = 0.5) - Flipped v
0.5f, -0.5f,  0.5f,   0.0f, 1.0f,   3.0f,  // 12: Bottom-front, v=1
0.5f, -0.5f, -0.5f,   1.0f, 1.0f,   3.0f,  // 13: Bottom-back, v=1
0.5f,  0.5f, -0.5f,   1.0f, 0.0f,   3.0f,  // 14: Top-back, v=0
0.5f,  0.5f,  0.5f,   0.0f, 0.0f,   3.0f,  // 15: Top-front, v=0
// Bottom face (y = -0.5) - Flipped v (assuming u left to right, v back to front)
-0.5f, -0.5f, -0.5f,   0.0f, 1.0f,   4.0f,  // 16: Left-back, v=1
0.5f, -0.5f, -0.5f,   1.0f, 1.0f,   4.0f,  // 17: Right-back, v=1
0.5f, -0.5f,  0.5f,   1.0f, 0.0f,   4.0f,  // 18: Right-front, v=0
-0.5f, -0.5f,  0.5f,   0.0f, 0.0f,   4.0f,  // 19: Left-front, v=0
// Top face (y = 0.5) - Flipped v
-0.5f,  0.5f,  0.5f,   0.0f, 1.0f,   5.0f,  // 20: Left-front, v=1 (flipped)
0.5f,  0.5f,  0.5f,   1.0f, 1.0f,   5.0f,  // 21: Right-front, v=1
0.5f,  0.5f, -0.5f,   1.0f, 0.0f,   5.0f,  // 22: Right-back, v=0
-0.5f,  0.5f, -0.5f,   0.0f, 0.0f,   5.0f   // 23: Left-back, v=0
};

   const GLuint cube_indices[] = {
    // Front face (counter-clockwise when viewed from outside)
    0, 2, 1,   0, 3, 2,
    // Back face
    4, 6, 5,   4, 7, 6,
    // Left face
    8, 10, 9,  8, 11, 10,
    // Right face
    12, 14, 13, 12, 15, 14,
    // Bottom face
    16, 18, 17, 16, 19, 18,
    // Top face
    20, 22, 21, 20, 23, 22
};
        // ===========================================
        // SETUP EXISTING 2D QUAD (for other effects)
        // ===========================================
        glGenVertexArrays(1, &server.quad_vao);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main B (after GenVertexArrays): 0x%x", gl_error); }
        glBindVertexArray(server.quad_vao);
        wlr_log(WLR_DEBUG, "MAIN_VAO_SETUP: VAO ID: %d bound", server.quad_vao);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main C (after BindVertexArray): 0x%x", gl_error); }

        glGenBuffers(1, &server.quad_vbo);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main D (after GenBuffers VBO): 0x%x", gl_error); }
        glBindBuffer(GL_ARRAY_BUFFER, server.quad_vbo); 
        wlr_log(WLR_DEBUG, "MAIN_VAO_SETUP: VBO ID: %d bound to GL_ARRAY_BUFFER", server.quad_vbo);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main E (after BindBuffer VBO): 0x%x", gl_error); }
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main F (after BufferData VBO): 0x%x", gl_error); }

        glGenBuffers(1, &server.quad_ibo);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main G (after GenBuffers IBO): 0x%x", gl_error); }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, server.quad_ibo); 
        wlr_log(WLR_DEBUG, "MAIN_VAO_SETUP: IBO ID: %d bound to GL_ELEMENT_ARRAY_BUFFER (associated with VAO %d)", server.quad_ibo, server.quad_vao);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main H (after BindBuffer IBO): 0x%x", gl_error); }
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error main I (after BufferData IBO): 0x%x", gl_error); }

        // Set up 2D quad vertex attributes (existing code)
        glBindVertexArray(server.quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, server.quad_vbo);
        
        // Position attribute at fixed location 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
        wlr_log(WLR_DEBUG, "VAO Setup: Enabled attrib 0 for position");

        // Texcoord attribute at fixed location 1
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
        wlr_log(WLR_DEBUG, "VAO Setup: Enabled attrib 1 for texcoord");

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, server.quad_ibo);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // ===========================================
        // SETUP NEW CUBE GEOMETRY
        // ===========================================
        glGenVertexArrays(1, &server.cube_vao);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error cube A (after GenVertexArrays): 0x%x", gl_error); }
        glBindVertexArray(server.cube_vao);
        wlr_log(WLR_DEBUG, "CUBE_VAO_SETUP: VAO ID: %d bound", server.cube_vao);

        glGenBuffers(1, &server.cube_vbo);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error cube B (after GenBuffers VBO): 0x%x", gl_error); }
        glBindBuffer(GL_ARRAY_BUFFER, server.cube_vbo); 
        wlr_log(WLR_DEBUG, "CUBE_VAO_SETUP: VBO ID: %d bound", server.cube_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verts), cube_verts, GL_STATIC_DRAW);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error cube C (after BufferData VBO): 0x%x", gl_error); }

        glGenBuffers(1, &server.cube_ibo);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error cube D (after GenBuffers IBO): 0x%x", gl_error); }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, server.cube_ibo); 
        wlr_log(WLR_DEBUG, "CUBE_VAO_SETUP: IBO ID: %d bound", server.cube_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
        while ((gl_error = glGetError()) != GL_NO_ERROR) { wlr_log(WLR_ERROR, "GL Error cube E (after BufferData IBO): 0x%x", gl_error); }

        // Set up cube vertex attributes
        // Position attribute at location 0 (vec3)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
        wlr_log(WLR_DEBUG, "CUBE Setup: Enabled attrib 0 for position (vec3)");

        // Texcoord attribute at location 1
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
        wlr_log(WLR_DEBUG, "CUBE Setup: Enabled attrib 1 for texcoord");

        // Face ID attribute at location 2
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(5 * sizeof(GLfloat)));
        wlr_log(WLR_DEBUG, "CUBE Setup: Enabled attrib 2 for face_id");

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        wlr_egl_unset_current(egl_main);
    }else {
            wlr_log(WLR_ERROR, "Failed to make EGL context current for quad VBO setup in main");
            server_destroy(&server);
            // return 1; // Or handle error appropriately
        }
        

        server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
        if (!server.allocator) {
            wlr_log(WLR_ERROR, "Failed to create allocator");
            server_destroy(&server);
            return 1;
        }


     
        wlr_compositor_create(server.wl_display, 5, server.renderer);
        wlr_subcompositor_create(server.wl_display);
        wlr_data_device_manager_create(server.wl_display);

//        server.output_layout = wlr_output_layout_create(server.wl_display);
        server.output_layout = wlr_output_layout_create();
        wl_list_init(&server.outputs);
        server.new_output.notify = server_new_output;
        wl_signal_add(&server.backend->events.new_output, &server.new_output);

       

     server.scene = wlr_scene_create();
        server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);
       
        wl_list_init(&server.toplevels);
        wl_list_init(&server.popups);

      // ...
    server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
    server.new_xdg_toplevel.notify = server_new_xdg_toplevel; // KEEP THIS
    //wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
    // ...

    server.xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(server.wl_display);
    if (!server.xdg_decoration_manager) {
        wlr_log(WLR_ERROR, "Failed to create XDG decoration manager v1");
        return 1;
    }
    server.xdg_decoration_new.notify = server_new_xdg_decoration; // Ensure this points to your callback
    wl_signal_add(&server.xdg_decoration_manager->events.new_toplevel_decoration,
                  &server.xdg_decoration_new); // Ensure this is the correct listener struct member
    wlr_log(WLR_ERROR, "!!!!!!!!!! XDG DECORATION LISTENER ADDED (main) for manager %p, listener %p, notify %p",
        (void*)server.xdg_decoration_manager, (void*)&server.xdg_decoration_new, (void*)server_new_xdg_decoration);
    // ...
     

        server.cursor = wlr_cursor_create();
        wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
        server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
        wlr_xcursor_manager_load(server.cursor_mgr, 1);

        server.cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
        server.cursor_motion.notify = server_cursor_motion;
        wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
        server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
        wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
        server.cursor_button.notify = server_cursor_button;
        wl_signal_add(&server.cursor->events.button, &server.cursor_button);
        server.cursor_axis.notify = server_cursor_axis;
        wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
        server.cursor_frame.notify = server_cursor_frame;
        wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

        wl_list_init(&server.keyboards);
        server.new_input.notify = server_new_input;
        wl_signal_add(&server.backend->events.new_input, &server.new_input);
     

    desktop_background(&server);
    desktop_panel(&server);

        const char *socket = wl_display_add_socket_auto(server.wl_display);
        if (!socket) {
            wlr_log(WLR_ERROR, "Unable to create wayland socket");
            wlr_seat_destroy(server.seat);
            wlr_xcursor_manager_destroy(server.cursor_mgr);
            wlr_cursor_destroy(server.cursor);
            wlr_output_layout_destroy(server.output_layout);
            wlr_allocator_destroy(server.allocator);
            wlr_renderer_destroy(server.renderer);
            wlr_backend_destroy(server.backend);
            wl_display_destroy(server.wl_display);
            return 1;
        }

        if (!wlr_backend_start(server.backend)) {
            wlr_log(WLR_ERROR, "Failed to start backend");
            server_destroy(&server);
            return 1;
        }

        setenv("WAYLAND_DISPLAY", socket, true);
        if (startup_cmd) {
            if (fork() == 0) {
                execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
            }
        }


        // NEW: Initialize the dock icons
    server.num_dock_icons = 10; // Set how many icons you want
    server.socket_name = socket; // Store the socket name to pass it to new applications


    // Define the command and texture for each icon
    server.dock_icons[0].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0 weston-simple-egl";
    server.dock_icons[1].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0 alacritty"; // Example: terminal
    server.dock_icons[2].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0  kitty"; // Example: browser
    server.dock_icons[3].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0 gtk3-demo"; // Example: calculator
    server.dock_icons[4].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0 gtk4-demo";
    server.dock_icons[5].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0 epiphany"; // Example: terminal
    server.dock_icons[6].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0  gedit"; // Example: browser
    server.dock_icons[7].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0 weston "; // Example: calculator
    server.dock_icons[8].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0  weston-flower"; // Example: browser
    server.dock_icons[9].app_command = "WAYLAND_DISPLAY=wayland-1 LIBGL_ALWAYS_SOFTWARE=0 weston-smoke "; // Example: calculator

      

        wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);



        wl_display_run(server.wl_display);

        wl_display_destroy_clients(server.wl_display);
        server_destroy(&server);
        return 0;
    }

/////////////////////////////////////////////////////////
/////////////////////////////////////
///////////////////////////////
///////////////////////////////////////////////
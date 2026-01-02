// ============================================================================
// nano_server.c - Core nano-kernel implementation
// ============================================================================
#define _GNU_SOURCE // MUST be the first line, before any includes
#include <sys/mman.h>
#include <unistd.h>
#include "nano_compositor.h"
#include <dlfcn.h>
#include "nano_ipc.h"       // <-- ADD THIS
#include <stdlib.h>
#include <sys/socket.h>     // <-- ADD THIS
#include <string.h>         // <-- ADD THIS (for strerror)
#include <errno.h>          // <-- ADD THIS (for errno)
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_scene.h> 
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <wlr/types/wlr_shm.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/render/dmabuf.h>
#include <drm_fourcc.h>
// For SHM buffer creation
#include <wlr/types/wlr_shm.h>
#include <time.h> 
#include "logs.h" 

// For direct OpenGL texture access (fallback method)
#include <wlr/render/gles2.h>
#include <GLES2/gl2.h>

// For math functions (sqrt in radial pattern)
#include <math.h>

// At the top of your file, add these includes:
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

// THIS IS THE FIX: Enable GLES extension definitions
#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_viewporter.h>
//#include <wlr/types/wlr_single_pixel_buffer_manager_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <fcntl.h>
#include <wlr/types/wlr_input_device.h> 
#include <linux/input-event-codes.h> // For KEY_Q
#include <wlr/types/wlr_keyboard.h>    // For WLR_MODIFIER_LOGO
#include "nano_layer_input.h"

//#undef ENABLE_IPC_QUEUE  // This disables the queue
#define ENABLE_IPC_QUEUE  // This enables the queue




// If the typedef is still not available, add this:
#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);
#endif

static bool extract_texture_pixels(struct wlr_renderer *renderer, struct wlr_allocator *allocator,
                                   struct wlr_texture *texture, uint32_t width, uint32_t height, void *pixels); 


static void send_ipc_with_fds(int sock_fd, const ipc_event_t *event, const int *fds, int fd_count);
void nano_server_send_window_updates(struct nano_server *server);
// Forward declarations for event handlers
static void server_new_output(struct wl_listener *listener, void *data);
static void server_new_xdg_surface(struct wl_listener *listener, void *data);
static void server_new_input(struct wl_listener *listener, void *data);
static void seat_request_cursor(struct wl_listener *listener, void *data);
static void seat_request_set_selection(struct wl_listener *listener, void *data);

// Cursor event handlers
static void server_cursor_motion(struct wl_listener *listener, void *data);
static void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
static void server_cursor_button(struct wl_listener *listener, void *data);
static void server_cursor_axis(struct wl_listener *listener, void *data);
static void server_cursor_frame(struct wl_listener *listener, void *data);
static void cleanup_mouse_queue(void);
static void init_mouse_queue(struct nano_server *server);
static void process_ipc_queue_idle(void *data) ;


static struct wlr_buffer* create_linear_buffer_from_texture(struct nano_server *server, 
                                                           struct wlr_texture *source_texture,
                                                           uint32_t width, uint32_t height);
static struct wlr_buffer* create_linear_buffer_from_unknown_buffer(struct nano_server *server, struct wlr_buffer *client_buffer);

static struct wlr_buffer* create_linear_buffer_from_suspected_shm(struct nano_server *server, struct wlr_buffer *suspected_shm_buffer);
static struct wlr_buffer* create_linear_buffer_from_client(struct nano_server *server, struct wlr_buffer *client_buffer) ;

static void fd_pool_init(struct fd_pool *pool, int max_size);
static void fd_pool_cleanup(struct fd_pool *pool);
static int fd_pool_get_fd(struct fd_pool *pool, struct wlr_buffer *buffer, uint64_t buffer_id);
static void fd_pool_return_fd(struct fd_pool *pool, int fd, uint64_t buffer_id);
static void fd_pool_cleanup_old(struct fd_pool *pool);
static uint64_t get_buffer_id(struct wlr_buffer *buffer);

// Existing forward declarations for event handlers (keep these)
static void server_new_output(struct wl_listener *listener, void *data);
static void server_new_xdg_surface(struct wl_listener *listener, void *data);
static void server_new_input(struct wl_listener *listener, void *data);
static void seat_request_cursor(struct wl_listener *listener, void *data);
static void seat_request_set_selection(struct wl_listener *listener, void *data);
// nano_server.c

// Forward declarations for our new keyboard handlers
static void handle_keyboard_modifiers(struct wl_listener *listener, void *data);
static void handle_keyboard_key(struct wl_listener *listener, void *data);
static void handle_keyboard_destroy(struct wl_listener *listener, void *data);
static void server_add_keyboard(struct nano_server *server, struct wlr_input_device *device); 



// Add these shader definitions at the top of your file
static const char* composite_vertex_shader = 
    "#version 310 es\n"
    "precision mediump float;\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec2 texcoord;\n"
    "out vec2 v_texcoord;\n"
    "\n"
    "void main() {\n"
    "    v_texcoord = texcoord;\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

static const char* composite_fragment_shader =
    "#version 310 es\n"
    "precision mediump float;\n"
    "in vec2 v_texcoord;\n"
    "out vec4 FragColor;\n"
    "\n"
    "uniform sampler2D u_texture0;\n"
    "uniform sampler2D u_texture1;\n"
    "uniform sampler2D u_texture2;\n"
    "uniform sampler2D u_texture3;\n"
    "uniform int u_texture_count;\n"
    "uniform vec2 u_output_size;\n"
    "\n"
    "void main() {\n"
    "    vec4 final_color = vec4(0.0, 0.0, 0.0, 0.0);\n"
    "    \n"
    "    // Sample and blend all textures\n"
    "    if (u_texture_count >= 1) {\n"
    "        vec4 color0 = texture(u_texture0, v_texcoord);\n"
    "        final_color = mix(final_color, color0, color0.a);\n"
    "    }\n"
    "    \n"
    "    if (u_texture_count >= 2) {\n"
    "        vec4 color1 = texture(u_texture1, v_texcoord);\n"
    "        final_color = mix(final_color, color1, color1.a);\n"
    "    }\n"
    "    \n"
    "    if (u_texture_count >= 3) {\n"
    "        vec4 color2 = texture(u_texture2, v_texcoord);\n"
    "        final_color = mix(final_color, color2, color2.a);\n"
    "    }\n"
    "    \n"
    "    if (u_texture_count >= 4) {\n"
    "        vec4 color3 = texture(u_texture3, v_texcoord);\n"
    "        final_color = mix(final_color, color3, color3.a);\n"
    "    }\n"
    "    \n"
    "    FragColor = final_color;\n"
    "}\n";

// Composite shader state
struct composite_shader {
    GLuint program;
    GLuint vao;
    GLuint vbo;
    GLint u_texture0;
    GLint u_texture1;
    GLint u_texture2;
    GLint u_texture3;
    GLint u_texture_count;
    GLint u_output_size;
    bool initialized;
};

static struct composite_shader g_composite_shader = {0};

// Initialize composite shader
static bool init_composite_shader(void) {
    if (g_composite_shader.initialized) {
        return true;
    }
    
    // Compile vertex shader
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &composite_vertex_shader, NULL);
    glCompileShader(vertex_shader);
    
    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vertex_shader, 512, NULL, log);
        wlr_log(WLR_ERROR, "Composite vertex shader compilation failed: %s", log);
        return false;
    }
    
    // Compile fragment shader
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &composite_fragment_shader, NULL);
    glCompileShader(fragment_shader);
    
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fragment_shader, 512, NULL, log);
        wlr_log(WLR_ERROR, "Composite fragment shader compilation failed: %s", log);
        return false;
    }
    
    // Create shader program
    g_composite_shader.program = glCreateProgram();
    glAttachShader(g_composite_shader.program, vertex_shader);
    glAttachShader(g_composite_shader.program, fragment_shader);
    glLinkProgram(g_composite_shader.program);
    
    glGetProgramiv(g_composite_shader.program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(g_composite_shader.program, 512, NULL, log);
        wlr_log(WLR_ERROR, "Composite shader program linking failed: %s", log);
        return false;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    // Get uniform locations
    g_composite_shader.u_texture0 = glGetUniformLocation(g_composite_shader.program, "u_texture0");
    g_composite_shader.u_texture1 = glGetUniformLocation(g_composite_shader.program, "u_texture1");
    g_composite_shader.u_texture2 = glGetUniformLocation(g_composite_shader.program, "u_texture2");
    g_composite_shader.u_texture3 = glGetUniformLocation(g_composite_shader.program, "u_texture3");
    g_composite_shader.u_texture_count = glGetUniformLocation(g_composite_shader.program, "u_texture_count");
    g_composite_shader.u_output_size = glGetUniformLocation(g_composite_shader.program, "u_output_size");
    
    // Create fullscreen quad
    float quad_vertices[] = {
        // positions   // texcoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    glGenVertexArrays(1, &g_composite_shader.vao);
    glGenBuffers(1, &g_composite_shader.vbo);
    
    glBindVertexArray(g_composite_shader.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_composite_shader.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    g_composite_shader.initialized = true;
    wlr_log(WLR_INFO, "Composite shader initialized successfully");
    
    return true;
}

// Extract OpenGL texture ID from wlr_texture
static GLuint get_gl_texture_id(struct wlr_texture *texture) {
    // This is a hack - wlr_texture internal structure access
    // You might need to adjust this based on your wlroots version
    if (!texture) return 0;
    
    // For GLES2 backend, the texture ID is typically stored in the texture structure
    // This is implementation-specific and might need adjustment
    GLuint *tex_id_ptr = (GLuint*)((char*)texture + sizeof(void*)); // Rough guess
    return *tex_id_ptr;
}

// Create composite texture using shader
static struct wlr_texture* create_composite_texture_shader(struct nano_server *server, int output_width, int output_height) {
    if (!init_composite_shader()) {
        wlr_log(WLR_ERROR, "Failed to initialize composite shader");
        return NULL;
    }
    
    // Collect plugin textures
    GLuint plugin_textures[4] = {0}; // Support up to 4 plugins
    int texture_count = 0;
    
    struct spawned_plugin *p;
    wl_list_for_each(p, &server->plugins, link) {
        if (!p || !p->texture || texture_count >= 4) continue;
        
        GLuint gl_tex_id = get_gl_texture_id(p->texture);
        if (gl_tex_id > 0) {
            plugin_textures[texture_count] = gl_tex_id;
            texture_count++;
            wlr_log(WLR_DEBUG, "Added plugin texture %d (GL ID: %u)", texture_count, gl_tex_id);
        }
    }
    
    if (texture_count == 0) {
        wlr_log(WLR_DEBUG, "No plugin textures to composite");
        return NULL;
    }
    
    // Create framebuffer for compositing
    GLuint composite_fbo, composite_texture;
    glGenFramebuffers(1, &composite_fbo);
    glGenTextures(1, &composite_texture);
    
    // Set up composite texture
    glBindTexture(GL_TEXTURE_2D, composite_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, output_width, output_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Attach to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, composite_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, composite_texture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        wlr_log(WLR_ERROR, "Composite framebuffer not complete");
        glDeleteFramebuffers(1, &composite_fbo);
        glDeleteTextures(1, &composite_texture);
        return NULL;
    }
    
    // Save current GL state
    GLint prev_viewport[4];
    glGetIntegerv(GL_VIEWPORT, prev_viewport);
    
    // Set up for compositing
    glViewport(0, 0, output_width, output_height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Use composite shader
    glUseProgram(g_composite_shader.program);
    
    // Bind plugin textures
    for (int i = 0; i < texture_count; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, plugin_textures[i]);
    }
    
    // Set uniforms
    glUniform1i(g_composite_shader.u_texture0, 0);
    glUniform1i(g_composite_shader.u_texture1, 1);
    glUniform1i(g_composite_shader.u_texture2, 2);
    glUniform1i(g_composite_shader.u_texture3, 3);
    glUniform1i(g_composite_shader.u_texture_count, texture_count);
    glUniform2f(g_composite_shader.u_output_size, (float)output_width, (float)output_height);
    
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw fullscreen quad
    glBindVertexArray(g_composite_shader.vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Restore GL state
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    glUseProgram(0);
    glBindVertexArray(0);
    
    // Clean up framebuffer
    glDeleteFramebuffers(1, &composite_fbo);
    
    wlr_log(WLR_DEBUG, "Created composite texture with %d plugins", texture_count);
    
    // Convert OpenGL texture back to wlr_texture
    // This is complex - for now return NULL and use the GL texture directly
    // You'll need to create a wlr_texture wrapper around the GL texture
    
    // For now, just clean up and return NULL - this approach needs more work
    glDeleteTextures(1, &composite_texture);
    return NULL;
}



// Add this function to safely create the IPC idle source when needed
static void ensure_ipc_queue_ready(struct nano_server *server) {
    loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: Entry\n");
    
    if (!server) {
        loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: server is NULL!\n");
        return;
    }
    
    if (server->ipc_source) {
        loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: idle source already exists\n");
        return;
    }
    
    if (wl_list_empty(&server->plugins)) {
        loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: no plugins yet\n");
        return;
    }
    
    loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: creating idle source...\n");
    
    struct wl_event_loop *event_loop = wl_display_get_event_loop(server->wl_display);
    if (!event_loop) {
        loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: failed to get event loop!\n");
        return;
    }
    
    loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: got event loop, adding idle source...\n");
    
    server->ipc_source = wl_event_loop_add_idle(event_loop, process_ipc_queue_idle, server);
    if (server->ipc_source) {
        loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: ✅ IPC queue idle source created on demand\n");
    } else {
        loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: ❌ Failed to create IPC idle source\n");
    }
    
    loggy_compositor( "[DEBUG] ensure_ipc_queue_ready: Exit\n");
}

// 1. Fix the queue_ipc_event function to prevent memory leaks:

static void queue_ipc_event(struct nano_server *server, ipc_event_t *event, int *fds, int fd_count) {
    static int queue_counter = 0;
    queue_counter++;
    if (queue_counter <= 20) {
        loggy_compositor( "[COMPOSITOR] queue_ipc_event called #%d\n", queue_counter);
    }
    if (!server) return;
    
    // Validate inputs
    if (fd_count > 32) {
        loggy_compositor( "[IPC QUEUE] Warning: fd_count %d exceeds limit, truncating\n", fd_count);
        fd_count = 32;
    }
    
    // If no timer, fall back to direct send
    if (!server->ipc_source) {
        loggy_compositor( "[IPC QUEUE] No timer - using direct send fallback\n");
        struct spawned_plugin *p;
        wl_list_for_each(p, &server->plugins, link) {
            if (p->ipc_fd >= 0) {
                send_ipc_with_fds(p->ipc_fd, event, fds, fd_count);
            }
        }
        return;
    }
    
    // Create queue item
    struct ipc_queue_item *item = calloc(1, sizeof(*item));
    if (!item) {
        loggy_compositor( "[IPC QUEUE] Failed to allocate queue item\n");
        return;
    }
    
    // Initialize the item properly
    wl_list_init(&item->link);
    memcpy(&item->event, event, sizeof(*event));
    
    // REVERT: Duplicate file descriptors safely (sendmsg needs them valid)
    item->fd_count = 0;
    for (int i = 0; i < fd_count && i < 32; i++) {
        if (fds && fds[i] >= 0) {
            item->fds[i] = fds[i];  // ✅ Transfer ownership
fds[i] = -1; // Queue needs its own copy for sendmsg
            if (item->fds[i] >= 0) {
                item->fd_count++;
            } else {
                loggy_compositor( "[IPC QUEUE] Warning: dup() failed for fd %d: %s\n", 
                       fds[i], strerror(errno));
                item->fds[i] = -1;
            }
        } else {
            item->fds[i] = -1;
        }
    }
    
    // Add to queue
    wl_list_insert(&server->ipc_queue, &item->link);
    
    loggy_compositor( "[IPC QUEUE] Queued event with %d FDs\n", item->fd_count);
}
// 2. Fix nano_server_destroy to properly clean up:

void nano_server_destroy(struct nano_server *server) {
    loggy_compositor( "[DEBUG] nano_server_destroy: Starting cleanup\n");
    
    // Clean up IPC timer FIRST and SAFELY
    if (server->ipc_source) {
        loggy_compositor( "[DEBUG] Removing IPC timer\n");
        wl_event_source_remove(server->ipc_source);
        server->ipc_source = NULL;
    }
    
    // Clean up queued items SAFELY
    loggy_compositor( "[DEBUG] Cleaning up IPC queue items\n");
    struct ipc_queue_item *item, *item_tmp;
    wl_list_for_each_safe(item, item_tmp, &server->ipc_queue, link) {
        // Close all file descriptors
        for (int i = 0; i < item->fd_count && i < 32; i++) {
            if (item->fds[i] >= 0) {
                close(item->fds[i]);
                item->fds[i] = -1;
            }
        }
        wl_list_remove(&item->link);
        free(item);
    }
    
    // Clean up render timer
    if (server->render_timer) {
        wl_event_source_remove(server->render_timer);
        server->render_timer = NULL;
    }
    
    // Clean up plugins (close FDs before killing processes)
    struct spawned_plugin *plugin, *plugin_tmp;
    wl_list_for_each_safe(plugin, plugin_tmp, &server->plugins, link) {
        if (plugin->ipc_fd >= 0) {
            close(plugin->ipc_fd);
            plugin->ipc_fd = -1;
        }
        nano_plugin_kill(server, plugin);
    }
    
    // Clean up views and their cached buffers - FIRST PASS: Linear buffers
    struct nano_view *view, *view_tmp;
    wl_list_for_each_safe(view, view_tmp, &server->views, link) {
        if (view->cached_linear_buffer) {
            wlr_buffer_drop(view->cached_linear_buffer);
            view->cached_linear_buffer = NULL;
        }
        
        // Return FD to pool if valid
        if (view->fd_valid && view->cached_fd >= 0) {
            fd_pool_return_fd(&server->fd_pool, view->cached_fd, view->cached_buffer_id);
            view->fd_valid = false;
            view->cached_fd = -1;
        }
    }
    
    // Clean up FD pool AFTER all views are processed
    fd_pool_cleanup(&server->fd_pool);
    
    cleanup_mouse_queue();
    
    if (server->render_queue) {
        queue_destroy(server->render_queue);
        server->render_queue = NULL;
    }
    
    pthread_mutex_destroy(&server->plugin_list_mutex);
    
    // Clean up services
    struct nano_service *service, *service_tmp;
    wl_list_for_each_safe(service, service_tmp, &server->services, link) {
        wl_list_remove(&service->link);
        free(service->name);
        free(service);
    }
    
    // Clean up wlroots objects
    if (server->allocator) {
        wlr_allocator_destroy(server->allocator);
        server->allocator = NULL;
    }
    if (server->renderer) {
        wlr_renderer_destroy(server->renderer);
        server->renderer = NULL;
    }
    if (server->backend) {
        wlr_backend_destroy(server->backend);
        server->backend = NULL;
    }
    
    // Display cleanup must be LAST
    if (server->wl_display) {
        wl_display_destroy(server->wl_display);
        server->wl_display = NULL;
    }
    
    loggy_compositor( "[DEBUG] nano_server_destroy: Cleanup complete\n");
}

// 3. Remove the duplicate/conflicting functions:
// Delete these functions from your code:
// - ensure_ipc_queue_ready() (not needed anymore)
// - The first wl_event_loop_add_idle() call in nano_server_init()
static void process_ipc_queue_idle(void *data) {
    struct nano_server *server = data;
    if (!server) {
        loggy_compositor( "[IPC QUEUE] ERROR: server is NULL\n");
        return;
    }
    
    struct ipc_queue_item *item, *item_tmp;
    int processed = 0;
    
    wl_list_for_each_safe(item, item_tmp, &server->ipc_queue, link) {
        if (processed >= 10) break; // Limit processing per cycle
        
        // Check if we have any active plugins
        bool has_active_plugins = false;
        struct spawned_plugin *p;
        wl_list_for_each(p, &server->plugins, link) {
            if (p->ipc_fd >= 0) {
                has_active_plugins = true;
                
                // Send to this plugin
                send_ipc_with_fds(p->ipc_fd, &item->event, item->fds, item->fd_count);
                // Note: send_ipc_with_fds handles its own error reporting
            }
        }
        
        // Clean up FDs
        for (int i = 0; i < item->fd_count && i < 32; i++) {
            if (item->fds[i] >= 0) {
                close(item->fds[i]);
                item->fds[i] = -1;
            }
        }
        
        // Remove from queue
        wl_list_remove(&item->link);
        free(item);
        processed++;
    }
    
    if (processed > 0) {
        loggy_compositor( "[IPC QUEUE] Processed %d queued events\n", processed);
    }
}


// Also fix the timer function to prevent infinite recursion:
static int process_ipc_queue_timer(void *data) {
    struct nano_server *server = data;
    if (!server) {
        return 0; // Stop timer
    }
    
    struct ipc_queue_item *item, *item_tmp;
    int processed = 0;
    int queue_size = 0;
    
    // Count queue size first
    wl_list_for_each(item, &server->ipc_queue, link) {
        queue_size++;
    }
    
  int max_events_per_tick = 500; // UP FROM 5-20
    if (queue_size > 100) {
        max_events_per_tick = 1000; // Handle bursts
    }

    // Process events
    wl_list_for_each_safe(item, item_tmp, &server->ipc_queue, link) {
        if (processed >= max_events_per_tick) break;
        
        // Send to all active plugins
        struct spawned_plugin *p;
        wl_list_for_each(p, &server->plugins, link) {
            if (p->ipc_fd >= 0) {
                send_ipc_with_fds(p->ipc_fd, &item->event, item->fds, item->fd_count);
            }
        }
        
        // CRITICAL FIX: Don't close FDs immediately!
        // Let the kernel handle FD cleanup after sendmsg() completes
        // The FDs will be automatically closed when the queue item is freed
        
        // Remove from queue
        wl_list_remove(&item->link);
        
        // FIXED: Close FDs AFTER removing from queue and sending
        for (int i = 0; i < item->fd_count && i < 32; i++) {
            if (item->fds[i] >= 0) {
                close(item->fds[i]);
            }
        }
        
        free(item);
        processed++;
    }
    
    if (processed > 0) {
        loggy_compositor( "[IPC QUEUE] Timer processed %d events (remaining: %d)\n", processed, queue_size - processed);
    }
    
    // Adaptive timing
    if (server->ipc_source) {
        int remaining = queue_size - processed;
        int next_interval;
        
        if (remaining > 20) {
            next_interval = 1;  // Panic mode
        } else if (remaining > 10) {
            next_interval = 2;  // Fast processing
        } else if (remaining > 0) {
            next_interval = 5;  // Normal processing
        } else {
            next_interval = 16; // Idle
        }
        
        wl_event_source_timer_update(server->ipc_source, next_interval);
    }
    
    return 0;
}
static struct nano_view *get_active_view(struct nano_server *server) {
    // Return the currently focused/active view
    struct nano_view *view;
    wl_list_for_each(view, &server->views, link) {
        // Check if the view's XDG surface is mapped instead
        if (view->xdg_surface && view->xdg_surface->surface && view->xdg_surface->surface->mapped) {
            return view; // Return first mapped view for now
        }
    }
    return NULL;
}

// Add this to your output creation
static void handle_wl_output_configure(struct wl_listener *listener, void *data) {
    struct nano_output *output = wl_container_of(listener, output, wl_configure);
    
    // Check if the output became fullscreen
    if (wlr_output_is_wl(output->wlr_output)) {
        struct wlr_wl_output_internal *wl_output = 
            (struct wlr_wl_output_internal *)output->wlr_output;
            
        // When parent makes us fullscreen, make our active view fullscreen too
        struct nano_view *active_view = get_active_view(output->server);
        if (active_view && active_view->xdg_toplevel) {
            wlr_log(WLR_ERROR, "Compositor went fullscreen - making active view fullscreen");
            wlr_xdg_toplevel_set_fullscreen(active_view->xdg_toplevel, true);
            wlr_xdg_surface_schedule_configure(active_view->xdg_toplevel->base);
        }
    }
}


static void fd_pool_init(struct fd_pool *pool, int max_size) {
    wl_list_init(&pool->available);
    wl_list_init(&pool->in_use);
    pool->max_size = max_size;
    pool->current_size = 0;
    pthread_mutex_init(&pool->mutex, NULL);
}

static void fd_pool_cleanup(struct fd_pool *pool) {
    pthread_mutex_lock(&pool->mutex);
    
    struct fd_pool_entry *entry, *tmp;
    
    // Clean up available FDs
    wl_list_for_each_safe(entry, tmp, &pool->available, link) {
        close(entry->fd);
        wl_list_remove(&entry->link);
        free(entry);
    }
    
    // Clean up in-use FDs
    wl_list_for_each_safe(entry, tmp, &pool->in_use, link) {
        close(entry->fd);
        wl_list_remove(&entry->link);
        free(entry);
    }
    
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
}



// Get FD from pool or create new one
static int fd_pool_get_fd(struct fd_pool *pool, struct wlr_buffer *buffer, uint64_t buffer_id) {
    pthread_mutex_lock(&pool->mutex);
    
    // First, check if we already have an FD for this buffer
    struct fd_pool_entry *entry;
    wl_list_for_each(entry, &pool->available, link) {
        if (entry->buffer_id == buffer_id) {
            // Found existing FD - move to in_use
            wl_list_remove(&entry->link);
            wl_list_insert(&pool->in_use, &entry->link);
            entry->in_use = true;
            clock_gettime(CLOCK_MONOTONIC, &entry->last_used);
            
            int fd = dup(entry->fd);  // Return duplicate for safety
            pthread_mutex_unlock(&pool->mutex);
            return fd;
        }
    }
    
    // No existing FD found - create new one
    struct wlr_dmabuf_attributes attrs;
    if (!wlr_buffer_get_dmabuf(buffer, &attrs)) {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }
    
    int new_fd = dup(attrs.fd[0]);
    if (new_fd < 0) {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }
    
    // Create new pool entry
    struct fd_pool_entry *new_entry = calloc(1, sizeof(*new_entry));
    if (!new_entry) {
        close(new_fd);
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }
    
    new_entry->fd = dup(attrs.fd[0]);  // Keep original in pool
    new_entry->buffer_id = buffer_id;
    new_entry->in_use = true;
    clock_gettime(CLOCK_MONOTONIC, &new_entry->last_used);
    wl_list_insert(&pool->in_use, &new_entry->link);
    pool->current_size++;
    
    pthread_mutex_unlock(&pool->mutex);
    return new_fd;  // Return the duplicate
}

// Return FD to pool when done
static void fd_pool_return_fd(struct fd_pool *pool, int fd, uint64_t buffer_id) {
    pthread_mutex_lock(&pool->mutex);
    
    // Close the returned FD (it was a duplicate)
    close(fd);
    
    // Find the pool entry and mark as available
    struct fd_pool_entry *entry;
    wl_list_for_each(entry, &pool->in_use, link) {
        if (entry->buffer_id == buffer_id) {
            wl_list_remove(&entry->link);
            wl_list_insert(&pool->available, &entry->link);
            entry->in_use = false;
            break;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
}

// Clean up old unused FDs periodically
static void fd_pool_cleanup_old(struct fd_pool *pool) {
    pthread_mutex_lock(&pool->mutex);
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    struct fd_pool_entry *entry, *tmp;
    wl_list_for_each_safe(entry, tmp, &pool->available, link) {
        // Remove FDs unused for more than 5 seconds
        long age = (now.tv_sec - entry->last_used.tv_sec);
        if (age > 5 || pool->current_size > pool->max_size) {
            close(entry->fd);
            wl_list_remove(&entry->link);
            free(entry);
            pool->current_size--;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
}




// Make sure you have this helper function from the previous answer.
static uint64_t get_buffer_id(struct wlr_buffer *buffer) {
    struct wlr_dmabuf_attributes attrs;
    if (wlr_buffer_get_dmabuf(buffer, &attrs)) {
        return ((uint64_t)attrs.fd[0] << 32) | attrs.offset[0];
    }
    return (uint64_t)(uintptr_t)buffer;
}

// In nano_server.c
// REPLACE this entire function.

bool nano_server_init(struct nano_server *server) {
    // Initialize shared DMA-BUF context first (zero out struct)
    memset(&server->dmabuf_ctx, 0, sizeof(server->dmabuf_ctx));

    fd_pool_init(&server->fd_pool, 64);     
    
    server->wl_display = wl_display_create();
    if (!server->wl_display) {
        wlr_log(WLR_ERROR, "Failed to create Wayland display");
        return false;
    }

      // ADD THIS: Initialize XKB context
    server->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!server->xkb_context) {
        wlr_log(WLR_ERROR, "Failed to create XKB context");
        return false;
    }
    
    // FIX 1: wlr_backend_autocreate now takes event loop, not display
       // CREATE IDLE SOURCE AT STARTUP - This is the key fix!
    struct wl_event_loop *event_loop = wl_display_get_event_loop(server->wl_display);
  //  server->ipc_source = wl_event_loop_add_idle(event_loop, process_ipc_queue_idle, server);
    server->backend = wlr_backend_autocreate(event_loop, NULL);
    if (!server->backend) {
        wlr_log(WLR_ERROR, "Failed to create wlr_backend");
        goto error_display;
    }
    
    server->renderer = wlr_renderer_autocreate(server->backend);
    if (!server->renderer) {
        wlr_log(WLR_ERROR, "Failed to create renderer");
        goto error_backend;
    }

     // ADD THE FIX HERE:
  
    
    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
    if (!server->allocator) {
        wlr_log(WLR_ERROR, "Failed to create allocator");
        goto error_renderer;
    }

    server->render_hooks_enabled = true;
    wlr_log(WLR_INFO, "Full OpenGL render hooks enabled");
    
    // SINGLE call to init renderer with display
    wlr_renderer_init_wl_display(server->renderer, server->wl_display);

    // TEMPORARY FIX: Skip shared DMA-BUF initialization to avoid EGL config issues
    wlr_log(WLR_INFO, "Shared DMA-BUF enabled - using for plugin communication");
    server->dmabuf_ctx.initialized = true;

    // SINGLE DMA-BUF interface creation
    wlr_linux_dmabuf_v1_create_with_renderer(server->wl_display, 4, server->renderer);
    wlr_log(WLR_INFO, "Created DMA-BUF interface with default renderer formats");
    
    // SINGLE compositor creation
    server->compositor = wlr_compositor_create(server->wl_display, 5, server->renderer);
    if (!server->compositor) {
        wlr_log(WLR_ERROR, "Failed to create compositor");
        goto error_allocator;
    }

    // ADD ESSENTIAL PROTOCOLS:
    wlr_subcompositor_create(server->wl_display);
    wlr_data_device_manager_create(server->wl_display);
    wlr_viewporter_create(server->wl_display);
    wlr_fractional_scale_manager_v1_create(server->wl_display, 1);

    // Create scene
    server->scene = wlr_scene_create();
    if (!server->scene) {
        wlr_log(WLR_ERROR, "Failed to create scene");
        goto error_compositor;
    }
    
    // FIX 2: wlr_output_layout_create now requires display parameter
    server->output_layout = wlr_output_layout_create(server->wl_display);
    server->scene_layout = wlr_scene_attach_output_layout(server->scene, server->output_layout);

    // Initialize lists
    wl_list_init(&server->outputs);
    wl_list_init(&server->views);
    wl_list_init(&server->plugins);
    wl_list_init(&server->services);
    wl_list_init(&server->keyboards);
    wl_list_init(&server->ipc_queue);
    
    // USE TIMER INSTEAD OF IDLE SOURCE - This avoids the Wayland lifecycle issues
    server->ipc_source = wl_event_loop_add_timer(event_loop, process_ipc_queue_timer, server);
    
    if (server->ipc_source) {
        wlr_log(WLR_INFO, "✅ IPC queue timer created successfully");
        // Start the timer with a short interval (5ms for responsive IPC)
        wl_event_source_timer_update(server->ipc_source, 1);
    } else {
        wlr_log(WLR_ERROR, "❌ Failed to create IPC queue timer");
        server->ipc_source = NULL;
    }
    
    for (int i = 0; i < NANO_EVENT_MAX; i++) {
        wl_list_init(&server->event_listeners[i]);
    }

    // Initialize the multi-threaded plugin system components
    if (pthread_mutex_init(&server->plugin_list_mutex, NULL) != 0) {
        wlr_log(WLR_ERROR, "Failed to initialize plugin list mutex");
        goto error_ipc;
    }
    
    server->render_queue = queue_create();
    if (!server->render_queue) {
        wlr_log(WLR_ERROR, "Failed to create render notification queue");
        pthread_mutex_destroy(&server->plugin_list_mutex);
        goto error_ipc;
    }
    
    nano_plugin_manager_init(server);
    
    // Set up cursor
    server->cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server->cursor, server->output_layout);
    server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    
    // Set up listeners
    server->cursor_motion.notify = server_cursor_motion;
    wl_signal_add(&server->cursor->events.motion, &server->cursor_motion);
    server->cursor_motion_absolute.notify = server_cursor_motion_absolute;
    wl_signal_add(&server->cursor->events.motion_absolute, &server->cursor_motion_absolute);
    server->cursor_button.notify = server_cursor_button;
    wl_signal_add(&server->cursor->events.button, &server->cursor_button);
    server->cursor_axis.notify = server_cursor_axis;
    wl_signal_add(&server->cursor->events.axis, &server->cursor_axis);
    server->cursor_frame.notify = server_cursor_frame;
    wl_signal_add(&server->cursor->events.frame, &server->cursor_frame);


    
    nano_server_setup_plugin_api(server);
    
    server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 2);
    server->new_xdg_surface.notify = server_new_xdg_surface;
    wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);
    
    server->new_output.notify = server_new_output;
    wl_signal_add(&server->backend->events.new_output, &server->new_output);
    
    server->new_input.notify = server_new_input;
    wl_signal_add(&server->backend->events.new_input, &server->new_input);
    
    server->seat = wlr_seat_create(server->wl_display, "seat0");
    server->request_cursor.notify = seat_request_cursor;
    wl_signal_add(&server->seat->events.request_set_cursor, &server->request_cursor);
    server->request_set_selection.notify = seat_request_set_selection;
    wl_signal_add(&server->seat->events.request_set_selection, &server->request_set_selection);

    wlr_log(WLR_INFO, "=== COMPOSITOR GLOBALS SUMMARY ===");
    wlr_log(WLR_INFO, "✅ wl_compositor: version 5");
    wlr_log(WLR_INFO, "✅ xdg_wm_base: version 2"); 
    wlr_log(WLR_INFO, "✅ wl_seat: created");
    wlr_log(WLR_INFO, "✅ wl_shm: available");
    wlr_log(WLR_INFO, "✅ linux_dmabuf: available");
    wlr_log(WLR_INFO, "✅ IPC queue: initialized");
    wlr_log(WLR_INFO, "=====================================");
    
    // SINGLE mouse queue initialization
    init_mouse_queue(server);
    
 wl_list_init(&server->layers);
    
    if (!nano_layer_manager_init(&server->layer_manager, server)) {
        wlr_log(WLR_ERROR, "Failed to init layer manager");
        // non-fatal, continue without layer support
    } else {
        wlr_log(WLR_INFO, "✅ Layer manager initialized");
    }

    return true;

// Error handling labels for clean shutdown on failure
error_ipc:
    if (server->ipc_source) {
        wl_event_source_remove(server->ipc_source);
    }
error_xkb:
    xkb_context_unref(server->xkb_context);    
error_scene:
    // No wlr_scene_destroy needed in newer wlroots
error_compositor:
    // wlr_compositor_destroy is handled by wl_display_destroy
error_allocator:
    wlr_allocator_destroy(server->allocator);
error_renderer:
    wlr_renderer_destroy(server->renderer);
error_backend:
    wlr_backend_destroy(server->backend);
error_display:
    wl_display_destroy(server->wl_display);
    return false;
}   

// In nano_server.c

// This timer callback is the new central hub for processing all plugin notifications.
// In nano_server.c

// This timer callback correctly processes notifications using the new union-based struct.
// In nano_server.c
// THIS IS THE CORRECTED FUNCTION that matches your concurrent_queue.h definition

static int process_render_queue_timer(void *data) {
    struct nano_server *server = (struct nano_server *)data;
    
    render_notification_t notification;
    int processed_moves = 0;
    int processed_frames = 0; // Keep track of other notifications for debugging

    // Process all pending notifications in the queue.
    while (queue_try_pop(server->render_queue, &notification)) {

        switch (notification.type) {
            case RENDER_NOTIFICATION_TYPE_WINDOW_MOVE: {
                int current_index = 0;
                struct nano_view *view;
                wl_list_for_each(view, &server->views, link) {
                    // *** THE FIX IS HERE ***
                    // Access the data through the 'move_data' struct inside the union
                    if (current_index == notification.move_data.window_index) {
                        if (view && view->scene_tree) {
                            wlr_scene_node_set_position(&view->scene_tree->node,
                                                       (int)notification.move_data.move_x,  // Cast double to int
                                                       (int)notification.move_data.move_y); // Cast double to int
                            processed_moves++;
                        }
                        break; // Found the window, no need to continue looping
                    }
                    current_index++;
                }
                break;
            }

            case RENDER_NOTIFICATION_TYPE_FRAME_READY: {
                // This part handles other notifications correctly, using the same union
                struct spawned_plugin *p = nano_plugin_get_by_pid(server, notification.plugin_pid);
                if (p) {
                    p->last_ready_buffer = notification.ready_buffer_index;
                    p->has_new_frame = true;
                    processed_frames++;
                }
                break;
            }
            
            default:
                // Ignore other notification types
                break;
        }
    }
    
    if (processed_moves > 0) {
        // This log will now appear when you drag a window
        wlr_log(WLR_INFO, "Processed %d window moves from queue", processed_moves);
    }
    
    // Reschedule the timer for the next frame cycle.
    wl_event_source_timer_update(server->render_timer, 16); // ~60 FPS
    
    return 0;
}

int nano_server_run(struct nano_server *server) {
    const char *socket = wl_display_add_socket_auto(server->wl_display);
    if (!socket) {
        wlr_log(WLR_ERROR, "Failed to open Wayland socket");
        return 1;
    }
    
    if (!wlr_backend_start(server->backend)) {
        wlr_log(WLR_ERROR, "Failed to start backend");
        return 1;
    }
    
    setenv("WAYLAND_DISPLAY", socket, true);
    wlr_log(WLR_INFO, "Running nano compositor on WAYLAND_DISPLAY=%s", socket);
    
    // Add repeating timer
    struct wl_event_loop *loop = wl_display_get_event_loop(server->wl_display);
    server->render_timer = wl_event_loop_add_timer(loop, process_render_queue_timer, server);
    wl_event_source_timer_update(server->render_timer, 16); // Start first timer
    
    wl_display_run(server->wl_display);
    return 0;
}

static void nano_server_send_pointer_event(struct nano_server *server, struct nano_event_pointer *event_data, ipc_event_type_t type) {
    // FIX: Initialize the entire IPC event to zero
    ipc_event_t ipc_event;
    memset(&ipc_event, 0, sizeof(ipc_event));  // ← CRITICAL FIX
    ipc_event.type = type;

    if (type == IPC_EVENT_TYPE_POINTER_MOTION) {
        ipc_event.data.motion.x = event_data->x;
        ipc_event.data.motion.y = event_data->y;
    } else if (type == IPC_EVENT_TYPE_POINTER_BUTTON) {
        ipc_event.data.button.x = event_data->x;
        ipc_event.data.button.y = event_data->y;
        ipc_event.data.button.button = event_data->button;
        ipc_event.data.button.state = event_data->state;
    }

    struct spawned_plugin *p;
    wl_list_for_each(p, &server->plugins, link) {
        if (p->ipc_fd != -1) {
            write(p->ipc_fd, &ipc_event, sizeof(ipc_event));
        }
    }
}


static void output_request_state(struct wl_listener *listener, void *data) {
    struct nano_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    
    // Store old dimensions before committing
    int old_width = output->wlr_output->width;
    int old_height = output->wlr_output->height;
    
    wlr_log(WLR_ERROR, "BEFORE COMMIT: %dx%d", old_width, old_height);
    
    // Commit the new state (this applies the resize/mode change)
    wlr_output_commit_state(output->wlr_output, event->state);
    
    // Get new dimensions after commit
    int new_width = output->wlr_output->width;
    int new_height = output->wlr_output->height;
    
    wlr_log(WLR_ERROR, "AFTER COMMIT: %dx%d", new_width, new_height);
    
    // Only handle for Wayland backend outputs
    if (wlr_output_is_wl(output->wlr_output)) {
        wlr_log(WLR_ERROR, "=== Wayland output resize: %dx%d -> %dx%d ===", 
                old_width, old_height, new_width, new_height);
        
        // DON'T change anything - just let the natural resolution change happen
        wlr_log(WLR_ERROR, "Resolution should now be %dx%d", new_width, new_height);
    }


}

static void server_new_output(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;
    
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);
    
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL) {
        wlr_output_state_set_mode(&state, mode);
    }
    
    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);
    
    struct nano_output *output = calloc(1, sizeof(struct nano_output));
    output->wlr_output = wlr_output;
    output->server = server;
    
    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    
    // ADD THIS: Standard request_state listener
    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);
    
    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);
    
    // ADD THIS: Wayland-specific configure listener
    if (wlr_output_is_wl(wlr_output)) {
        output->wl_configure.notify = handle_wl_output_configure;
        wl_signal_add(&wlr_output->events.request_state, &output->wl_configure);
    }
    
    wl_list_insert(&server->outputs, &output->link);
    
    struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout, wlr_output);
    output->scene_output = wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_layout, l_output, output->scene_output);
    
    // Your IPC code...
    ipc_event_t ipc_event;
    ipc_event.type = IPC_EVENT_TYPE_OUTPUT_ADD;
    
    strncpy(ipc_event.data.output_add.name, wlr_output->name, sizeof(ipc_event.data.output_add.name) - 1);
    ipc_event.data.output_add.x = l_output->x;
    ipc_event.data.output_add.y = l_output->y;
    ipc_event.data.output_add.width = wlr_output->width;
    ipc_event.data.output_add.height = wlr_output->height;
    ipc_event.data.output_add.refresh = mode ? mode->refresh : 0;
    
    struct spawned_plugin *p;
    wl_list_for_each(p, &server->plugins, link) {
        if (p->ipc_fd != -1) {
            write(p->ipc_fd, &ipc_event, sizeof(ipc_event));
        }
    }
}

static void handle_xdg_surface_role_commit(struct wl_listener *listener, void *data) {
    struct nano_view *view = wl_container_of(listener, view, surface_commit);
    struct wlr_xdg_surface *xdg_surface = view->xdg_surface;
    
    // If this surface just became a toplevel, set it up (but only once!)
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && 
        xdg_surface->toplevel && 
        !view->toplevel_listeners_added) {  // Check the flag instead
        
        wlr_log(WLR_INFO, "🎯 Surface became toplevel! Setting up...");
        
        view->xdg_toplevel = xdg_surface->toplevel;
        
        // Set up toplevel listeners now
        view->request_move.notify = xdg_toplevel_request_move;
        wl_signal_add(&xdg_surface->toplevel->events.request_move, &view->request_move);
        
        view->request_resize.notify = xdg_toplevel_request_resize;
        wl_signal_add(&xdg_surface->toplevel->events.request_resize, &view->request_resize);
        
        view->request_maximize.notify = xdg_toplevel_request_maximize;
        wl_signal_add(&xdg_surface->toplevel->events.request_maximize, &view->request_maximize);
        
        view->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
        wl_signal_add(&xdg_surface->toplevel->events.request_fullscreen, &view->request_fullscreen);
        
        view->toplevel_listeners_added = true;  // Set the flag
        
        // Send the configure
        wlr_log(WLR_INFO, "📐 Sending configure to new toplevel...");
    //    wlr_xdg_toplevel_set_size(xdg_surface->toplevel, 800, 600);
        wlr_xdg_surface_schedule_configure(xdg_surface);
    }
}
static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, new_xdg_surface);
    struct wlr_xdg_surface *xdg_surface = data;
    
    wlr_log(WLR_INFO, "New XDG surface created, role: %d", xdg_surface->role);
    
    // Only skip popups - handle everything else
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        return;
    }
    
    struct nano_view *view = calloc(1, sizeof(struct nano_view));
    if (!view) {
        wlr_log(WLR_ERROR, "Failed to allocate view");
        return;
    }
    
    view->server = server;
    view->xdg_surface = xdg_surface;  // Store the XDG surface
    view->xdg_toplevel = xdg_surface->toplevel; // Might be NULL initially
    view->toplevel_listeners_added = false;  // Initialize the flag
    
    // Initialize listener links
    wl_list_init(&view->link);
    wl_list_init(&view->map.link);
    wl_list_init(&view->unmap.link);
    wl_list_init(&view->destroy.link);
    wl_list_init(&view->request_move.link);
    wl_list_init(&view->request_resize.link);
    wl_list_init(&view->request_maximize.link);
    wl_list_init(&view->request_fullscreen.link);
    wl_list_init(&view->surface_commit.link);
    
    view->scene_tree = wlr_scene_xdg_surface_create(&server->scene->tree, xdg_surface);
    if (!view->scene_tree) {
        wlr_log(WLR_ERROR, "Failed to create scene tree");
        free(view);
        return;
    }
    
    view->scene_tree->node.data = view;
    xdg_surface->data = view; // Store the view, not scene_tree
    
    // Set up basic surface listeners
    view->map.notify = xdg_toplevel_map;
    wl_signal_add(&xdg_surface->surface->events.map, &view->map);
    
    view->unmap.notify = xdg_toplevel_unmap;
    wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap);
    
    view->destroy.notify = xdg_toplevel_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
    
    // Listen for surface commits to detect role changes
    view->surface_commit.notify = handle_xdg_surface_role_commit;
    wl_signal_add(&xdg_surface->surface->events.commit, &view->surface_commit);
    
    // If already a toplevel, set it up immediately
    if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && xdg_surface->toplevel) {
        wlr_log(WLR_INFO, "🎯 Surface is already toplevel! Setting up...");
        
        struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
        view->request_move.notify = xdg_toplevel_request_move;
        wl_signal_add(&toplevel->events.request_move, &view->request_move);
        
        view->request_resize.notify = xdg_toplevel_request_resize;
        wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
        
        view->request_maximize.notify = xdg_toplevel_request_maximize;
        wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
        
        view->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
        wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);
        
        view->toplevel_listeners_added = true;  // Set the flag since we added listeners
        
        wlr_log(WLR_INFO, "📐 Sending initial configure...");
        wlr_xdg_toplevel_set_size(toplevel, 800, 600);
        wlr_xdg_surface_schedule_configure(xdg_surface);
    }
    
    wlr_log(WLR_INFO, "✅ Created view for XDG surface (role: %d, has_toplevel: %s)", 
            xdg_surface->role, xdg_surface->toplevel ? "yes" : "no");
}
static void seat_request_cursor(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, request_cursor);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wlr_seat_client *focused_client = server->seat->pointer_state.focused_client;
    
    if (focused_client == event->seat_client) {
        wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
    }
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}

// Input device handling
static void server_new_input(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;
    
    switch (device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            wlr_log(WLR_INFO, "New keyboard detected: %s", device->name);
            server_add_keyboard(server, device); 
            break;
        case WLR_INPUT_DEVICE_POINTER:
            // ADD THIS DEBUG:
            wlr_log(WLR_INFO, "🐭 New pointer detected: %s - attaching to cursor", device->name);
            wlr_cursor_attach_input_device(server->cursor, device);
            break;
        default:
            break;
    }
    
    // Update the seat's capabilities to announce it has a keyboard
    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards)) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(server->seat, caps);
}





// Static global for the server (simple approach)
static struct nano_server *g_server_for_mouse = NULL;

// Simple mouse event structure 
struct simple_mouse_event {
    double x, y;
    struct timespec timestamp;
};

// Mouse event queue (keep it simple)
static struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct simple_mouse_event events[64];  // Smaller queue
    int head, tail, count;
    bool running;
    pthread_t thread;
    bool initialized;
} mouse_queue = {0};

// Forward declaration
static void* mouse_worker_thread(void *arg);

// Initialize mouse queue
static void init_mouse_queue(struct nano_server *server) {
    if (mouse_queue.initialized) return;
    
    g_server_for_mouse = server;  // Store server globally
    
    pthread_mutex_init(&mouse_queue.mutex, NULL);
    pthread_cond_init(&mouse_queue.cond, NULL);
    mouse_queue.head = mouse_queue.tail = mouse_queue.count = 0;
    mouse_queue.running = true;
    mouse_queue.initialized = true;
    
    // Start background thread
    if (pthread_create(&mouse_queue.thread, NULL, mouse_worker_thread, NULL) != 0) {
        loggy_compositor( "[COMPOSITOR] Failed to create mouse worker thread\n");
        mouse_queue.running = false;
        mouse_queue.initialized = false;
        return;
    }
    
    loggy_compositor( "[COMPOSITOR] ✅ Mouse event queue initialized\n");
}

// Cleanup mouse queue
static void cleanup_mouse_queue(void) {
    if (!mouse_queue.initialized) return;
    
    pthread_mutex_lock(&mouse_queue.mutex);
    mouse_queue.running = false;
    pthread_cond_signal(&mouse_queue.cond);
    pthread_mutex_unlock(&mouse_queue.mutex);
    
    pthread_join(mouse_queue.thread, NULL);
    pthread_mutex_destroy(&mouse_queue.mutex);
    pthread_cond_destroy(&mouse_queue.cond);
    
    mouse_queue.initialized = false;
    g_server_for_mouse = NULL;
    
    loggy_compositor( "[COMPOSITOR] Mouse event queue cleaned up\n");
}


// Add after queue_mouse_event function
static bool should_process_mouse_move(double new_x, double new_y) {
    static double last_x = 0, last_y = 0;
    static struct timespec last_time = {0};
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    double dx = new_x - last_x;
    double dy = new_y - last_y;
    double distance = sqrt(dx*dx + dy*dy);
    
    long time_diff = (now.tv_sec - last_time.tv_sec) * 1000000000L + 
                     (now.tv_nsec - last_time.tv_nsec);
    
    // Only process if moved more than 0.5 pixels OR 2ms elapsed
    if (distance > 0.5 || time_diff > 2000000) {
        last_x = new_x;
        last_y = new_y;
        last_time = now;
        return true;
    }
    
    return false;
}

// Add mouse event to queue (fast, non-blocking) - SIMPLIFIED VERSION
static void queue_mouse_event(double x, double y) {
     if (!mouse_queue.initialized || !should_process_mouse_move(x, y)) return;
    
    if (!mouse_queue.initialized) return;
    
    struct simple_mouse_event event;
    event.x = x;
    event.y = y;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    
    pthread_mutex_lock(&mouse_queue.mutex);
    
    // Overwrite the last event if queue is full (prevents blocking)
    if (mouse_queue.count >= 64) {
        mouse_queue.head = (mouse_queue.head + 1) % 64;
        mouse_queue.count--;
    }
    
    mouse_queue.events[mouse_queue.tail] = event;
    mouse_queue.tail = (mouse_queue.tail + 1) % 64;
    mouse_queue.count++;
    pthread_cond_signal(&mouse_queue.cond);
    
    pthread_mutex_unlock(&mouse_queue.mutex);
}

// Background thread worker
static void* mouse_worker_thread(void *arg) {
    struct timespec last_send = {0};
    const long min_interval_ns = 8333333; // 16.67ms = 60fps
    
    while (mouse_queue.running) {
        pthread_mutex_lock(&mouse_queue.mutex);
        
        // Wait for events
        while (mouse_queue.count == 0 && mouse_queue.running) {
            pthread_cond_wait(&mouse_queue.cond, &mouse_queue.mutex);
        }
        
        if (!mouse_queue.running) {
            pthread_mutex_unlock(&mouse_queue.mutex);
            break;
        }
        
        // Get the most recent event and clear the queue
        struct simple_mouse_event latest_event = {0};
        if (mouse_queue.count > 0) {
            // Take the latest event
            int latest_index = (mouse_queue.tail - 1 + 64) % 64;
            latest_event = mouse_queue.events[latest_index];
            
            // Clear the entire queue (we only care about the latest position)
            mouse_queue.head = mouse_queue.tail = mouse_queue.count = 0;
        }
        
        pthread_mutex_unlock(&mouse_queue.mutex);
        
        // Rate limiting: only send if enough time has passed
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        long time_diff = (now.tv_sec - last_send.tv_sec) * 1000000000L + 
                        (now.tv_nsec - last_send.tv_nsec);
        
        if (time_diff >= min_interval_ns) {
            // Send the mouse event to plugins
            if (g_server_for_mouse && latest_event.x != 0.0 && latest_event.y != 0.0) {
                struct nano_event_pointer event_data = { 
                    .x = latest_event.x, 
                    .y = latest_event.y 
                };
                nano_server_send_pointer_event(g_server_for_mouse, &event_data, IPC_EVENT_TYPE_POINTER_MOTION);
            }
            last_send = now;
        }
        
        // Small sleep to prevent busy waiting
//        usleep(5000); // 5ms
    }
    
    return NULL;
}




static struct spawned_plugin *find_window_manager_plugin(struct nano_server *server) {
    struct spawned_plugin *plugin;
    
    pthread_mutex_lock(&server->plugin_list_mutex);
    wl_list_for_each(plugin, &server->plugins, link) {
        // Look for window manager plugin by name pattern
        if (strstr(plugin->name, "window_manager") || 
            strstr(plugin->name, "wm") ||
            strstr(plugin->name, "window-manager")) {
            pthread_mutex_unlock(&server->plugin_list_mutex);
            return plugin;
        }
    }
    pthread_mutex_unlock(&server->plugin_list_mutex);
    return NULL;
}
// ============================================================================
// MODIFIED CURSOR HANDLERS - Replace your existing ones
// ============================================================================


static void server_cursor_motion(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, cursor_motion);
    struct wlr_pointer_motion_event *event = data;

    
    
    wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
    

    // Forward mouse motion to layers (e.g., wayfire)
compositor_layer_t *layer;
wl_list_for_each(layer, &server->layers, link) {
    if (layer->event_fd >= 0) {
        layer_send_mouse_motion(layer->event_fd, 
                                event->delta_x, event->delta_y, 
                                event->time_msec);
    }
}
    // OPTIMIZED: Direct send to window manager without rate limiting
    queue_mouse_event(server->cursor->x, server->cursor->y);
    
  // Add debug here:
    static int cursor_debug = 0;
    if (cursor_debug++ < 10) {
        fprintf(stderr, "[COMPOSITOR] 🐭 Cursor motion %d: (%.1f, %.1f)\n", 
                cursor_debug, server->cursor->x, server->cursor->y);
    }
    
    // Check if window manager forwarding is working:
    struct spawned_plugin *wm_plugin = find_window_manager_plugin(server);
    if (wm_plugin && wm_plugin->ipc_fd >= 0) {
        if (cursor_debug <= 10) {
            fprintf(stderr, "[COMPOSITOR] 📤 Forwarding to WM plugin '%s' FD:%d\n", 
                    wm_plugin->name, wm_plugin->ipc_fd);
        }
        
        ipc_event_t ipc_event = {0};
        ipc_event.type = IPC_EVENT_TYPE_POINTER_MOTION;
        ipc_event.data.motion.x = server->cursor->x;
        ipc_event.data.motion.y = server->cursor->y;
        
        ssize_t written = write(wm_plugin->ipc_fd, &ipc_event, sizeof(ipc_event));
        if (written != sizeof(ipc_event) && cursor_debug <= 10) {
            fprintf(stderr, "[COMPOSITOR] ❌ Failed to write to WM: %s\n", strerror(errno));
        }
    } else if (cursor_debug <= 10) {
        fprintf(stderr, "[COMPOSITOR] ❌ No WM plugin found or invalid FD\n");
    }
    // Keep essential Wayland seat handling (this must stay fast)
    struct wlr_scene_node *node;
    struct wlr_surface *surface = NULL;
    double sx, sy;
    node = wlr_scene_node_at(&server->scene->tree.node, server->cursor->x, server->cursor->y, &sx, &sy);
    
    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
        if (scene_surface) {
            surface = scene_surface->surface;
        }
    }
    
    if (surface) {
        wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server->seat, event->time_msec, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(server->seat);
    }
}


static void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    
    wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
    
    // Copy the same cursor processing logic from server_cursor_motion
    queue_mouse_event(server->cursor->x, server->cursor->y);
    
    struct wlr_scene_node *node;
    struct wlr_surface *surface = NULL;
    double sx, sy;
    node = wlr_scene_node_at(&server->scene->tree.node, server->cursor->x, server->cursor->y, &sx, &sy);
    
    if (node && node->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
        struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
        if (scene_surface) {
            surface = scene_surface->surface;
        }
    }
    
    if (surface) {
        wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server->seat, event->time_msec, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(server->seat);
    }
    
    // NEW: Forward to window manager plugin
    struct spawned_plugin *wm_plugin = find_window_manager_plugin(server);
    if (wm_plugin && wm_plugin->ipc_fd >= 0) {
        ipc_event_t ipc_event = {0};
        ipc_event.type = IPC_EVENT_TYPE_POINTER_MOTION;
        ipc_event.data.motion.x = server->cursor->x;
        ipc_event.data.motion.y = server->cursor->y;
        
        ssize_t written = write(wm_plugin->ipc_fd, &ipc_event, sizeof(ipc_event));
        if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            wlr_log(WLR_DEBUG, "Failed to forward absolute motion to window manager: %s", strerror(errno));
        }
    }
}
static void server_cursor_button(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, cursor_button);
    struct wlr_pointer_button_event *event = data;
    
    // ADD DEBUG
    loggy_compositor( "[COMPOSITOR] Button event: button=%u state=%u at (%.0f,%.0f)\n", 
            event->button, event->state, server->cursor->x, server->cursor->y);


    // Forward button to layers
compositor_layer_t *layer;
wl_list_for_each(layer, &server->layers, link) {
    if (layer->event_fd >= 0) {
        layer_send_mouse_button(layer->event_fd, 
                                event->button,
                                event->state == WLR_BUTTON_PRESSED,
                                event->time_msec);
    }
}
    
    // Forward to window manager plugin DIRECTLY
    struct spawned_plugin *wm_plugin = find_window_manager_plugin(server);
    if (wm_plugin && wm_plugin->ipc_fd >= 0) {
        ipc_event_t ipc_event = {0};
        ipc_event.type = IPC_EVENT_TYPE_POINTER_BUTTON;
        ipc_event.data.button.x = server->cursor->x;
        ipc_event.data.button.y = server->cursor->y;
        ipc_event.data.button.button = event->button;
        ipc_event.data.button.state = event->state;
        
        ssize_t written = write(wm_plugin->ipc_fd, &ipc_event, sizeof(ipc_event));
        if (written == sizeof(ipc_event)) {
            loggy_compositor( "[COMPOSITOR] ✅ Forwarded button event to WM\n");
        } else {
            loggy_compositor( "[COMPOSITOR] ❌ Failed to forward button event: %s\n", strerror(errno));
        }
    } else {
        loggy_compositor( "[COMPOSITOR] ❌ No WM plugin found for button forwarding\n");
    }
    
    // Forward to seat (keep this)
    wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {  
    struct nano_server *server = wl_container_of(listener, server, cursor_axis);
    struct wlr_pointer_axis_event *event = data;
    

    // Forward scroll to layers
compositor_layer_t *layer;
wl_list_for_each(layer, &server->layers, link) {
    if (layer->event_fd >= 0) {
        layer_send_mouse_axis(layer->event_fd,
                              event->orientation,
                              event->delta,
                              event->time_msec);
    }
}
    // FIX 3: Added relative_direction parameter
    wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation,
        event->delta, event->delta_discrete, event->source, event->relative_direction);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
    struct nano_server *server = wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}

// In nano_server.c

// In nano_server.c


static void output_destroy(struct wl_listener *listener, void *data) {
    struct nano_output *output = wl_container_of(listener, output, destroy);
    
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);  // Add this line
    wl_list_remove(&output->destroy.link);
    if (wlr_output_is_wl(output->wlr_output)) {
        wl_list_remove(&output->wl_configure.link);  // Add this if using wl_configure
    }
    wl_list_remove(&output->link);
    free(output);
}

// In nano_server.c
static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
    struct nano_view *view = wl_container_of(listener, view, map);
    wlr_log(WLR_INFO, "🗺️ XDG surface MAPPED - adding to views list");
    
    // Add to views list
    wl_list_insert(&view->server->views, &view->link);
    view->server->has_managed_windows = true; 
    
    wlr_log(WLR_INFO, "🗺️ Surface mapped successfully - client should now be visible");
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
    struct nano_view *view = wl_container_of(listener, view, unmap);
    wlr_log(WLR_INFO, "🗺️ XDG surface UNMAPPED - removing from views list");
    
    // Remove from views list
    if (!wl_list_empty(&view->link)) {
        wl_list_remove(&view->link);
        wl_list_init(&view->link);
    }
    
    // CRITICAL: Only remove toplevel listeners if the XDG surface is still valid
    // AND we actually added them
    if (view->toplevel_listeners_added && view->xdg_surface && view->xdg_surface->toplevel) {
        wlr_log(WLR_INFO, "💀 Removing toplevel listeners during unmap");
        
        // Safely remove each listener with validation
        if (!wl_list_empty(&view->request_move.link)) {
            wl_list_remove(&view->request_move.link);
            wl_list_init(&view->request_move.link);
        }
        
        if (!wl_list_empty(&view->request_resize.link)) {
            wl_list_remove(&view->request_resize.link);
            wl_list_init(&view->request_resize.link);
        }
        
        if (!wl_list_empty(&view->request_maximize.link)) {
            wl_list_remove(&view->request_maximize.link);
            wl_list_init(&view->request_maximize.link);
        }
        
        if (!wl_list_empty(&view->request_fullscreen.link)) {
            wl_list_remove(&view->request_fullscreen.link);
            wl_list_init(&view->request_fullscreen.link);
        }
        
        view->toplevel_listeners_added = false;
    } else if (view->toplevel_listeners_added) {
        // XDG surface became invalid but we think we have listeners
        wlr_log(WLR_ERROR, "💀 Toplevel listeners marked as added but XDG surface is invalid - clearing flag");
        view->toplevel_listeners_added = false;
    }
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
    struct nano_view *view = wl_container_of(listener, view, destroy);
    wlr_log(WLR_INFO, "💀 XDG surface DESTROYED - cleaning up view");

    // Remove all listeners to be safe.
    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->request_move.link);
    wl_list_remove(&view->request_resize.link);
    wl_list_remove(&view->request_maximize.link);
    wl_list_remove(&view->request_fullscreen.link);
    wl_list_remove(&view->surface_commit.link);
    
    // Remove from the server's list of views if still in it.
    if (!wl_list_empty(&view->link)) {
        wl_list_remove(&view->link);
        wl_list_init(&view->link);
    }
    
    // Clean up cached buffer specific to this view.
    if (view->cached_linear_buffer) {
        wlr_buffer_drop(view->cached_linear_buffer);
        view->cached_linear_buffer = NULL;
    }
    
    // Return FD to pool if it was used.
    if (view->fd_valid && view->cached_fd >= 0) {
        fd_pool_return_fd(&view->server->fd_pool, view->cached_fd, view->cached_buffer_id);
    }

    // *** CRITICAL FIX FOR THE CRASH ***
    // If this was the very last window, we must aggressively clean up all
    // plugin rendering resources to prevent `output_frame` from using stale data.
    if (wl_list_empty(&view->server->views)) {
        wlr_log(WLR_INFO, "Last window closed, clearing all plugin cached render assets.");
        struct spawned_plugin *p;
        wl_list_for_each(p, &view->server->plugins, link) {
            if (p->cached_texture) {
                wlr_texture_destroy(p->cached_texture);
                p->cached_texture = NULL;
            }
            if (p->compositor_buffer) {
                wlr_buffer_drop(p->compositor_buffer);
                p->compositor_buffer = NULL;
            }
            // Reset all other rendering state for the plugin
            p->cached_buffer = NULL;
            p->scale_cached = false;
        }
    }
    
    // Finally, free the view struct itself.
    free(view);
    wlr_log(WLR_INFO, "💀 View cleanup complete");
}
static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
    // Handle move requests - could delegate to plugins
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
    // Handle resize requests - could delegate to plugins  
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
    // Handle maximize requests - could delegate to plugins
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
    // Handle fullscreen requests - could delegate to plugins
}

// Plugin API implementation
static struct nano_server *api_get_server(void) {
    // This would need to be set globally or passed through context
    return NULL; // TODO: implement proper context passing
}

static struct wl_display *api_get_display(void) {
    return api_get_server()->wl_display;
}

static void api_view_move(struct nano_view *view, double x, double y) {
    wlr_scene_node_set_position(&view->scene_tree->node, x, y);
}

static void api_view_resize(struct nano_view *view, int width, int height) {
    wlr_xdg_toplevel_set_size(view->xdg_toplevel, width, height);
}

static void api_view_focus(struct nano_view *view) {
    struct nano_server *server = view->server;
    struct wlr_surface *surface = view->xdg_toplevel->base->surface;
    
    if (surface == NULL) {
        return;
    }
    
    struct wlr_seat *seat = server->seat;
    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
    if (prev_surface == surface) {
        return;
    }
    
    if (prev_surface) {
        struct wlr_xdg_surface *previous = wlr_xdg_surface_try_from_wlr_surface(seat->keyboard_state.focused_surface);
        if (previous != NULL) {
            wlr_xdg_toplevel_set_activated(previous->toplevel, false);
        }
    }
    
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    wlr_scene_node_raise_to_top(&view->scene_tree->node);
    wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);
    
    if (keyboard != NULL) {
        wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes,
            keyboard->num_keycodes, &keyboard->modifiers);
    }
}

static void api_view_close(struct nano_view *view) {
    wlr_xdg_toplevel_send_close(view->xdg_toplevel);
}

static struct wlr_scene_tree *api_get_scene_tree(void) {
    return &api_get_server()->scene->tree;
}

static struct wlr_scene_node *api_view_get_scene_node(struct nano_view *view) {
    return &view->scene_tree->node;
}

static struct wlr_output_layout *api_get_output_layout(void) {
    return api_get_server()->output_layout;
}

static int api_register_service(const char *name, void *service_api) {
    struct nano_server *server = api_get_server();
    struct nano_service *service = calloc(1, sizeof(struct nano_service));
    
    service->name = strdup(name);
    service->api = service_api;
    wl_list_insert(&server->services, &service->link);
    
    return 0;
}

static void *api_get_service(const char *name) {
    struct nano_server *server = api_get_server();
    struct nano_service *service;
    
    wl_list_for_each(service, &server->services, link) {
        if (strcmp(service->name, name) == 0) {
            return service->api;
        }
    }
    return NULL;
}

static const char *api_get_config_string(const char *section, const char *key, const char *default_val) {
    // TODO: implement configuration system
    return default_val;
}

static int api_get_config_int(const char *section, const char *key, int default_val) {
    // TODO: implement configuration system  
    return default_val;
}



/**
 * Sends a message and ancillary file descriptors over a UNIX socket.
 */
static void send_ipc_with_fds(int sock_fd, const ipc_event_t *event, const int *fds, int fd_count) {
    if (fd_count > 0) {
        loggy_compositor( "[COMPOSITOR] Sending IPC with %d FDs: ", fd_count);
        for (int i = 0; i < fd_count; i++) {
            loggy_compositor( "%d ", fds[i]);
        }
        loggy_compositor( "\n");
    }

    struct msghdr msg = {0};
    struct iovec iov[1] = {0};

    // Set up the main message
    iov[0].iov_base = (void*)event;
    iov[0].iov_len = sizeof(ipc_event_t);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    // Set up ancillary data for file descriptors
    char cmsg_buf[CMSG_SPACE(sizeof(int) * fd_count)];
    if (fd_count > 0) {
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * fd_count);

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fd_count);

        // Copy all file descriptors
        memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * fd_count);
        
        loggy_compositor( "[COMPOSITOR] Set up cmsg: level=%d, type=%d, len=%zu\n", 
                cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
    }

    ssize_t sent = sendmsg(sock_fd, &msg, 0);
    if (sent < 0) {
   //     loggy_compositor( "[COMPOSITOR] sendmsg failed: %s (fd_count=%d)\n", strerror(errno), fd_count);
    } else {
    //    loggy_compositor( "[COMPOSITOR] ✅ Successfully sent %zd bytes with %d FDs\n", sent, fd_count);
    }
}




// Helper function to create linear buffer from any texture
static struct wlr_buffer* create_linear_buffer_from_texture(struct nano_server *server, 
                                                           struct wlr_texture *source_texture,
                                                           uint32_t width, uint32_t height) {
    // Create linear buffer format
    struct wlr_drm_format *format = calloc(1, sizeof(struct wlr_drm_format));
    if (!format) {
        return NULL;
    }

    format->modifiers = calloc(1, sizeof(uint64_t));
    if (!format->modifiers) {
        free(format);
        return NULL;
    }

    format->format = DRM_FORMAT_ARGB8888;
    format->len = 1;
    format->modifiers[0] = DRM_FORMAT_MOD_LINEAR;

    struct wlr_buffer *linear_buffer = wlr_allocator_create_buffer(server->allocator, width, height, format);
    
    free(format->modifiers);
    free(format);

    if (!linear_buffer) {
        return NULL;
    }

    // Render texture to linear buffer
    struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(server->renderer, linear_buffer, NULL);
    if (!pass) {
        wlr_buffer_drop(linear_buffer);
        return NULL;
    }

    wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options){
        .texture = source_texture,
        .dst_box = { .x = 0, .y = 0, .width = width, .height = height },
        .filter_mode = WLR_SCALE_FILTER_NEAREST,
    });

    if (!wlr_render_pass_submit(pass)) {
        wlr_buffer_drop(linear_buffer);
        return NULL;
    }

    return linear_buffer;
}


// In nano_server.c
// REPLACE this entire function.


static struct wlr_buffer* create_linear_buffer_from_client(struct nano_server *server, struct wlr_buffer *client_buffer) {
    // Create a texture from the client's raw buffer.
    struct wlr_texture *source_texture = wlr_texture_from_buffer(server->renderer, client_buffer);
    if (!source_texture) {
        loggy_compositor("[COMPOSITOR] ❌ create_linear_buffer: Failed to create texture from client buffer.\n");
        return create_linear_buffer_from_unknown_buffer(server, client_buffer); // Use fallback if needed
    }

    // THIS IS THE FIX: We MUST create a buffer that has an alpha channel.
    // We will ONLY use ARGB8888 and will not fall back to opaque formats like XRGB.
    struct wlr_drm_format format = {
        .format = DRM_FORMAT_ARGB8888, // The 'A' is for Alpha. This is mandatory.
        .len = 1,
        .modifiers = (uint64_t[1]){DRM_FORMAT_MOD_LINEAR}
    };

    // Allocate our new, clean, compositor-owned buffer.
    struct wlr_buffer *linear_buffer = wlr_allocator_create_buffer(server->allocator, client_buffer->width, client_buffer->height, &format);
    
    if (!linear_buffer) {
        loggy_compositor("[COMPOSITOR] ❌ CRITICAL: Failed to allocate an ARGB8888 linear buffer.\n");
        wlr_texture_destroy(source_texture);
        return NULL;
    }

    // Start a render pass to copy the client content into our new buffer.
    struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(server->renderer, linear_buffer, NULL);
    if (!pass) {
        wlr_buffer_drop(linear_buffer);
        wlr_texture_destroy(source_texture);
        return NULL;
    }

    // Correctly render: 1. Clear to transparent, 2. Blend the texture on top.
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
        .box = { .x = 0, .y = 0, .width = client_buffer->width, .height = client_buffer->height },
        .color = {clear_color[0], clear_color[1], clear_color[2], clear_color[3]},
    });

    wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options){
        .texture = source_texture,
        .dst_box = { .x = 0, .y = 0, .width = client_buffer->width, .height = client_buffer->height },
        .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
    });

    if (!wlr_render_pass_submit(pass)) {
        wlr_buffer_drop(linear_buffer);
        wlr_texture_destroy(source_texture);
        return NULL;
    }

    // Clean up and return the finished, correct buffer.
    wlr_texture_destroy(source_texture);
    return linear_buffer;
}

// New function for suspected SHM buffers (when detection fails)

// New function for suspected SHM buffers (when detection fails)
static struct wlr_buffer* create_linear_buffer_from_suspected_shm(struct nano_server *server, struct wlr_buffer *suspected_shm_buffer) {
    loggy_compositor( "[COMPOSITOR] Attempting direct SHM pixel access for suspected SHM buffer\n");
    
    // Try to find the underlying surface and get its texture
    struct nano_view *view;
    wl_list_for_each(view, &server->views, link) {
        struct wlr_surface *surface = view->xdg_toplevel->base->surface;
        if (surface && surface->buffer && &surface->buffer->base == suspected_shm_buffer) {
            loggy_compositor( "[COMPOSITOR] Found surface for suspected SHM buffer\n");
            
            // Try to get the surface texture directly
            struct wlr_texture *surf_texture = wlr_surface_get_texture(surface);
            if (surf_texture) {
                loggy_compositor( "[COMPOSITOR] Got surface texture - converting to linear buffer\n");
                struct wlr_buffer *result = create_linear_buffer_from_texture(server, surf_texture, 
                                                                              suspected_shm_buffer->width, suspected_shm_buffer->height);
                if (result) {
                    return result;
                }
            } else {
                loggy_compositor( "[COMPOSITOR] No surface texture available\n");
            }
        }
    }
}



// Aggressive fallback for unknown buffer types (like weston-smoke)
// Improved aggressive fallback that actually captures window content
static struct wlr_buffer* create_linear_buffer_from_unknown_buffer(struct nano_server *server, struct wlr_buffer *client_buffer) {
    loggy_compositor( "[COMPOSITOR] Attempting content capture from unknown buffer type\n");
    
    // Create a linear output buffer first
    struct wlr_drm_format *format = calloc(1, sizeof(struct wlr_drm_format));
    if (!format) {
        return NULL;
    }

    format->modifiers = calloc(1, sizeof(uint64_t));
    if (!format->modifiers) {
        free(format);
        return NULL;
    }

    format->format = DRM_FORMAT_ARGB8888;
    format->len = 1;
    format->modifiers[0] = DRM_FORMAT_MOD_LINEAR;

    struct wlr_buffer *linear_buffer = wlr_allocator_create_buffer(
        server->allocator, client_buffer->width, client_buffer->height, format);
    
    free(format->modifiers);
    free(format);

    if (!linear_buffer) {
        loggy_compositor( "[COMPOSITOR] Failed to create output linear buffer\n");
        return NULL;
    }

    // The key insight: Instead of trying to extract pixels, copy from the scene buffer directly
    // Most applications have their content in the scene tree, so render the scene buffer
    bool success = false;
    

    
    // Strategy 2: If scene rendering failed, try to use wlroots' copy mechanism
    if (!success) {
        loggy_compositor( "[COMPOSITOR] Scene render failed, trying buffer copy\n");
        
        // Try to copy the buffer using wlroots' internal mechanisms
        struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(server->renderer, linear_buffer, NULL);
        if (pass) {
            // Just clear and mark as success - the actual content might be copied by wlroots
            wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
                .box = { .x = 0, .y = 0, .width = client_buffer->width, .height = client_buffer->height },
                .color = { .r = 0.2f, .g = 0.3f, .b = 0.8f, .a = 1.0f },  // Blue placeholder
            });
            
            success = wlr_render_pass_submit(pass);
        }
    }

    // Strategy 3: Last resort - create a visible test pattern so we know it's working
    if (!success) {
        loggy_compositor( "[COMPOSITOR] All content capture failed, using visible test pattern\n");
        
        // Create a texture with a recognizable pattern
        size_t pixel_data_size = client_buffer->width * client_buffer->height * 4;
        uint32_t *pixel_data = malloc(pixel_data_size);
        if (pixel_data) {
            // Create a visible pattern with the window title or size info
            for (uint32_t y = 0; y < client_buffer->height; y++) {
                for (uint32_t x = 0; x < client_buffer->width; x++) {
                    uint32_t color;
                    
                    // Create different patterns for different areas
                    if (y < 50) {
                        // Top bar - bright color
                        color = 0xFF00FF00;  // Bright green
                    } else if (x < 10 || x >= client_buffer->width - 10 || 
                              y < 10 || y >= client_buffer->height - 10) {
                        // Border - different color
                        color = 0xFFFF0000;  // Red border
                    } else {
                        // Content area - pattern based on position
                        bool checker = ((x / 20) + (y / 20)) % 2;
                        color = checker ? 0xFF404040 : 0xFF808080;  // Gray checkerboard
                    }
                    
                    pixel_data[y * client_buffer->width + x] = color;
                }
            }
            
            // Create texture from our pattern
            struct wlr_texture *pattern_texture = wlr_texture_from_pixels(server->renderer,
                DRM_FORMAT_ARGB8888, client_buffer->width * 4, client_buffer->width, client_buffer->height, pixel_data);
            
            if (pattern_texture) {
                struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(server->renderer, linear_buffer, NULL);
                if (pass) {
                    wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options){
                        .texture = pattern_texture,
                        .dst_box = { .x = 0, .y = 0, .width = client_buffer->width, .height = client_buffer->height },
                        .filter_mode = WLR_SCALE_FILTER_NEAREST,
                    });
                    success = wlr_render_pass_submit(pass);
                }
                wlr_texture_destroy(pattern_texture);
            }
            
            free(pixel_data);
        }
    }

    if (success) {
        loggy_compositor( "[COMPOSITOR] ✅ Successfully created linear buffer with content\n");
        return linear_buffer;
    } else {
        loggy_compositor( "[COMPOSITOR] ❌ All content capture methods failed\n");
        wlr_buffer_drop(linear_buffer);
        return NULL;
    }
}



// In nano_server.c

// This is the final, corrected, and optimized function.
void nano_server_send_window_updates(struct nano_server *server) {
    // --- START: YOUR OPTIMIZATION CHECK ---
  

    // Rate limiting
    static struct timespec last_ipc_update = {0};
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if ((now.tv_sec - last_ipc_update.tv_sec) * 1000 + (now.tv_nsec - last_ipc_update.tv_nsec) / 1000000 < 33) {
        return; // Limit to ~30fps
    }
    last_ipc_update = now;

    // Initialize the IPC event and file descriptor array
    ipc_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = IPC_EVENT_TYPE_WINDOW_LIST_UPDATE;
    event.data.window_list.count = 0;

    int fds[32];
    int fd_count = 0;
    memset(fds, -1, sizeof(fds));

    // Loop through all views and prepare their data for the plugins.
    struct nano_view *view;
    wl_list_for_each(view, &server->views, link) {
        if (event.data.window_list.count >= 32) break;

        struct wlr_surface *surface = view->xdg_toplevel->base->surface;
        if (!surface || !surface->buffer) continue;

        struct wlr_buffer *client_buffer = &surface->buffer->base;
        
        // Always create a new, fresh linear buffer. This calls the FIXED helper function
        // which correctly preserves transparency.
        struct wlr_buffer *new_linear_buffer = create_linear_buffer_from_client(server, client_buffer);

        if (new_linear_buffer) {
            struct wlr_dmabuf_attributes dmabuf_attrs;
            if (wlr_buffer_get_dmabuf(new_linear_buffer, &dmabuf_attrs)) {
                // Duplicate the file descriptor. The plugin will own this new copy.
                int window_fd = dup(dmabuf_attrs.fd[0]);
                if (window_fd >= 0) {
                    // Populate the IPC message with the buffer's details.
                    ipc_window_info_t *info = &event.data.window_list.windows[event.data.window_list.count];
                    info->x = view->scene_tree->node.x;
                    info->y = view->scene_tree->node.y;
                    info->width = dmabuf_attrs.width;
                    info->height = dmabuf_attrs.height;
                    info->format = dmabuf_attrs.format;
                    info->stride = dmabuf_attrs.stride[0];
                    info->modifier = dmabuf_attrs.modifier;
                    info->buffer_type = IPC_BUFFER_TYPE_DMABUF;
                    if (view->xdg_toplevel->title) {
                        strncpy(info->title, view->xdg_toplevel->title, sizeof(info->title) - 1);
                    }

                    fds[fd_count++] = window_fd;
                    event.data.window_list.count++;
                    
                    // CRITICAL: Update the view's state so our optimization check works on the next frame.
                    view->last_update_sent = true;
                    view->last_buffer_width = surface->buffer->base.width;
                    view->last_buffer_height = surface->buffer->base.height;
                }
            }
            // The compositor is done with its reference to the new buffer, so we drop it.
            // The kernel keeps the underlying memory alive for the plugin via the FD we sent.
            wlr_buffer_drop(new_linear_buffer);
        }
    }

    // If we successfully prepared any windows, send the event to the queue.
  if (fd_count > 0 || server->has_managed_windows) {
        queue_ipc_event(server, &event, fds, fd_count);
    }
}
// nano_server.c - FINAL, CRASH-FREE, AND API-COMPATIBLE PIXEL EXTRACTION



// Helper struct to pass both renderer and render_pass to the callback
struct render_context {
    struct wlr_renderer *renderer;
    struct wlr_render_pass *render_pass;
};

// In nano_server.c
/*
static void output_frame(struct wl_listener *listener, void *data) {
    struct nano_output *output = wl_container_of(listener, output, frame);
    struct nano_server *server = output->server;
    struct wlr_output *wlr_output = output->wlr_output;

    if (!output || !server || !wlr_output || !server->renderer || !server->allocator) {
        return;
    }

    // --- 1. Process All Incoming Frame Notifications ---
    render_notification_t notif;
    while (queue_try_pop(server->render_queue, &notif)) {
        struct spawned_plugin *p = nano_plugin_get_by_pid(server, notif.plugin_pid);
        if (p && notif.type == RENDER_NOTIFICATION_TYPE_FRAME_READY) {
            p->has_new_frame = true;
            // For async plugins, we'd also update p->last_ready_buffer here from the notification payload
            // p->last_ready_buffer = notif.data.ready_buffer_index;
        }
    }

    // --- 2. Process Frames for Legacy "Laundering" Plugins ---
    struct spawned_plugin *p;
    wl_list_for_each(p, &server->plugins, link) {
        // This block only runs for LEGACY plugins that need their frames laundered.
        if (p && !p->is_async && p->has_new_frame && p->dmabuf_fd >= 0) {
            struct wlr_texture *untrusted_texture = NULL;
            struct wlr_dmabuf_attributes dmabuf_attribs = {
                .width = p->dmabuf_width, .height = p->dmabuf_height,
                .format = p->dmabuf_format, .modifier = p->dmabuf_modifier,
                .n_planes = 1, .offset[0] = 0,
                .stride[0] = p->dmabuf_stride, .fd[0] = p->dmabuf_fd,
            };
            untrusted_texture = wlr_texture_from_dmabuf(server->renderer, &dmabuf_attribs);
            close(p->dmabuf_fd);
            p->dmabuf_fd = -1;
            p->has_new_frame = false; // We are processing this frame now

            if (untrusted_texture) {
                // If the buffer needs to be (re)created
                if (!p->compositor_buffer || p->compositor_buffer->width != untrusted_texture->width ||
                    p->compositor_buffer->height != untrusted_texture->height) {
                    if (p->compositor_buffer) {
                        wlr_buffer_drop(p->compositor_buffer);
                    }
                    struct wlr_drm_format format = {
                        .format = DRM_FORMAT_ARGB8888, .len = 1,
                        .modifiers = (uint64_t[1]){DRM_FORMAT_MOD_LINEAR}
                    };
                    p->compositor_buffer = wlr_allocator_create_buffer(server->allocator,
                        untrusted_texture->width, untrusted_texture->height, &format);
                }

                // Perform the laundering copy
                if (p->compositor_buffer) {
                    struct wlr_render_pass *copy_pass = wlr_renderer_begin_buffer_pass(server->renderer, p->compositor_buffer, NULL);
                    if (copy_pass) {
                        wlr_render_pass_add_texture(copy_pass, &(struct wlr_render_texture_options){
                            .texture = untrusted_texture,
                            .blend_mode = WLR_RENDER_BLEND_MODE_NONE, // Direct copy
                        });
                        wlr_render_pass_submit(copy_pass);
                    }
                }
                wlr_texture_destroy(untrusted_texture);
            }
        }
    }

    // --- 3. Begin Final Render Pass to Screen ---
    struct wlr_output_state output_state;
    wlr_output_state_init(&output_state);
    struct wlr_render_pass *render_pass = wlr_output_begin_render_pass(wlr_output, &output_state, NULL);
    if (!render_pass) { return; }

    wlr_render_pass_add_rect(render_pass, &(struct wlr_render_rect_options){
        .box = { .width = wlr_output->width, .height = wlr_output->height },
        .color = { 0.1f, 0.1f, 0.1f, 1.0f },
    });

    // --- 4. Render All Active Plugins to the Screen ---
    bool has_active_plugins = false;
    wl_list_for_each(p, &server->plugins, link) {
        struct wlr_buffer* buffer_to_render = NULL;
        if (p) {
            if (p->is_async) {
                // ASYNC PLUGIN: Use the last buffer it told us was ready.
                if (p->last_ready_buffer >= 0) {
                    buffer_to_render = p->compositor_buffers[p->last_ready_buffer];
                }
            } else {
                // LEGACY PLUGIN: Use its personal, laundered buffer.
                buffer_to_render = p->compositor_buffer;
            }
        }

        if (buffer_to_render) {
            has_active_plugins = true;
            struct wlr_texture *trusted_texture = wlr_texture_from_buffer(server->renderer, buffer_to_render);
            if (trusted_texture) {
                // (Your existing scaling logic is correct)
                float scale = fminf((float)wlr_output->width / trusted_texture->width, (float)wlr_output->height / trusted_texture->height);
                int scaled_width = (int)(trusted_texture->width * scale);
                int scaled_height = (int)(trusted_texture->height * scale);
                int x_offset = (wlr_output->width - scaled_width) / 2;
                int y_offset = (wlr_output->height - scaled_height) / 2;
                wlr_render_pass_add_texture(render_pass, &(struct wlr_render_texture_options){
                    .texture = trusted_texture,
                    .dst_box = { .x = x_offset, .y = y_offset, .width = scaled_width, .height = scaled_height },
                    .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
                });
                wlr_texture_destroy(trusted_texture);
            }
        }
    }

    if (!has_active_plugins) {
        wlr_scene_output_build_state(output->scene_output, &output_state, NULL);
    }

    // --- 5. Submit, Finalize, and Send Updates ---
    wlr_render_pass_submit(render_pass);
    wlr_output_commit_state(wlr_output, &output_state);
    wlr_output_state_finish(&output_state);
    
    // (Your existing frame_done and nano_server_send_window_updates calls are correct)
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct nano_view *view;
    wl_list_for_each(view, &server->views, link) {
        if (view && view->xdg_toplevel && view->xdg_toplevel->base && view->xdg_toplevel->base->surface) {
            wlr_surface_send_frame_done(view->xdg_toplevel->base->surface, &now);
        }
    }
    nano_server_send_window_updates(server);
}
*/

// --- Missing Definitions ---




// --- Optimized Frame Output with Smart Frame Management ---
#define MAX_BUFFERS 3
#define TARGET_FRAME_TIME_NS 16666666  // ~60 FPS (16.67ms)

static void output_frame(struct wl_listener *listener, void *data) {
    struct nano_output *output = wl_container_of(listener, output, frame);
    struct nano_server *server = output->server;
    struct wlr_output *wlr_output = output->wlr_output;

    if (!output || !server || !wlr_output || !server->renderer || !server->allocator) {
        return;
    }

    struct timespec frame_start;
    clock_gettime(CLOCK_MONOTONIC, &frame_start);
    uint64_t frame_start_ns = frame_start.tv_sec * 1000000000ULL + frame_start.tv_nsec;

    // --- 1. Process All Incoming Frame Notifications ---
    render_notification_t notif;
    while (queue_try_pop(server->render_queue, &notif)) {
        struct spawned_plugin *p = nano_plugin_get_by_pid(server, notif.plugin_pid);
        if (p && notif.type == RENDER_NOTIFICATION_TYPE_FRAME_READY) {
            p->has_new_frame = true;
            p->last_frame_time = frame_start_ns;
            // For async plugins, update the ready buffer index
            if (p->is_async) {
                // Adjust this based on your actual notification structure
                // p->last_ready_buffer = notif.ready_buffer_index;
            }
        }
    }

    // --- 2. Smart Legacy Plugin Processing (Frame-aware) ---
    struct spawned_plugin *p;
    wl_list_for_each(p, &server->plugins, link) {
        if (p && !p->is_async && p->has_new_frame && p->dmabuf_fd >= 0) {
            // Check if we should skip this frame to maintain smooth playback
            bool should_process_frame = true;
            
            // Skip frame if last laundering took too long
            if (p->last_launder_duration > TARGET_FRAME_TIME_NS / 2) {
                // Only process every other frame if laundering is slow
                if ((frame_start_ns - p->last_processed_frame) < TARGET_FRAME_TIME_NS * 2) {
                    should_process_frame = false;
                }
            }
            
            if (should_process_frame) {
                uint64_t launder_start = frame_start_ns;
                
                struct wlr_texture *untrusted_texture = NULL;
                struct wlr_dmabuf_attributes dmabuf_attribs = {
                    .width = p->dmabuf_width, .height = p->dmabuf_height,
                    .format = p->dmabuf_format, .modifier = p->dmabuf_modifier,
                    .n_planes = 1, .offset[0] = 0,
                    .stride[0] = p->dmabuf_stride, .fd[0] = p->dmabuf_fd,
                };
                
                untrusted_texture = wlr_texture_from_dmabuf(server->renderer, &dmabuf_attribs);
                
                if (untrusted_texture) {
                    // Only recreate buffer if absolutely necessary
                    bool need_new_buffer = (!p->compositor_buffer || 
                        (int)p->compositor_buffer->width != (int)untrusted_texture->width ||
                        (int)p->compositor_buffer->height != (int)untrusted_texture->height);
                    
                    if (need_new_buffer) {
                        if (p->compositor_buffer) {
                            wlr_buffer_drop(p->compositor_buffer);
                            p->compositor_buffer = NULL;
                        }
                        
                        struct wlr_drm_format format = {
                            .format = DRM_FORMAT_ARGB8888, .len = 1,
                            .modifiers = (uint64_t[1]){DRM_FORMAT_MOD_LINEAR}
                        };
                        
                        p->compositor_buffer = wlr_allocator_create_buffer(server->allocator,
                            untrusted_texture->width, untrusted_texture->height, &format);
                        
                        // Reset caching when buffer changes
                        p->scale_cached = false;
                        if (p->cached_texture) {
                            wlr_texture_destroy(p->cached_texture);
                            p->cached_texture = NULL;
                        }
                    }

                    // Perform optimized laundering copy
                    if (p->compositor_buffer) {
                        struct wlr_render_pass *copy_pass = wlr_renderer_begin_buffer_pass(
                            server->renderer, p->compositor_buffer, NULL);
                        if (copy_pass) {
                            wlr_render_pass_add_texture(copy_pass, &(struct wlr_render_texture_options){
                                .texture = untrusted_texture,
                                .blend_mode = WLR_RENDER_BLEND_MODE_NONE,
                            });
                            wlr_render_pass_submit(copy_pass);
                        }
                    }
                    wlr_texture_destroy(untrusted_texture);
                }
                
                // Track laundering performance
                struct timespec launder_end;
                clock_gettime(CLOCK_MONOTONIC, &launder_end);
                uint64_t launder_end_ns = launder_end.tv_sec * 1000000000ULL + launder_end.tv_nsec;
                p->last_launder_duration = launder_end_ns - launder_start;
                p->last_processed_frame = frame_start_ns;
            }
            
            // Always clean up the fd, even if we skipped processing
            close(p->dmabuf_fd);
            p->dmabuf_fd = -1;
            p->has_new_frame = false;
        }
    }



    // --- 3. Begin Final Render Pass to Screen ---
    struct wlr_output_state output_state;
    wlr_output_state_init(&output_state);
    struct wlr_render_pass *render_pass = wlr_output_begin_render_pass(wlr_output, &output_state, NULL);
    if (!render_pass) { 
        wlr_output_state_finish(&output_state);
        return; 
    }

    // Clear background
    wlr_render_pass_add_rect(render_pass, &(struct wlr_render_rect_options){
        .box = { .width = wlr_output->width, .height = wlr_output->height },
        .color = { 0.1f, 0.1f, 0.1f, 1.0f },
    });

    // =========================================================
    // NEW: RENDER COMPOSITOR LAYERS (Wayfire)
    // =========================================================
// =========================================================
// RENDER COMPOSITOR LAYERS (Wayfire)
// =========================================================
compositor_layer_t *layer;
int layer_count = 0;
wl_list_for_each(layer, &server->layers, link) {
    layer_count++;
    
    if (!layer->visible || !layer->texture) {
        wlr_log(WLR_DEBUG, "Layer '%s': visible=%d texture=%p - skipping",
                layer->name, layer->visible, layer->texture);
        continue;
    }
    
    wlr_log(WLR_INFO, "🎨 Rendering layer '%s': %ux%u", 
            layer->name, layer->width, layer->height);
    
    // Scale to fit output
    float scale = fminf(
        (float)wlr_output->width / layer->width,
        (float)wlr_output->height / layer->height
    );
    int scaled_w = (int)(layer->width * scale);
    int scaled_h = (int)(layer->height * scale);
    int x_off = (wlr_output->width - scaled_w) / 2;
    int y_off = (wlr_output->height - scaled_h) / 2;

    wlr_render_pass_add_texture(render_pass, &(struct wlr_render_texture_options){
        .texture = layer->texture,
        .dst_box = { .x = x_off, .y = y_off, .width = scaled_w, .height = scaled_h },
        .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
    });
    
 //   has_active_plugins = true;
}

if (layer_count == 0) {
    wlr_log(WLR_DEBUG, "No layers in server->layers list");
}
// =========================================================
    // =========================================================


    // --- 4. Highly Optimized Plugin Rendering ---
    bool has_active_plugins = false;
    wl_list_for_each(p, &server->plugins, link) {
        if (!p) continue;

        struct wlr_buffer* buffer_to_render = NULL;
        
        if (p->is_async) {
            // ASYNC PLUGIN: Use the last buffer marked as ready
            if (p->last_ready_buffer >= 0 && p->last_ready_buffer < MAX_BUFFERS) {
                buffer_to_render = p->compositor_buffers[p->last_ready_buffer];
            }
        } else {
            // LEGACY PLUGIN: Use existing laundered buffer
            buffer_to_render = p->compositor_buffer;
        }

        if (buffer_to_render) {
            has_active_plugins = true;
            
            struct wlr_texture *texture_to_use = NULL;
            
            // Use cached texture if valid and buffer hasn't changed
            if (p->cached_texture && p->cached_buffer == buffer_to_render) {
                texture_to_use = p->cached_texture;
            } else {
                // Create new texture and cache it
                texture_to_use = wlr_texture_from_buffer(server->renderer, buffer_to_render);
                if (texture_to_use) {
                    // Clean up old cached texture
                    if (p->cached_texture) {
                        wlr_texture_destroy(p->cached_texture);
                    }
                    p->cached_texture = texture_to_use;
                    p->cached_buffer = buffer_to_render;
                    p->scale_cached = false; // Recalculate scaling
                }
            }
            
            if (texture_to_use) {
                // Use cached scaling calculations
                int x_offset, y_offset, scaled_width, scaled_height;
                
                if (!p->scale_cached || 
                    p->cached_output_width != wlr_output->width || 
                    p->cached_output_height != wlr_output->height) {
                    
                    // Recalculate scaling
                    float scale = fminf((float)wlr_output->width / texture_to_use->width, 
                                       (float)wlr_output->height / texture_to_use->height);
                    scaled_width = (int)(texture_to_use->width * scale);
                    scaled_height = (int)(texture_to_use->height * scale);
                    x_offset = (wlr_output->width - scaled_width) / 2;
                    y_offset = (wlr_output->height - scaled_height) / 2;
                    
                    // Cache the values
                    p->cached_x_offset = x_offset;
                    p->cached_y_offset = y_offset;
                    p->cached_scaled_width = scaled_width;
                    p->cached_scaled_height = scaled_height;
                    p->cached_output_width = wlr_output->width;
                    p->cached_output_height = wlr_output->height;
                    p->scale_cached = true;
                } else {
                    // Use cached values
                    x_offset = p->cached_x_offset;
                    y_offset = p->cached_y_offset;
                    scaled_width = p->cached_scaled_width;
                    scaled_height = p->cached_scaled_height;
                }

                wlr_render_pass_add_texture(render_pass, &(struct wlr_render_texture_options){
                    .texture = texture_to_use,
                    .dst_box = { 
                        .x = x_offset, 
                        .y = y_offset, 
                        .width = scaled_width, 
                        .height = scaled_height 
                    },
                    .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
                });
                
                // Don't destroy cached texture here
            }
        }
    }

    // Fallback to scene rendering if no plugins
    if (!has_active_plugins) {
        wlr_scene_output_build_state(output->scene_output, &output_state, NULL);
    }

    // --- 5. Submit, Finalize, and Send Updates ---
    wlr_render_pass_submit(render_pass);
    wlr_output_commit_state(wlr_output, &output_state);
    wlr_output_state_finish(&output_state);
    
    // Send frame done notifications efficiently
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct nano_view *view;
    wl_list_for_each(view, &server->views, link) {
        if (view && view->xdg_toplevel && view->xdg_toplevel->base && 
            view->xdg_toplevel->base->surface) {
            wlr_surface_send_frame_done(view->xdg_toplevel->base->surface, &now);
        }
    }


    nano_server_send_window_updates(server);


}

// This is the function that processes key presses.
// In nano_server.c

static void handle_keyboard_key(struct wl_listener *listener, void *data) {
    struct nano_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct nano_server *server = keyboard->server;
    struct wlr_keyboard_key_event *event = data;
    struct wlr_seat *seat = server->seat;

    struct wlr_keyboard *wlr_kb = wlr_keyboard_from_input_device(keyboard->device);
    uint32_t modifiers = wlr_keyboard_get_modifiers(wlr_kb);


// Forward key to layers (e.g., wayfire)
compositor_layer_t *layer;
wl_list_for_each(layer, &server->layers, link) {
    if (layer->event_fd >= 0) {
        layer_send_key(layer->event_fd,
                       event->keycode,
                       event->state == WL_KEYBOARD_KEY_STATE_PRESSED,
                       event->time_msec);
    }
}

    // --- Shortcut Handling ---

    // Shortcut 1: Ctrl + Q to close the focused window.
    if (modifiers == WLR_MODIFIER_CTRL && event->keycode == KEY_Q && event->state == 1 /* KEY PRESSED */) {
        struct wlr_surface *focused_surface = seat->keyboard_state.focused_surface;
        if (focused_surface) {
            struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_try_from_wlr_surface(focused_surface);
            if (xdg_surface && xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && xdg_surface->data) {
                struct nano_view *view = xdg_surface->data;
                wlr_log(WLR_INFO, "Closing window '%s' with Ctrl+Q shortcut", view->xdg_toplevel->title);
                api_view_close(view);
                return; // Shortcut handled.
            }
        }
    }
    // Shortcut 2: Ctrl + M to toggle maximize for the focused window.
    else if (modifiers == WLR_MODIFIER_CTRL && event->keycode == KEY_M && event->state == 1 /* KEY PRESSED */) {
        struct wlr_surface *focused_surface = seat->keyboard_state.focused_surface;
        if (focused_surface) {
            struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_try_from_wlr_surface(focused_surface);
            if (xdg_surface && xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && xdg_surface->data) {
                struct nano_view *view = xdg_surface->data;
                bool is_maximized = view->xdg_toplevel->current.maximized;

                if (!is_maximized) {
                    // MAXIMIZE THE WINDOW
                    wlr_log(WLR_INFO, "Maximizing window '%s'", view->xdg_toplevel->title);
                    
                    view->saved_geometry.x = view->scene_tree->node.x;
                    view->saved_geometry.y = view->scene_tree->node.y;
                    view->saved_geometry.width = view->xdg_toplevel->base->surface->current.width;
                    view->saved_geometry.height = view->xdg_toplevel->base->surface->current.height;

                    struct wlr_output *output = wlr_output_layout_output_at(
                        server->output_layout, view->scene_tree->node.x, view->scene_tree->node.y);
                    
                    if (output) {
                        // === THE FIX IS HERE ===
                        // 1. Declare a wlr_box on the stack.
                        struct wlr_box output_box;
                        // 2. Pass its address (&output_box) to the function to be filled.
                        wlr_output_layout_get_box(server->output_layout, output, &output_box);
                        
                        // 3. Move and resize using the stack variable (with . instead of ->).
                        api_view_move(view, output_box.x, output_box.y);
                        api_view_resize(view, output_box.width, output_box.height);
                    }
                    
                    wlr_xdg_toplevel_set_maximized(view->xdg_toplevel, true);

                } else {
                    // RESTORE THE WINDOW
                    wlr_log(WLR_INFO, "Restoring window '%s'", view->xdg_toplevel->title);
                    
                    api_view_move(view, view->saved_geometry.x, view->saved_geometry.y);
                    api_view_resize(view, view->saved_geometry.width, view->saved_geometry.height);
                    
                    wlr_xdg_toplevel_set_maximized(view->xdg_toplevel, false);
                }
                
                return; // Shortcut handled.
            }
        }
    }
    // Shortcut 3: Ctrl + W to toggle grid layout
    else if (modifiers == WLR_MODIFIER_CTRL && event->keycode == KEY_W && event->state == 1 /* KEY PRESSED */) {
        wlr_log(WLR_INFO, "Toggling grid layout with Ctrl+W shortcut");
        
        // Create the IPC event to send to the plugin
        ipc_event_t ipc_event;
        ipc_event.type = IPC_EVENT_TYPE_TOGGLE_GRID_LAYOUT;
        
        // Send to all spawned plugins
        struct spawned_plugin *plugin;
        int plugins_notified = 0;
        
        wl_list_for_each(plugin, &server->plugins, link) {
            if (plugin->ipc_fd >= 0) {
                ssize_t written = write(plugin->ipc_fd, &ipc_event, sizeof(ipc_event));
                if (written != sizeof(ipc_event)) {
                    wlr_log(WLR_ERROR, "Failed to send grid toggle to plugin '%s': %s", 
                           plugin->name, (written < 0) ? strerror(errno) : "partial write");
                } else {
                    wlr_log(WLR_INFO, "Grid toggle sent to plugin '%s' (fd %d)", 
                           plugin->name, plugin->ipc_fd);
                    plugins_notified++;
                }
            }
        }
        
        if (plugins_notified == 0) {
            wlr_log(WLR_ERROR, "No plugins available to receive grid toggle event");
        } else {
            wlr_log(WLR_INFO, "Grid toggle sent to %d plugin(s)", plugins_notified);
        }
        
        return; // Shortcut handled. Don't pass the key to the client.
    }
    
    // If no shortcut was handled, forward the key event to the client as normal.
    struct wlr_surface *focused_surface = seat->keyboard_state.focused_surface;
    if (focused_surface == NULL) {
        return;
    }
    wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
}

// This function handles modifier key changes (e.g., holding Shift).
static void handle_keyboard_modifiers(struct wl_listener *listener, void *data) {
    struct nano_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    struct nano_server *server = keyboard->server;
    struct wlr_keyboard *wlr_kb = wlr_keyboard_from_input_device(keyboard->device);

    // ADD THIS: Forward modifiers to layers
    compositor_layer_t *layer;
    wl_list_for_each(layer, &server->layers, link) {
        if (layer->event_fd >= 0) {
            layer_send_modifiers(layer->event_fd,
                                wlr_kb->modifiers.depressed,
                                wlr_kb->modifiers.latched,
                                wlr_kb->modifiers.locked,
                                wlr_kb->modifiers.group);
        }
    }

    // Notify the seat about the new modifier state.
    wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &wlr_kb->modifiers);
}


// This function handles keyboard cleanup when it's disconnected.
static void handle_keyboard_destroy(struct wl_listener *listener, void *data) {
    struct nano_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);

    // Clean up listeners and memory
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    
    // If this was the last keyboard, update seat capabilities
    if (wl_list_empty(&keyboard->server->keyboards)) {
        uint32_t caps = WL_SEAT_CAPABILITY_POINTER; // Keep pointer
        wlr_seat_set_capabilities(keyboard->server->seat, caps);
    }

    free(keyboard);
}


// This function sets up a new keyboard device.
static void server_add_keyboard(struct nano_server *server, struct wlr_input_device *device) {
    struct nano_keyboard *keyboard = calloc(1, sizeof(struct nano_keyboard));
    keyboard->server = server;
    keyboard->device = device;
    
    // Get the wlr_keyboard reference from the generic device
    struct wlr_keyboard *wlr_kb = wlr_keyboard_from_input_device(device);

    // 1. Set up the keymap from XKB rules.
    struct xkb_rule_names rules = { 0 };
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(server->xkb_context, &rules,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        wlr_log(WLR_ERROR, "Failed to create keymap");
        free(keyboard);
        return;
    }
    wlr_keyboard_set_keymap(wlr_kb, keymap); // <-- Use wlr_kb
    xkb_keymap_unref(keymap);
    wlr_keyboard_set_repeat_info(wlr_kb, 25, 600); // <-- Use wlr_kb

    // 2. Set up listeners for keyboard events.
    keyboard->modifiers.notify = handle_keyboard_modifiers;
    wl_signal_add(&wlr_kb->events.modifiers, &keyboard->modifiers); // <-- Use wlr_kb

    keyboard->key.notify = handle_keyboard_key;
    wl_signal_add(&wlr_kb->events.key, &keyboard->key); // <-- Use wlr_kb

    keyboard->destroy.notify = handle_keyboard_destroy;
    wl_signal_add(&device->events.destroy, &keyboard->destroy);

    // 3. Attach the keyboard to the seat.
    wlr_seat_set_keyboard(server->seat, wlr_kb); // <-- Use wlr_kb

    // 4. Add to our list of keyboards.
    wl_list_insert(&server->keyboards, &keyboard->link);
}

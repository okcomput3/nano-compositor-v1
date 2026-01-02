// real_mouse_plugin.c - FIXED STANDALONE EXECUTABLE WITH REAL SCENE BUFFERS
////////////////////////breadcrumbs///////////////////////////////////
//////////////////////////////////////////////////////////////////////
#define _GNU_SOURCE 
#include "../nano_compositor.h"
#include "../nano_ipc.h" // The shared IPC protocol
#include "logs.h" 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>
#include <linux/input-event-codes.h>

// For headless EGL/OpenGL rendering
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h> 
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include <sys/socket.h> // Needed for recvmsg
#include <errno.h>

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h> // For memfd_create
#include "../libdmabuf_share/dmabuf_share.h"
#include <sys/socket.h>  // for sendmsg, struct msghdr
#include <sys/un.h>      // for ancillary data

#include <wlr/render/gles2.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_drm.h> // For wlr_drm_format
// Add these includes at the top of your plugin file
#include <drm_fourcc.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <linux/dma-heap.h>

static pthread_mutex_t dmabuf_mutex = PTHREAD_MUTEX_INITIALIZER;
int MAX_TRAIL_POINTS=20;



#ifndef PFNGLEGLIMAGETARGETTEXTURE2DOESPROC
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
#endif

struct cursor_state {
    float x, y;                    // Current cursor position
    float target_x, target_y;      // Target position for smooth movement
    bool visible;                  // Is cursor visible
    float animation_time;          // Current animation time
    float last_move_time;          // Time of last movement
    GLuint cursor_program;         // Shader program for cursor
    GLuint cursor_vao, cursor_vbo; // VAO and VBO for cursor
    GLint cursor_position_uniform; // Uniform for cursor position
    GLint cursor_time_uniform;     // Uniform for animation time
    GLint cursor_size_uniform;     // Uniform for cursor size
    GLint cursor_alpha_uniform;    // Uniform for cursor alpha
    float size;                    // Cursor size (radius)
    
    // Mouse trail data (using fixed size instead of #define)
    struct {
        float x, y;
        float timestamp;
        float alpha;
    } trail_points[20];            // Fixed size array
    int trail_head;                // Index of newest point
    int trail_count;               // Number of active trail points
    float trail_lifetime;          // How long trail points last (seconds)
} cursor;

// The plugin's internal state
// The plugin's internal state
struct plugin_state {
    int drm_fd; 
    int ipc_fd;
    void* shm_ptr;
    int dmabuf_fd;
    void* dmabuf_ptr; // Can still be useful for debugging
    EGLImage egl_image;
    
    // Headless rendering context
    struct gbm_device *gbm;
    EGLDisplay egl_display;
    EGLContext egl_context;
    
    // Off-screen rendering resources
    GLuint fbo;
    GLuint fbo_texture;
    int width, height; // Dimensions of our off-screen buffer
    
    // OpenGL resources - matching the original plugin
    GLuint window_program;
    GLuint quad_vao;
    GLuint quad_vbo;
    GLuint quad_ibo;
    
    // Shader uniforms
    GLint window_texture_uniform;
    GLint window_transform_uniform;
    GLint window_size_uniform;
    GLint alpha_uniform;
    
    // Timing
    struct timeval start_time;
    struct timeval last_frame_time;
    double frame_delta;
    bool gl_initialized;
    
    // Window data - matching original
    struct {
        struct wlr_scene_buffer *scene_buffer;
        struct wlr_texture *wlr_texture;
        GLuint gl_texture;
        float x, y, width, height;
        char *title;
        bool is_valid;
        bool is_focused;
        float alpha;
        
        // Animation tracking
        uint32_t last_sequence;
        struct timeval last_update_time;
        bool content_changed;
        bool is_dmabuf;
        
        // Force refresh tracking
        int frames_since_change;
        uint32_t last_texture_checksum;  // Simple checksum to detect changes
        int frames_since_update;         // Skip some updates to reduce flickering
        bool force_update; 
        uint64_t buffer_id;
    } windows[10];
    int window_count;
    
    // Renderer for texture extraction
    struct wlr_renderer *renderer;
    
    // IPC interface to get scene buffers
    const struct nano_plugin_api *api;
    
    // Interaction state
    int focused_window_index;
    bool dragging;
    float drag_offset_x, drag_offset_y;
    float pointer_x, pointer_y;
    
    // Animation control
    uint64_t frame_counter;
    bool continuous_render;
    
    // Double buffering
    void* shm_ptr_front;  // What compositor reads
    void* shm_ptr_back;   // What we write to
    bool front_buffer_ready;  // Flag to indicate buffer is ready
    int current_back_buffer;  // 0 or 1
    
    EGLSurface egl_surface;
    dmabuf_share_context_t dmabuf_ctx;
    
    int cached_memfd;
    void *cached_memfd_ptr;
    size_t cached_memfd_size;
    
    struct wlr_allocator *allocator;
    struct wlr_buffer *export_buffer; 
    
    bool content_changed_this_frame;
    int frames_since_last_change;
    struct timeval last_significant_change;
    
    // DMA-BUF pool for reduced IPC overhead
    struct {
        int fds[8];           // Pool of 8 DMA-BUF FDs
        GLuint textures[8];   // Corresponding GL textures
        bool in_use[8];       // Track which are currently used
        int pool_size;
        bool initialized;
    } dmabuf_pool;

    bool use_egl_export; 

    bool dmabuf_sent_to_compositor;      // Track if we sent the DMA-BUF already
    uint32_t last_frame_sequence;       // Track frame sequence
    struct timeval dmabuf_send_time; 

    struct cursor_state cursor;

      int instance_id;  // Unique per plugin
    char instance_name[64];  // For debugging


};

// Add this forward declaration near the top with other declarations
// Add these function declarations
static GLuint create_texture_hybrid(struct plugin_state *state, const ipc_window_info_t *info, int window_index);
static GLuint create_texture_from_dmabuf_manual(struct plugin_state *state, const ipc_window_info_t *info);
static GLuint create_texture_with_stride_fix_optimized(struct plugin_state *state, const ipc_window_info_t *info);
static GLuint texture_from_dmabuf(struct plugin_state *state, const ipc_window_info_t *info);
static GLuint create_texture_with_stride_fix(struct plugin_state *state, const ipc_window_info_t *info);
// Add these declarations near the top of your plugin file
static void handle_window_list_with_textures(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count);
static void render_frame_opengl_with_windows(struct plugin_state *state, int frame_count);
static int export_dmabuf_fallback(struct plugin_state *state) ;
static void add_trail_point(struct plugin_state *state, float x, float y, float timestamp);
// Add these function declarations

// Quad vertices with flipped texture coordinates (from original)
// Updated quad vertices with corrected texture coordinates (flipped Y)
// Fixed quad vertices - flip the texture Y coordinates




// Cursor vertex shader - creates a circle with multicolored gradient
static const char* cursor_vertex_shader = 
    "#version 100\n"
    "precision mediump float;\n"
    "attribute vec2 position;\n"
    "uniform vec2 cursor_position;\n"
    "uniform float cursor_size;\n"
    "uniform vec2 screen_size;\n"
    "varying vec2 v_local_pos;\n"
    "\n"
    "void main() {\n"
    "    v_local_pos = position;\n"
    "    \n"
    "    // Convert cursor position to NDC\n"
    "    vec2 cursor_ndc = (cursor_position / screen_size) * 2.0 - 1.0;\n"
    "    cursor_ndc.y = -cursor_ndc.y;\n" // Flip Y coordinate
    "    \n"
    "    // Scale position by cursor size and offset by cursor position\n"
    "    vec2 scaled_pos = position * cursor_size / screen_size * 2.0;\n"
    "    gl_Position = vec4(cursor_ndc + scaled_pos, 0.0, 1.0);\n"
    "}\n";

// Cursor fragment shader - creates multicolored animated circle
static const char* cursor_fragment_shader =
    "#version 100\n"
    "precision mediump float;\n"
    "uniform float time;\n"
    "uniform float cursor_size;\n"
    "uniform float alpha;\n"
    "varying vec2 v_local_pos;\n"
    "\n"
    "void main() {\n"
    "    float dist = length(v_local_pos);\n"
    "    \n"
    "    // Discard pixels outside the circle\n"
    "    if (dist > 1.0) {\n"
    "        discard;\n"
    "    }\n"
    "    \n"
    "    // Calculate angle for color gradient\n"
    "    float angle = atan(v_local_pos.y, v_local_pos.x);\n"
    "    float normalized_angle = (angle + 3.14159) / (2.0 * 3.14159);\n"
    "    \n"
    "    // Animated rainbow gradient\n"
    "    float hue = mod(normalized_angle + time * 0.5, 1.0);\n"
    "    \n"
    "    // Convert HSV to RGB (simplified)\n"
    "    vec3 color;\n"
    "    float c = 1.0;\n" // Full saturation and value
    "    float h = hue * 6.0;\n"
    "    float x = 1.0 - abs(mod(h, 2.0) - 1.0);\n"
    "    \n"
    "    if (h < 1.0) {\n"
    "        color = vec3(1.0, x, 0.0);\n"
    "    } else if (h < 2.0) {\n"
    "        color = vec3(x, 1.0, 0.0);\n"
    "    } else if (h < 3.0) {\n"
    "        color = vec3(0.0, 1.0, x);\n"
    "    } else if (h < 4.0) {\n"
    "        color = vec3(0.0, x, 1.0);\n"
    "    } else if (h < 5.0) {\n"
    "        color = vec3(x, 0.0, 1.0);\n"
    "    } else {\n"
    "        color = vec3(1.0, 0.0, x);\n"
    "    }\n"
    "    \n"
    "    // Add pulsing effect based on distance from center\n"
    "    float pulse = 0.8 + 0.2 * sin(time * 3.0 + dist * 5.0);\n"
    "    color *= pulse;\n"
    "    \n"
    "    // Soft edge with anti-aliasing\n"
    "    float edge_fade = 1.0 - smoothstep(0.8, 1.0, dist);\n"
    "    \n"
    "    // Inner ring highlight\n"
    "    float inner_ring = smoothstep(0.3, 0.4, dist) - smoothstep(0.6, 0.7, dist);\n"
    "    color += vec3(0.3) * inner_ring;\n"
    "    \n"
    "    gl_FragColor = vec4(color, edge_fade * alpha);\n"
    "}\n";

// Cursor quad vertices (square that will be shaped by fragment shader)
static const GLfloat cursor_vertices[] = {
    -1.0f, -1.0f,  // Bottom-left
     1.0f, -1.0f,  // Bottom-right
    -1.0f,  1.0f,  // Top-left
     1.0f,  1.0f,  // Top-right
};

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            char* log = malloc(length);
            glGetShaderInfoLog(shader, length, NULL, log);
            loggy_real_mouse( "Shader error: %s\n", log);
            free(log);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void handle_mouse_move(struct plugin_state *state, float x, float y) {
    // Add trail point at old position before moving
    struct timeval now;
    gettimeofday(&now, NULL);
    float current_time = now.tv_sec + now.tv_usec / 1000000.0f;
    
    add_trail_point(state, state->cursor.x, state->cursor.y, current_time);
    
    state->cursor.target_x = x;
    state->cursor.target_y = state->height - y;
    state->cursor.last_move_time = current_time;
    state->cursor.visible = true;
    state->content_changed_this_frame = true;
}

// Handle mouse button clicks
static void handle_mouse_button(struct plugin_state *state, int button, int pressed) {
    if (button == BTN_LEFT && pressed) {
        loggy_real_mouse( "[PLUGIN] Left click at (%.1f, %.1f)\n", state->cursor.x, state->cursor.y);
    }
    
    if (button == BTN_RIGHT && pressed) {
        loggy_real_mouse( "[PLUGIN] Right click at (%.1f, %.1f)\n", state->cursor.x, state->cursor.y);
    }
}


static int create_dmabuf(size_t size, int instance_id) {
    int result = -1;
    int lock_fd = -1;
    pid_t pid = getpid();
    
    // FIXED: Instance-specific lock file
    char lock_file[256];
    snprintf(lock_file, sizeof(lock_file), "/tmp/dmabuf_create_%d.lock", instance_id);
    
    lock_fd = open(lock_file, O_CREAT|O_RDWR, 0666);
    if (lock_fd < 0) {
        loggy_real_mouse( "[PLUGIN-%d] Failed to create lock file: %s\n", instance_id, strerror(errno));
        return -1;
    }
    
    // Lock for multi-process safety
    if (flock(lock_fd, LOCK_EX) < 0) {
        loggy_real_mouse( "[PLUGIN] PID=%d Failed to acquire lock: %s\n", pid, strerror(errno));
        close(lock_fd);
        return -1;
    }
    
   loggy_real_mouse( "[PLUGIN] PID=%d Creating DMA-BUF of size %zu bytes\n", 
        pid, size);
    
    // Method 1: Try DMA heap allocation
    const char* heap_path = "/dev/dma_heap/system";
    loggy_real_mouse( "[PLUGIN] PID=%d Attempting to open: %s\n", pid, heap_path);
    
    int heap_fd = open(heap_path, O_RDONLY | O_CLOEXEC);
    if (heap_fd < 0) {
        loggy_real_mouse( "[PLUGIN] PID=%d Failed to open %s: %s (errno=%d)\n", 
                pid, heap_path, strerror(errno), errno);
        goto try_fallback;
    }
    
    loggy_real_mouse( "[PLUGIN] PID=%d ✅ Successfully opened %s (fd=%d)\n", 
            pid, heap_path, heap_fd);
    
    struct dma_heap_allocation_data heap_data = {
        .len = size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };
    
    int ioctl_result = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
    if (ioctl_result == 0) {
        loggy_real_mouse( "[PLUGIN] PID=%d ✅ DMA heap allocation SUCCESS: fd=%d\n", 
                pid, heap_data.fd);
        
        // Test the returned fd
        struct stat stat_buf;
        if (fstat(heap_data.fd, &stat_buf) == 0) {
            loggy_real_mouse( "[PLUGIN] PID=%d DMA-BUF fd stats: size=%ld, mode=0%o\n", 
                    pid, stat_buf.st_size, stat_buf.st_mode);
        }
        
        result = heap_data.fd;
        goto cleanup_heap_fd;
    }
    
    loggy_real_mouse( "[PLUGIN] PID=%d ioctl DMA_HEAP_IOCTL_ALLOC FAILED: %s (errno=%d)\n", 
            pid, strerror(errno), errno);
    
    // Method 2: Try smaller allocation if the original was large
    if (size > 1024 * 1024) {  // If > 1MB
        loggy_real_mouse( "[PLUGIN] PID=%d Large allocation failed, trying smaller test allocation...\n", pid);
        
        struct dma_heap_allocation_data test_data = {
            .len = 4096,  // Just 4KB test
            .fd_flags = O_RDWR | O_CLOEXEC,
        };
        
        if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &test_data) == 0) {
            loggy_real_mouse( "[PLUGIN] PID=%d ✅ Small test allocation worked! The issue might be size limits.\n", pid);
            close(test_data.fd);  // Clean up test allocation
            
            // Try a medium size (typical framebuffer size)
            size_t medium_size = 1920 * 1080 * 4;  // 1080p RGBA
            if (medium_size > size) {
                medium_size = size;  // Don't go larger than requested
            }
            
            struct dma_heap_allocation_data medium_data = {
                .len = medium_size,
                .fd_flags = O_RDWR | O_CLOEXEC,
            };
            
            if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &medium_data) == 0) {
                loggy_real_mouse( "[PLUGIN] PID=%d ✅ Medium allocation worked: fd=%d\n", 
                        pid, medium_data.fd);
                result = medium_data.fd;
                goto cleanup_heap_fd;
            } else {
                loggy_real_mouse( "[PLUGIN] PID=%d Medium allocation failed: %s\n", 
                        pid, strerror(errno));
            }
        } else {
            loggy_real_mouse( "[PLUGIN] PID=%d Even small test allocation failed: %s\n", 
                    pid, strerror(errno));
        }
    }

cleanup_heap_fd:
    close(heap_fd);
    heap_fd = -1;
    
    if (result >= 0) {
        goto success;
    }

try_fallback:
    // Method 3: Try memfd as fallback with explicit warning
    loggy_real_mouse( "[PLUGIN] PID=%d DMA heap failed, falling back to memfd (won't work for zero-copy)\n", pid);
    
    int memfd = memfd_create("plugin-fbo-fallback", MFD_CLOEXEC);
    if (memfd < 0) {
        loggy_real_mouse( "[PLUGIN] PID=%d memfd_create failed: %s\n", pid, strerror(errno));
        goto error;
    }
    
    if (ftruncate(memfd, size) != 0) {
        loggy_real_mouse( "[PLUGIN] PID=%d memfd ftruncate failed: %s\n", pid, strerror(errno));
        close(memfd);
        goto error;
    }
    
    loggy_real_mouse( "[PLUGIN] PID=%d ⚠️  Created memfd fallback: fd=%d (THIS WON'T WORK FOR ZERO-COPY)\n", 
            pid, memfd);
    result = memfd;
    goto success;

error:
    loggy_real_mouse( "[PLUGIN] PID=%d ❌ All allocation methods failed\n", pid);
    result = -1;

success:
    // Unlock and cleanup
    if (lock_fd >= 0) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
    return result;
}



// Initialize cursor rendering resources
static bool init_cursor_resources(struct plugin_state *state) {
    loggy_real_mouse( "[PLUGIN] Initializing cursor resources with trails...\n");
    
    // Compile cursor shaders
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, cursor_vertex_shader);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, cursor_fragment_shader);
    
    if (!vertex || !fragment) {
        loggy_real_mouse( "[PLUGIN] Failed to compile cursor shaders\n");
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return false;
    }
    
    // Create and link cursor program
    state->cursor.cursor_program = glCreateProgram();
    glAttachShader(state->cursor.cursor_program, vertex);
    glAttachShader(state->cursor.cursor_program, fragment);
    
    glBindAttribLocation(state->cursor.cursor_program, 0, "position");
    glLinkProgram(state->cursor.cursor_program);
    
    GLint linked;
    glGetProgramiv(state->cursor.cursor_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLchar log[512];
        glGetProgramInfoLog(state->cursor.cursor_program, sizeof(log), NULL, log);
        loggy_real_mouse( "[PLUGIN] Cursor program link error: %s\n", log);
        glDeleteProgram(state->cursor.cursor_program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    // Get uniform locations
    state->cursor.cursor_position_uniform = glGetUniformLocation(state->cursor.cursor_program, "cursor_position");
    state->cursor.cursor_time_uniform = glGetUniformLocation(state->cursor.cursor_program, "time");
    state->cursor.cursor_size_uniform = glGetUniformLocation(state->cursor.cursor_program, "cursor_size");
    state->cursor.cursor_alpha_uniform = glGetUniformLocation(state->cursor.cursor_program, "alpha");
    GLint screen_size_uniform = glGetUniformLocation(state->cursor.cursor_program, "screen_size");
    
    // Set screen size (constant)
    glUseProgram(state->cursor.cursor_program);
    glUniform2f(screen_size_uniform, (float)state->width, (float)state->height);
    glUseProgram(0);
    
    // Create cursor VAO and VBO
    glGenVertexArrays(1, &state->cursor.cursor_vao);
    glBindVertexArray(state->cursor.cursor_vao);
    
    glGenBuffers(1, &state->cursor.cursor_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, state->cursor.cursor_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cursor_vertices), cursor_vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // Initialize cursor state
    state->cursor.x = state->width / 2.0f;
    state->cursor.y = state->height / 2.0f;
    state->cursor.target_x = state->cursor.x;
    state->cursor.target_y = state->cursor.y;
    state->cursor.visible = true;
    state->cursor.animation_time = 0.0f;
    state->cursor.size = 25.0f; // 20 pixel radius
    state->cursor.trail_head = 0;
    state->cursor.trail_count = 0;
    state->cursor.trail_lifetime = 0.8f; // Trail lasts 0.8 seconds
    
    struct timeval now;
    gettimeofday(&now, NULL);
    state->cursor.last_move_time = now.tv_sec + now.tv_usec / 1000000.0f;
    
    loggy_real_mouse( "[PLUGIN] ✅ Cursor resources initialized with trails\n");
    return true;
}


// Add a trail point when cursor moves significantly
static void add_trail_point(struct plugin_state *state, float x, float y, float timestamp) {
    // Only add if cursor moved significantly (reduce trail density)
    if (state->cursor.trail_count > 0) {
        int prev_idx = (state->cursor.trail_head - 1 + MAX_TRAIL_POINTS) % MAX_TRAIL_POINTS;
        float dx = x - state->cursor.trail_points[prev_idx].x;
        float dy = y - state->cursor.trail_points[prev_idx].y;
        float distance = sqrtf(dx*dx + dy*dy);
        
        // Only add point if moved at least 5 pixels
        if (distance < 5.0f) {
            return;
        }
    }
    
    // Add new trail point
    state->cursor.trail_points[state->cursor.trail_head].x = x;
    state->cursor.trail_points[state->cursor.trail_head].y = y;
    state->cursor.trail_points[state->cursor.trail_head].timestamp = timestamp;
    state->cursor.trail_points[state->cursor.trail_head].alpha = 1.0f;
    
    state->cursor.trail_head = (state->cursor.trail_head + 1) % MAX_TRAIL_POINTS;
    if (state->cursor.trail_count < MAX_TRAIL_POINTS) {
        state->cursor.trail_count++;
    }
}

// Update trail point alphas based on age
static void update_trail_alphas(struct plugin_state *state, float current_time) {
    for (int i = 0; i < state->cursor.trail_count; i++) {
        float age = current_time - state->cursor.trail_points[i].timestamp;
        if (age > state->cursor.trail_lifetime) {
            // This point is too old, mark for removal
            state->cursor.trail_points[i].alpha = 0.0f;
        } else {
            // Fade out over time with smooth curve
            float life_ratio = 1.0f - (age / state->cursor.trail_lifetime);
            state->cursor.trail_points[i].alpha = life_ratio * life_ratio; // Quadratic fade
        }
    }
    
    // Remove old points by compacting the array
    int write_idx = 0;
    for (int read_idx = 0; read_idx < state->cursor.trail_count; read_idx++) {
        int actual_idx = (state->cursor.trail_head - state->cursor.trail_count + read_idx + MAX_TRAIL_POINTS) % MAX_TRAIL_POINTS;
        if (state->cursor.trail_points[actual_idx].alpha > 0.01f) {
            if (write_idx != read_idx) {
                int write_actual_idx = (state->cursor.trail_head - state->cursor.trail_count + write_idx + MAX_TRAIL_POINTS) % MAX_TRAIL_POINTS;
                state->cursor.trail_points[write_actual_idx] = state->cursor.trail_points[actual_idx];
            }
            write_idx++;
        }
    }
    state->cursor.trail_count = write_idx;
}

// Update cursor position and animation
static void update_cursor(struct plugin_state *state, float delta_time) {
    state->cursor.animation_time += delta_time;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    float current_time = now.tv_sec + now.tv_usec / 1000000.0f;
    
    // Smooth cursor movement
    float move_speed = 8.0f; // How fast cursor moves to target
    float old_x = state->cursor.x;
    float old_y = state->cursor.y;
    
    float dx = state->cursor.target_x - state->cursor.x;
    float dy = state->cursor.target_y - state->cursor.y;
    
    state->cursor.x += dx * move_speed * delta_time;
    state->cursor.y += dy * move_speed * delta_time;
    
    // Add trail point if cursor moved significantly
    float move_distance = sqrtf((state->cursor.x - old_x) * (state->cursor.x - old_x) + 
                               (state->cursor.y - old_y) * (state->cursor.y - old_y));
    if (move_distance > 2.0f) {
        add_trail_point(state, old_x, old_y, current_time);
        state->content_changed_this_frame = true;
    }
    
    // Update trail alphas
    update_trail_alphas(state, current_time);
    
    // Check if cursor moved significantly to trigger re-render
    if (move_distance > 1.0f || state->cursor.trail_count > 0) {
        state->content_changed_this_frame = true;
    }
}


// Render a single cursor instance at given position with given alpha
static void render_cursor_instance(struct plugin_state *state, float x, float y, float alpha, float size_multiplier) {
    glUseProgram(state->cursor.cursor_program);
    
    // Set uniforms for this instance
    glUniform2f(state->cursor.cursor_position_uniform, x, y);
    glUniform1f(state->cursor.cursor_time_uniform, state->cursor.animation_time);
    glUniform1f(state->cursor.cursor_size_uniform, state->cursor.size * size_multiplier);
    glUniform1f(state->cursor.cursor_alpha_uniform, alpha);
    
    // Render cursor quad
    glBindVertexArray(state->cursor.cursor_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// Updated render_cursor function with trail rendering
static void render_cursor(struct plugin_state *state) {
    const int MAX_TRAIL_POINTS = 20; // Define locally
    
    if (!state->cursor.visible) {
        return;
    }
    
    // Save current OpenGL state
    GLint current_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    
    GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    GLint blend_src, blend_dst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst);
    
    // Set up cursor rendering
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render trail points (oldest to newest)
    for (int i = 0; i < state->cursor.trail_count; i++) {
        int idx = (state->cursor.trail_head - state->cursor.trail_count + i + MAX_TRAIL_POINTS) % MAX_TRAIL_POINTS;
        
        if (state->cursor.trail_points[idx].alpha > 0.01f) {
            // Calculate size based on age (smaller for older points)
            float age_ratio = (float)i / (float)state->cursor.trail_count;
            float size_multiplier = 1.0f - (1.0f - age_ratio) * 0.7f; // From 30% to 100% size
            
            render_cursor_instance(state, 
                                 state->cursor.trail_points[idx].x, 
                                 state->cursor.trail_points[idx].y,
                                 state->cursor.trail_points[idx].alpha * 0.2f, // Trail is slightly more transparent
                                 size_multiplier);
        }
    }
    
    // Render main cursor on top
    render_cursor_instance(state, state->cursor.x, state->cursor.y, 0.9f, 1.0f);
    
    // Restore OpenGL state
    if (!blend_enabled) {
        glDisable(GL_BLEND);
    } else {
        glBlendFunc(blend_src, blend_dst);
    }
    
    glUseProgram(current_program);
}






static bool init_dmabuf_fbo_robust(struct plugin_state *state, int width, int height) {
    loggy_real_mouse( "[PLUGIN] Attempting robust DMA-BUF backed FBO (%dx%d)\n", width, height);
    
    state->width = width;
    state->height = height;
    
    // Try multiple DMA-BUF creation methods
    for (int attempt = 0; attempt < 3; attempt++) {
        state->dmabuf_fd = create_dmabuf(width * height * 4, state->instance_id);
        if (state->dmabuf_fd < 0) {
            continue;
        }
        
        loggy_real_mouse( "[PLUGIN] Attempt %d: Created DMA-BUF fd: %d\n", attempt + 1, state->dmabuf_fd);
        
        // ✅ FIXED: Better DMA-BUF validation that doesn't reject real DMA-BUFs
        struct dma_buf_sync sync = { .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ };
        bool is_real_dmabuf = (ioctl(state->dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync) == 0);
        
        if (is_real_dmabuf) {
            loggy_real_mouse( "[PLUGIN] ✅ Confirmed real DMA-BUF, proceeding with EGL import\n");
        } else {
            // Instead of rejecting, let's check if it's a usable memfd
            struct stat fd_stat;
            if (fstat(state->dmabuf_fd, &fd_stat) == 0 && fd_stat.st_size == (width * height * 4)) {
                loggy_real_mouse( "[PLUGIN] ⚠️  Not a real DMA-BUF but valid memfd, proceeding\n");
            } else {
                loggy_real_mouse( "[PLUGIN] ❌ FD %d is invalid, trying next attempt\n", state->dmabuf_fd);
                close(state->dmabuf_fd);
                state->dmabuf_fd = -1;
                continue;
            }
        }
        
        // Try EGL import
        PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 
            (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 
            (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        
        if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
            loggy_real_mouse( "[PLUGIN] EGL extensions not available\n");
            close(state->dmabuf_fd);
            state->dmabuf_fd = -1;
            return false;
        }
        
        EGLint attribs[] = {
            EGL_WIDTH, width,
            EGL_HEIGHT, height,
            EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
            EGL_DMA_BUF_PLANE0_FD_EXT, state->dmabuf_fd,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, width * 4,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
            EGL_NONE
        };
        
        state->egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, 
                                           EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
        if (state->egl_image == EGL_NO_IMAGE_KHR) {
            EGLint error = eglGetError();
            loggy_real_mouse( "[PLUGIN] EGL image creation failed (attempt %d): 0x%x\n", 
                    attempt + 1, error);
            close(state->dmabuf_fd);
            state->dmabuf_fd = -1;
            continue;
        }
        
        // Success! Continue with texture and FBO creation
        glGenTextures(1, &state->fbo_texture);
        glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
        
        // ✅ CRITICAL: Set texture parameters BEFORE calling glEGLImageTargetTexture2DOES
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image);
        
        // ✅ Add error checking like the working version
        GLenum gl_error = glGetError();
        if (gl_error != GL_NO_ERROR) {
            loggy_real_mouse( "[PLUGIN] Failed to create texture from EGL image: 0x%x\n", gl_error);
            
            switch (gl_error) {
                case GL_INVALID_VALUE:
                    loggy_real_mouse( "[PLUGIN] GL_INVALID_VALUE - Invalid texture target or EGL image\n");
                    break;
                case GL_INVALID_OPERATION:
                    loggy_real_mouse( "[PLUGIN] GL_INVALID_OPERATION - Context/texture state issue\n");
                    break;
                default:
                    loggy_real_mouse( "[PLUGIN] Unknown OpenGL error\n");
                    break;
            }
            
            glDeleteTextures(1, &state->fbo_texture);
            PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
                (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
            if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
            close(state->dmabuf_fd);
            state->dmabuf_fd = -1;
            continue;
        }
        
        loggy_real_mouse( "[PLUGIN] ✅ Created GL texture %u from EGL image\n", state->fbo_texture);
        
        glGenFramebuffers(1, &state->fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                              GL_TEXTURE_2D, state->fbo_texture, 0);
        
        GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fb_status == GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
            // ✅ Report the actual status
            if (is_real_dmabuf) {
                loggy_real_mouse( "[PLUGIN] ✅ TRUE ZERO-COPY: Real DMA-BUF FBO created on attempt %d!\n", attempt + 1);
            } else {
                loggy_real_mouse( "[PLUGIN] ✅ PSEUDO ZERO-COPY: MemFD FBO created on attempt %d!\n", attempt + 1);
            }
            return true;
        } else {
            loggy_real_mouse( "[PLUGIN] Framebuffer incomplete on attempt %d: 0x%x\n", 
                    attempt + 1, fb_status);
            // Clean up and try again
            glDeleteFramebuffers(1, &state->fbo);
            glDeleteTextures(1, &state->fbo_texture);
            PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
                (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
            if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
            close(state->dmabuf_fd);
            state->dmabuf_fd = -1;
        }
    }
    
    loggy_real_mouse( "[PLUGIN] ❌ All DMA-BUF FBO creation attempts failed\n");
    return false;
}
/**
 * Initializes the plugin's FBO, backed by a DMA-BUF for zero-copy sharing.
 */
//////////////////////////////////////////////////////////////////////////////breadcrumb
/* nvidia only
// In your plugin executable - create your own EGL context
static bool init_plugin_egl(struct plugin_state *state) {
    loggy_real_mouse( "[PLUGIN-%d] Initializing isolated EGL context...\n", state->instance_id);
    
    // Initialize EGL display (shared but that's OK)
    state->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state->egl_display == EGL_NO_DISPLAY) {
        loggy_real_mouse( "[PLUGIN-%d] Failed to get EGL display\n", state->instance_id);
        return false;
    }

    if (!eglInitialize(state->egl_display, NULL, NULL)) {
        loggy_real_mouse( "[PLUGIN-%d] Failed to initialize EGL\n", state->instance_id);
        return false;
    }

    // Log EGL version for debugging
    const char* egl_version = eglQueryString(state->egl_display, EGL_VERSION);
    loggy_real_mouse( "[PLUGIN-%d] EGL Version: %s\n", state->instance_id, egl_version);

    // Enhanced config selection for better isolation
    EGLConfig config;
    EGLint config_count;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,  // CHANGED: Use PBUFFER instead of WINDOW for better isolation
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_CONFIG_CAVEAT, EGL_NONE,       // ADDED: Ensure no slow configs
        EGL_CONFORMANT, EGL_OPENGL_ES2_BIT, // ADDED: Ensure conformant
        EGL_NONE
    };

    if (!eglChooseConfig(state->egl_display, config_attribs, &config, 1, &config_count)) {
        loggy_real_mouse( "[PLUGIN-%d] Failed to choose EGL config\n", state->instance_id);
        return false;
    }

    if (config_count == 0) {
        loggy_real_mouse( "[PLUGIN-%d] No suitable EGL config found\n", state->instance_id);
        return false;
    }

    loggy_real_mouse( "[PLUGIN-%d] Found %d EGL config(s)\n", state->instance_id, config_count);

    // Enhanced context creation with isolation attributes
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_CONTEXT_MAJOR_VERSION_KHR, 2,              // ADDED: Explicit version
        EGL_CONTEXT_MINOR_VERSION_KHR, 0,              // ADDED: Explicit version  
        EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR, // ADDED: Reset isolation
        EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_TRUE, // ADDED: Robust access
        EGL_NONE
    };

    // CRITICAL: Create instance-specific context (no sharing!)
    state->egl_context = eglCreateContext(state->egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (state->egl_context == EGL_NO_CONTEXT) {
        EGLint error = eglGetError();
        loggy_real_mouse( "[PLUGIN-%d] Failed to create EGL context: 0x%x\n", state->instance_id, error);
        
        // Fallback: try simpler context creation
        loggy_real_mouse( "[PLUGIN-%d] Trying fallback context creation...\n", state->instance_id);
        EGLint simple_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        state->egl_context = eglCreateContext(state->egl_display, config, EGL_NO_CONTEXT, simple_attribs);
        if (state->egl_context == EGL_NO_CONTEXT) {
            loggy_real_mouse( "[PLUGIN-%d] Fallback context creation also failed\n", state->instance_id);
            return false;
        }
    }

    loggy_real_mouse( "[PLUGIN-%d] ✅ EGL context created: %p\n", state->instance_id, state->egl_context);

    // Create instance-specific PBuffer surface (larger for better compatibility)
    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 64,                    // CHANGED: Larger size
        EGL_HEIGHT, 64,                   // CHANGED: Larger size
        EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE, // ADDED: No texture binding needed
        EGL_TEXTURE_TARGET, EGL_NO_TEXTURE, // ADDED: No texture target
        EGL_LARGEST_PBUFFER, EGL_TRUE,    // ADDED: Use largest possible
        EGL_NONE
    };
    
    state->egl_surface = eglCreatePbufferSurface(state->egl_display, config, pbuffer_attribs);
    if (state->egl_surface == EGL_NO_SURFACE) {
        EGLint error = eglGetError();
        loggy_real_mouse( "[PLUGIN-%d] Failed to create PBuffer surface: 0x%x\n", state->instance_id, error);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }

    loggy_real_mouse( "[PLUGIN-%d] ✅ PBuffer surface created: %p\n", state->instance_id, state->egl_surface);

    // Make context current with proper error checking
    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        EGLint error = eglGetError();
        loggy_real_mouse( "[PLUGIN-%d] Failed to make EGL context current: 0x%x\n", state->instance_id, error);
        eglDestroySurface(state->egl_display, state->egl_surface);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }

    // Verify OpenGL context is working
    const char* gl_vendor = (const char*)glGetString(GL_VENDOR);
    const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
    const char* gl_version = (const char*)glGetString(GL_VERSION);
    
    loggy_real_mouse( "[PLUGIN-%d] ✅ OpenGL Context Active:\n", state->instance_id);
    loggy_real_mouse( "[PLUGIN-%d]   Vendor: %s\n", state->instance_id, gl_vendor ? gl_vendor : "Unknown");
    loggy_real_mouse( "[PLUGIN-%d]   Renderer: %s\n", state->instance_id, gl_renderer ? gl_renderer : "Unknown");
    loggy_real_mouse( "[PLUGIN-%d]   Version: %s\n", state->instance_id, gl_version ? gl_version : "Unknown");

    // Test basic OpenGL operation to ensure context works
    GLuint test_texture;
    glGenTextures(1, &test_texture);
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_real_mouse( "[PLUGIN-%d] OpenGL context test failed: 0x%x\n", state->instance_id, gl_error);
        eglMakeCurrent(state->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(state->egl_display, state->egl_surface);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }
    glDeleteTextures(1, &test_texture);

    loggy_real_mouse( "[PLUGIN-%d] ✅ EGL context fully initialized and tested\n", state->instance_id);
    return true;
}*/
static bool init_plugin_egl(struct plugin_state *state) {
    fprintf(stderr, "[PLUGIN-%d] Initializing EGL via GBM (Intel-compatible)...\n", state->instance_id);
    
    // Step 1: Open DRM render node
    state->drm_fd = open("/dev/dri/renderD128", O_RDWR);
    if (state->drm_fd < 0) {
        fprintf(stderr, "[PLUGIN-%d] Failed to open /dev/dri/renderD128: %s\n", state->instance_id, strerror(errno));
        return false;
    }
    fprintf(stderr, "[PLUGIN-%d] Opened render node: fd=%d\n", state->instance_id, state->drm_fd);
    
    // Step 2: Create GBM device
    state->gbm = gbm_create_device(state->drm_fd);
    if (!state->gbm) {
        fprintf(stderr, "[PLUGIN-%d] Failed to create GBM device\n", state->instance_id);
        close(state->drm_fd);
        return false;
    }
    fprintf(stderr, "[PLUGIN-%d] Created GBM device\n", state->instance_id);
    
    // Step 3: Get EGL display from GBM
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = 
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    
    if (eglGetPlatformDisplayEXT) {
        state->egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, state->gbm, NULL);
    } else {
        state->egl_display = eglGetDisplay((EGLNativeDisplayType)state->gbm);
    }
    
    if (state->egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "[PLUGIN-%d] Failed to get EGL display from GBM\n", state->instance_id);
        gbm_device_destroy(state->gbm);
        close(state->drm_fd);
        return false;
    }
    
    // Step 4: Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(state->egl_display, &major, &minor)) {
        fprintf(stderr, "[PLUGIN-%d] Failed to initialize EGL: 0x%x\n", state->instance_id, eglGetError());
        gbm_device_destroy(state->gbm);
        close(state->drm_fd);
        return false;
    }
    fprintf(stderr, "[PLUGIN-%d] EGL %d.%d initialized\n", state->instance_id, major, minor);
    
    // Step 5: Choose config - TRY MULTIPLE VARIANTS (THIS IS THE FIX!)
    EGLConfig config;
    EGLint config_count;
    bool config_found = false;
    
    // Variant 1: Surfaceless - NO SURFACE_TYPE (works on Intel GBM!)
    EGLint config_attribs_v1[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    // Variant 2: Window bit (sometimes works)
    EGLint config_attribs_v2[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    // Variant 3: Minimal
    EGLint config_attribs_v3[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    
    if (eglChooseConfig(state->egl_display, config_attribs_v1, &config, 1, &config_count) && config_count > 0) {
        fprintf(stderr, "[PLUGIN-%d] Found EGL config (surfaceless variant)\n", state->instance_id);
        config_found = true;
    } else if (eglChooseConfig(state->egl_display, config_attribs_v2, &config, 1, &config_count) && config_count > 0) {
        fprintf(stderr, "[PLUGIN-%d] Found EGL config (window variant)\n", state->instance_id);
        config_found = true;
    } else if (eglChooseConfig(state->egl_display, config_attribs_v3, &config, 1, &config_count) && config_count > 0) {
        fprintf(stderr, "[PLUGIN-%d] Found EGL config (minimal variant)\n", state->instance_id);
        config_found = true;
    }
    
    if (!config_found) {
        fprintf(stderr, "[PLUGIN-%d] Failed to find any EGL config\n", state->instance_id);
        eglTerminate(state->egl_display);
        gbm_device_destroy(state->gbm);
        close(state->drm_fd);
        return false;
    }
    
    // Step 6: Create context
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    state->egl_context = eglCreateContext(state->egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (state->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "[PLUGIN-%d] Failed to create EGL context: 0x%x\n", state->instance_id, eglGetError());
        eglTerminate(state->egl_display);
        gbm_device_destroy(state->gbm);
        close(state->drm_fd);
        return false;
    }
    fprintf(stderr, "[PLUGIN-%d] Created EGL context\n", state->instance_id);
    
    // Step 7: Use SURFACELESS context (KEY FOR INTEL!)
    state->egl_surface = EGL_NO_SURFACE;
    
    if (!eglMakeCurrent(state->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, state->egl_context)) {
        fprintf(stderr, "[PLUGIN-%d] Surfaceless eglMakeCurrent failed: 0x%x\n", state->instance_id, eglGetError());
        eglDestroyContext(state->egl_display, state->egl_context);
        eglTerminate(state->egl_display);
        gbm_device_destroy(state->gbm);
        close(state->drm_fd);
        return false;
    }
    
    // Verify OpenGL is working
    const char* gl_vendor = (const char*)glGetString(GL_VENDOR);
    const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
    const char* gl_version = (const char*)glGetString(GL_VERSION);
    
    fprintf(stderr, "[PLUGIN-%d] ✅ OpenGL Context Active:\n", state->instance_id);
    fprintf(stderr, "[PLUGIN-%d]   Vendor: %s\n", state->instance_id, gl_vendor ? gl_vendor : "Unknown");
    fprintf(stderr, "[PLUGIN-%d]   Renderer: %s\n", state->instance_id, gl_renderer ? gl_renderer : "Unknown");
    fprintf(stderr, "[PLUGIN-%d]   Version: %s\n", state->instance_id, gl_version ? gl_version : "Unknown");
    
    // Test basic OpenGL operation
    GLuint test_texture;
    glGenTextures(1, &test_texture);
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        fprintf(stderr, "[PLUGIN-%d] OpenGL context test failed: 0x%x\n", state->instance_id, gl_error);
        eglMakeCurrent(state->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(state->egl_display, state->egl_context);
        eglTerminate(state->egl_display);
        gbm_device_destroy(state->gbm);
        close(state->drm_fd);
        return false;
    }
    glDeleteTextures(1, &test_texture);
    
    fprintf(stderr, "[PLUGIN-%d] ✅ EGL context fully initialized and tested\n", state->instance_id);
    return true;
}

// REPLACE your texture_from_dmabuf function with this version:
// Make sure your texture_from_dmabuf function looks like this:

static GLuint texture_from_dmabuf(struct plugin_state *state, const ipc_window_info_t *info) {
    if (!state->dmabuf_ctx.initialized) {
        loggy_real_mouse( "[PLUGIN] Shared DMA-BUF context not available, using fallback\n");
        return 0;  // Will fallback to stride fix method
    }
    
    loggy_real_mouse( "[PLUGIN] Importing window DMA-BUF via shared library: %dx%d, fd=%d, format=0x%x, modifier=0x%lx\n",
            info->width, info->height, info->fd, info->format, info->modifier);
    
    // Convert to shared library format
    dmabuf_share_buffer_t dmabuf = {
        .fd = info->fd,
        .width = info->width,
        .height = info->height,
        .format = info->format,
        .stride = info->stride,
        .modifier = info->modifier
    };
    
    // Use shared library to import
    GLuint texture = dmabuf_share_import_texture(&state->dmabuf_ctx, &dmabuf);
    
    // ALWAYS close the FD - the shared library should have made its own references
   // loggy_real_mouse( "[PLUGIN] 🔍 Shared library done, closing FD %d\n", info->fd);
  //  close(info->fd);
    
    if (texture > 0) {
        loggy_real_mouse( "[PLUGIN] ✅ Imported window DMA-BUF via shared library: texture %u\n", texture);
    } else {
        loggy_real_mouse( "[PLUGIN] Shared library DMA-BUF import failed for window\n");
    }
    
    return texture;
}




static void send_frame_notification_smart(struct plugin_state *state) {
    static int notification_counter = 0;
    static struct timeval last_notification = {0};
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Calculate time since last notification
    struct timeval diff;
    timersub(&now, &last_notification, &diff);
    
    // Only send notification if:
    // 1. Content actually changed, OR
    // 2. It's been more than 33ms (30fps max notification rate)
    bool should_notify = false;
    
    if (state->content_changed_this_frame) {
        should_notify = true;  // Always notify on content change
    } else if (diff.tv_sec > 0 || diff.tv_usec > 33333) {
        // Limit to 30fps when no content changes
        should_notify = true;
    }
    
    if (should_notify) {
        ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
        write(state->ipc_fd, &notification, sizeof(notification));
        last_notification = now;
        
        // Debug log occasionally
        if (++notification_counter <= 3) {
            loggy_real_mouse("[PLUGIN] Sent notification #%d\n", notification_counter);
        }
    }
}

// Add these pool management functions
static int get_free_pool_buffer(struct plugin_state *state) {
    if (!state->dmabuf_pool.initialized) {
        return -1;
    }
    
    // Find first available buffer
    for (int i = 0; i < state->dmabuf_pool.pool_size; i++) {
        if (!state->dmabuf_pool.in_use[i]) {
            state->dmabuf_pool.in_use[i] = true;
            loggy_real_mouse( "[PLUGIN] 📦 Using pool buffer %d (fd=%d)\n", i, state->dmabuf_pool.fds[i]);
            return i;
        }
    }
    
    loggy_real_mouse( "[PLUGIN] ⚠️  No free pool buffers available\n");
    return -1;  // No free buffers
}

static void release_pool_buffer(struct plugin_state *state, int buffer_id) {
    if (buffer_id >= 0 && buffer_id < state->dmabuf_pool.pool_size) {
        state->dmabuf_pool.in_use[buffer_id] = false;
        loggy_real_mouse( "[PLUGIN] 📤 Released pool buffer %d\n", buffer_id);
    }
}


// Add this function to test pool-based rendering
static void test_pool_rendering(struct plugin_state *state) {
    loggy_real_mouse( "[PLUGIN] === STEP 3: Testing pool-based rendering ===\n");
    
    if (!state->dmabuf_pool.initialized) {
        loggy_real_mouse( "[PLUGIN] ❌ Pool not initialized\n");
        return;
    }
    
    // Get a buffer from pool
    int buffer_id = get_free_pool_buffer(state);
    if (buffer_id < 0) {
        loggy_real_mouse( "[PLUGIN] ❌ No free pool buffer for rendering\n");
        return;
    }
    
    // For now, just test that we can "use" the buffer
    int pool_fd = state->dmabuf_pool.fds[buffer_id];
    loggy_real_mouse( "[PLUGIN] 🎨 Simulating render to pool buffer %d (fd=%d)\n", buffer_id, pool_fd);
    
    // Simulate some work
    usleep(1000); // 1ms "render time"
    
    // For now, just release it (later we'll send it to compositor)
    loggy_real_mouse( "[PLUGIN] ✅ Render complete, releasing buffer %d\n", buffer_id);
    release_pool_buffer(state, buffer_id);
    
    loggy_real_mouse( "[PLUGIN] ✅ Pool rendering test complete\n");
}

// Add this function to send pool FDs to compositor
static bool send_pool_to_compositor(struct plugin_state *state) {
    loggy_real_mouse( "[PLUGIN] === STEP 4: Sending pool to compositor ===\n");
    
    if (!state->dmabuf_pool.initialized) {
        loggy_real_mouse( "[PLUGIN] ❌ Pool not initialized\n");
        return false;
    }
    
    // Send simple notification + all FDs in one message
    ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
    
    struct msghdr msg = {0};
    struct iovec iov[1];
    char cmsg_buf[CMSG_SPACE(sizeof(int) * 4)]; // Space for 4 FDs

    // Message data
    iov[0].iov_base = &notification;
    iov[0].iov_len = sizeof(notification);
    
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    // Pack all 4 FDs into ancillary data
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * 4);
    
    int *fd_data = (int*)CMSG_DATA(cmsg);
    for (int i = 0; i < 4; i++) {
        fd_data[i] = dup(state->dmabuf_pool.fds[i]); // Dup for sending
        loggy_real_mouse( "[PLUGIN] 📤 Packing pool buffer %d: fd=%d -> dup_fd=%d\n", 
                i, state->dmabuf_pool.fds[i], fd_data[i]);
    }

    // Send the message with all FDs at once
    if (sendmsg(state->ipc_fd, &msg, 0) < 0) {
        loggy_real_mouse( "[PLUGIN] ❌ Failed to send pool FDs: %s\n", strerror(errno));
        
        // Clean up duped FDs on failure
        for (int i = 0; i < 4; i++) {
            close(fd_data[i]);
        }
        return false;
    }
    
    loggy_real_mouse( "[PLUGIN] ✅ Sent pool (4 FDs) to compositor in single message!\n");
    return true;
}
static void test_pool_usage(struct plugin_state *state) {
    loggy_real_mouse( "[PLUGIN] === STEP 2: Testing pool usage ===\n");
    
    // Test getting and releasing buffers
    int buf1 = get_free_pool_buffer(state);
    int buf2 = get_free_pool_buffer(state);
    int buf3 = get_free_pool_buffer(state);
    
    loggy_real_mouse( "[PLUGIN] Got buffers: %d, %d, %d\n", buf1, buf2, buf3);
    
    // Release one and get another
    release_pool_buffer(state, buf1);
    int buf4 = get_free_pool_buffer(state);
    
    loggy_real_mouse( "[PLUGIN] After release/reuse: buf4=%d\n", buf4);
    
    // Clean up
    release_pool_buffer(state, buf2);
    release_pool_buffer(state, buf3);
    release_pool_buffer(state, buf4);
    
    loggy_real_mouse( "[PLUGIN] ✅ Pool usage test complete\n");
}


// In your plugin code:
void send_dmabuf_to_compositor(int ipc_fd, int dmabuf_fd, uint32_t width, uint32_t height, 
                               uint32_t format, uint32_t stride, uint64_t modifier) {
    ipc_notification_t notification = {0};
    notification.type = IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT;
    notification.data.dmabuf_frame_info.width = width;
    notification.data.dmabuf_frame_info.height = height;
    notification.data.dmabuf_frame_info.format = format;
    notification.data.dmabuf_frame_info.stride = stride;
    notification.data.dmabuf_frame_info.modifier = modifier;
    
    struct msghdr msg = {0};
    struct iovec iov = { .iov_base = &notification, .iov_len = sizeof(notification) };
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &dmabuf_fd, sizeof(int));
    
    sendmsg(ipc_fd, &msg, 0);
}



// Add these EGL extension function pointers (add after your includes)
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = NULL;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = NULL;
static PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA = NULL;
static PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA = NULL;

// Add this initialization function (call it once in your plugin init)
static bool init_egl_extensions(EGLDisplay display) {
    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    eglExportDMABUFImageQueryMESA = (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)eglGetProcAddress("eglExportDMABUFImageQueryMESA");
    eglExportDMABUFImageMESA = (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");
    
    if (!eglCreateImageKHR || !eglDestroyImageKHR) {
        loggy_real_mouse( "[PLUGIN] Basic EGL image extensions not available\n");
        return false;
    }
    
    if (!eglExportDMABUFImageQueryMESA || !eglExportDMABUFImageMESA) {
        loggy_real_mouse( "[PLUGIN] MESA DMA-BUF export extensions not available\n");
        return false;
    }
    
    return true;
}



static void render_frame_hybrid_zero_copy_with_cursor(struct plugin_state *state, int frame_count) {
    // Calculate delta time for smooth cursor animation
    static struct timeval last_time = {0};
    struct timeval now;
    gettimeofday(&now, NULL);
    
    float delta_time = 0.016f; // Default to 60fps
    if (last_time.tv_sec != 0) {
        delta_time = (now.tv_sec - last_time.tv_sec) + 
                    (now.tv_usec - last_time.tv_usec) / 1000000.0f;
        delta_time = fminf(delta_time, 0.1f); // Cap at 100ms to prevent large jumps
    }
    last_time = now;
    
    // Update cursor animation
    update_cursor(state, delta_time);
    
    // Your existing zero-copy rendering setup
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glViewport(0, 0, state->width, state->height);
    
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0); 
    

    // NEW: Render cursor on top of everything
    render_cursor(state);
    
    glDisable(GL_BLEND);
    glFlush();
    glFinish(); // Make sure rendering is complete for DMA-BUF
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Your existing DMA-BUF sending logic
    int dmabuf_fd_to_send = -1;
    if (state->dmabuf_fd >= 0) {
        dmabuf_fd_to_send = dup(state->dmabuf_fd);
        if (dmabuf_fd_to_send >= 0) {
            send_dmabuf_to_compositor(
                state->ipc_fd, 
                dmabuf_fd_to_send,
                state->width,
                state->height,
                DRM_FORMAT_ARGB8888,
                state->width * 4,
                DRM_FORMAT_MOD_LINEAR
            );

            close(dmabuf_fd_to_send);
            loggy_real_mouse( "[PLUGIN] ✅ Sent zero-copy frame with cursor (orig fd=%d)\n", state->dmabuf_fd);
        } else {
             loggy_real_mouse( "[PLUGIN] ❌ dup() failed for sending frame: %s\n", strerror(errno));
        }
    } else {
        loggy_real_mouse( "[PLUGIN] No DMA-BUF available, falling back to SHM\n");
        // SHM fallback if needed
    }
}

// REPLACE your render_with_pool_smart function with this version:
static void render_with_pool_smart(struct plugin_state *state, int frame_count) {
    static struct timeval last_render = {0};
    struct timeval now;
    gettimeofday(&now, NULL);
    
    struct timeval diff;
    timersub(&now, &last_render, &diff);
    
    // Only render if content changed OR 16ms elapsed (60fps)
    bool should_render = state->content_changed_this_frame || 
                        (diff.tv_sec > 0 || diff.tv_usec >= 16667);
    
    if (!should_render) {
        return; // Skip this frame
    }
    
    last_render = now;
    
    // CHOOSE THE RIGHT RENDER METHOD based on DMA-BUF availability
    if (state->dmabuf_fd >= 0) {
        // True zero-copy: render directly to DMA-BUF
        render_frame_hybrid_zero_copy_with_cursor(state, frame_count);
        
        static int zero_copy_log = 0;
        if (++zero_copy_log <= 5) {
            loggy_real_mouse( "[PLUGIN] ✅ TRUE ZERO-COPY render to DMA-BUF fd=%d (no glReadPixels!)\n", 
                    state->dmabuf_fd);
        }
    } else {
        // Fallback: render to regular FBO + copy to SHM
      //  render_frame_hybrid(state, frame_count);
        
        static int shm_log = 0;
        if (++shm_log <= 5) {
            loggy_real_mouse( "[PLUGIN] SHM fallback render (with glReadPixels)\n");
        }
    }
    
    // Send notifications when content changes
    if (state->content_changed_this_frame) {
        send_frame_notification_smart(state);
        state->content_changed_this_frame = false;
        
        if (frame_count <= 5) {
            loggy_real_mouse( "[PLUGIN] 📢 Sent notification for content change\n");
        }
    }
    
    // Occasionally send heartbeat (every 2 seconds)
    static int heartbeat_counter = 0;
    if (++heartbeat_counter % 120 == 0) {  // Every 2 seconds at 60fps
        send_frame_notification_smart(state);
    }
}


// Helper to create a Framebuffer Object (FBO) for off-screen rendering
static bool init_fbo(struct plugin_state *state, int width, int height) {
    state->width = width;
    state->height = height;

    glGenFramebuffers(1, &state->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);

    glGenTextures(1, &state->fbo_texture);
    glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state->fbo_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        loggy_real_mouse( "Framebuffer is not complete!\n");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}


static bool validate_window_texture(struct plugin_state *state, int window_index) {
    if (window_index < 0 || window_index >= state->window_count) {
        return false;
    }
    
    if (!state->windows[window_index].is_valid) {
        return false;
    }
    
    GLuint texture = state->windows[window_index].gl_texture;
    if (texture == 0) {
        return false;
    }
    
    // Check if texture object is still valid
    if (!glIsTexture(texture)) {
        loggy_real_mouse( "[PLUGIN] Window %d texture %u is no longer valid\n", window_index, texture);
        state->windows[window_index].gl_texture = 0;
        state->windows[window_index].is_valid = false;
        return false;
    }
    
    // Check texture parameters
    glBindTexture(GL_TEXTURE_2D, texture);
    GLint width, height;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        loggy_real_mouse( "[PLUGIN] Error validating texture %u: 0x%x\n", texture, error);
        state->windows[window_index].gl_texture = 0;
        state->windows[window_index].is_valid = false;
        return false;
    }
    
    if (width <= 0 || height <= 0) {
        loggy_real_mouse( "[PLUGIN] Window %d texture has invalid dimensions: %dx%d\n", 
                window_index, width, height);
        state->windows[window_index].gl_texture = 0;
        state->windows[window_index].is_valid = false;
        return false;
    }
    
    return true;
}





// Add this function - just creates the pool, doesn't send anything yet
static bool init_dmabuf_pool(struct plugin_state *state) {
    loggy_real_mouse( "[PLUGIN] === STEP 1: Creating DMA-BUF pool ===\n");
    
    state->dmabuf_pool.pool_size = 4;  // Start small - just 4 buffers
    state->dmabuf_pool.initialized = false;
    
    // Create 4 DMA-BUFs
    for (int i = 0; i < 4; i++) {
        int fd = create_dmabuf(SHM_WIDTH * SHM_HEIGHT * 4, state->instance_id);
        if (fd < 0) {
            loggy_real_mouse( "[PLUGIN] ❌ Failed to create pool DMA-BUF %d\n", i);
            return false;
        }
        
        state->dmabuf_pool.fds[i] = fd;
        state->dmabuf_pool.in_use[i] = false;
        state->dmabuf_pool.textures[i] = 0;
        
        loggy_real_mouse( "[PLUGIN] ✅ Created pool buffer %d: fd=%d\n", i, fd);
    }
    
    state->dmabuf_pool.initialized = true;
    loggy_real_mouse( "[PLUGIN] ✅ Pool ready with %d buffers\n", 4);
    return true;
}




// BETTER APPROACH: Send DMA-BUF once, then just send notifications

// Send DMA-BUF info only once at startup
static bool send_dmabuf_info_once(struct plugin_state *state) {
   // CRITICAL FIX: Only send the DMA-BUF info TRULY ONCE
    if (state->dmabuf_sent_to_compositor || state->dmabuf_fd < 0) {
        return state->dmabuf_sent_to_compositor;
    }
    
    loggy_real_mouse( "[PLUGIN] Sending DMA-BUF info for the FIRST AND ONLY TIME (fd: %d)\n", state->dmabuf_fd);
    
    ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT;
    ipc_dmabuf_frame_info_t frame_info = {
        .width = state->width,
        .height = state->height,
        .format = DRM_FORMAT_ABGR8888,
        .stride = state->width * 4,
        .modifier = DRM_FORMAT_MOD_LINEAR,
        .offset = 0,
    };
    
    int fd_to_send = dup(state->dmabuf_fd);
    if (fd_to_send < 0) {
        loggy_real_mouse( "[PLUGIN] Failed to dup DMA-BUF fd: %s\n", strerror(errno));
        return false;
    }

    struct msghdr msg = {0};
    struct iovec iov[2];
    char cmsg_buf[CMSG_SPACE(sizeof(int))];

    iov[0].iov_base = &notification;
    iov[0].iov_len = sizeof(notification);
    iov[1].iov_base = &frame_info;
    iov[1].iov_len = sizeof(frame_info);

    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *((int*)CMSG_DATA(cmsg)) = fd_to_send;

    if (sendmsg(state->ipc_fd, &msg, 0) < 0) {
        loggy_real_mouse( "[PLUGIN] Failed to send DMA-BUF info: %s\n", strerror(errno));
        close(fd_to_send); // Also close on failure
        return false;
    }
    
    // FIX: Close the duplicated FD after it has been sent.
    close(fd_to_send);

    // CRITICAL: Mark as sent and NEVER send again
    state->dmabuf_sent_to_compositor = true;
    
    
    loggy_real_mouse( "[PLUGIN] ✅ DMA-BUF info sent ONCE - will never send again\n");
    return true;
}

// Simple frame ready notification (no FD)
static void send_frame_ready(struct plugin_state *state) {
    // Only send simple notification - NO FDs, NO buffer info
    ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
    
    if (write(state->ipc_fd, &notification, sizeof(notification)) < 0) {
        loggy_real_mouse( "[PLUGIN] Failed to send frame ready: %s\n", strerror(errno));
        return;
    }
    
    state->last_frame_sequence++;
    
    // Only log the first few times
    static int log_count = 0;
    if (++log_count <= 5) {
        loggy_real_mouse( "[PLUGIN] ✅ Frame ready notification sent (seq=%u, NO FDs)\n", 
                state->last_frame_sequence);
    }
}





// ============================================================================
// NEW HELPER FUNCTIONS FOR DMA-BUF IPC
// ============================================================================

/**
 * Receives a message and ancillary file descriptors over a UNIX socket.
 * This is the magic that allows the compositor to send DMA-BUF handles.
 */
// Enhanced IPC FD reception with better error handling and debugging
static ssize_t read_ipc_with_fds(int sock_fd, ipc_event_t *event, int *fds, int *fd_count) {
    struct msghdr msg = {0};
    struct iovec iov[1] = {0};
    char cmsg_buf[CMSG_SPACE(sizeof(int) * 32)];

    iov[0].iov_base = event;
    iov[0].iov_len = sizeof(ipc_event_t);

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

ssize_t n = recvmsg(sock_fd, &msg, 0);
    if (n <= 0) {
        if (n < 0) {
            loggy_real_mouse( "[PLUGIN] ❌ recvmsg FAILED: %s\n", strerror(errno));
        }
        return n;
    }

    *fd_count = 0;
    
    loggy_real_mouse( "[PLUGIN] recvmsg received %zd bytes\n", n);
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg) {
        loggy_real_mouse( "[PLUGIN] No control message (no FDs sent)\n");
    }
    
    while (cmsg != NULL) {
        loggy_real_mouse( "[PLUGIN] Control message: level=%d, type=%d, len=%zu\n", 
                cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
                
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int num_fds_in_msg = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int *received_fds = (int *)CMSG_DATA(cmsg);
            
            loggy_real_mouse( "[PLUGIN] ✅ Received %d FDs: ", num_fds_in_msg);
            for (int i = 0; i < num_fds_in_msg; i++) {
                if (*fd_count < 32) {
                    fds[*fd_count] = received_fds[i];
                    loggy_real_mouse( "%d ", received_fds[i]);
                    (*fd_count)++;
                } else {
                    loggy_real_mouse( "(overflow, closing %d) ", received_fds[i]);
                    close(received_fds[i]);
                }
            }
            loggy_real_mouse( "\n");
        }
        cmsg = CMSG_NXTHDR(&msg, cmsg);
    }

    return n;
}




// Hybrid texture import: Try fast methods first, fallback to reliable methods
// REPLACE your create_texture_hybrid function with this fixed version:

static GLuint create_texture_hybrid(struct plugin_state *state, const ipc_window_info_t *info, int window_index) {
    if (info->fd < 0) {
        loggy_real_mouse( "[PLUGIN] Invalid file descriptor for window %d\n", window_index);
        return 0;
    }
    
    // Log format for debugging weston apps
    static int format_log_count = 0;
    if (format_log_count < 10) {
        loggy_real_mouse( "[PLUGIN] Window %d format: 0x%x (%dx%d, stride=%u, modifier=0x%lx)\n", 
                window_index, info->format, info->width, info->height, info->stride, info->modifier);
        format_log_count++;
    }
    
   GLuint texture = 0;
    
    if (info->buffer_type == IPC_BUFFER_TYPE_DMABUF && state->dmabuf_ctx.initialized) {
        texture = texture_from_dmabuf(state, info);
        // DO NOT close the FD here. Just return.
        if (texture > 0) return texture;
    }
    
    if (info->buffer_type == IPC_BUFFER_TYPE_DMABUF) {
        texture = create_texture_from_dmabuf_manual(state, info);
        // DO NOT close the FD here.
        if (texture > 0) return texture;
    }
    
    texture = create_texture_with_stride_fix_optimized(state, info);
    // DO NOT close the FD here.
    if (texture > 0) return texture;
    
   
    
    loggy_real_mouse( "[PLUGIN] ❌ All texture creation methods failed for window %d (fd %d)\n", window_index, info->fd);
    
    // Even on failure, DO NOT close the FD. The caller must do it.
    return 0; 
}
// Manual EGL DMA-BUF import (bypass wlroots complexity)

// Enhanced manual DMA-BUF import with better format support
static GLuint create_texture_from_dmabuf_manual(struct plugin_state *state, const ipc_window_info_t *info) {
    // Validate FD first
    if (info->fd < 0) {
        return 0;
    }
    
    // Test FD validity
    struct stat fd_stat;
    if (fstat(info->fd, &fd_stat) != 0) {
        loggy_real_mouse( "[PLUGIN] Invalid FD %d: %s\n", info->fd, strerror(errno));
        return 0;
    }
    
    // Get EGL extension functions
    static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = NULL;
    static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = NULL;
    static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = NULL;
    
    if (!eglCreateImageKHR) {
        eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    }
    
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES || !eglDestroyImageKHR) {
        return 0;
    }
    
    // FIXED: More robust format handling
    EGLint drm_format;
    switch (info->format) {
        case 0x34325241: // DRM_FORMAT_ARGB8888
            drm_format = info->format;
            break;
        case 0x34325258: // DRM_FORMAT_XRGB8888
            drm_format = info->format;
            break;
        case 0x34324142: // DRM_FORMAT_ABGR8888
            drm_format = info->format;
            break;
        case 0x34324258: // DRM_FORMAT_XBGR8888
            drm_format = info->format;
            break;
        default:
            loggy_real_mouse( "[PLUGIN] Unsupported DMA-BUF format: 0x%x\n", info->format);
            return 0;
    }
    
    // FIXED: Ensure context is current before EGL operations
    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        loggy_real_mouse( "[PLUGIN] Failed to make EGL context current\n");
        return 0;
    }
    
    // Create EGL image with proper attributes
    EGLint attribs[] = {
        EGL_WIDTH, (EGLint)info->width,
        EGL_HEIGHT, (EGLint)info->height,
        EGL_LINUX_DRM_FOURCC_EXT, drm_format,
        EGL_DMA_BUF_PLANE0_FD_EXT, info->fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)info->stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE
    };
    
    EGLImageKHR egl_image = eglCreateImageKHR(
        state->egl_display, 
        EGL_NO_CONTEXT,  // FIXED: Use EGL_NO_CONTEXT for DMA-BUF
        EGL_LINUX_DMA_BUF_EXT, 
        NULL, 
        attribs
    );
    
    if (egl_image == EGL_NO_IMAGE_KHR) {
        EGLint egl_error = eglGetError();
        loggy_real_mouse( "[PLUGIN] Failed to create EGL image: 0x%x\n", egl_error);
        return 0;
    }
    
    // Create GL texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // CRITICAL: Set texture parameters BEFORE EGL target call
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Bind EGL image to texture
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image);
    
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_real_mouse( "[PLUGIN] OpenGL error creating texture from EGL image: 0x%x\n", gl_error);
        glDeleteTextures(1, &texture);
        eglDestroyImageKHR(state->egl_display, egl_image);
        return 0;
    }
    
    // CRITICAL: Clean up EGL image immediately - texture holds the reference
    eglDestroyImageKHR(state->egl_display, egl_image);
    
    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texture;
}
// Optimized stride correction (less debugging, faster processing)
static GLuint create_texture_with_stride_fix_optimized(struct plugin_state *state, const ipc_window_info_t *info) {
    uint32_t expected_stride = info->width * 4;
    
    // Enhanced format detection and handling
    uint32_t bytes_per_pixel = 4;
    bool needs_format_conversion = false;
    
    // Detect format and adjust accordingly
    switch (info->format) {
        case 0x34325241: // DRM_FORMAT_ARGB8888
        case 0x34325258: // DRM_FORMAT_XRGB8888  
        case 0x34324142: // DRM_FORMAT_ABGR8888
        case 0x34324258: // DRM_FORMAT_XBGR8888
            bytes_per_pixel = 4;
            needs_format_conversion = true;
            break;
        case 0x36314752: // DRM_FORMAT_RGB565
            bytes_per_pixel = 2;
            expected_stride = info->width * 2;
            break;
        default:
            // Assume RGBA8888 for unknown formats
            bytes_per_pixel = 4;
            break;
    }
    
    // Log format information for debugging
    if (info->stride != expected_stride) {
        loggy_real_mouse( "[PLUGIN] Format 0x%x: stride %u vs expected %u (bpp=%u)\n", 
                info->format, info->stride, expected_stride, bytes_per_pixel);
    }
    
    size_t map_size = info->stride * info->height;
    void *mapped_data = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, info->fd, 0);
    if (mapped_data == MAP_FAILED) {
        loggy_real_mouse( "[PLUGIN] mmap failed: %s\n", strerror(errno));
        return 0;
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set proper pixel store parameters
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // Changed from 4 to 1 for better compatibility
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    
    // Fast path: if stride matches and no format conversion needed
    if (info->stride == expected_stride && !needs_format_conversion) {
        GLenum gl_format = GL_RGBA;
        GLenum gl_type = GL_UNSIGNED_BYTE;
        
        // Handle RGB565 format
        if (info->format == 0x36314752) {
            gl_format = GL_RGB;
            gl_type = GL_UNSIGNED_SHORT_5_6_5;
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 
                     0, gl_format, gl_type, mapped_data);
    } else {
        // Slow path: stride correction and/or format conversion needed
        size_t corrected_size = info->width * info->height * 4;
        uint8_t *corrected_data = malloc(corrected_size);
        if (!corrected_data) {
            munmap(mapped_data, map_size);
            glDeleteTextures(1, &texture);
            return 0;
        }
        
        uint8_t *src = (uint8_t *)mapped_data;
        uint8_t *dst = corrected_data;
        
        for (uint32_t y = 0; y < info->height; y++) {
            if (needs_format_conversion) {
                // Convert pixel formats
                for (uint32_t x = 0; x < info->width; x++) {
                    uint32_t *src_pixel = (uint32_t *)(src + x * 4);
                    uint32_t *dst_pixel = (uint32_t *)(dst + x * 4);
                    uint32_t pixel = *src_pixel;
                    
                    switch (info->format) {
                        case 0x34325241: // ARGB8888 -> RGBA8888
                        case 0x34325258: // XRGB8888 -> RGBA8888
                        {
                            uint32_t a = (info->format == 0x34325241) ? (pixel >> 24) & 0xFF : 0xFF;
                            uint32_t r = (pixel >> 16) & 0xFF;
                            uint32_t g = (pixel >> 8) & 0xFF;
                            uint32_t b = pixel & 0xFF;
                            *dst_pixel = (r << 0) | (g << 8) | (b << 16) | (a << 24);
                            break;
                        }
                        case 0x34324142: // ABGR8888 -> RGBA8888
                        case 0x34324258: // XBGR8888 -> RGBA8888
                        {
                            uint32_t a = (info->format == 0x34324142) ? (pixel >> 24) & 0xFF : 0xFF;
                            uint32_t b = (pixel >> 16) & 0xFF;
                            uint32_t g = (pixel >> 8) & 0xFF;
                            uint32_t r = pixel & 0xFF;
                            *dst_pixel = (r << 0) | (g << 8) | (b << 16) | (a << 24);
                            break;
                        }
                        default:
                            *dst_pixel = pixel; // No conversion
                            break;
                    }
                }
            } else {
                // Simple stride correction without format conversion
                memcpy(dst, src, info->width * bytes_per_pixel);
            }
            
            src += info->stride;
            dst += info->width * 4;
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, corrected_data);
        
        free(corrected_data);
    }
    
    munmap(mapped_data, map_size);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return texture;
}










// HYBRID APPROACH: Simple initialization + Fast DMA-BUF import when possible
// Modified main() function to use DMA-BUF output instead of SHM
int main(int argc, char *argv[]) {
    loggy_real_mouse( "[PLUGIN] === HYBRID FAST VERSION WITH DMA-BUF OUTPUT ===\n");
    
    // Parse arguments (same as before)
    int ipc_fd = -1;
    const char* shm_name = NULL;
    
    if (argc >= 3) {
        if (strncmp(argv[1], "--", 2) != 0) {
            ipc_fd = atoi(argv[1]);
            shm_name = argv[2];
        } else {
            for (int i = 1; i < argc - 1; i++) {
                if (strcmp(argv[i], "--ipc-fd") == 0 && i + 1 < argc) {
                    ipc_fd = atoi(argv[i + 1]);
                    i++;
                } else if (strcmp(argv[i], "--shm-name") == 0 && i + 1 < argc) {
                    shm_name = argv[i + 1];
                    i++;
                }
            }
        }
    }
    
    if (ipc_fd < 0 || !shm_name) {
        loggy_real_mouse( "[PLUGIN] ERROR: Invalid arguments\n");
        return 1;
    }

    loggy_real_mouse( "[PLUGIN] Starting hybrid plugin with DMA-BUF output\n");

       struct plugin_state state = {0};
    state.instance_id = getpid() % 1000;  // Simple unique ID
    snprintf(state.instance_name, sizeof(state.instance_name), "plugin-%d", state.instance_id);
    
    loggy_real_mouse( "[%s] Starting isolated plugin instance\n", state.instance_name);
    
    // Update all your existing calls:
    state.dmabuf_fd = create_dmabuf(SHM_WIDTH * SHM_HEIGHT * 4, state.instance_id);

    // Initialize state
  
    state.ipc_fd = ipc_fd;
    state.focused_window_index = -1;
    state.width = SHM_WIDTH;
    state.height = SHM_HEIGHT;

state.dmabuf_sent_to_compositor = false;
    state.last_frame_sequence = 0;
    

    // Set up SHM buffer as fallback
    int shm_fd = shm_open(shm_name, O_RDWR, 0600);
    if (shm_fd < 0) { 
        loggy_real_mouse( "[PLUGIN] ERROR: shm_open failed: %s\n", strerror(errno));
        return 1; 
    }
    
    state.shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (state.shm_ptr == MAP_FAILED) { 
        loggy_real_mouse( "[PLUGIN] ERROR: mmap failed: %s\n", strerror(errno));
        close(shm_fd); 
        return 1; 
    }
    close(shm_fd);
    loggy_real_mouse( "[PLUGIN] ✅ SHM buffer ready (fallback)\n");

    // Initialize EGL context
    loggy_real_mouse( "[PLUGIN] Initializing EGL context...\n");
    if (!init_plugin_egl(&state)) {
        loggy_real_mouse( "[PLUGIN] ERROR: EGL initialization failed\n");
        return 1;
    }
    loggy_real_mouse( "[PLUGIN] ✅ EGL context ready\n");

// Right after: loggy_real_mouse( "[PLUGIN] ✅ EGL context ready\n");

// CRITICAL: Initialize EGL extensions
loggy_real_mouse( "[PLUGIN] Initializing EGL extensions...\n");
if (!init_egl_extensions(state.egl_display)) {
    loggy_real_mouse( "[PLUGIN] ⚠️  EGL DMA-BUF export extensions not available\n");
    loggy_real_mouse( "[PLUGIN] Will use fallback DMA-BUF sharing\n");
    // Set flag to use fallback method
    state.use_egl_export = false;
} else {
    loggy_real_mouse( "[PLUGIN] ✅ EGL DMA-BUF export extensions ready\n");
    state.use_egl_export = true;
}

  

    // STEP 1: Test pool creation
loggy_real_mouse( "[PLUGIN] Testing DMA-BUF pool creation...\n");
if (init_dmabuf_pool(&state)) {
    loggy_real_mouse( "[PLUGIN] ✅ Pool creation successful!\n");
} else {
    loggy_real_mouse( "[PLUGIN] ❌ Pool creation failed\n");
}

if (!init_cursor_resources(&state)) {
    loggy_real_mouse( "[PLUGIN] ERROR: Cursor initialization failed\n");
    return 1;
}

test_pool_usage(&state);

test_pool_rendering(&state);    


// Add this right after: loggy_real_mouse( "[PLUGIN] ✅ Pool rendering test complete\n");

// STEP 4: Send pool to compositor
if (send_pool_to_compositor(&state)) {
    loggy_real_mouse( "[PLUGIN] ✅ Pool transfer successful!\n");
} else {
    loggy_real_mouse( "[PLUGIN] ❌ Pool transfer failed!\n");
}

    // Try to initialize DMA-BUF backed FBO (the key change!)
    loggy_real_mouse( "[PLUGIN] Attempting DMA-BUF backed FBO...\n");
    //bool dmabuf_fbo_available = init_dmabuf_fbo(&state, SHM_WIDTH, SHM_HEIGHT);

bool dmabuf_fbo_available = init_dmabuf_fbo_robust(&state, SHM_WIDTH, SHM_HEIGHT);
    
    if (!dmabuf_fbo_available) {
        loggy_real_mouse( "[PLUGIN] DMA-BUF FBO failed, falling back to regular FBO...\n");
        if (!init_fbo(&state, SHM_WIDTH, SHM_HEIGHT)) {
            loggy_real_mouse( "[PLUGIN] ERROR: Fallback FBO initialization failed\n");
            return 1;
        }
    }

    // Try to initialize shared DMA-BUF context for window import
    loggy_real_mouse( "[PLUGIN] Attempting shared DMA-BUF context...\n");
    bool dmabuf_lib_available = dmabuf_share_init_with_display(&state.dmabuf_ctx, state.egl_display);
    if (dmabuf_lib_available) {
        loggy_real_mouse( "[PLUGIN] ✅ Shared DMA-BUF library available\n");
    } else {
        loggy_real_mouse( "[PLUGIN] ⚠️  Shared DMA-BUF library not available\n");
    }

    loggy_real_mouse( "[PLUGIN] 🚀 Hybrid plugin ready!\n");
    loggy_real_mouse( "[PLUGIN] Capabilities:\n");
    loggy_real_mouse( "[PLUGIN]   - DMA-BUF output FBO: %s\n", dmabuf_fbo_available ? "YES" : "NO");
    loggy_real_mouse( "[PLUGIN]   - Shared DMA-BUF library: %s\n", dmabuf_lib_available ? "YES" : "NO");
    loggy_real_mouse( "[PLUGIN]   - EGL DMA-BUF import: YES\n");
    loggy_real_mouse( "[PLUGIN]   - SHM fallback: YES\n");

    // Test IPC connection
    ipc_notification_type_t startup_notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
    write(state.ipc_fd, &startup_notification, sizeof(startup_notification));
    loggy_real_mouse( "[PLUGIN] ✅ IPC connection verified\n");

    // Send DMA-BUF info once if available
    if (dmabuf_fbo_available) {
        if (send_dmabuf_info_once(&state)) {
            loggy_real_mouse( "[PLUGIN] ✅ DMA-BUF output enabled - zero-copy to compositor!\n");
          //  verify_dmabuf_zero_copy(&state);
        } else {
            loggy_real_mouse( "[PLUGIN] ⚠️  DMA-BUF output failed, will use SHM fallback\n");
            dmabuf_fbo_available = false;
        }
    }

    loggy_real_mouse( "[PLUGIN] Entering main event loop...\n");

    // Main loop with DMA-BUF or SHM output
    int frame_count = 0;
    while (true) {
        ipc_event_t event;
        int fds[32];
        int fd_count = 0;
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(state.ipc_fd, &read_fds);
        
        struct timeval timeout = {0, 1000};
        int select_result = select(state.ipc_fd + 1, &read_fds, NULL, NULL, &timeout);
        
if (select_result == 0) {
    frame_count++;
    
    // ADD: Log more frequently to see frame rendering
    if (frame_count <= 10 || frame_count % 60 == 0) {  // Every second at 60fps
        loggy_real_mouse( "[PLUGIN] Frame %d (%s) - Windows: %d\n", frame_count, 
                (state.dmabuf_fd >= 0) ? "ZERO-COPY" : "SHM-COPY", state.window_count);
    }
    
    // ADD: Force a test render even with no windows
    if (frame_count <= 5) {
        loggy_real_mouse( "[PLUGIN] Test render frame %d...\n", frame_count);
        
        // Test render with debug background
        glBindFramebuffer(GL_FRAMEBUFFER, state.fbo);
        glViewport(0, 0, state.width, state.height);
        
        // Different color each frame for debugging
        float colors[][4] = {
            {1.0f, 0.0f, 0.0f, 1.0f}, // Red
            {0.0f, 1.0f, 0.0f, 1.0f}, // Green  
            {0.0f, 0.0f, 1.0f, 1.0f}, // Blue
            {1.0f, 1.0f, 0.0f, 1.0f}, // Yellow
            {1.0f, 0.0f, 1.0f, 1.0f}, // Magenta
        };
        
        int color_idx = (frame_count - 1) % 5;
        glClearColor(colors[color_idx][0], colors[color_idx][1], colors[color_idx][2], colors[color_idx][3]);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        loggy_real_mouse( "[PLUGIN] Test render complete (color %d)\n", color_idx);
    }
    
    // Use the smart rendering that chooses zero-copy or SHM automatically
    render_with_pool_smart(&state, frame_count);
            
    // Send frame notification (works for both DMA-BUF and SHM)
    if (dmabuf_fbo_available && state.dmabuf_fd >= 0) {
        // Zero-copy: just notify compositor that DMA-BUF has new content
        send_frame_ready(&state);

    } else {
        // SHM fallback: copy the FBO to SHM then notify
        if (state.shm_ptr) {
            glBindFramebuffer(GL_FRAMEBUFFER, state.fbo);
    //        glReadPixels(0, 0, state.width, state.height, GL_RGBA, GL_UNSIGNED_BYTE, state.shm_ptr);
            glFlush();
            msync(state.shm_ptr, SHM_BUFFER_SIZE, MS_ASYNC);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        
        ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
        write(state.ipc_fd, &notification, sizeof(notification));
    }
    
    continue;
}


        
  // Handle IPC events with enhanced error checking
        ssize_t n = read_ipc_with_fds(state.ipc_fd, &event, fds, &fd_count);
        if (n <= 0) {
            if (n == 0) {
                loggy_real_mouse( "[PLUGIN] IPC connection closed by compositor\n");
            } else {
                loggy_real_mouse( "[PLUGIN] IPC read error: %s\n", strerror(errno));
            }
            break;
        }
        
       // Process different event types
        if (event.type == IPC_EVENT_TYPE_WINDOW_LIST_UPDATE) {
        //    handle_window_list_hybrid(&state, &event, fds, fd_count);
        } else if (event.type == IPC_EVENT_TYPE_POINTER_MOTION) {
            // HANDLE REAL MOUSE MOTION
            handle_mouse_move(&state, event.data.motion.x, event.data.motion.y);
            loggy_real_mouse( "[PLUGIN] Mouse moved to (%.1f, %.1f)\n", 
                    event.data.motion.x, event.data.motion.y);
        } else if (event.type == IPC_EVENT_TYPE_POINTER_BUTTON) {
            // HANDLE MOUSE BUTTON EVENTS
            handle_mouse_button(&state, event.data.button.button, event.data.button.state);
        } else {
            loggy_real_mouse( "[PLUGIN] Unknown IPC event type: %d\n", event.type);
        }
        
        // Close any remaining unclaimed FDs to prevent leaks
        for (int i = state.window_count; i < fd_count; i++) {
            if (fds[i] >= 0) {
                loggy_real_mouse( "[PLUGIN] Closing unclaimed FD %d\n", fds[i]);
                close(fds[i]);
            }
        }
}
    // Cleanup
    loggy_real_mouse( "[PLUGIN] Cleaning up hybrid plugin...\n");
    
    for (int i = 0; i < state.window_count; i++) {
        if (state.windows[i].gl_texture != 0) {
            glDeleteTextures(1, &state.windows[i].gl_texture);
        }
    }
    
    if (dmabuf_lib_available) {
        dmabuf_share_cleanup(&state.dmabuf_ctx);
    }
    
    if (state.shm_ptr) munmap(state.shm_ptr, SHM_BUFFER_SIZE);
    if (state.fbo) glDeleteFramebuffers(1, &state.fbo);
    if (state.fbo_texture) glDeleteTextures(1, &state.fbo_texture);
    if (state.dmabuf_fd >= 0) close(state.dmabuf_fd);
    
    loggy_real_mouse( "[PLUGIN] Hybrid plugin shutdown complete\n");
    return 0;
}


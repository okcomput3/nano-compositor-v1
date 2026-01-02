// opengl_desktop_plugin.c - FIXED STANDALONE EXECUTABLE WITH REAL SCENE BUFFERS
#define _GNU_SOURCE 
#include "../nano_compositor.h"
#include "../nano_ipc.h" // The shared IPC protocol

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>
#include <linux/input-event-codes.h>
#include "logs.h"

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

// ADD THESE INCLUDES (add to existing includes)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CIRCLE_SEGMENTS 32

#ifndef PFNGLEGLIMAGETARGETTEXTURE2DOESPROC
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
#endif



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

       // ADD THESE MOUSE CIRCLE FIELDS:
    GLuint circle_program;
    GLuint circle_vao;
    GLuint circle_vbo;
    GLuint circle_ibo;
    GLint circle_position_uniform;
    GLint circle_size_uniform;
    GLint circle_color_uniform;
    GLint circle_screen_size_uniform;
    GLint circle_time_uniform;
    
    float mouse_x, mouse_y;
    bool mouse_visible;
    float circle_time;

};

// Add this forward declaration near the top with other declarations
// Add these function declarations
static GLuint create_texture_hybrid(struct plugin_state *state, const ipc_window_info_t *info, int window_index);
static GLuint create_texture_from_dmabuf_manual(struct plugin_state *state, const ipc_window_info_t *info);
static GLuint create_texture_with_stride_fix_optimized(struct plugin_state *state, const ipc_window_info_t *info);
static void handle_window_list_hybrid(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count);
static void render_frame_hybrid(struct plugin_state *state, int frame_count);
static GLuint texture_from_dmabuf(struct plugin_state *state, const ipc_window_info_t *info);
// Add these declarations near the top of your plugin file
static void handle_window_list_with_textures(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count);
static void render_frame_opengl_with_windows(struct plugin_state *state, int frame_count);
// Add these function declarations


// ADD THESE VARIABLES (add after existing static variables)
static float circle_vertices[CIRCLE_SEGMENTS * 2 + 2];
static GLuint circle_indices[CIRCLE_SEGMENTS * 3];

// ADD THESE SHADERS (add after existing shaders)
static const char* circle_vertex_shader = 
    "#version 100\n"
    "precision mediump float;\n"
    "attribute vec2 position;\n"
    "uniform vec2 circle_position;\n"
    "uniform float circle_size;\n"
    "uniform vec2 screen_size;\n"
    "\n"
    "void main() {\n"
    "    vec2 scaled_pos = position * circle_size;\n"
    "    vec2 world_pos = circle_position + scaled_pos;\n"
    "    vec2 ndc = (world_pos / screen_size) * 2.0 - 1.0;\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";

static const char* circle_fragment_shader =
    "#version 100\n"
    "precision mediump float;\n"
    "uniform vec4 circle_color;\n"
    "uniform float time;\n"
    "\n"
    "void main() {\n"
    "    float r = 0.5 + 0.5 * sin(time * 2.0);\n"
    "    float g = 0.5 + 0.5 * sin(time * 2.0 + 2.0);\n"
    "    float b = 0.5 + 0.5 * sin(time * 2.0 + 4.0);\n"
    "    gl_FragColor = vec4(r, g, b, 0.0);\n"
    "}\n";


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
            loggy_show_mouse( "Shader error: %s\n", log);
            free(log);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}


// REPLACE your init_headless_renderer function
static bool init_headless_renderer(struct plugin_state *state) {
    int drm_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
        perror("[PLUGIN] Failed to open DRM render node");
        return false;
    }

    state->renderer = wlr_gles2_renderer_create_with_drm_fd(drm_fd);
    if (!state->renderer) {
        loggy_show_mouse( "[PLUGIN] Failed to create wlr_gles2_renderer\n");
        close(drm_fd);
        return false;
    }

    state->allocator = wlr_allocator_autocreate(NULL, state->renderer);
    if (!state->allocator) {
        loggy_show_mouse( "[PLUGIN] Failed to create wlr_allocator\n");
        wlr_renderer_destroy(state->renderer);
        return false;
    }

    loggy_show_mouse( "[PLUGIN] ✅ Successfully initialized headless wlroots context\n");
    return true;
}
static int create_dmabuf(size_t size) {
    loggy_show_mouse( "[PLUGIN] Creating DMA-BUF of size %zu bytes\n", size);
    
    // Method 1: Try DMA heap first (preferred)
    int heap_fd = open("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);
    if (heap_fd >= 0) {
        loggy_show_mouse( "[PLUGIN] Trying DMA heap method\n");
        struct dma_heap_allocation_data heap_data = {
            .len = size,
            .fd_flags = O_RDWR | O_CLOEXEC,
        };
        if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data) == 0) {
            close(heap_fd);
            loggy_show_mouse( "[PLUGIN] ✅ Created DMA-BUF via DMA heap: fd=%d\n", heap_data.fd);
            return heap_data.fd;
        } else {
            loggy_show_mouse( "[PLUGIN] DMA heap allocation failed: %s\n", strerror(errno));
        }
        close(heap_fd);
    } else {
        loggy_show_mouse( "[PLUGIN] DMA heap not available: %s\n", strerror(errno));
    }
    
    // Method 2: Try memfd as fallback
    loggy_show_mouse( "[PLUGIN] Trying memfd fallback\n");
    int fd = memfd_create("plugin-fbo", MFD_CLOEXEC);
    if (fd >= 0) {
        if (ftruncate(fd, size) == 0) {
            loggy_show_mouse( "[PLUGIN] ✅ Created DMA-BUF via memfd: fd=%d\n", fd);
            return fd;
        } else {
            loggy_show_mouse( "[PLUGIN] memfd ftruncate failed: %s\n", strerror(errno));
        }
        close(fd);
    } else {
        loggy_show_mouse( "[PLUGIN] memfd_create failed: %s\n", strerror(errno));
    }
    
    loggy_show_mouse( "[PLUGIN] ❌ All DMA-BUF creation methods failed\n");
    return -1;
}

/**
 * Initializes the plugin's FBO, backed by a DMA-BUF for zero-copy sharing.
 */


// In your plugin executable - create your own EGL context
static bool init_plugin_egl(struct plugin_state *state) {
    // Initialize EGL display
    state->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state->egl_display == EGL_NO_DISPLAY) {
        loggy_show_mouse( "[PLUGIN] Failed to get EGL display\n");
        return false;
    }

    if (!eglInitialize(state->egl_display, NULL, NULL)) {
        loggy_show_mouse( "[PLUGIN] Failed to initialize EGL\n");
        return false;
    }

    // Create EGL context for DMA-BUF import
    EGLConfig config;
    EGLint config_count;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    if (!eglChooseConfig(state->egl_display, config_attribs, &config, 1, &config_count)) {
        loggy_show_mouse( "[PLUGIN] Failed to choose EGL config\n");
        return false;
    }

    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    state->egl_context = eglCreateContext(state->egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (state->egl_context == EGL_NO_CONTEXT) {
        loggy_show_mouse( "[PLUGIN] Failed to create EGL context\n");
        return false;
    }

    // You'll need a surface - could be pbuffer for offscreen
    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    state->egl_surface = eglCreatePbufferSurface(state->egl_display, config, pbuffer_attribs);

    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        loggy_show_mouse( "[PLUGIN] Failed to make EGL context current\n");
        return false;
    }

    loggy_show_mouse( "[PLUGIN] ✅ EGL context initialized for DMA-BUF import\n");
    return true;
}

static GLuint texture_from_dmabuf(struct plugin_state *state, const ipc_window_info_t *info) {
    if (!state->dmabuf_ctx.initialized) {
        loggy_show_mouse( "[PLUGIN] Shared DMA-BUF context not available, using fallback\n");
        return 0;  // Will fallback to stride fix method
    }
    
    loggy_show_mouse( "[PLUGIN] Importing window DMA-BUF via shared library: %dx%d, fd=%d, format=0x%x, modifier=0x%lx\n",
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
    
    if (texture > 0) {
        loggy_show_mouse( "[PLUGIN] ✅ Imported window DMA-BUF via shared library: texture %u\n", texture);
    } else {
        loggy_show_mouse( "[PLUGIN] Shared library DMA-BUF import failed for window\n");
    }
    
    return texture;
}

// In plugin main(), add FD monitoring:
static void check_fd_usage(void) {
    static int check_counter = 0;
    if (++check_counter % 60 == 0) {
        // Count open FDs
        int fd_count = 0;
        for (int i = 0; i < 1000; i++) {
            if (fcntl(i, F_GETFD) != -1) {
                fd_count++;
            }
        }
        loggy_show_mouse( "[PLUGIN] Open file descriptors: %d/1024\n", fd_count);
        
        if (fd_count > 900) {
            loggy_show_mouse( "[PLUGIN] WARNING: High FD usage! Possible leak!\n");
        }
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
        loggy_show_mouse( "Framebuffer is not complete!\n");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

static bool init_opengl_resources(struct plugin_state *state) {
    loggy_show_mouse( "[WM] Initializing OpenGL resources\n");
    

   
    // Get uniform locations
    state->window_texture_uniform = glGetUniformLocation(state->window_program, "window_texture");
    state->window_transform_uniform = glGetUniformLocation(state->window_program, "transform");
    state->window_size_uniform = glGetUniformLocation(state->window_program, "window_size");
    state->alpha_uniform = glGetUniformLocation(state->window_program, "alpha");

    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    
    glBindVertexArray(0);
    
    loggy_show_mouse( "[WM] OpenGL resources initialized successfully\n");
    return true;
}




// Function to continuously check if framebuffer is complete
static bool check_framebuffer_status(struct plugin_state *state) {
    if (!state->fbo) {
        loggy_show_mouse( "[WM] Warning: No FBO to check\n");
        return false;
    }
    
    // Bind the framebuffer to check its status
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    
    // Restore the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE:
            // Everything is good
            return true;
            
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            loggy_show_mouse( "[WM] Error: Framebuffer incomplete - attachment issue\n");
            break;
            
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            loggy_show_mouse( "[WM] Error: Framebuffer incomplete - missing attachment\n");
            break;
            
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            loggy_show_mouse( "[WM] Error: Framebuffer incomplete - dimension mismatch\n");
            break;
            
        case GL_FRAMEBUFFER_UNSUPPORTED:
            loggy_show_mouse( "[WM] Error: Framebuffer unsupported format\n");
            break;
            
        default:
            loggy_show_mouse( "[WM] Error: Unknown framebuffer status: 0x%x\n", status);
            break;
    }
    
    return false;
}

// In your plugin - replace the regular FBO with DMA-BUF backed FBO
// Enhanced init_dmabuf_fbo with detailed debugging
static bool init_dmabuf_fbo(struct plugin_state *state, int width, int height) {
    loggy_show_mouse( "[PLUGIN] Attempting to create DMA-BUF backed FBO (%dx%d)\n", width, height);
    
    state->width = width;
    state->height = height;
    
    // Step 1: Create DMA-BUF
    state->dmabuf_fd = create_dmabuf(width * height * 4);
    if (state->dmabuf_fd < 0) {
        loggy_show_mouse( "[PLUGIN] Failed to create DMA-BUF\n");
        return false;
    }
    loggy_show_mouse( "[PLUGIN] Created DMA-BUF fd: %d\n", state->dmabuf_fd);
    
    // Step 2: Check for required EGL extensions
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    if (!eglCreateImageKHR) {
        loggy_show_mouse( "[PLUGIN] Missing eglCreateImageKHR extension\n");
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    if (!glEGLImageTargetTexture2DOES) {
        loggy_show_mouse( "[PLUGIN] Missing glEGLImageTargetTexture2DOES extension\n");
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    loggy_show_mouse( "[PLUGIN] EGL extensions available\n");

    // Step 3: Create EGL image from DMA-BUF
    EGLint attribs[] = {
        EGL_WIDTH, width, 
        EGL_HEIGHT, height,
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
        EGL_DMA_BUF_PLANE0_FD_EXT, state->dmabuf_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, width * 4,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE
    };
    
    loggy_show_mouse( "[PLUGIN] Creating EGL image with format ABGR8888, pitch %d\n", width * 4);
    
    state->egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
    if (state->egl_image == EGL_NO_IMAGE_KHR) {
        EGLint error = eglGetError();
        loggy_show_mouse( "[PLUGIN] Failed to create EGL image: 0x%x\n", error);
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    loggy_show_mouse( "[PLUGIN] Created EGL image successfully\n");
    
    // Step 4: Create GL texture from EGL image
    glGenTextures(1, &state->fbo_texture);
    glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image);
    
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_show_mouse( "[PLUGIN] Failed to create texture from EGL image: 0x%x\n", gl_error);
        glDeleteTextures(1, &state->fbo_texture);
        // Clean up EGL image
        PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    loggy_show_mouse( "[PLUGIN] Created GL texture %u from EGL image\n", state->fbo_texture);
    
    // Step 5: Create framebuffer
    glGenFramebuffers(1, &state->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state->fbo_texture, 0);
    
    GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
        loggy_show_mouse( "[PLUGIN] DMA-BUF framebuffer not complete: 0x%x\n", fb_status);
        glDeleteFramebuffers(1, &state->fbo);
        glDeleteTextures(1, &state->fbo_texture);
        // Clean up EGL image
        PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    loggy_show_mouse( "[PLUGIN] ✅ Successfully created DMA-BUF backed FBO (%dx%d)\n", width, height);
    return true;
}

// BETTER APPROACH: Send DMA-BUF once, then just send notifications

// Send DMA-BUF info only once at startup
static bool send_dmabuf_info_once(struct plugin_state *state) {
    static bool dmabuf_sent = false;
    
    if (dmabuf_sent || state->dmabuf_fd < 0) {
        return dmabuf_sent;
    }
    
    loggy_show_mouse( "[PLUGIN] Sending DMA-BUF info for the first time (fd: %d)\n", state->dmabuf_fd);
    
    ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT;
    ipc_dmabuf_frame_info_t frame_info = {
        .width = state->width,
        .height = state->height,
        .format = DRM_FORMAT_ABGR8888,
        .stride = state->width * 4,
        .modifier = DRM_FORMAT_MOD_LINEAR,
        .offset = 0,
    };
    
    // Send the DMA-BUF fd once
    int fd_to_send = dup(state->dmabuf_fd);
    if (fd_to_send < 0) {
        loggy_show_mouse( "[PLUGIN] Failed to dup DMA-BUF fd: %s\n", strerror(errno));
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
        loggy_show_mouse( "[PLUGIN] Failed to send DMA-BUF info: %s\n", strerror(errno));
        close(fd_to_send);
        return false;
    }
    
    dmabuf_sent = true;
    loggy_show_mouse( "[PLUGIN] ✅ DMA-BUF info sent successfully\n");
    return true;
}

// Simple frame ready notification (no FD)
static void send_frame_ready(struct plugin_state *state) {
    ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
    write(state->ipc_fd, &notification, sizeof(notification));
}


// ============================================================================
// NEW HELPER FUNCTIONS FOR DMA-BUF IPC
// ============================================================================

/**
 * Receives a message and ancillary file descriptors over a UNIX socket.
 * This is the magic that allows the compositor to send DMA-BUF handles.
 */
static ssize_t read_ipc_with_fds(int sock_fd, ipc_event_t *event, int *fds, int *fd_count) {
    struct msghdr msg = {0};
    struct iovec iov[1] = {0};
    char cmsg_buf[CMSG_SPACE(sizeof(int) * 32)]; // Space for up to 32 FDs

    iov[0].iov_base = event;
    iov[0].iov_len = sizeof(ipc_event_t);

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    ssize_t n = recvmsg(sock_fd, &msg, 0);
    if (n <= 0) {
        return n; // Error or connection closed
    }

    *fd_count = 0;
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    while (cmsg != NULL) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int num_fds_in_msg = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int *received_fds = (int *)CMSG_DATA(cmsg);
            for (int i = 0; i < num_fds_in_msg; i++) {
                if (*fd_count < 32) {
                    fds[*fd_count] = received_fds[i];
                    (*fd_count)++;
                } else {
                    // Too many FDs, close the extra ones to avoid leaks
                    close(received_fds[i]);
                }
            }
        }
        cmsg = CMSG_NXTHDR(&msg, cmsg);
    }

    return n;
}



// ============================================================================
// NEW HELPER FUNCTION FOR SHM TEXTURES
// ============================================================================

/**
 * Creates a GL texture by mapping a shared memory (SHM) file descriptor.
 */
static GLuint texture_from_shm(struct plugin_state *state, const ipc_window_info_t *info) {
    if (info->fd < 0) {
        loggy_show_mouse( "[WM] Invalid SHM file descriptor.\n");
        return 0;
    }

    // 1. Map the shared memory segment from the file descriptor
    void *shm_data = mmap(NULL, info->stride * info->height, PROT_READ, MAP_PRIVATE, info->fd, 0);
    if (shm_data == MAP_FAILED) {
        perror("[WM] Failed to mmap SHM buffer");
        return 0;
    }

    // 2. Create an OpenGL texture and upload the pixel data
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload the pixels from the mapped memory
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, shm_data);

    // 3. Unmap the memory as we are done with it
    munmap(shm_data, info->stride * info->height);

    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_show_mouse( "[WM] OpenGL error creating texture from SHM: 0x%x\n", gl_error);
        glDeleteTextures(1, &texture_id);
        return 0;
    }

    loggy_show_mouse( "[WM] Successfully created texture %u from SHM (fd:%d, %dx%d)\n", 
            texture_id, info->fd, (int)info->width, (int)info->height);

    return texture_id;
}




// Hybrid texture import: Try fast methods first, fallback to reliable methods
static GLuint create_texture_hybrid(struct plugin_state *state, const ipc_window_info_t *info, int window_index) {
    loggy_show_mouse( "[PLUGIN] Creating texture for window %d: %dx%d, type=%d\n", 
            window_index, info->width, info->height, info->buffer_type);
    
    if (info->fd < 0) {
        loggy_show_mouse( "[PLUGIN] Invalid file descriptor\n");
        return 0;
    }
    
    GLuint texture = 0;
    
    // Method 1: Try shared DMA-BUF library (fastest available method)
    if (info->buffer_type == IPC_BUFFER_TYPE_DMABUF && state->dmabuf_ctx.initialized) {
        loggy_show_mouse( "[PLUGIN] Attempting shared DMA-BUF library import...\n");
        texture = texture_from_dmabuf(state, info);
        if (texture > 0) {
            loggy_show_mouse( "[PLUGIN] ✅ Fast shared DMA-BUF import successful: texture %u\n", texture);
            return texture;
        } else {
            loggy_show_mouse( "[PLUGIN] Shared DMA-BUF import failed, trying next method...\n");
        }
    }
    
    // Method 2: Try manual EGL DMA-BUF import (medium speed)
    if (info->buffer_type == IPC_BUFFER_TYPE_DMABUF) {
        loggy_show_mouse( "[PLUGIN] Attempting manual EGL DMA-BUF import...\n");
        texture = create_texture_from_dmabuf_manual(state, info);
        if (texture > 0) {
            loggy_show_mouse( "[PLUGIN] ✅ Manual EGL DMA-BUF import successful: texture %u\n", texture);
            return texture;
        } else {
            loggy_show_mouse( "[PLUGIN] Manual EGL DMA-BUF import failed, trying fallback...\n");
        }
    }
    
    // Method 3: Optimized stride correction (medium-slow but reliable)
    loggy_show_mouse( "[PLUGIN] Using optimized stride correction...\n");
    texture = create_texture_with_stride_fix_optimized(state, info);
    if (texture > 0) {
        loggy_show_mouse( "[PLUGIN] ✅ Optimized stride correction successful: texture %u\n", texture);
        return texture;
    }
    
    // Method 4: SHM fallback (slowest but always works)
    if (info->buffer_type == IPC_BUFFER_TYPE_SHM) {
        loggy_show_mouse( "[PLUGIN] Using SHM fallback...\n");
        texture = texture_from_shm(state, info);
        if (texture > 0) {
            loggy_show_mouse( "[PLUGIN] ✅ SHM fallback successful: texture %u\n", texture);
            return texture;
        }
    }
    
    loggy_show_mouse( "[PLUGIN] ❌ All texture creation methods failed\n");
    return 0;
}

// Manual EGL DMA-BUF import (bypass wlroots complexity)
static GLuint create_texture_from_dmabuf_manual(struct plugin_state *state, const ipc_window_info_t *info) {
    // Get EGL extension functions
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
        loggy_show_mouse( "[PLUGIN] EGL DMA-BUF extensions not available\n");
        return 0;
    }
    
    // Create EGL image from DMA-BUF
    EGLint attribs[] = {
        EGL_WIDTH, (int)info->width,
        EGL_HEIGHT, (int)info->height,
        EGL_LINUX_DRM_FOURCC_EXT, (int)info->format,
        EGL_DMA_BUF_PLANE0_FD_EXT, info->fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, (int)info->stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (int)(info->modifier & 0xFFFFFFFF),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (int)(info->modifier >> 32),
        EGL_NONE
    };
    
    EGLImage egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, 
                                          EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
    if (egl_image == EGL_NO_IMAGE_KHR) {
        loggy_show_mouse( "[PLUGIN] Failed to create EGL image from DMA-BUF\n");
        return 0;
    }
    
    // Create GL texture from EGL image
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        loggy_show_mouse( "[PLUGIN] OpenGL error creating texture from EGL image: 0x%x\n", error);
        glDeleteTextures(1, &texture);
        
        PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
            (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if (eglDestroyImageKHR) {
            eglDestroyImageKHR(state->egl_display, egl_image);
        }
        return 0;
    }
    
    // Store EGL image for cleanup (optional - could store in window state)
    // For now, we'll clean it up immediately since GL texture holds reference
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if (eglDestroyImageKHR) {
        eglDestroyImageKHR(state->egl_display, egl_image);
    }
    
    return texture;
}

// Optimized stride correction (less debugging, faster processing)
static GLuint create_texture_with_stride_fix_optimized(struct plugin_state *state, const ipc_window_info_t *info) {
    uint32_t expected_stride = info->width * 4;
    
    // Only log if there's actually a stride mismatch
    if (info->stride != expected_stride) {
        loggy_show_mouse( "[PLUGIN] Stride mismatch: %u vs %u, correcting...\n", info->stride, expected_stride);
    }
    
    size_t map_size = info->stride * info->height;
    void *mapped_data = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, info->fd, 0);
    if (mapped_data == MAP_FAILED) {
        return 0;
    }
    
    // Fast path: if stride matches, use direct copy
    if (info->stride == expected_stride) {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, mapped_data);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        munmap(mapped_data, map_size);
        return texture;
    }
    
    // Slow path: stride correction needed
    size_t corrected_size = expected_stride * info->height;
    uint8_t *corrected_data = malloc(corrected_size);
    if (!corrected_data) {
        munmap(mapped_data, map_size);
        return 0;
    }
    
    uint8_t *src = (uint8_t *)mapped_data;
    uint8_t *dst = corrected_data;
    
    for (uint32_t y = 0; y < info->height; y++) {
        memcpy(dst, src, expected_stride);
        src += info->stride;
        dst += expected_stride;
    }
    
    munmap(mapped_data, map_size);
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, corrected_data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    free(corrected_data);
    return texture;
}



static void render_frame_hybrid(struct plugin_state *state, int frame_count) {
    // Update circle time
    state->circle_time = frame_count * 0.1f;
    
    // Debug first few frames
    if (frame_count <= 5) {
        loggy_show_mouse( "[PLUGIN] render_frame_hybrid called for frame %d\n", frame_count);
    }
    
    // Bind FBO
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glViewport(0, 0, state->width, state->height);
    
    // Static background (let window content be the animation)
    glClearColor(0.0f, 0.00f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    /*
    // Render windows with their live animated content
    int rendered_windows = 0;
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid && state->windows[i].gl_texture != 0) {
            render_window(state, i);
            rendered_windows++;
        }
    }*/
    
    // ADD MOUSE CIRCLE RENDERING HERE:
    if (state->mouse_visible) {
        glUseProgram(state->circle_program);
        
        glUniform2f(state->circle_screen_size_uniform, (float)state->width, (float)state->height);
        glUniform1f(state->circle_time_uniform, state->circle_time);
        glUniform2f(state->circle_position_uniform, state->mouse_x, state->mouse_y);
        glUniform1f(state->circle_size_uniform, 20.0f);
        
        float color[] = {1.0f, 1.0f, 1.0f, 0.8f};
        glUniform4fv(state->circle_color_uniform, 1, color);
        
        glBindVertexArray(state->circle_vao);
        glDrawElements(GL_TRIANGLES, CIRCLE_SEGMENTS * 3, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    /*
    // Performance logging
    if (frame_count % 300 == 0) {  // Every 5 seconds
        loggy_show_mouse( "[PLUGIN] Frame %d: %d/%d windows with live content!\n", 
                frame_count, rendered_windows, state->window_count);
    }*/
    
    glDisable(GL_BLEND);
    glFinish();
    
    // Read to SHM
    glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, state->shm_ptr);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR && frame_count <= 5) {
        loggy_show_mouse( "[PLUGIN] OpenGL error in frame %d: 0x%x\n", frame_count, error);
    }
    
    glFinish();
    msync(state->shm_ptr, SHM_BUFFER_SIZE, MS_SYNC);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    if (frame_count <= 5) {
        loggy_show_mouse( "[PLUGIN] Frame %d rendering complete with live content\n", frame_count);
    }
}

// ADD THIS FUNCTION (add after existing functions)
static void generate_circle_geometry() {
    circle_vertices[0] = 0.0f;
    circle_vertices[1] = 0.0f;
    
    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        float angle = 2.0f * M_PI * i / CIRCLE_SEGMENTS;
        circle_vertices[2 + i * 2] = cosf(angle);
        circle_vertices[2 + i * 2 + 1] = sinf(angle);
    }
    
    for (int i = 0; i < CIRCLE_SEGMENTS; i++) {
        circle_indices[i * 3] = 0;
        circle_indices[i * 3 + 1] = i + 1;
        circle_indices[i * 3 + 2] = (i + 1) % CIRCLE_SEGMENTS + 1;
    }
}

// ADD THIS FUNCTION (add after existing functions)
static bool init_mouse_circle_resources(struct plugin_state *state) {
    generate_circle_geometry();
    
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, circle_vertex_shader);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, circle_fragment_shader);
    
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return false;
    }
    
    state->circle_program = glCreateProgram();
    glAttachShader(state->circle_program, vertex);
    glAttachShader(state->circle_program, fragment);
    glBindAttribLocation(state->circle_program, 0, "position");
    glLinkProgram(state->circle_program);
    
    GLint linked;
    glGetProgramiv(state->circle_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(state->circle_program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    // Get uniform locations
    state->circle_position_uniform = glGetUniformLocation(state->circle_program, "circle_position");
    state->circle_size_uniform = glGetUniformLocation(state->circle_program, "circle_size");
    state->circle_color_uniform = glGetUniformLocation(state->circle_program, "circle_color");
    state->circle_screen_size_uniform = glGetUniformLocation(state->circle_program, "screen_size");
    state->circle_time_uniform = glGetUniformLocation(state->circle_program, "time");
    
    // Create VAO and VBO
    glGenVertexArrays(1, &state->circle_vao);
    glBindVertexArray(state->circle_vao);
    
    glGenBuffers(1, &state->circle_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, state->circle_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(circle_vertices), circle_vertices, GL_STATIC_DRAW);
    
    glGenBuffers(1, &state->circle_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->circle_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(circle_indices), circle_indices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);
    
    glBindVertexArray(0);
    
    return true;
}



// ADD MOUSE HANDLING FUNCTIONS (add after existing functions)
static void handle_mouse_motion(struct plugin_state *state, float x, float y) {
    state->mouse_x = x;
    state->mouse_y = y;
    state->mouse_visible = true;
}

// Enhanced mouse click handling function - replace your existing handle_mouse_button function
// Simple mouse click handling - replace your existing handle_mouse_button function
static void handle_mouse_button(struct plugin_state *state, int button, int pressed) {
    if (button == BTN_LEFT && pressed) {
        loggy_show_mouse( "[PLUGIN] Left click at (%.1f, %.1f)\n", state->mouse_x, state->mouse_y);
        
        // Check if we clicked on any window and forward the click
        for (int i = 0; i < state->window_count; i++) {
            if (state->windows[i].is_valid) {
                float win_x = state->windows[i].x;
                float win_y = state->windows[i].y;
                float win_w = state->windows[i].width;
                float win_h = state->windows[i].height;
                
                // Check if click is inside this window
                if (state->mouse_x >= win_x && state->mouse_x <= win_x + win_w &&
                    state->mouse_y >= win_y && state->mouse_y <= win_y + win_h) {
                    
                    // Calculate relative position within the window
                    float rel_x = state->mouse_x - win_x;
                    float rel_y = state->mouse_y - win_y;
                    
                    loggy_show_mouse( "[PLUGIN] Clicked window %d at relative pos (%.1f, %.1f)\n", 
                            i, rel_x, rel_y);
                    
                    // Send click event to compositor via IPC
                    ipc_event_t click_event = {0};
                    click_event.type = IPC_EVENT_TYPE_WINDOW_CLICK;
                    click_event.data.window_click.window_index = i;
                    click_event.data.window_click.x = rel_x;
                    click_event.data.window_click.y = rel_y;
                    click_event.data.window_click.button = button;
                    click_event.data.window_click.state = pressed;
                    
                    write(state->ipc_fd, &click_event, sizeof(click_event));
                    break;
                }
            }
        }
    }
    
    if (button == BTN_RIGHT && pressed) {
        loggy_show_mouse( "[PLUGIN] Right click at (%.1f, %.1f)\n", state->mouse_x, state->mouse_y);
    }
    
    if (button == BTN_MIDDLE && pressed) {
        loggy_show_mouse( "[PLUGIN] Middle click at (%.1f, %.1f)\n", state->mouse_x, state->mouse_y);
    }
}

// HYBRID APPROACH: Simple initialization + Fast DMA-BUF import when possible
// Modified main() function to use DMA-BUF output instead of SHM
int main(int argc, char *argv[]) {
    loggy_show_mouse( "[PLUGIN] === HYBRID FAST VERSION WITH DMA-BUF OUTPUT ===\n");
    
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
        loggy_show_mouse( "[PLUGIN] ERROR: Invalid arguments\n");
        return 1;
    }

    loggy_show_mouse( "[PLUGIN] Starting hybrid plugin with DMA-BUF output\n");

    // Initialize state
    struct plugin_state state = {0};
    state.ipc_fd = ipc_fd;
    state.focused_window_index = -1;
    state.width = SHM_WIDTH;
    state.height = SHM_HEIGHT;

    // Set up SHM buffer as fallback
    int shm_fd = shm_open(shm_name, O_RDWR, 0600);
    if (shm_fd < 0) { 
        loggy_show_mouse( "[PLUGIN] ERROR: shm_open failed: %s\n", strerror(errno));
        return 1; 
    }
    
    state.shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (state.shm_ptr == MAP_FAILED) { 
        loggy_show_mouse( "[PLUGIN] ERROR: mmap failed: %s\n", strerror(errno));
        close(shm_fd); 
        return 1; 
    }
    close(shm_fd);
    loggy_show_mouse( "[PLUGIN] ✅ SHM buffer ready (fallback)\n");

    // Initialize EGL context
    loggy_show_mouse( "[PLUGIN] Initializing EGL context...\n");
    if (!init_plugin_egl(&state)) {
        loggy_show_mouse( "[PLUGIN] ERROR: EGL initialization failed\n");
        return 1;
    }
    loggy_show_mouse( "[PLUGIN] ✅ EGL context ready\n");

    // Initialize OpenGL resources
    loggy_show_mouse( "[PLUGIN] Initializing OpenGL resources...\n");
    if (!init_opengl_resources(&state)) {
        loggy_show_mouse( "[PLUGIN] ERROR: OpenGL initialization failed\n");
        return 1;
    }
    loggy_show_mouse( "[PLUGIN] ✅ OpenGL resources ready\n");

    loggy_show_mouse( "[PLUGIN] Initializing mouse circle...\n");
    if (!init_mouse_circle_resources(&state)) {
        loggy_show_mouse( "[PLUGIN] ERROR: Mouse circle initialization failed\n");
        return 1;
    }
    loggy_show_mouse( "[PLUGIN] ✅ Mouse circle ready\n");
    
    // Initialize mouse position (ADD THIS)
    state.mouse_x = SHM_WIDTH / 2.0f;
    state.mouse_y = SHM_HEIGHT / 2.0f;
    state.mouse_visible = true;


    // Try to initialize DMA-BUF backed FBO (the key change!)
    loggy_show_mouse( "[PLUGIN] Attempting DMA-BUF backed FBO...\n");
    bool dmabuf_fbo_available = init_dmabuf_fbo(&state, SHM_WIDTH, SHM_HEIGHT);
    
    if (!dmabuf_fbo_available) {
        loggy_show_mouse( "[PLUGIN] DMA-BUF FBO failed, falling back to regular FBO...\n");
        if (!init_fbo(&state, SHM_WIDTH, SHM_HEIGHT)) {
            loggy_show_mouse( "[PLUGIN] ERROR: Fallback FBO initialization failed\n");
            return 1;
        }
    }

    // Try to initialize shared DMA-BUF context for window import
    loggy_show_mouse( "[PLUGIN] Attempting shared DMA-BUF context...\n");
    bool dmabuf_lib_available = dmabuf_share_init_with_display(&state.dmabuf_ctx, state.egl_display);
    if (dmabuf_lib_available) {
        loggy_show_mouse( "[PLUGIN] ✅ Shared DMA-BUF library available\n");
    } else {
        loggy_show_mouse( "[PLUGIN] ⚠️  Shared DMA-BUF library not available\n");
    }

    loggy_show_mouse( "[PLUGIN] 🚀 Hybrid plugin ready!\n");
    loggy_show_mouse( "[PLUGIN] Capabilities:\n");
    loggy_show_mouse( "[PLUGIN]   - DMA-BUF output FBO: %s\n", dmabuf_fbo_available ? "YES" : "NO");
    loggy_show_mouse( "[PLUGIN]   - Shared DMA-BUF library: %s\n", dmabuf_lib_available ? "YES" : "NO");
    loggy_show_mouse( "[PLUGIN]   - EGL DMA-BUF import: YES\n");
    loggy_show_mouse( "[PLUGIN]   - SHM fallback: YES\n");

    // Test IPC connection
    ipc_notification_type_t startup_notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
    write(state.ipc_fd, &startup_notification, sizeof(startup_notification));
    loggy_show_mouse( "[PLUGIN] ✅ IPC connection verified\n");

    // Send DMA-BUF info once if available
    if (dmabuf_fbo_available) {
        if (send_dmabuf_info_once(&state)) {
            loggy_show_mouse( "[PLUGIN] ✅ DMA-BUF output enabled - zero-copy to compositor!\n");
        } else {
            loggy_show_mouse( "[PLUGIN] ⚠️  DMA-BUF output failed, will use SHM fallback\n");
            dmabuf_fbo_available = false;
        }
    }

    loggy_show_mouse( "[PLUGIN] Entering main event loop...\n");

    // Main loop with DMA-BUF or SHM output
    int frame_count = 0;
    while (true) {
        ipc_event_t event;
        int fds[32];
        int fd_count = 0;
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(state.ipc_fd, &read_fds);
        
        struct timeval timeout = {0, 16666}; // 60fps
        int select_result = select(state.ipc_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result < 0) {
            loggy_show_mouse( "[PLUGIN] select() error: %s\n", strerror(errno));
            break;
        } else if (select_result == 0) {
            // Timeout - render frame
            frame_count++;
            
            if (frame_count <= 10 || frame_count % 120 == 0) {
                loggy_show_mouse( "[PLUGIN] Rendering frame %d (%s output)...\n", 
                        frame_count, dmabuf_fbo_available ? "DMA-BUF" : "SHM");
            }
            
            // Render to FBO (either DMA-BUF backed or regular)
            render_frame_hybrid(&state, frame_count);
            
            // Send frame using appropriate method
            if (dmabuf_fbo_available) {
                // Send DMA-BUF frame notification (zero-copy!)
                send_frame_ready(&state);
            } else {
                // Fallback: copy to SHM
                glBindFramebuffer(GL_FRAMEBUFFER, state.fbo);
                glReadPixels(0, 0, state.width, state.height, GL_RGBA, GL_UNSIGNED_BYTE, state.shm_ptr);
                glFinish();
                msync(state.shm_ptr, SHM_BUFFER_SIZE, MS_SYNC);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                
                ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
                write(state.ipc_fd, &notification, sizeof(notification));
            }
            
            continue;
        }
        
        // Handle IPC events (same as before)
        ssize_t n = read_ipc_with_fds(state.ipc_fd, &event, fds, &fd_count);
        if (n <= 0) {
            loggy_show_mouse( "[PLUGIN] IPC connection closed (n=%zd): %s\n", n, strerror(errno));
            break;
        }
        
if (event.type == IPC_EVENT_TYPE_WINDOW_LIST_UPDATE) {
   
} else if (event.type == IPC_EVENT_TYPE_POINTER_MOTION) {
    handle_mouse_motion(&state, event.data.motion.x, event.data.motion.y);
} else if (event.type == IPC_EVENT_TYPE_POINTER_BUTTON) {
    handle_mouse_button(&state, event.data.button.button, event.data.button.state);
}
    }

    // Cleanup
    loggy_show_mouse( "[PLUGIN] Cleaning up hybrid plugin...\n");
    
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

    // ADD TO CLEANUP (add to existing cleanup section):
    if (state.circle_program) glDeleteProgram(state.circle_program);
    if (state.circle_vao) glDeleteVertexArrays(1, &state.circle_vao);
    if (state.circle_vbo) glDeleteBuffers(1, &state.circle_vbo);
    if (state.circle_ibo) glDeleteBuffers(1, &state.circle_ibo);
    
    loggy_show_mouse( "[PLUGIN] Hybrid plugin shutdown complete\n");
    return 0;
}

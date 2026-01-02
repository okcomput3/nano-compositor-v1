#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <drm/drm_fourcc.h>

// This is the magic header that lets C run Python
#include <Python.h>
#include <sys/prctl.h>


// This is the FBO and DMA-BUF creation code from your original,
// working C executable. This code is PROVEN to work.
// (You will need to paste your helper functions like `create_dmabuf`, etc. here)
// For simplicity, here is a minimal version:
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

// opengl_desktop_plugin.c - FIXED STANDALONE EXECUTABLE WITH REAL SCENE BUFFERS
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
#include <sys/un.h>      
#include <wlr/render/gles2.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_drm.h> 
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
#include <signal.h>        
#include <sys/wait.h>      
#include <sys/select.h>    
#include <sys/socket.h>    
#include <sys/un.h>        
#include <errno.h>        
#include <drm_fourcc.h>
#include <sys/ioctl.h>

// Function to query actual DMA-BUF properties

static pthread_mutex_t dmabuf_mutex = PTHREAD_MUTEX_INITIALIZER;


#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0x0000000000000000ULL
#endif

#ifndef DRM_FORMAT_ABGR8888
#define DRM_FORMAT_ABGR8888 0x34324142
#endif

#ifndef PFNGLEGLIMAGETARGETTEXTURE2DOESPROC
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
#endif

// The plugin's internal state
// The plugin's internal state
struct plugin_state {
    int drm_fd; 
    int ipc_fd;
    void* shm_ptr;
    int dmabuf_fd;
     // ✅ New metadata fields for DMA-BUF
    int dmabuf_stride;
    int dmabuf_format;
    unsigned long long dmabuf_modifier;
    int dmabuf_offset;

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

      int instance_id;  // Unique per plugin
    char instance_name[64];  // For debugging



        // --- ASYNC Double Buffering Fields ---
    int dmabuf_fds[2];
    uint32_t dmabuf_strides[2];
    uint32_t dmabuf_formats[2];
    uint64_t dmabuf_modifiers[2];
    uint32_t dmabuf_offsets[2];

 

    // We will create two textures and two FBOs
    EGLImage egl_images[2];
    GLuint fbo_textures[2];
    GLuint fbos[2];

    PyObject *py_module;
    PyObject *py_main_func;

};

// Add this forward declaration near the top with other declarations
// Add these function declarations
static GLuint create_texture_hybrid(struct plugin_state *state, const ipc_window_info_t *info, int window_index);
static GLuint create_texture_from_dmabuf_manual(struct plugin_state *state, const ipc_window_info_t *info);
static GLuint create_texture_with_stride_fix_optimized(struct plugin_state *state, const ipc_window_info_t *info);
static void handle_window_list_hybrid(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count);
static void render_frame_hybrid(struct plugin_state *state, int frame_count);
// Add these declarations near the top of your plugin file
static void render_window(struct plugin_state *state, int index) ;
// Add these function declarations

static int create_dmabuf(size_t size, int instance_id) {
    int result = -1;
    int lock_fd = -1;
    pid_t pid = getpid();
    
    // FIXED: Instance-specific lock file
    char lock_file[256];
    snprintf(lock_file, sizeof(lock_file), "/tmp/dmabuf_create_%d.lock", instance_id);
    
    lock_fd = open(lock_file, O_CREAT|O_RDWR, 0666);
    if (lock_fd < 0) {
        loggy_opengl_desktop( "[PLUGIN-%d] Failed to create lock file: %s\n", instance_id, strerror(errno));
        return -1;
    }
    
    // Lock for multi-process safety
    if (flock(lock_fd, LOCK_EX) < 0) {
        loggy_opengl_desktop( "[PLUGIN] PID=%d Failed to acquire lock: %s\n", pid, strerror(errno));
        close(lock_fd);
        return -1;
    }
    
   loggy_opengl_desktop( "[PLUGIN] PID=%d Creating DMA-BUF of size %zu bytes\n", 
        pid, size);
    
    // Method 1: Try DMA heap allocation
    const char* heap_path = "/dev/dma_heap/system";
    loggy_opengl_desktop( "[PLUGIN] PID=%d Attempting to open: %s\n", pid, heap_path);
    
    int heap_fd = open(heap_path, O_RDONLY | O_CLOEXEC);
    if (heap_fd < 0) {
        loggy_opengl_desktop( "[PLUGIN] PID=%d Failed to open %s: %s (errno=%d)\n", 
                pid, heap_path, strerror(errno), errno);
        goto try_fallback;
    }
    
    loggy_opengl_desktop( "[PLUGIN] PID=%d ✅ Successfully opened %s (fd=%d)\n", 
            pid, heap_path, heap_fd);
    
    struct dma_heap_allocation_data heap_data = {
        .len = size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };
    
    int ioctl_result = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
    if (ioctl_result == 0) {
        loggy_opengl_desktop( "[PLUGIN] PID=%d ✅ DMA heap allocation SUCCESS: fd=%d\n", 
                pid, heap_data.fd);
        
        // Test the returned fd
        struct stat stat_buf;
        if (fstat(heap_data.fd, &stat_buf) == 0) {
            loggy_opengl_desktop( "[PLUGIN] PID=%d DMA-BUF fd stats: size=%ld, mode=0%o\n", 
                    pid, stat_buf.st_size, stat_buf.st_mode);
        }
        
        result = heap_data.fd;
        goto cleanup_heap_fd;
    }
    
    loggy_opengl_desktop( "[PLUGIN] PID=%d ioctl DMA_HEAP_IOCTL_ALLOC FAILED: %s (errno=%d)\n", 
            pid, strerror(errno), errno);
    
    // Method 2: Try smaller allocation if the original was large
    if (size > 1024 * 1024) {  // If > 1MB
        loggy_opengl_desktop( "[PLUGIN] PID=%d Large allocation failed, trying smaller test allocation...\n", pid);
        
        struct dma_heap_allocation_data test_data = {
            .len = 4096,  // Just 4KB test
            .fd_flags = O_RDWR | O_CLOEXEC,
        };
        
        if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &test_data) == 0) {
            loggy_opengl_desktop( "[PLUGIN] PID=%d ✅ Small test allocation worked! The issue might be size limits.\n", pid);
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
                loggy_opengl_desktop( "[PLUGIN] PID=%d ✅ Medium allocation worked: fd=%d\n", 
                        pid, medium_data.fd);
                result = medium_data.fd;
                goto cleanup_heap_fd;
            } else {
                loggy_opengl_desktop( "[PLUGIN] PID=%d Medium allocation failed: %s\n", 
                        pid, strerror(errno));
            }
        } else {
            loggy_opengl_desktop( "[PLUGIN] PID=%d Even small test allocation failed: %s\n", 
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
    loggy_opengl_desktop( "[PLUGIN] PID=%d DMA heap failed, falling back to memfd (won't work for zero-copy)\n", pid);
    
    int memfd = memfd_create("plugin-fbo-fallback", MFD_CLOEXEC);
    if (memfd < 0) {
        loggy_opengl_desktop( "[PLUGIN] PID=%d memfd_create failed: %s\n", pid, strerror(errno));
        goto error;
    }
    
    if (ftruncate(memfd, size) != 0) {
        loggy_opengl_desktop( "[PLUGIN] PID=%d memfd ftruncate failed: %s\n", pid, strerror(errno));
        close(memfd);
        goto error;
    }
    
    loggy_opengl_desktop( "[PLUGIN] PID=%d ⚠️  Created memfd fallback: fd=%d (THIS WON'T WORK FOR ZERO-COPY)\n", 
            pid, memfd);
    result = memfd;
    goto success;

error:
    loggy_opengl_desktop( "[PLUGIN] PID=%d ❌ All allocation methods failed\n", pid);
    result = -1;

success:
    // Unlock and cleanup
    if (lock_fd >= 0) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
    return result;
}


/**
 * Initializes the plugin's FBO, backed by a DMA-BUF for zero-copy sharing.
 */
//////////////////////////////////////////////////////////////////////////////breadcrumb
/* nvidia only
// In your plugin executable - create your own EGL context
static bool init_plugin_egl(struct plugin_state *state) {
    loggy_opengl_desktop( "[PLUGIN-%d] Initializing isolated EGL context...\n", state->instance_id);
    
    // Initialize EGL display (shared but that's OK)
    state->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state->egl_display == EGL_NO_DISPLAY) {
        loggy_opengl_desktop( "[PLUGIN-%d] Failed to get EGL display\n", state->instance_id);
        return false;
    }

    if (!eglInitialize(state->egl_display, NULL, NULL)) {
        loggy_opengl_desktop( "[PLUGIN-%d] Failed to initialize EGL\n", state->instance_id);
        return false;
    }

    // Log EGL version for debugging
    const char* egl_version = eglQueryString(state->egl_display, EGL_VERSION);
    loggy_opengl_desktop( "[PLUGIN-%d] EGL Version: %s\n", state->instance_id, egl_version);

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
        loggy_opengl_desktop( "[PLUGIN-%d] Failed to choose EGL config\n", state->instance_id);
        return false;
    }

    if (config_count == 0) {
        loggy_opengl_desktop( "[PLUGIN-%d] No suitable EGL config found\n", state->instance_id);
        return false;
    }

    loggy_opengl_desktop( "[PLUGIN-%d] Found %d EGL config(s)\n", state->instance_id, config_count);

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
        loggy_opengl_desktop( "[PLUGIN-%d] Failed to create EGL context: 0x%x\n", state->instance_id, error);
        
        // Fallback: try simpler context creation
        loggy_opengl_desktop( "[PLUGIN-%d] Trying fallback context creation...\n", state->instance_id);
        EGLint simple_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        state->egl_context = eglCreateContext(state->egl_display, config, EGL_NO_CONTEXT, simple_attribs);
        if (state->egl_context == EGL_NO_CONTEXT) {
            loggy_opengl_desktop( "[PLUGIN-%d] Fallback context creation also failed\n", state->instance_id);
            return false;
        }
    }

    loggy_opengl_desktop( "[PLUGIN-%d] ✅ EGL context created: %p\n", state->instance_id, state->egl_context);

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
        loggy_opengl_desktop( "[PLUGIN-%d] Failed to create PBuffer surface: 0x%x\n", state->instance_id, error);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }

    loggy_opengl_desktop( "[PLUGIN-%d] ✅ PBuffer surface created: %p\n", state->instance_id, state->egl_surface);

    // Make context current with proper error checking
    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        EGLint error = eglGetError();
        loggy_opengl_desktop( "[PLUGIN-%d] Failed to make EGL context current: 0x%x\n", state->instance_id, error);
        eglDestroySurface(state->egl_display, state->egl_surface);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }

    // Verify OpenGL context is working
    const char* gl_vendor = (const char*)glGetString(GL_VENDOR);
    const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
    const char* gl_version = (const char*)glGetString(GL_VERSION);
    
    loggy_opengl_desktop( "[PLUGIN-%d] ✅ OpenGL Context Active:\n", state->instance_id);
    loggy_opengl_desktop( "[PLUGIN-%d]   Vendor: %s\n", state->instance_id, gl_vendor ? gl_vendor : "Unknown");
    loggy_opengl_desktop( "[PLUGIN-%d]   Renderer: %s\n", state->instance_id, gl_renderer ? gl_renderer : "Unknown");
    loggy_opengl_desktop( "[PLUGIN-%d]   Version: %s\n", state->instance_id, gl_version ? gl_version : "Unknown");

    // Test basic OpenGL operation to ensure context works
    GLuint test_texture;
    glGenTextures(1, &test_texture);
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_opengl_desktop( "[PLUGIN-%d] OpenGL context test failed: 0x%x\n", state->instance_id, gl_error);
        eglMakeCurrent(state->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(state->egl_display, state->egl_surface);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }
    glDeleteTextures(1, &test_texture);

    loggy_opengl_desktop( "[PLUGIN-%d] ✅ EGL context fully initialized and tested\n", state->instance_id);
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


// Render window (matching original coordinate system)
// Fixed render_window function - correct coordinate system
// Fixed render_window function - correct Y coordinate conversion
static void render_window(struct plugin_state *state, int index) {
    if (!validate_window_texture(state, index)) {
        return;
    }
    
    // Save current state
    GLint current_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    
    glUseProgram(state->window_program);
    
    // Bind and validate texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->windows[index].gl_texture);
    
    // Check for binding errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        loggy_opengl_desktop( "[PLUGIN] Error binding texture %u: 0x%x\n", 
                state->windows[index].gl_texture, error);
        glUseProgram(current_program);
        return;
    }
    
    glUniform1i(state->window_texture_uniform, 0);
    
    // Calculate transform matrix (your existing logic)
    float left = state->windows[index].x;
    float top = state->windows[index].y; 
    float width = state->windows[index].width;
    float height = state->windows[index].height;
    
    float ndc_left = (left / state->width) * 2.0f - 1.0f;
    float ndc_right = ((left + width) / state->width) * 2.0f - 1.0f;
    float ndc_top = ((top) / state->height) * 2.0f - 1.0f;
    float ndc_bottom = ((top + height) / state->height) * 2.0f - 1.0f;
    
    float ndc_width = ndc_right - ndc_left;
    float ndc_height = ndc_top - ndc_bottom;
    
    float transform[9];
    transform[0] = ndc_width;  transform[3] = 0.0f;        transform[6] = ndc_left;
    transform[1] = 0.0f;       transform[4] = ndc_height;  transform[7] = ndc_bottom;
    transform[2] = 0.0f;       transform[5] = 0.0f;        transform[8] = 1.0f;
    
    glUniformMatrix3fv(state->window_transform_uniform, 1, GL_FALSE, transform);
    glUniform2f(state->window_size_uniform, 1.0f, 1.0f);
    glUniform1f(state->alpha_uniform, state->windows[index].alpha);
    
    // Render
    glBindVertexArray(state->quad_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore state
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(current_program);
}




// In your plugin - replace the regular FBO with DMA-BUF backed FBO
// Enhanced init_dmabuf_fbo with detailed debugging
static bool init_dmabuf_fbo(struct plugin_state *state, int width, int height) {
    loggy_opengl_desktop("[PLUGIN] 🚀 Starting DMA-BUF FBO creation with debug info\n");
    
    state->width = width;
    state->height = height;
    
    // Test different stride values
    uint32_t stride_candidates[] = {
        width * 4,                            // Standard: 7680
        ((width * 4 + 63) / 64) * 64,        // 64-byte aligned: 7680 (already aligned)
        ((width * 4 + 255) / 256) * 256,     // 256-byte aligned: 7936
        ((width * 4 + 31) / 32) * 32,        // 32-byte aligned: 7680
        ((width * 4 + 127) / 128) * 128,     // 128-byte aligned: 7808
    };
    
    uint32_t format_candidates[] = {
        DRM_FORMAT_ARGB8888,  // 0x34325241 - what you're currently using
        DRM_FORMAT_XRGB8888,  // 0x34325258 - no alpha channel
        DRM_FORMAT_ABGR8888,  // 0x34324142 - swapped byte order
    };
    
    int num_strides = sizeof(stride_candidates) / sizeof(stride_candidates[0]);
    int num_formats = sizeof(format_candidates) / sizeof(format_candidates[0]);
    
    const char* format_names[] = {"ARGB8888", "XRGB8888", "ABGR8888"};
    
    for (int fmt_idx = 0; fmt_idx < num_formats; fmt_idx++) {
        for (int stride_idx = 0; stride_idx < num_strides; stride_idx++) {
            uint32_t test_format = format_candidates[fmt_idx];
            uint32_t test_stride = stride_candidates[stride_idx];
            
            loggy_opengl_desktop("[PLUGIN] 🧪 Testing: %s format, stride %u\n", 
                    format_names[fmt_idx], test_stride);
            
            // Create buffer with exact size needed
            size_t buffer_size = test_stride * height;
            state->dmabuf_fd = create_dmabuf(buffer_size, state->instance_id);
            if (state->dmabuf_fd < 0) {
                loggy_opengl_desktop("[PLUGIN] ❌ Failed to create DMA-BUF\n");
                continue;
            }
            
            // Debug the created buffer
            if (!debug_dmabuf_properties(state->dmabuf_fd, test_stride, width, height)) {
                loggy_opengl_desktop("[PLUGIN] ⚠️  Buffer properties don't match expectations\n");
                // Continue anyway - sometimes it still works
            }
            
            // Try EGL import
            PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 
                (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
            PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 
                (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
            
            if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
                loggy_opengl_desktop("[PLUGIN] ❌ EGL extensions not available\n");
                close(state->dmabuf_fd);
                state->dmabuf_fd = -1;
                return false;
            }
            
            EGLint attribs[] = {
                EGL_WIDTH, width,
                EGL_HEIGHT, height,
                EGL_LINUX_DRM_FOURCC_EXT, (EGLint)test_format,
                EGL_DMA_BUF_PLANE0_FD_EXT, state->dmabuf_fd,
                EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)test_stride,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                EGL_NONE
            };
            
            loggy_opengl_desktop("[PLUGIN] 🔧 Creating EGL image with format=0x%x, stride=%u, fd=%d\n", 
                    test_format, test_stride, state->dmabuf_fd);
            
            state->egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, 
                                               EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
            
            if (state->egl_image == EGL_NO_IMAGE_KHR) {
                EGLint error = eglGetError();
                loggy_opengl_desktop("[PLUGIN] ❌ EGL image failed: 0x%x (%s)\n", error, 
                        error == EGL_BAD_ACCESS ? "EGL_BAD_ACCESS" : 
                        error == EGL_BAD_MATCH ? "EGL_BAD_MATCH" : 
                        error == EGL_BAD_PARAMETER ? "EGL_BAD_PARAMETER" : "OTHER");
                
                close(state->dmabuf_fd);
                state->dmabuf_fd = -1;
                continue;
            }
            
            loggy_opengl_desktop("[PLUGIN] ✅ EGL image created successfully!\n");
            
            // Continue with GL texture and FBO...
            glGenTextures(1, &state->fbo_texture);
            glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image);
            
            GLenum gl_error = glGetError();
            if (gl_error != GL_NO_ERROR) {
                loggy_opengl_desktop("[PLUGIN] ❌ GL texture failed: 0x%x\n", gl_error);
                
                glDeleteTextures(1, &state->fbo_texture);
                PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
                    (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
                if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
                close(state->dmabuf_fd);
                state->dmabuf_fd = -1;
                continue;
            }
            
            // Create FBO
            glGenFramebuffers(1, &state->fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                  GL_TEXTURE_2D, state->fbo_texture, 0);
            
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                
                // SUCCESS! Store metadata
                state->dmabuf_stride = test_stride;
                state->dmabuf_format = test_format;
                state->dmabuf_modifier = DRM_FORMAT_MOD_LINEAR;
                state->dmabuf_offset = 0;
                
                loggy_opengl_desktop("[PLUGIN] 🎉 SUCCESS! Working configuration:\n");
                loggy_opengl_desktop("[PLUGIN]   Format: %s (0x%x)\n", format_names[fmt_idx], test_format);
                loggy_opengl_desktop("[PLUGIN]   Stride: %u bytes\n", test_stride);
                loggy_opengl_desktop("[PLUGIN]   FD: %d\n", state->dmabuf_fd);
                loggy_opengl_desktop("[PLUGIN]   Modifier: 0x%llx\n", (unsigned long long)state->dmabuf_modifier);
                
                return true;
            } else {
                loggy_opengl_desktop("[PLUGIN] ❌ Framebuffer incomplete\n");
                
                // Cleanup and continue
                glDeleteFramebuffers(1, &state->fbo);
                glDeleteTextures(1, &state->fbo_texture);
                PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
                    (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
                if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
                close(state->dmabuf_fd);
                state->dmabuf_fd = -1;
            }
        }
    }
    
    loggy_opengl_desktop("[PLUGIN] 💥 All combinations failed!\n");
    return false;
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
            loggy_opengl_desktop( "[PLUGIN] ❌ recvmsg FAILED: %s\n", strerror(errno));
        }
        return n;
    }

    *fd_count = 0;
    
    loggy_opengl_desktop( "[PLUGIN] recvmsg received %zd bytes\n", n);
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg) {
        loggy_opengl_desktop( "[PLUGIN] No control message (no FDs sent)\n");
    }
    
    while (cmsg != NULL) {
        loggy_opengl_desktop( "[PLUGIN] Control message: level=%d, type=%d, len=%zu\n", 
                cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
                
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int num_fds_in_msg = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int *received_fds = (int *)CMSG_DATA(cmsg);
            
            loggy_opengl_desktop( "[PLUGIN] ✅ Received %d FDs: ", num_fds_in_msg);
            for (int i = 0; i < num_fds_in_msg; i++) {
                if (*fd_count < 32) {
                    fds[*fd_count] = received_fds[i];
                    loggy_opengl_desktop( "%d ", received_fds[i]);
                    (*fd_count)++;
                } else {
                    loggy_opengl_desktop( "(overflow, closing %d) ", received_fds[i]);
                    close(received_fds[i]);
                }
            }
            loggy_opengl_desktop( "\n");
        }
        cmsg = CMSG_NXTHDR(&msg, cmsg);
    }

    return n;
}



// ALTERNATIVE: SIMPLE SMOOTH VERSION - no adaptive frame rate, just consistent 60fps
static bool should_render_frame_smart(struct plugin_state *state) {
    static struct timeval last_render = {0};
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // If content changed, always render immediately
    if (state->content_changed_this_frame) {
        state->content_changed_this_frame = false;
        return true;
    }
    
    // Check time since last render
    struct timeval time_diff;
    timersub(&now, &last_render, &time_diff);
    
    // SIMPLE: Always maintain 60fps for smoothness (16.66ms = 60fps)
    if (time_diff.tv_sec > 0 || time_diff.tv_usec > 8000) {
        last_render = now;
        state->frames_since_last_change++;
        return true;
    }
    
    return false;
}







// This function initializes Python and calls the script's main function
bool launch_python_script(struct plugin_state *state, const char *script_path) {
    char module_name[256];
    char script_dir[512];
    char python_cmd[1024];
    char basename[256];

    loggy_opengl_desktop("[C LOADER] DEBUG: script_path = '%s'\n", script_path);

    if (strstr(script_path, "hosted_mouse_plugin3.py") == NULL) {
        loggy_opengl_desktop("[C LOADER] WARNING: Got unexpected script path, using plugins/hosted_mouse_plugin3.py\n");
        script_path = "plugins/hosted_mouse_plugin3.py";
    }

    if (!Py_IsInitialized()) {
        Py_Initialize();
        if (!Py_IsInitialized()) {
            loggy_opengl_desktop("[C LOADER] Failed to initialize Python interpreter\n");
            return false;
        }
    }

    /* Add script directory to sys.path so imports work */
    strncpy(script_dir, script_path, sizeof(script_dir) - 1);
    script_dir[sizeof(script_dir) - 1] = '\0';
    char *last_slash = strrchr(script_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        snprintf(python_cmd, sizeof(python_cmd), "import sys; sys.path.insert(0, '%s')", script_dir);
        PyRun_SimpleString(python_cmd);
        loggy_opengl_desktop("[C LOADER] Added to Python path: %s\n", script_dir);
    }

    /* Extract module basename */
    const char *filename = strrchr(script_path, '/');
    if (filename) filename++;
    else filename = script_path;
    strncpy(basename, filename, sizeof(basename) - 1);
    basename[sizeof(basename) - 1] = '\0';

    loggy_opengl_desktop("[C LOADER] DEBUG: basename = '%s'\n", basename);

    strncpy(module_name, basename, sizeof(module_name) - 1);
    module_name[sizeof(module_name) - 1] = '\0';
    char *dot = strrchr(module_name, '.');
    if (dot) *dot = '\0';
    loggy_opengl_desktop("[C LOADER] DEBUG: module_name = '%s'\n", module_name);

    PyObject *py_module = PyImport_ImportModule(module_name);
    if (!py_module) {
        PyErr_Print();
        loggy_opengl_desktop("[C LOADER] Failed to import Python module: %s\n", module_name);
        PyRun_SimpleString("import sys, os");
        PyRun_SimpleString("print('Current directory:', os.getcwd())");
        PyRun_SimpleString("print('Files in current dir:', os.listdir('.'))");
        PyRun_SimpleString("print('Python path:', sys.path)");
        return false;
    }

    PyObject *py_main_func = PyObject_GetAttrString(py_module, "main");
    if (!py_main_func || !PyCallable_Check(py_main_func)) {
        loggy_opengl_desktop("[C LOADER] Python module missing callable 'main' function\n");
        Py_XDECREF(py_main_func);
        Py_DECREF(py_module);
        return false;
    }

    state->py_module = py_module;
    state->py_main_func = py_main_func;


    PyObject *args = PyTuple_New(16);
    if (!args) {
        loggy_opengl_desktop("[C LOADER] Failed to create args tuple for 16 arguments");
        // ... (cleanup) ...
        return false;
    }

    // --- Core Arguments (Indices 0-3) ---
    PyTuple_SetItem(args, 0, PyLong_FromLong((long)state->ipc_fd));
    PyTuple_SetItem(args, 1, PyUnicode_FromString("shm_placeholder"));
    PyTuple_SetItem(args, 2, PyLong_FromVoidPtr((void *)state->egl_display));
    PyTuple_SetItem(args, 3, PyLong_FromVoidPtr((void *)state->egl_context));

    // --- Buffer 0 Arguments (Indices 4-9) ---
    PyTuple_SetItem(args, 4, PyLong_FromLong((long)state->fbo_textures[0]));
    PyTuple_SetItem(args, 5, PyLong_FromLong((long)state->dmabuf_fds[0]));
    PyTuple_SetItem(args, 6, PyLong_FromLong((long)state->width));
    PyTuple_SetItem(args, 7, PyLong_FromLong((long)state->height));
    PyTuple_SetItem(args, 8, PyLong_FromLong((long)state->dmabuf_strides[0]));
    PyTuple_SetItem(args, 9, PyLong_FromLong((long)state->dmabuf_formats[0]));

    // --- Buffer 1 Arguments (Indices 10-15) ---
    PyTuple_SetItem(args, 10, PyLong_FromLong((long)state->fbo_textures[1]));
    PyTuple_SetItem(args, 11, PyLong_FromLong((long)state->dmabuf_fds[1]));
    PyTuple_SetItem(args, 12, PyLong_FromLong((long)state->width));
    PyTuple_SetItem(args, 13, PyLong_FromLong((long)state->height));
    PyTuple_SetItem(args, 14, PyLong_FromLong((long)state->dmabuf_strides[1]));
    PyTuple_SetItem(args, 15, PyLong_FromLong((long)state->dmabuf_formats[1]));

    loggy_opengl_desktop("[C LOADER] Passing Buffer 0: tex=%u, fd=%d, stride=%u",
        state->fbo_textures[0], state->dmabuf_fds[0], state->dmabuf_strides[0]);
    loggy_opengl_desktop("[C LOADER] Passing Buffer 1: tex=%u, fd=%d, stride=%u",
        state->fbo_textures[1], state->dmabuf_fds[1], state->dmabuf_strides[1]);

    loggy_opengl_desktop("[C LOADER] Calling Python main function...");
    PyObject *result = PyObject_CallObject(state->py_main_func, args);

    int return_code = 1;
    if (result) {
        if (PyLong_Check(result)) {
            return_code = (int) PyLong_AsLong(result);
        } else {
            /* If script returned None or non-int, treat as success(0) */
            return_code = 0;
        }
        Py_DECREF(result);
    } else {
        PyErr_Print();
        return_code = 1;
    }

    /* Cleanup */
    Py_DECREF(args);
    loggy_opengl_desktop("[C LOADER] Python main function returned: %d\n", return_code);

    /* NOTE: Python now owns dup_fd, close our original later in C cleanup if needed.
       If you want C to keep the original dmabuf_fd open, do not close it here. */

    return return_code == 0;
}



// Method 1: Create DMA-BUF BEFORE initializing Python
// This makes the process appear "pure C" during DMA-BUF creation
// FIXED: This version makes the parent process re-initialize its EGL context
// after the child exits, solving the EGL_BAD_DISPLAY (0x3008) error.
bool create_dmabuf_before_python(struct plugin_state *state, int width, int height) {
    // Solution: Fork a pure C child process to create the DMA-BUF, then
    // have the parent re-create its EGL context which was corrupted by the fork.

    loggy_opengl_desktop("[PLUGIN] 🍴 Forking pure C process for trusted DMA-BUF creation\n");

    // Create a socket pair for passing the file descriptor from child to parent
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        loggy_opengl_desktop("[PLUGIN] ❌ Failed to create socketpair: %s\n", strerror(errno));
        return false;
    }

    pid_t child_pid = fork();

    if (child_pid == -1) {
        loggy_opengl_desktop("[PLUGIN] ❌ Fork failed: %s\n", strerror(errno));
        close(sv[0]);
        close(sv[1]);
        return false;
    }

    if (child_pid == 0) {
        // ===================================================================
        // ===== CHILD PROCESS (PURE C, TEMPORARY) =========================
        // ===================================================================
        close(sv[0]); // Close parent's end of the socket

        loggy_opengl_desktop("[PLUGIN-CHILD] Pure C child process started (PID=%d)\n", getpid());

        // Initialize a temporary, isolated EGL context in the child.
        // This is necessary to interact with the graphics driver.
        EGLDisplay child_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (child_egl_display == EGL_NO_DISPLAY || !eglInitialize(child_egl_display, NULL, NULL)) {
            loggy_opengl_desktop("[PLUGIN-CHILD] ❌ EGL init failed\n");
            exit(1);
        }

        EGLConfig config;
        EGLint config_count;
        EGLint config_attribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };
        eglChooseConfig(child_egl_display, config_attribs, &config, 1, &config_count);

        EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        EGLContext child_context = eglCreateContext(child_egl_display, config, EGL_NO_CONTEXT, context_attribs);

        EGLint pbuffer_attribs[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
        EGLSurface child_surface = eglCreatePbufferSurface(child_egl_display, config, pbuffer_attribs);

        if (!eglMakeCurrent(child_egl_display, child_surface, child_surface, child_context)) {
            loggy_opengl_desktop("[PLUGIN-CHILD] ❌ EGL make current failed\n");
            exit(1);
        }
        loggy_opengl_desktop("[PLUGIN-CHILD] ✅ EGL context initialized successfully\n");

        // Create the DMA-BUF using a known-good stride and format
        uint32_t stride = ((width * 4) + 255) & ~255; // 256-byte aligned
        uint32_t format = DRM_FORMAT_XRGB8888;
        size_t buffer_size = stride * height;

        int dmabuf_fd = create_dmabuf(buffer_size, getpid());
        if (dmabuf_fd < 0) {
            loggy_opengl_desktop("[PLUGIN-CHILD] ❌ DMA-BUF creation failed\n");
            exit(1);
        }
        loggy_opengl_desktop("[PLUGIN-CHILD] ✅ Created DMA-BUF: fd=%d, size=%zu\n", dmabuf_fd, buffer_size);

        // Send the created DMA-BUF file descriptor to the parent process
        struct msghdr msg = {0};
        struct iovec iov[1];
        char data = 'D'; // Dummy data, required for sendmsg
        char cmsg_buf[CMSG_SPACE(sizeof(int))];

        iov[0].iov_base = &data;
        iov[0].iov_len = 1;
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = sizeof(cmsg_buf);

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(cmsg), &dmabuf_fd, sizeof(int));

        if (sendmsg(sv[1], &msg, 0) == -1) {
            loggy_opengl_desktop("[PLUGIN-CHILD] ❌ Failed to send DMA-BUF FD: %s\n", strerror(errno));
            exit(1);
        }
        loggy_opengl_desktop("[PLUGIN-CHILD] ✅ Sent DMA-BUF FD %d to parent\n", dmabuf_fd);

        // Clean up all child resources before exiting
        close(dmabuf_fd);
        close(sv[1]);
        eglMakeCurrent(child_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(child_egl_display, child_context);
        eglDestroySurface(child_egl_display, child_surface);
        eglTerminate(child_egl_display);
        exit(0); // Clean exit
    }

    // ===================================================================
    // ===== PARENT PROCESS (C+Python) ===================================
    // ===================================================================
    close(sv[1]); // Close child's end of the socket

    loggy_opengl_desktop("[PLUGIN] Waiting for pure C child to create DMA-BUF...\n");

    // Receive the DMA-BUF FD from the child
    struct msghdr msg = {0};
    struct iovec iov[1];
    char data;
    char cmsg_buf[CMSG_SPACE(sizeof(int))];

    iov[0].iov_base = &data;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    ssize_t recv_result = recvmsg(sv[0], &msg, 0);

    // Wait for the child to exit completely to ensure all its resources are released
    int child_status;
    waitpid(child_pid, &child_status, 0);
    close(sv[0]);

    if (recv_result <= 0) {
        loggy_opengl_desktop("[PLUGIN] ❌ Failed to receive DMA-BUF FD from child: %s\n", strerror(errno));
        return false;
    }
    if (WEXITSTATUS(child_status) != 0) {
        loggy_opengl_desktop("[PLUGIN] ❌ Child process failed with exit code %d\n", WEXITSTATUS(child_status));
        return false;
    }

    // Extract the FD from the message
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    int received_fd = -1;
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    }

    if (received_fd < 0) {
        loggy_opengl_desktop("[PLUGIN] ❌ No valid FD received from child\n");
        return false;
    }
    loggy_opengl_desktop("[PLUGIN] ✅ Received DMA-BUF FD %d from pure C child\n", received_fd);


    // ===================================================================
    // ===== THE FIX: RE-INITIALIZE PARENT'S EGL CONTEXT =================
    // ===================================================================
    loggy_opengl_desktop("[PLUGIN] ⚠️  Parent EGL context is assumed corrupt due to fork. Re-initializing...\n");

    // If a display connection already exists, it MUST be terminated.
    if (state->egl_display != EGL_NO_DISPLAY) {
        eglTerminate(state->egl_display);
        loggy_opengl_desktop("[PLUGIN] Terminated stale EGL display connection.\n");
    }

    // Re-initialize a completely new, clean EGL context for the parent.
    // We call the same function the parent would have called initially.
    if (!init_plugin_egl(state)) {
        loggy_opengl_desktop("[PLUGIN] ❌ Failed to re-initialize a clean EGL context for parent.\n");
        close(received_fd);
        return false;
    }
    loggy_opengl_desktop("[PLUGIN] ✅ Parent EGL context re-initialized successfully.\n");


    // Now, with a clean EGL context, use the DMA-BUF FD received from the child
    state->dmabuf_fd = received_fd;
    state->dmabuf_stride = ((width * 4) + 255) & ~255; // Must match child's calculation
    state->dmabuf_format = DRM_FORMAT_XRGB8888;
    state->dmabuf_modifier = DRM_FORMAT_MOD_LINEAR;
    state->dmabuf_offset = 0;
    state->width = width;
    state->height = height;

    // Create an EGL image in the parent's NEW context from the DMA-BUF
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    EGLint attribs[] = {
        EGL_WIDTH, width, EGL_HEIGHT, height,
        EGL_LINUX_DRM_FOURCC_EXT, state->dmabuf_format,
        EGL_DMA_BUF_PLANE0_FD_EXT, state->dmabuf_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, state->dmabuf_stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE
    };

    state->egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT,
                                       EGL_LINUX_DMA_BUF_EXT, NULL, attribs);

    if (state->egl_image == EGL_NO_IMAGE_KHR) {
        loggy_opengl_desktop("[PLUGIN] ❌ Failed to import pure C DMA-BUF in parent's new context. Error: 0x%x\n", eglGetError());
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }

    // Create the final GL texture and FBO in the parent
    glGenTextures(1, &state->fbo_texture);
    glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image);

    glGenFramebuffers(1, &state->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, state->fbo_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        loggy_opengl_desktop("[PLUGIN] ❌ FBO incomplete with pure C DMA-BUF\n");
        // Clean up resources from this failed attempt
        glDeleteFramebuffers(1, &state->fbo);
        glDeleteTextures(1, &state->fbo_texture);
        close(state->dmabuf_fd);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    loggy_opengl_desktop("[PLUGIN] 🎉 SUCCESS! DMA-BUF created by child and imported into parent's clean EGL context.\n");
    loggy_opengl_desktop("[PLUGIN]   FD: %d, Stride: %u, Format: 0x%x\n",
                         state->dmabuf_fd, state->dmabuf_stride, state->dmabuf_format);

    return true;
}

// Add this to your main C program
// Fixed version with proper executable path handling
bool create_dmabuf_via_external_process(struct plugin_state *state, int width, int height) {
    loggy_opengl_desktop("[PLUGIN] 🚀 Creating DMA-BUF via external pure C process\n");
    
    // First, check if dmabuf_creator exists in various locations
    const char* possible_paths[] = {
        "./dmabuf_creator_plugin",           // Current directory
        "./plugins/dmabuf_creator_plugin",   // plugins subdirectory
        "dmabuf_creator_plugin",             // PATH search
        "../dmabuf_creator_plugin",          // Parent directory
    };
    
    const char* exec_path = NULL;
    for (int i = 0; i < 4; i++) {
        if (access(possible_paths[i], X_OK) == 0) {
            exec_path = possible_paths[i];
            loggy_opengl_desktop("[PLUGIN] Found dmabuf_creator at: %s\n", exec_path);
            break;
        }
    }
    
    if (!exec_path) {
        loggy_opengl_desktop("[PLUGIN] ❌ dmabuf_creator executable not found in any of these locations:\n");
        for (int i = 0; i < 4; i++) {
            loggy_opengl_desktop("[PLUGIN]   %s\n", possible_paths[i]);
        }
        loggy_opengl_desktop("[PLUGIN] Please compile it with: gcc -o dmabuf_creator dmabuf_creator.c -lEGL -lGL\n");
        return false;
    }
    
    // Create Unix socket for communication
    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "/tmp/dmabuf_creator_%d.sock", getpid());
    unlink(socket_path); // Remove if exists
    
    int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock < 0) {
        loggy_opengl_desktop("[PLUGIN] Failed to create socket: %s\n", strerror(errno));
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        loggy_opengl_desktop("[PLUGIN] Socket bind failed: %s\n", strerror(errno));
        close(server_sock);
        return false;
    }
    
    if (listen(server_sock, 1) < 0) {
        loggy_opengl_desktop("[PLUGIN] Socket listen failed: %s\n", strerror(errno));
        close(server_sock);
        unlink(socket_path);
        return false;
    }
    
    loggy_opengl_desktop("[PLUGIN] Socket created at %s\n", socket_path);
    
    // Launch external pure C process
    char width_str[32], height_str[32];
    snprintf(width_str, sizeof(width_str), "%d", width);
    snprintf(height_str, sizeof(height_str), "%d", height);
    
    loggy_opengl_desktop("[PLUGIN] Launching: %s %s %s %s\n", exec_path, width_str, height_str, socket_path);
    
    pid_t child_pid = fork();
    if (child_pid == -1) {
        loggy_opengl_desktop("[PLUGIN] Fork failed: %s\n", strerror(errno));
        close(server_sock);
        unlink(socket_path);
        return false;
    }
    
    if (child_pid == 0) {
        // Child: exec the pure C DMA-BUF creator
        close(server_sock);
        
        // Use execv instead of execl for better path handling
        char *args[] = {
            "dmabuf_creator",
            width_str,
            height_str,
            socket_path,
            NULL
        };
        
        execv(exec_path, args);
        
        // If we get here, exec failed
        fprintf(stderr, "[PLUGIN-CHILD] exec failed for %s: %s\n", exec_path, strerror(errno));
        exit(1);
    }
    
    // Parent: wait for connection from pure C process
    loggy_opengl_desktop("[PLUGIN] Waiting for pure C process (PID=%d) to connect...\n", child_pid);
    
    // Set a timeout for accept to avoid hanging forever
    fd_set read_fds;
    struct timeval timeout;
    FD_ZERO(&read_fds);
    FD_SET(server_sock, &read_fds);
    timeout.tv_sec = 5;  // 5 second timeout
    timeout.tv_usec = 0;
    
    int select_result = select(server_sock + 1, &read_fds, NULL, NULL, &timeout);
    if (select_result <= 0) {
        loggy_opengl_desktop("[PLUGIN] Timeout waiting for pure C process connection\n");
        close(server_sock);
        unlink(socket_path);
        
        // Check if child process is still running
        int child_status;
        pid_t wait_result = waitpid(child_pid, &child_status, WNOHANG);
        if (wait_result == child_pid) {
            loggy_opengl_desktop("[PLUGIN] Child process exited with status %d\n", WEXITSTATUS(child_status));
        } else {
            loggy_opengl_desktop("[PLUGIN] Child process still running, killing it\n");
            kill(child_pid, SIGTERM);
            waitpid(child_pid, NULL, 0);
        }
        
        return false;
    }
    
    int client_sock = accept(server_sock, NULL, NULL);
    if (client_sock < 0) {
        loggy_opengl_desktop("[PLUGIN] Accept failed: %s\n", strerror(errno));
        close(server_sock);
        unlink(socket_path);
        waitpid(child_pid, NULL, 0);
        return false;
    }
    
    loggy_opengl_desktop("[PLUGIN] ✅ Connected to pure C process\n");
    
    // Receive metadata and DMA-BUF FD
    struct {
        int width, height;
        uint32_t stride, format, modifier, offset;
    } metadata;
    
    struct msghdr msg = {0};
    struct iovec iov[1];
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    
    iov[0].iov_base = &metadata;
    iov[0].iov_len = sizeof(metadata);
    
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    
    ssize_t recv_result = recvmsg(client_sock, &msg, 0);
    if (recv_result < 0) {
        loggy_opengl_desktop("[PLUGIN] Failed to receive from pure C process: %s\n", strerror(errno));
        close(client_sock);
        close(server_sock);
        unlink(socket_path);
        waitpid(child_pid, NULL, 0);
        return false;
    }
    
    // Extract the FD
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
        loggy_opengl_desktop("[PLUGIN] No FD received from pure C process\n");
        close(client_sock);
        close(server_sock);
        unlink(socket_path);
        waitpid(child_pid, NULL, 0);
        return false;
    }
    
    int received_fd;
    memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    
    loggy_opengl_desktop("[PLUGIN] ✅ Received DMA-BUF from pure C process:\n");
    loggy_opengl_desktop("[PLUGIN]   FD: %d\n", received_fd);
    loggy_opengl_desktop("[PLUGIN]   Stride: %u\n", metadata.stride);
    loggy_opengl_desktop("[PLUGIN]   Format: 0x%x\n", metadata.format);
    loggy_opengl_desktop("[PLUGIN]   Modifier: 0x%x\n", metadata.modifier);
    
    // Store the trusted DMA-BUF metadata
    state->dmabuf_fd = received_fd;
    state->dmabuf_stride = metadata.stride;
    state->dmabuf_format = metadata.format;
    state->dmabuf_modifier = metadata.modifier;
    state->dmabuf_offset = metadata.offset;
    state->width = width;
    state->height = height;
    
    // Create EGL image in parent process using trusted DMA-BUF
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
        loggy_opengl_desktop("[PLUGIN] EGL extensions not available in parent\n");
        close(received_fd);
        close(client_sock);
        close(server_sock);
        unlink(socket_path);
        return false;
    }
    
    EGLint attribs[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_LINUX_DRM_FOURCC_EXT, (EGLint)metadata.format,
        EGL_DMA_BUF_PLANE0_FD_EXT, received_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)metadata.stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, (EGLint)metadata.offset,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLint)(metadata.modifier & 0xFFFFFFFF),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLint)(metadata.modifier >> 32),
        EGL_NONE
    };
    
    state->egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, 
                                       EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
    
    if (state->egl_image == EGL_NO_IMAGE_KHR) {
        loggy_opengl_desktop("[PLUGIN] Failed to import trusted DMA-BUF in parent\n");
        close(received_fd);
        close(client_sock);
        close(server_sock);
        unlink(socket_path);
        return false;
    }
    
    // Create GL texture and FBO
    glGenTextures(1, &state->fbo_texture);
    glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image);
    
    glGenFramebuffers(1, &state->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                          GL_TEXTURE_2D, state->fbo_texture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        loggy_opengl_desktop("[PLUGIN] FBO incomplete with trusted DMA-BUF\n");
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Cleanup
    close(client_sock);
    close(server_sock);
    unlink(socket_path);
    waitpid(child_pid, NULL, 0);
    
    loggy_opengl_desktop("[PLUGIN] 🎉 SUCCESS! Using DMA-BUF created by external pure C process\n");
    loggy_opengl_desktop("[PLUGIN]   This DMA-BUF should be trusted by the compositor\n");
    
    return true;
}




// Debug function for process info
void log_process_info_for_debugging(void) {
    loggy_opengl_desktop("[C LOADER] 🔍 Process debugging info:\n");
    
    // Check executable path
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        loggy_opengl_desktop("[C LOADER]   Executable: %s\n", exe_path);
    }
    
    // Check process name
    FILE *comm = fopen("/proc/self/comm", "r");
    if (comm) {
        char process_name[256];
        if (fgets(process_name, sizeof(process_name), comm)) {
            process_name[strcspn(process_name, "\n")] = 0;
            loggy_opengl_desktop("[C LOADER]   Process name: %s\n", process_name);
            
            if (strstr(process_name, "python") || strstr(process_name, "Python")) {
                loggy_opengl_desktop("[C LOADER]   ⚠️  Process name contains 'python' - may affect GPU trust\n");
            }
        }
        fclose(comm);
    }
    
    loggy_opengl_desktop("[C LOADER]   PID: %d\n", getpid());
    loggy_opengl_desktop("[C LOADER]   UID: %d, GID: %d\n", getuid(), getgid());
}


// Same-context DMA-BUF creation to avoid cross-context sharing issues
bool create_dmabuf_same_context(struct plugin_state *state, int width, int height) {
    loggy_opengl_desktop("[PLUGIN] 🚀 Creating DMA-BUF in same EGL context (avoiding cross-context issues)\n");
    
    // Ensure we're in the right EGL context
    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        loggy_opengl_desktop("[PLUGIN] Failed to make EGL context current\n");
        return false;
    }
    
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
        loggy_opengl_desktop("[PLUGIN] Required EGL extensions not available\n");
        return false;
    }
    
    // Test different DMA heap allocators in order of compositor compatibility
    const char* heap_paths[] = {
        "/dev/dma_heap/system-uncached",    // Best for cross-process sharing
        "/dev/dma_heap/system",             // Most compatible fallback
        "/dev/dma_heap/linux,cma",          // CMA heap (if available)
        "/dev/dma_heap/reserved",           // Reserved memory heap
    };
    
    // Test stride alignments that work well with nested Wayland
    uint32_t alignments[] = {512, 256, 1024, 64}; // Start with larger alignments
    uint32_t format = DRM_FORMAT_XRGB8888;
    
    loggy_opengl_desktop("[PLUGIN] Testing DMA heaps and alignments for %dx%d XRGB8888\n", width, height);
    
    for (int heap_idx = 0; heap_idx < 4; heap_idx++) {
        const char* heap_path = heap_paths[heap_idx];
        
        int heap_fd = open(heap_path, O_RDONLY | O_CLOEXEC);
        if (heap_fd < 0) {
            loggy_opengl_desktop("[PLUGIN]   %s: not available (%s)\n", heap_path, strerror(errno));
            continue;
        }
        
        loggy_opengl_desktop("[PLUGIN]   Testing heap: %s\n", heap_path);
        
        for (int align_idx = 0; align_idx < 4; align_idx++) {
            uint32_t alignment = alignments[align_idx];
            uint32_t stride = ((width * 4) + alignment - 1) & ~(alignment - 1);
            size_t buffer_size = stride * height;
            
            loggy_opengl_desktop("[PLUGIN]     Trying stride %u (%u-byte aligned), size %zu\n", 
                               stride, alignment, buffer_size);
            
            // Allocate DMA-BUF
            struct dma_heap_allocation_data heap_data = {
                .len = buffer_size,
                .fd_flags = O_RDWR | O_CLOEXEC,
            };
            
            if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data) != 0) {
                loggy_opengl_desktop("[PLUGIN]       Allocation failed: %s\n", strerror(errno));
                continue;
            }
            
            int dmabuf_fd = heap_data.fd;
            
            // Test EGL import in the SAME context that will be used later
            EGLint attribs[] = {
                EGL_WIDTH, width,
                EGL_HEIGHT, height,
                EGL_LINUX_DRM_FOURCC_EXT, format,
                EGL_DMA_BUF_PLANE0_FD_EXT, dmabuf_fd,
                EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, 0,
                EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, 0,
                EGL_NONE
            };
            
            loggy_opengl_desktop("[PLUGIN]       Testing EGL import in same context...\n");
            EGLImageKHR egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, 
                                                    EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
            
            if (egl_image == EGL_NO_IMAGE_KHR) {
                EGLint error = eglGetError();
                loggy_opengl_desktop("[PLUGIN]       ❌ EGL import failed: 0x%x\n", error);
                close(dmabuf_fd);
                continue;
            }
            
            // Success! Test GL texture creation too
            GLuint test_texture;
            glGenTextures(1, &test_texture);
            glBindTexture(GL_TEXTURE_2D, test_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, test_texture);
            
            GLenum gl_error = glGetError();
            if (gl_error != GL_NO_ERROR) {
                loggy_opengl_desktop("[PLUGIN]       ❌ GL texture binding failed: 0x%x\n", gl_error);
                glDeleteTextures(1, &test_texture);
                close(dmabuf_fd);
                continue;
            }
            
            // Test FBO creation
            GLuint test_fbo;
            glGenFramebuffers(1, &test_fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, test_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                  GL_TEXTURE_2D, test_texture, 0);
            
            GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (fbo_status != GL_FRAMEBUFFER_COMPLETE) {
                loggy_opengl_desktop("[PLUGIN]       ❌ FBO incomplete: 0x%x\n", fbo_status);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDeleteFramebuffers(1, &test_fbo);
                glDeleteTextures(1, &test_texture);
                close(dmabuf_fd);
                continue;
            }
            
            loggy_opengl_desktop("[PLUGIN]       ✅ COMPLETE SUCCESS! Found working same-context DMA-BUF:\n");
            loggy_opengl_desktop("[PLUGIN]         Heap: %s\n", heap_path);
            loggy_opengl_desktop("[PLUGIN]         Format: 0x%x\n", format);
            loggy_opengl_desktop("[PLUGIN]         Stride: %u (%u-byte aligned)\n", stride, alignment);
            loggy_opengl_desktop("[PLUGIN]         Size: %zu bytes\n", buffer_size);
            loggy_opengl_desktop("[PLUGIN]         FD: %d\n", dmabuf_fd);
            
            // Store the working configuration
            state->dmabuf_fd = dmabuf_fd;
            state->dmabuf_stride = stride;
            state->dmabuf_format = format;
            state->dmabuf_modifier = DRM_FORMAT_MOD_LINEAR;
            state->dmabuf_offset = 0;
            state->width = width;
            state->height = height;
            state->egl_image = egl_image;
            state->fbo_texture = test_texture;
            state->fbo = test_fbo;
            
            // Reset GL state
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            
            close(heap_fd);
            
            loggy_opengl_desktop("[PLUGIN] 🎉 Same-context DMA-BUF creation successful!\n");
            loggy_opengl_desktop("[PLUGIN]   This should NOT cause cross-context sharing issues\n");
            
            return true;
        }
        
        close(heap_fd);
    }
    
    loggy_opengl_desktop("[PLUGIN] ❌ No working same-context DMA-BUF configuration found\n");
    return false;
}

// In python_plugin.c

/**
 * Enhanced DMA-BUF import, updated for the double-buffering architecture.
 * This function now takes an index (0 or 1) and operates on the arrays within the plugin_state.
 */
bool import_external_dmabuf_with_retry(struct plugin_state *state, int index) {
    // Get the correct buffer attributes from the state arrays using the index
    int dmabuf_fd         = state->dmabuf_fds[index];
    uint32_t width        = state->width;
    uint32_t height       = state->height;
    uint32_t stride       = state->dmabuf_strides[index];
    uint32_t format       = state->dmabuf_formats[index];
    uint64_t modifier     = state->dmabuf_modifiers[index];
    uint32_t offset       = state->dmabuf_offsets[index];

    loggy_opengl_desktop("[PLUGIN] 🔍 Importing external DMA-BUF (index %d) with enhanced debugging:\n", index);
    loggy_opengl_desktop("[PLUGIN]   FD: %d, Size: %ux%u, Stride: %u, Format: 0x%x",
                         dmabuf_fd, width, height, stride, format);

    // FD validation and EGL context setup (remains the same)
    struct stat fd_stat;
    if (fstat(dmabuf_fd, &fd_stat) != 0) {
        loggy_opengl_desktop("[PLUGIN] ❌ DMA-BUF FD %d validation failed: %s", dmabuf_fd, strerror(errno));
        return false;
    }
    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        loggy_opengl_desktop("[PLUGIN] ❌ Failed to make EGL context current: 0x%x", eglGetError());
        return false;
    }

    // Get EGL function pointers (remains the same)
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
        loggy_opengl_desktop("[PLUGIN] ❌ Required EGL extensions not available");
        return false;
    }

    // --- Import strategies (logic is the same, just using local variables) ---
    EGLint attribs[] = {
        EGL_WIDTH, (EGLint)width,
        EGL_HEIGHT, (EGLint)height,
        EGL_LINUX_DRM_FOURCC_EXT, (EGLint)format,
        EGL_DMA_BUF_PLANE0_FD_EXT, dmabuf_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, (EGLint)offset,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLint)(modifier & 0xFFFFFFFF),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLint)(modifier >> 32),
        EGL_NONE
    };

    EGLImageKHR egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);

    if (egl_image == EGL_NO_IMAGE_KHR) {
        loggy_opengl_desktop("[PLUGIN] ❌ EGL image creation for buffer %d failed. Error: 0x%x", index, eglGetError());
        return false;
    }
    
    // Store the created EGL image in the state array
    state->egl_images[index] = egl_image;

    // --- OpenGL Texture and FBO Creation ---
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // **THE FIX for the type error is to cast the EGLImageKHR.**
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)egl_image);

    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_opengl_desktop("[PLUGIN] ❌ OpenGL texture binding for buffer %d failed: 0x%x", index, gl_error);
        // ... (cleanup egl_image, texture) ...
        return false;
    }
    
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fbo_status != GL_FRAMEBUFFER_COMPLETE) {
        loggy_opengl_desktop("[PLUGIN] ❌ FBO %d incomplete: 0x%x", index, fbo_status);
        // ... (cleanup egl_image, texture, fbo) ...
        return false;
    }

    loggy_opengl_desktop("[PLUGIN] 🎉 COMPLETE SUCCESS! Imported DMA-BUF for index %d.", index);
    loggy_opengl_desktop("[PLUGIN]   GL Texture: %u, GL FBO: %u", texture, fbo);

    // Store the working FBO and texture in the state arrays
    state->fbo_textures[index] = texture;
    state->fbos[index] = fbo;

    // Reset GL state
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

// Enhanced external process creation with better error handling
// =======================================================================================
// FULLY MODIFIED AND FIXED FUNCTION
// =======================================================================================
// This version solves both the EGL_BAD_DISPLAY error in the parent by re-initializing
// its EGL context, and the EGL_BAD_ACCESS ("invalid pitch") error in the compositor
// by creating the DMA-BUF with a compatible, padded stride.
// =======================================================================================


// In python_plugin.c

static bool receive_initial_buffer(struct plugin_state *state) {
    loggy_opengl_desktop("[C LOADER] Waiting for compositor to send allocated DMA-BUF(s)...");

    ipc_event_t event;
    int received_fds[2]; // We now expect two FDs
    int fd_count = 0;

    ssize_t n = read_ipc_with_fds(state->ipc_fd, &event, received_fds, &fd_count);

    if (n <= 0) {
        loggy_opengl_desktop("[C LOADER] ❌ Failed to read from IPC socket: %s",
                             n == 0 ? "Connection closed" : strerror(errno));
        return false;
    }

    if (event.type != IPC_EVENT_TYPE_BUFFER_ALLOCATED) {
        loggy_opengl_desktop("[C LOADER] ❌ Expected BUFFER_ALLOCATED event, but got type %d", event.type);
        if (fd_count > 0) { close(received_fds[0]); }
        if (fd_count > 1) { close(received_fds[1]); }
        return false;
    }

    // *** THE CORE FIX IS HERE ***
    // We now check for the correct number of FDs and parse the correct union member.
    if (fd_count != 2 || received_fds[0] < 0 || received_fds[1] < 0) {
        loggy_opengl_desktop("[C LOADER] ❌ BUFFER_ALLOCATED event received but fd_count is %d (expected 2) or FDs are invalid.", fd_count);
        if (fd_count > 0) { close(received_fds[0]); }
        if (fd_count > 1) { close(received_fds[1]); }
        return false;
    }

    // Extract the multi-buffer info from the correct union member
    ipc_multi_buffer_info_t *info = &event.data.multi_buffer_info;

    loggy_opengl_desktop("[C LOADER] ✅ Received %d DMA-BUFs from compositor:", info->count);

    // Store properties for both buffers
    for (int i = 0; i < 2; i++) {
        state->dmabuf_fds[i]        = received_fds[i];
        state->width                = info->buffers[i].width; // Assume both are the same size
        state->height               = info->buffers[i].height;
        state->dmabuf_strides[i]    = info->buffers[i].stride;
        state->dmabuf_formats[i]    = info->buffers[i].format;
        state->dmabuf_modifiers[i]  = info->buffers[i].modifier;
        state->dmabuf_offsets[i]    = info->buffers[i].offset;

        loggy_opengl_desktop("[C LOADER]   Buffer %d: FD=%d, Size=%ux%u, Stride=%u, Format=0x%x",
            i, state->dmabuf_fds[i], state->width, state->height,
            state->dmabuf_strides[i], state->dmabuf_formats[i]);
    }

    return true;
}

// In python_plugin.c
// REPLACE your existing main function with this one.

int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, "plugin_host", 0, 0, 0);
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ipc_fd> <python_script_path>\n", argv[0]);
        return 1;
    }

    struct plugin_state state = {0};
    state.ipc_fd = atoi(argv[1]);
    const char *python_script_path = argv[2];

    loggy_opengl_desktop("[C LOADER] Starting up...\n");
    log_process_info_for_debugging();

    // ===================================================================
    //  STEP 1: RECEIVE THE PRE-ALLOCATED BUFFER FROM THE COMPOSITOR
    // ===================================================================
    // All the old forking and buffer creation logic is GONE.
    if (!receive_initial_buffer(&state)) {
        loggy_opengl_desktop("[C LOADER] FATAL: Did not receive a valid buffer from the compositor.\n");
        return 1;
    }

    // ===================================================================
    //  STEP 2: INITIALIZE EGL (NO FORKING NEEDED)
    // ===================================================================
    // Since we are not forking, our EGL context will be clean and valid.
    if (!init_plugin_egl(&state)) {
        loggy_opengl_desktop("[C LOADER] FATAL: Failed to initialize EGL context.\n");
        close(state.dmabuf_fd);
        return 1;
    }

    // ===================================================================
    //  STEP 3: IMPORT THE COMPOSITOR'S BUFFER
    // ===================================================================
    // We now import the buffer that the compositor GAVE us.
    // This is guaranteed to be compatible.
    // Import BOTH buffers
    for (int i = 0; i < 2; i++) {
        if (!import_external_dmabuf_with_retry(&state, i)) {
             loggy_opengl_desktop("[C LOADER] FATAL: Failed to import compositor-provided DMA-BUF %d.", i);
             // ... cleanup ...
             return 1;
        }
    }

    loggy_opengl_desktop("[C LOADER] ✅ Successfully imported 2 compositor-provided buffers into EGL.");


    // ===================================================================
    //  STEP 4: LAUNCH PYTHON
    // ===================================================================
    loggy_opengl_desktop("[C LOADER] Launching Python with compositor-provided DMA-BUF...\n");

    if (!launch_python_script(&state, python_script_path)) {
        loggy_opengl_desktop("[C LOADER] FATAL: Python script execution failed.\n");
    }

    // ===================================================================
    //  STEP 5: CLEANUP
    // ===================================================================
    loggy_opengl_desktop("[C LOADER] Python script finished. Cleaning up.\n");

    // ... (Your existing cleanup logic for EGL, Python, FDs, etc.) ...
    // Make sure to close state.dmabuf_fd here.
    if (state.dmabuf_fd >= 0) {
        close(state.dmabuf_fd);
    }

    loggy_opengl_desktop("[C LOADER] Cleanup complete.\n");
    return 0;
}

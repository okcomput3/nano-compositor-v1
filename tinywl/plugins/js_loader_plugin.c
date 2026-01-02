// js_plugin_loader.c - JavaScript/Node.js Plugin Host with DMA-BUF and WebGL Support
// Compile: gcc -o plugins/js_loader_plugin js_plugin_loader.c -lEGL -lGLESv2 -lgbm -ldrm -I/usr/include/libdrm
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <gbm.h>
#include <drm/drm_fourcc.h>

// Use the real IPC header from your project
#include "nano_ipc.h"

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct js_plugin_state {
    int ipc_fd;
    int drm_fd;
    struct gbm_device *gbm;
    
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    
    // Double buffering
    int dmabuf_fds[2];
    uint32_t width, height;
    uint32_t strides[2];
    uint32_t formats[2];
    uint64_t modifiers[2];
    
    EGLImage egl_images[2];
    GLuint fbo_textures[2];
    GLuint fbos[2];
    
    pid_t node_pid;
    int node_stdin;
    int node_stdout;
    
    // For standalone rendering mode
    GLuint shader_program;
    GLuint vbo;
    GLint u_time;
    GLint u_resolution;
};

struct mapped_buffer {
    void *data;
    size_t size;
};

// Global state for signal handler
static volatile bool g_running = true;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static bool init_egl(struct js_plugin_state *state);
static bool receive_buffers(struct js_plugin_state *state);
static bool import_buffers(struct js_plugin_state *state);
static bool launch_node(struct js_plugin_state *state, const char *script_path);
static bool run_webgl_bridge(struct js_plugin_state *state);
static bool run_standalone_render(struct js_plugin_state *state);

// ============================================================================
// SIGNAL HANDLER
// ============================================================================

static void signal_handler(int sig) {
    fprintf(stderr, "[JS-LOADER] Received signal %d, shutting down...\n", sig);
    g_running = false;
}

// ============================================================================
// IPC FUNCTIONS
// ============================================================================

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
    if (n <= 0) return n;

    *fd_count = 0;
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    while (cmsg != NULL) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int num_fds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int *received_fds = (int *)CMSG_DATA(cmsg);
            for (int i = 0; i < num_fds && *fd_count < 32; i++) {
                fds[(*fd_count)++] = received_fds[i];
            }
        }
        cmsg = CMSG_NXTHDR(&msg, cmsg);
    }
    return n;
}

// Send frame notification to compositor
static void send_frame_notification(int ipc_fd, int buffer_idx) {
    // This must match your nano_ipc.h notification structure
    struct {
        uint32_t type;
        int32_t index;
    } __attribute__((packed)) notification = {
        .type = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT,
        .index = buffer_idx
    };
    
    ssize_t written = write(ipc_fd, &notification, sizeof(notification));
    if (written != sizeof(notification)) {
        fprintf(stderr, "[JS-LOADER] Failed to send frame notification: %s\n", strerror(errno));
    }
}

// ============================================================================
// EGL/GL INITIALIZATION
// ============================================================================

static bool init_egl(struct js_plugin_state *state) {
    fprintf(stderr, "[JS-LOADER] Initializing EGL via GBM...\n");
    
    state->drm_fd = open("/dev/dri/renderD128", O_RDWR);
    if (state->drm_fd < 0) {
        fprintf(stderr, "[JS-LOADER] Failed to open render node: %s\n", strerror(errno));
        return false;
    }
    
    state->gbm = gbm_create_device(state->drm_fd);
    if (!state->gbm) {
        fprintf(stderr, "[JS-LOADER] Failed to create GBM device\n");
        return false;
    }
    
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = 
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    
    if (eglGetPlatformDisplayEXT) {
        state->egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, state->gbm, NULL);
    } else {
        state->egl_display = eglGetDisplay((EGLNativeDisplayType)state->gbm);
    }
    
    if (state->egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "[JS-LOADER] Failed to get EGL display\n");
        return false;
    }
    
    EGLint major, minor;
    if (!eglInitialize(state->egl_display, &major, &minor)) {
        fprintf(stderr, "[JS-LOADER] Failed to initialize EGL\n");
        return false;
    }
    
    EGLConfig config;
    EGLint config_count;
    EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    if (!eglChooseConfig(state->egl_display, config_attribs, &config, 1, &config_count) || config_count == 0) {
        fprintf(stderr, "[JS-LOADER] Failed to choose EGL config\n");
        return false;
    }
    
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    state->egl_context = eglCreateContext(state->egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (state->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "[JS-LOADER] Failed to create EGL context\n");
        return false;
    }
    
    // Surfaceless context
    if (!eglMakeCurrent(state->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, state->egl_context)) {
        fprintf(stderr, "[JS-LOADER] Failed to make context current\n");
        return false;
    }
    
    fprintf(stderr, "[JS-LOADER] ✅ EGL initialized: %s\n", glGetString(GL_RENDERER));
    return true;
}

static bool receive_buffers(struct js_plugin_state *state) {
    fprintf(stderr, "[JS-LOADER] Waiting for compositor to send DMA-BUFs on fd %d...\n", state->ipc_fd);
    
    ipc_event_t event;
    int fds[32];
    int fd_count = 0;
    
    memset(&event, 0, sizeof(event));
    memset(fds, -1, sizeof(fds));
    
    ssize_t n = read_ipc_with_fds(state->ipc_fd, &event, fds, &fd_count);
    
    if (n < 0) {
        fprintf(stderr, "[JS-LOADER] ❌ recvmsg failed: %s (errno=%d)\n", strerror(errno), errno);
        return false;
    }
    
    if (n == 0) {
        fprintf(stderr, "[JS-LOADER] ❌ Connection closed by compositor (n=0)\n");
        return false;
    }
    
    fprintf(stderr, "[JS-LOADER] Received %zd bytes, event type=%d, fd_count=%d\n", 
            n, event.type, fd_count);
    
    if (event.type != IPC_EVENT_TYPE_BUFFER_ALLOCATED) {
        fprintf(stderr, "[JS-LOADER] ❌ Wrong event type: got %d, expected %d\n", 
                event.type, IPC_EVENT_TYPE_BUFFER_ALLOCATED);
        return false;
    }
    
    if (fd_count != 2) {
        fprintf(stderr, "[JS-LOADER] ❌ Wrong FD count: got %d, expected 2\n", fd_count);
        for (int i = 0; i < fd_count; i++) {
            fprintf(stderr, "[JS-LOADER]   FD[%d] = %d\n", i, fds[i]);
        }
        return false;
    }
    
    ipc_multi_buffer_info_t *info = &event.data.multi_buffer_info;
    fprintf(stderr, "[JS-LOADER] ✅ Buffer info: count=%u\n", info->count);
    
    for (int i = 0; i < 2; i++) {
        state->dmabuf_fds[i] = fds[i];
        state->width = info->buffers[i].width;
        state->height = info->buffers[i].height;
        state->strides[i] = info->buffers[i].stride;
        state->formats[i] = info->buffers[i].format;
        state->modifiers[i] = info->buffers[i].modifier;
        
        fprintf(stderr, "[JS-LOADER] ✅ Buffer %d: fd=%d %ux%u stride=%u format=0x%x\n",
                i, fds[i], state->width, state->height, state->strides[i], state->formats[i]);
    }
    
    return true;
}

static bool import_buffers(struct js_plugin_state *state) {
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
        fprintf(stderr, "[JS-LOADER] ❌ Required EGL extensions not available\n");
        return false;
    }
    
    for (int i = 0; i < 2; i++) {
        EGLint attribs[] = {
            EGL_WIDTH, state->width,
            EGL_HEIGHT, state->height,
            EGL_LINUX_DRM_FOURCC_EXT, state->formats[i],
            EGL_DMA_BUF_PLANE0_FD_EXT, state->dmabuf_fds[i],
            EGL_DMA_BUF_PLANE0_PITCH_EXT, state->strides[i],
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
            EGL_NONE
        };
        
        state->egl_images[i] = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT,
                                                  EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
        if (state->egl_images[i] == EGL_NO_IMAGE_KHR) {
            fprintf(stderr, "[JS-LOADER] ❌ Failed to create EGL image %d (error: 0x%x)\n", 
                    i, eglGetError());
            return false;
        }
        
        glGenTextures(1, &state->fbo_textures[i]);
        glBindTexture(GL_TEXTURE_2D, state->fbo_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_images[i]);
        
        GLenum gl_error = glGetError();
        if (gl_error != GL_NO_ERROR) {
            fprintf(stderr, "[JS-LOADER] ❌ GL error after EGLImageTargetTexture2D: 0x%x\n", gl_error);
            return false;
        }
        
        glGenFramebuffers(1, &state->fbos[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, state->fbos[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state->fbo_textures[i], 0);
        
        GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fbo_status != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "[JS-LOADER] ❌ FBO %d incomplete: 0x%x\n", i, fbo_status);
            return false;
        }
        
        fprintf(stderr, "[JS-LOADER] ✅ Imported buffer %d: texture=%u fbo=%u\n",
                i, state->fbo_textures[i], state->fbos[i]);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

// ============================================================================
// STANDALONE SHADER RENDERING (when Node.js is not needed)
// ============================================================================

static const char *vertex_shader_src = 
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

static const char *fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform float u_time;\n"
    "uniform vec2 u_resolution;\n"
    "\n"
    "void main() {\n"
    "    vec2 uv = v_texcoord;\n"
    "    vec2 p = uv * 2.0 - 1.0;\n"
    "    p.x *= u_resolution.x / u_resolution.y;\n"
    "    \n"
    "    float t = u_time * 0.5;\n"
    "    \n"
    "    // Plasma effect\n"
    "    float v1 = sin(p.x * 5.0 + t);\n"
    "    float v2 = sin(5.0 * (p.x * sin(t / 2.0) + p.y * cos(t / 3.0)) + t);\n"
    "    float v3 = sin(sqrt(p.x * p.x + p.y * p.y) * 8.0 + t);\n"
    "    float v = v1 + v2 + v3;\n"
    "    \n"
    "    vec3 color = vec3(\n"
    "        sin(v * 3.14159 + 0.0) * 0.5 + 0.5,\n"
    "        sin(v * 3.14159 + 2.094) * 0.5 + 0.5,\n"
    "        sin(v * 3.14159 + 4.188) * 0.5 + 0.5\n"
    "    );\n"
    "    \n"
    "    gl_FragColor = vec4(color, 1.0);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "[JS-LOADER] Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static bool init_standalone_shaders(struct js_plugin_state *state) {
    fprintf(stderr, "[JS-LOADER] Initializing standalone shaders...\n");
    
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    
    if (!vs || !fs) {
        return false;
    }
    
    state->shader_program = glCreateProgram();
    glAttachShader(state->shader_program, vs);
    glAttachShader(state->shader_program, fs);
    glBindAttribLocation(state->shader_program, 0, "a_position");
    glBindAttribLocation(state->shader_program, 1, "a_texcoord");
    glLinkProgram(state->shader_program);
    
    GLint linked;
    glGetProgramiv(state->shader_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(state->shader_program, sizeof(log), NULL, log);
        fprintf(stderr, "[JS-LOADER] Program link error: %s\n", log);
        return false;
    }
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    state->u_time = glGetUniformLocation(state->shader_program, "u_time");
    state->u_resolution = glGetUniformLocation(state->shader_program, "u_resolution");
    
    // Create fullscreen quad
    float vertices[] = {
        // positions   // texcoords
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    
    glGenBuffers(1, &state->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    fprintf(stderr, "[JS-LOADER] ✅ Standalone shaders initialized\n");
    return true;
}

static void render_standalone_frame(struct js_plugin_state *state, int buffer_idx, float time) {
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbos[buffer_idx]);
    glViewport(0, 0, state->width, state->height);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(state->shader_program);
    glUniform1f(state->u_time, time);
    glUniform2f(state->u_resolution, (float)state->width, (float)state->height);
    
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glFinish();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static bool run_standalone_render(struct js_plugin_state *state) {
    fprintf(stderr, "[JS-LOADER] Running standalone render loop\n");
    
    if (!init_standalone_shaders(state)) {
        fprintf(stderr, "[JS-LOADER] ❌ Failed to initialize shaders\n");
        return false;
    }
    
    struct timespec start_time, current_time, frame_start, frame_end;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    const int target_fps = 60;
    const long frame_ns = 1000000000L / target_fps;
    
    int current_buffer = 0;
    int frame_count = 0;
    
    while (g_running) {
        clock_gettime(CLOCK_MONOTONIC, &frame_start);
        
        float elapsed = (frame_start.tv_sec - start_time.tv_sec) + 
                       (frame_start.tv_nsec - start_time.tv_nsec) / 1e9f;
        
        // Render to current buffer
        render_standalone_frame(state, current_buffer, elapsed);
        
        // Notify compositor
        send_frame_notification(state->ipc_fd, current_buffer);
        
        // Swap buffers
        current_buffer = 1 - current_buffer;
        frame_count++;
        
        // Log FPS every 60 frames
        if (frame_count % 60 == 0) {
            fprintf(stderr, "[JS-LOADER] Frame %d, time: %.1fs\n", frame_count, elapsed);
        }
        
        // Frame timing
        clock_gettime(CLOCK_MONOTONIC, &frame_end);
        long frame_time = (frame_end.tv_sec - frame_start.tv_sec) * 1000000000L +
                         (frame_end.tv_nsec - frame_start.tv_nsec);
        
        if (frame_time < frame_ns) {
            struct timespec sleep_time = { 
                .tv_sec = 0, 
                .tv_nsec = frame_ns - frame_time 
            };
            nanosleep(&sleep_time, NULL);
        }
    }
    
    return true;
}

// ============================================================================
// NODE.JS WEBGL BRIDGE
// ============================================================================

static bool map_dmabuf(int fd, uint32_t size, struct mapped_buffer *out) {
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "[JS-LOADER] Failed to mmap DMA-BUF: %s\n", strerror(errno));
        return false;
    }
    out->data = data;
    out->size = size;
    return true;
}

static void unmap_dmabuf(struct mapped_buffer *buf) {
    if (buf->data && buf->data != MAP_FAILED) {
        munmap(buf->data, buf->size);
        buf->data = NULL;
    }
}

static bool launch_node(struct js_plugin_state *state, const char *script_path) {
    fprintf(stderr, "[JS-LOADER] Launching Node.js: %s\n", script_path);
    
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        fprintf(stderr, "[JS-LOADER] Failed to create pipes\n");
        return false;
    }
    
    state->node_pid = fork();
    if (state->node_pid < 0) {
        fprintf(stderr, "[JS-LOADER] Fork failed\n");
        return false;
    }
    
    if (state->node_pid == 0) {
        // Child: exec Node.js
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        // Pass DMA-BUF info via environment variables
        char env_buf[256];
        
        snprintf(env_buf, sizeof(env_buf), "%d", state->dmabuf_fds[0]);
        setenv("DMABUF_FD_0", env_buf, 1);
        snprintf(env_buf, sizeof(env_buf), "%d", state->dmabuf_fds[1]);
        setenv("DMABUF_FD_1", env_buf, 1);
        
        snprintf(env_buf, sizeof(env_buf), "%u", state->width);
        setenv("DMABUF_WIDTH", env_buf, 1);
        snprintf(env_buf, sizeof(env_buf), "%u", state->height);
        setenv("DMABUF_HEIGHT", env_buf, 1);
        
        snprintf(env_buf, sizeof(env_buf), "%u", state->strides[0]);
        setenv("DMABUF_STRIDE_0", env_buf, 1);
        snprintf(env_buf, sizeof(env_buf), "%u", state->strides[1]);
        setenv("DMABUF_STRIDE_1", env_buf, 1);
        
        snprintf(env_buf, sizeof(env_buf), "0x%x", state->formats[0]);
        setenv("DMABUF_FORMAT", env_buf, 1);
        
        snprintf(env_buf, sizeof(env_buf), "%u", state->fbo_textures[0]);
        setenv("GL_TEXTURE_0", env_buf, 1);
        snprintf(env_buf, sizeof(env_buf), "%u", state->fbo_textures[1]);
        setenv("GL_TEXTURE_1", env_buf, 1);
        
        snprintf(env_buf, sizeof(env_buf), "%u", state->fbos[0]);
        setenv("GL_FBO_0", env_buf, 1);
        snprintf(env_buf, sizeof(env_buf), "%u", state->fbos[1]);
        setenv("GL_FBO_1", env_buf, 1);
        
        snprintf(env_buf, sizeof(env_buf), "%p", (void*)state->egl_display);
        setenv("EGL_DISPLAY", env_buf, 1);
        snprintf(env_buf, sizeof(env_buf), "%p", (void*)state->egl_context);
        setenv("EGL_CONTEXT", env_buf, 1);
        
        snprintf(env_buf, sizeof(env_buf), "%d", state->ipc_fd);
        setenv("IPC_FD", env_buf, 1);
        
        execlp("node", "node", script_path, NULL);
        fprintf(stderr, "[JS-LOADER] exec failed: %s\n", strerror(errno));
        _exit(1);
    }
    
    // Parent - don't wait, we'll process frames
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    state->node_stdin = stdin_pipe[1];
    state->node_stdout = stdout_pipe[0];
    
    fprintf(stderr, "[JS-LOADER] ✅ Node.js launched with PID %d\n", state->node_pid);
    return true;
}

static bool run_webgl_bridge(struct js_plugin_state *state) {
    fprintf(stderr, "[JS-LOADER] Starting WebGL bridge...\n");
    
    // Calculate buffer sizes
    uint32_t buffer_size = state->width * state->height * 4; // RGBA
    
    // Map both DMA-BUFs for CPU write access
    struct mapped_buffer mapped[2] = {0};
    
    for (int i = 0; i < 2; i++) {
        uint32_t actual_size = state->strides[i] * state->height;
        
        if (!map_dmabuf(state->dmabuf_fds[i], actual_size, &mapped[i])) {
            fprintf(stderr, "[JS-LOADER] ❌ Failed to map DMA-BUF %d\n", i);
            for (int j = 0; j < i; j++) {
                unmap_dmabuf(&mapped[j]);
            }
            return false;
        }
        fprintf(stderr, "[JS-LOADER] ✅ Mapped DMA-BUF %d: %p (%u bytes, stride=%u)\n", 
                i, mapped[i].data, actual_size, state->strides[i]);
    }
    
    // Allocate buffer for receiving pixel data from Node
    uint8_t *pixel_buffer = malloc(buffer_size);
    if (!pixel_buffer) {
        fprintf(stderr, "[JS-LOADER] ❌ Failed to allocate pixel buffer\n");
        unmap_dmabuf(&mapped[0]);
        unmap_dmabuf(&mapped[1]);
        return false;
    }
    
    // Header buffer: buffer_index(4) + width(4) + height(4)
    uint8_t header[12];
    int frame_count = 0;
    
    fprintf(stderr, "[JS-LOADER] Waiting for frames from Node.js...\n");
    
    while (g_running) {
        // Check if Node.js is still running
        int status;
        pid_t result = waitpid(state->node_pid, &status, WNOHANG);
        if (result > 0) {
            fprintf(stderr, "[JS-LOADER] Node.js exited (status=%d)\n", WEXITSTATUS(status));
            break;
        }
        
        // Read header
        ssize_t header_read = 0;
        while (header_read < 12 && g_running) {
            ssize_t n = read(state->node_stdout, header + header_read, 12 - header_read);
            if (n <= 0) {
                if (n == 0) {
                    fprintf(stderr, "[JS-LOADER] Node.js closed connection\n");
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    fprintf(stderr, "[JS-LOADER] Read error: %s\n", strerror(errno));
                }
                goto cleanup;
            }
            header_read += n;
        }
        
        if (!g_running) break;
        
        uint32_t buffer_idx = *(uint32_t*)&header[0];
        uint32_t width = *(uint32_t*)&header[4];
        uint32_t height = *(uint32_t*)&header[8];
        
        if (buffer_idx > 1 || width != state->width || height != state->height) {
            fprintf(stderr, "[JS-LOADER] Invalid frame header: buf=%u, %ux%u (expected %ux%u)\n", 
                    buffer_idx, width, height, state->width, state->height);
            continue;
        }
        
        // Read pixel data
        uint32_t expected = width * height * 4;
        ssize_t pixels_read = 0;
        while (pixels_read < expected && g_running) {
            ssize_t n = read(state->node_stdout, pixel_buffer + pixels_read, expected - pixels_read);
            if (n <= 0) {
                fprintf(stderr, "[JS-LOADER] Failed to read pixels\n");
                goto cleanup;
            }
            pixels_read += n;
        }
        
        if (!g_running) break;
        
        // Copy pixels to DMA-BUF (handling stride and Y-flip)
        // WebGL has origin at bottom-left, DMA-BUF typically top-left
        uint8_t *dst = (uint8_t*)mapped[buffer_idx].data;
        uint8_t *src = pixel_buffer;
        uint32_t row_bytes = width * 4;
        
        for (uint32_t y = 0; y < height; y++) {
            uint32_t src_row = height - 1 - y;  // Flip Y
            memcpy(dst + y * state->strides[buffer_idx], 
                   src + src_row * row_bytes, 
                   row_bytes);
        }
        
        // Notify compositor that frame is ready
        send_frame_notification(state->ipc_fd, buffer_idx);
        
        frame_count++;
        if (frame_count % 60 == 0) {
            fprintf(stderr, "[JS-LOADER] Processed %d frames from Node.js\n", frame_count);
        }
    }
    
cleanup:
    free(pixel_buffer);
    unmap_dmabuf(&mapped[0]);
    unmap_dmabuf(&mapped[1]);
    
    return true;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, "js_plugin_host", 0, 0, 0);
    
    // Set up signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ipc_fd> <script.js>\n", argv[0]);
        fprintf(stderr, "       Use 'standalone' as script to run built-in demo\n");
        return 1;
    }
    
    struct js_plugin_state state = {0};
    state.ipc_fd = atoi(argv[1]);
    const char *script_path = argv[2];
    
    // Initialize dmabuf_fds to -1
    state.dmabuf_fds[0] = -1;
    state.dmabuf_fds[1] = -1;
    state.node_pid = -1;
    
    fprintf(stderr, "[JS-LOADER] Starting JavaScript plugin host...\n");
    fprintf(stderr, "[JS-LOADER] IPC FD: %d, Script: %s\n", state.ipc_fd, script_path);
    
    // Step 1: Receive buffers from compositor
    if (!receive_buffers(&state)) {
        fprintf(stderr, "[JS-LOADER] FATAL: Failed to receive buffers\n");
        return 1;
    }
    
    // Step 2: Initialize EGL
    if (!init_egl(&state)) {
        fprintf(stderr, "[JS-LOADER] FATAL: Failed to initialize EGL\n");
        return 1;
    }
    
    // Step 3: Import buffers into EGL/GL
    if (!import_buffers(&state)) {
        fprintf(stderr, "[JS-LOADER] FATAL: Failed to import buffers\n");
        return 1;
    }
    
 // In main(), replace the bridge call with just waiting:

// Step 4: Launch Node.js - it will render directly to DMA-BUFs!
if (!launch_node(&state, script_path)) {
    fprintf(stderr, "[JS-LOADER] FATAL: Failed to launch Node.js\n");
    return 1;
}

// Node.js renders directly to DMA-BUFs via native addon
// Just wait for it to finish
fprintf(stderr, "[JS-LOADER] Node.js rendering directly to DMA-BUFs (zero-copy)\n");

int status;
waitpid(state.node_pid, &status, 0);
fprintf(stderr, "[JS-LOADER] Node.js exited with status %d\n", WEXITSTATUS(status));
    
    // Cleanup
    fprintf(stderr, "[JS-LOADER] Cleaning up...\n");
    
    // Clean up GL resources
    if (state.shader_program) {
        glDeleteProgram(state.shader_program);
    }
    if (state.vbo) {
        glDeleteBuffers(1, &state.vbo);
    }
    
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    
    for (int i = 0; i < 2; i++) {
        if (state.fbos[i]) {
            glDeleteFramebuffers(1, &state.fbos[i]);
        }
        if (state.fbo_textures[i]) {
            glDeleteTextures(1, &state.fbo_textures[i]);
        }
        if (state.egl_images[i] && eglDestroyImageKHR) {
            eglDestroyImageKHR(state.egl_display, state.egl_images[i]);
        }
        if (state.dmabuf_fds[i] >= 0) {
            close(state.dmabuf_fds[i]);
        }
    }
    
    if (state.egl_context != EGL_NO_CONTEXT) {
        eglDestroyContext(state.egl_display, state.egl_context);
    }
    if (state.egl_display != EGL_NO_DISPLAY) {
        eglTerminate(state.egl_display);
    }
    if (state.gbm) {
        gbm_device_destroy(state.gbm);
    }
    if (state.drm_fd >= 0) {
        close(state.drm_fd);
    }
    
    fprintf(stderr, "[JS-LOADER] Cleanup complete\n");
    return 0;
}
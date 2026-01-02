// wayfire_capture_wrapper.c - With Virtual Pointer/Keyboard Support
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <time.h>

// memfd_create may need these on older glibc
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif

// Fallback for systems without memfd_create in libc
#ifndef __NR_memfd_create
#ifdef __x86_64__
#define __NR_memfd_create 319
#elif defined(__i386__)
#define __NR_memfd_create 356
#elif defined(__aarch64__)
#define __NR_memfd_create 279
#else
#error "Unknown architecture for memfd_create syscall number"
#endif
#endif

static inline int my_memfd_create(const char *name, unsigned int flags) {
    return syscall(__NR_memfd_create, name, flags);
}

#include <gbm.h>
#include <xf86drm.h>
#include <drm_fourcc.h>
#include <wayland-client.h>

#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

// ============================================================================
// IPC Definitions (must match nano compositor)
// ============================================================================

// Notification types (wrapper -> compositor)
typedef enum {
    IPC_NOTIFICATION_TYPE_CRASHED = 0,
    IPC_NOTIFICATION_TYPE_FRAME_SUBMIT = 1,
    IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT = 2,
    IPC_NOTIFICATION_TYPE_PLUGIN_READY = 20,
} ipc_notification_type_t;

typedef struct {
    uint32_t width; uint32_t height; uint32_t format; uint64_t modifier;
    uint32_t num_planes; uint32_t stride[4]; uint32_t offset[4];
} ipc_dmabuf_multiplane_info_t;

typedef struct {
    char plugin_name[64]; char plugin_version[32]; uint32_t api_version;
    bool supports_dmabuf; bool supports_window_management;
} ipc_plugin_ready_t;

typedef struct {
    ipc_notification_type_t type; uint64_t timestamp_ns;
    union {
        ipc_dmabuf_multiplane_info_t dmabuf_multiplane_info;
        ipc_plugin_ready_t plugin_ready;
    } data;
} ipc_notification_t;

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

// ============================================================================
// Globals
// ============================================================================

static volatile sig_atomic_t g_running = 1;

typedef struct {
    pid_t wayfire_pid;
    char wayfire_socket[128];
    char render_node[64]; 

    // Wayland Core
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_output *output;
    struct wl_seat *seat;
    
    // Screencopy
    struct zwlr_screencopy_manager_v1 *screencopy_manager;
    struct zwp_linux_dmabuf_v1 *linux_dmabuf;

    // Virtual Input
    struct zwlr_virtual_pointer_manager_v1 *vpointer_manager;
    struct zwlr_virtual_pointer_v1 *vpointer;
    struct zwp_virtual_keyboard_manager_v1 *vkeyboard_manager;
    struct zwp_virtual_keyboard_v1 *vkeyboard;

    // GBM
    int drm_fd;
    struct gbm_device *gbm_dev;

    // State - keep previous bo alive!
    struct zwlr_screencopy_frame_v1 *current_frame;
    struct gbm_bo *current_bo;
    struct gbm_bo *previous_bo;
    struct wl_buffer *wl_buf;
    struct wl_buffer *previous_wl_buf;

    uint32_t req_width, req_height, req_format;
    uint32_t output_width, output_height;  // Actual output dimensions
    
    int event_fd;
    int notify_fd;
    uint32_t frames_captured;
    uint32_t frames_sent;
    
    bool input_initialized;
} capture_context_t;

static capture_context_t g_ctx = {0};

// ============================================================================
// Signal Handler
// ============================================================================

static void signal_handler(int sig) { 
    fprintf(stderr, "[wayfire-wrapper] Signal %d received, shutting down\n", sig);
    g_running = 0; 
}

// ============================================================================
// IPC Helpers
// ============================================================================

static int send_dmabuf_fds(int socket, int *fds, int num_fds) {
    if (num_fds <= 0 || num_fds > 4) return -1;
    struct msghdr msg = {0};
    struct iovec iov[1];
    char buf[1] = {'F'};
    char control[CMSG_SPACE(sizeof(int) * 4)];
    memset(control, 0, sizeof(control));
    iov[0].iov_base = buf; iov[0].iov_len = 1;
    msg.msg_iov = iov; msg.msg_iovlen = 1;
    msg.msg_control = control; msg.msg_controllen = CMSG_SPACE(sizeof(int) * num_fds);
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * num_fds);
    cmsg->cmsg_level = SOL_SOCKET; cmsg->cmsg_type = SCM_RIGHTS;
    memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * num_fds);
    ssize_t ret = sendmsg(socket, &msg, 0);
    if (ret < 0) {
        fprintf(stderr, "[wayfire-wrapper] sendmsg failed: %s\n", strerror(errno));
    }
    return ret;
}

static void send_notification(ipc_notification_t *notif) {
    if (g_ctx.notify_fd >= 0) {
        ssize_t ret = write(g_ctx.notify_fd, notif, sizeof(*notif));
        if (ret < 0) {
            fprintf(stderr, "[wayfire-wrapper] Failed to send notification: %s\n", strerror(errno));
        }
    }
}

// ============================================================================
// Virtual Input Functions
// ============================================================================

static void forward_mouse_motion(double dx, double dy, uint32_t time) {
    if (!g_ctx.vpointer) return;
    
    zwlr_virtual_pointer_v1_motion(g_ctx.vpointer, time, 
        wl_fixed_from_double(dx), wl_fixed_from_double(dy));
    zwlr_virtual_pointer_v1_frame(g_ctx.vpointer);
    wl_display_flush(g_ctx.display);
}

static void forward_mouse_motion_absolute(double x, double y, uint32_t width, uint32_t height, uint32_t time) {
    if (!g_ctx.vpointer) return;
    
    // Convert to device coordinates (0 to UINT32_MAX range)
    uint32_t abs_x = (uint32_t)((x / width) * UINT32_MAX);
    uint32_t abs_y = (uint32_t)((y / height) * UINT32_MAX);
    
    zwlr_virtual_pointer_v1_motion_absolute(g_ctx.vpointer, time, abs_x, abs_y,
        g_ctx.output_width ? g_ctx.output_width : 1920,
        g_ctx.output_height ? g_ctx.output_height : 1080);
    zwlr_virtual_pointer_v1_frame(g_ctx.vpointer);
    wl_display_flush(g_ctx.display);
}

static void forward_mouse_button(uint32_t button, uint32_t state, uint32_t time) {
    if (!g_ctx.vpointer) return;
    
    zwlr_virtual_pointer_v1_button(g_ctx.vpointer, time, button, state);
    zwlr_virtual_pointer_v1_frame(g_ctx.vpointer);
    wl_display_flush(g_ctx.display);
}

static void forward_mouse_axis(uint32_t axis, double value, uint32_t time) {
    if (!g_ctx.vpointer) return;
    
    // axis: 0 = vertical scroll, 1 = horizontal scroll
    zwlr_virtual_pointer_v1_axis(g_ctx.vpointer, time, axis, wl_fixed_from_double(value));
    zwlr_virtual_pointer_v1_frame(g_ctx.vpointer);
    wl_display_flush(g_ctx.display);
}

static void forward_key(uint32_t key, uint32_t state, uint32_t time) {
    if (!g_ctx.vkeyboard) return;
    
    zwp_virtual_keyboard_v1_key(g_ctx.vkeyboard, time, key, state);
    wl_display_flush(g_ctx.display);
}

static void forward_modifiers(uint32_t mods_depressed, uint32_t mods_latched, 
                              uint32_t mods_locked, uint32_t group) {
    if (!g_ctx.vkeyboard) return;
    
    zwp_virtual_keyboard_v1_modifiers(g_ctx.vkeyboard, mods_depressed, 
                                       mods_latched, mods_locked, group);
    wl_display_flush(g_ctx.display);
}

// Process incoming input events from compositor
static void process_input_event(ipc_input_event_t *event) {
    switch (event->type) {
        case IPC_INPUT_MOUSE_MOTION:
            forward_mouse_motion(event->data.motion.dx, event->data.motion.dy, event->time_ms);
            break;
            
        case IPC_INPUT_MOUSE_MOTION_ABSOLUTE:
            forward_mouse_motion_absolute(
                event->data.motion_abs.x, event->data.motion_abs.y,
                event->data.motion_abs.width, event->data.motion_abs.height,
                event->time_ms);
            break;
            
        case IPC_INPUT_MOUSE_BUTTON:
            forward_mouse_button(event->data.button.button, event->data.button.state, event->time_ms);
            break;
            
        case IPC_INPUT_MOUSE_AXIS:
            forward_mouse_axis(event->data.axis.axis, event->data.axis.value, event->time_ms);
            break;
            
        case IPC_INPUT_KEY:
            forward_key(event->data.key.key, event->data.key.state, event->time_ms);
            break;
            
        case IPC_INPUT_MODIFIERS:
            forward_modifiers(event->data.modifiers.mods_depressed,
                            event->data.modifiers.mods_latched,
                            event->data.modifiers.mods_locked,
                            event->data.modifiers.group);
            break;
            
        default:
            fprintf(stderr, "[wayfire-wrapper] Unknown input event type: %d\n", event->type);
            break;
    }
}

// ============================================================================
// GBM Initialization
// ============================================================================

static bool init_gbm(void) {
    const char *cards[] = {"/dev/dri/renderD128", "/dev/dri/renderD129", NULL};
    for (int i = 0; cards[i]; i++) {
        g_ctx.drm_fd = open(cards[i], O_RDWR | O_CLOEXEC);
        if (g_ctx.drm_fd >= 0) {
            strncpy(g_ctx.render_node, cards[i], sizeof(g_ctx.render_node) - 1);
            fprintf(stderr, "[wayfire-wrapper] Using DRM device: %s\n", cards[i]);
            break;
        }
    }
    if (g_ctx.drm_fd < 0) {
        fprintf(stderr, "[wayfire-wrapper] Failed to open any DRM render device\n");
        return false;
    }
    g_ctx.gbm_dev = gbm_create_device(g_ctx.drm_fd);
    if (!g_ctx.gbm_dev) {
        fprintf(stderr, "[wayfire-wrapper] Failed to create GBM device\n");
        return false;
    }
    fprintf(stderr, "[wayfire-wrapper] GBM device created successfully\n");
    return true;
}

// ============================================================================
// Frame Cleanup
// ============================================================================

static void cleanup_frame_only(void) {
    if (g_ctx.current_frame) { 
        zwlr_screencopy_frame_v1_destroy(g_ctx.current_frame); 
        g_ctx.current_frame = NULL; 
    }
}

static void cleanup_previous_frame(void) {
    if (g_ctx.previous_wl_buf) { 
        wl_buffer_destroy(g_ctx.previous_wl_buf); 
        g_ctx.previous_wl_buf = NULL; 
    }
    if (g_ctx.previous_bo) { 
        gbm_bo_destroy(g_ctx.previous_bo); 
        g_ctx.previous_bo = NULL; 
    }
}

// ============================================================================
// Screencopy Protocol Handlers
// ============================================================================

static void sc_handle_buffer(void *data, struct zwlr_screencopy_frame_v1 *frame,
                             uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
    fprintf(stderr, "[wayfire-wrapper] sc_handle_buffer: %ux%u fmt=0x%x stride=%u\n", 
            width, height, format, stride);
}

static void sc_handle_flags(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags) {
    fprintf(stderr, "[wayfire-wrapper] sc_handle_flags: 0x%x\n", flags);
}

static void sc_handle_ready(void *data, struct zwlr_screencopy_frame_v1 *frame,
                            uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
    fprintf(stderr, "[wayfire-wrapper] sc_handle_ready: Frame %u ready!\n", g_ctx.frames_captured);
    
    int fd = gbm_bo_get_fd(g_ctx.current_bo);
    if (fd < 0) {
        fprintf(stderr, "[wayfire-wrapper] Failed to get GBM BO fd: %s\n", strerror(errno));
        cleanup_frame_only();
        return;
    }

    ipc_notification_t notif = {0};
    notif.type = IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    notif.timestamp_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    notif.data.dmabuf_multiplane_info.width = gbm_bo_get_width(g_ctx.current_bo);
    notif.data.dmabuf_multiplane_info.height = gbm_bo_get_height(g_ctx.current_bo);
    notif.data.dmabuf_multiplane_info.format = gbm_bo_get_format(g_ctx.current_bo);
    notif.data.dmabuf_multiplane_info.modifier = gbm_bo_get_modifier(g_ctx.current_bo);
    notif.data.dmabuf_multiplane_info.num_planes = gbm_bo_get_plane_count(g_ctx.current_bo);

    for (int i = 0; i < (int)notif.data.dmabuf_multiplane_info.num_planes && i < 4; i++) {
        notif.data.dmabuf_multiplane_info.stride[i] = gbm_bo_get_stride_for_plane(g_ctx.current_bo, i);
        notif.data.dmabuf_multiplane_info.offset[i] = gbm_bo_get_offset(g_ctx.current_bo, i);
    }

    // Store output dimensions for absolute motion
    g_ctx.output_width = notif.data.dmabuf_multiplane_info.width;
    g_ctx.output_height = notif.data.dmabuf_multiplane_info.height;

    fprintf(stderr, "[wayfire-wrapper] Sending frame: %ux%u fmt=0x%x mod=0x%lx planes=%u fd=%d\n",
            notif.data.dmabuf_multiplane_info.width,
            notif.data.dmabuf_multiplane_info.height,
            notif.data.dmabuf_multiplane_info.format,
            notif.data.dmabuf_multiplane_info.modifier,
            notif.data.dmabuf_multiplane_info.num_planes,
            fd);

    send_notification(&notif);

    int fds[1] = {fd};
    if (send_dmabuf_fds(g_ctx.notify_fd, fds, 1) > 0) {
        g_ctx.frames_sent++;
        fprintf(stderr, "[wayfire-wrapper] Frame %u sent successfully\n", g_ctx.frames_sent);
    }

    close(fd);
    
    cleanup_previous_frame();
    g_ctx.previous_bo = g_ctx.current_bo;
    g_ctx.previous_wl_buf = g_ctx.wl_buf;
    g_ctx.current_bo = NULL;
    g_ctx.wl_buf = NULL;
    
    cleanup_frame_only();
    g_ctx.frames_captured++;
}

static void sc_handle_failed(void *data, struct zwlr_screencopy_frame_v1 *frame) {
    fprintf(stderr, "[wayfire-wrapper] Screencopy FAILED!\n");
    cleanup_frame_only();
}

static void sc_handle_linux_dmabuf(void *data, struct zwlr_screencopy_frame_v1 *frame,
                                   uint32_t format, uint32_t width, uint32_t height) {
    fprintf(stderr, "[wayfire-wrapper] sc_handle_linux_dmabuf: %ux%u fmt=0x%x\n", 
            width, height, format);
    g_ctx.req_width = width;
    g_ctx.req_height = height;
    g_ctx.req_format = format;
}

static void sc_handle_buffer_done(void *data, struct zwlr_screencopy_frame_v1 *frame) {
    fprintf(stderr, "[wayfire-wrapper] sc_handle_buffer_done: Allocating %ux%u fmt=0x%x\n",
            g_ctx.req_width, g_ctx.req_height, g_ctx.req_format);
    
    g_ctx.current_bo = gbm_bo_create(g_ctx.gbm_dev, 
                                     g_ctx.req_width, g_ctx.req_height, 
                                     g_ctx.req_format, 
                                     GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
    if (!g_ctx.current_bo) {
        g_ctx.current_bo = gbm_bo_create(g_ctx.gbm_dev, 
                                         g_ctx.req_width, g_ctx.req_height, 
                                         g_ctx.req_format, 
                                         GBM_BO_USE_RENDERING);
    }
    
    if (!g_ctx.current_bo) {
        fprintf(stderr, "[wayfire-wrapper] GBM alloc failed: %s\n", strerror(errno));
        return;
    }
    
    fprintf(stderr, "[wayfire-wrapper] GBM BO created: %p modifier=0x%lx\n", 
            g_ctx.current_bo, gbm_bo_get_modifier(g_ctx.current_bo));

    struct zwp_linux_buffer_params_v1 *params = zwp_linux_dmabuf_v1_create_params(g_ctx.linux_dmabuf);
    int num_planes = gbm_bo_get_plane_count(g_ctx.current_bo);
    uint64_t modifier = gbm_bo_get_modifier(g_ctx.current_bo);
    
    int fds[4] = {-1, -1, -1, -1};

    for (int i = 0; i < num_planes; i++) {
        fds[i] = gbm_bo_get_fd_for_plane(g_ctx.current_bo, i);
        if (fds[i] < 0) {
            fprintf(stderr, "[wayfire-wrapper] Failed to get FD for plane %d\n", i);
            zwp_linux_buffer_params_v1_destroy(params);
            for(int j = 0; j < i; j++) close(fds[j]);
            gbm_bo_destroy(g_ctx.current_bo);
            g_ctx.current_bo = NULL;
            return;
        }

        uint32_t stride = gbm_bo_get_stride_for_plane(g_ctx.current_bo, i);
        uint32_t offset = gbm_bo_get_offset(g_ctx.current_bo, i);
        
        fprintf(stderr, "[wayfire-wrapper] Plane %d: fd=%d stride=%u offset=%u\n", 
                i, fds[i], stride, offset);
        
        zwp_linux_buffer_params_v1_add(params, fds[i], i, offset, stride, 
                                       modifier >> 32, modifier & 0xffffffff);
    }

    g_ctx.wl_buf = zwp_linux_buffer_params_v1_create_immed(params, 
                                                           g_ctx.req_width, g_ctx.req_height, 
                                                           g_ctx.req_format, 0);
    zwp_linux_buffer_params_v1_destroy(params);

    if (!g_ctx.wl_buf) {
        fprintf(stderr, "[wayfire-wrapper] Failed to create wl_buffer\n");
        for (int i = 0; i < num_planes; i++) {
            if (fds[i] >= 0) close(fds[i]);
        }
        gbm_bo_destroy(g_ctx.current_bo);
        g_ctx.current_bo = NULL;
        return;
    }

    fprintf(stderr, "[wayfire-wrapper] wl_buffer created, starting copy\n");
    zwlr_screencopy_frame_v1_copy(frame, g_ctx.wl_buf);

    wl_display_flush(g_ctx.display);

    for (int i = 0; i < num_planes; i++) {
        if (fds[i] >= 0) close(fds[i]);
    }
}

static const struct zwlr_screencopy_frame_v1_listener sc_listener = {
    .buffer = sc_handle_buffer,
    .flags = sc_handle_flags,
    .ready = sc_handle_ready,
    .failed = sc_handle_failed,
    .linux_dmabuf = sc_handle_linux_dmabuf,
    .buffer_done = sc_handle_buffer_done,
};

// ============================================================================
// Registry Handler
// ============================================================================

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    fprintf(stderr, "[wayfire-wrapper] Registry: %s v%u\n", interface, version);
    
    if (strcmp(interface, "wl_output") == 0) {
        if (!g_ctx.output) {
            g_ctx.output = wl_registry_bind(registry, name, &wl_output_interface, 1);
            fprintf(stderr, "[wayfire-wrapper] Bound wl_output: %p\n", g_ctx.output);
        }
    } else if (strcmp(interface, "wl_seat") == 0) {
        if (!g_ctx.seat) {
            g_ctx.seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
            fprintf(stderr, "[wayfire-wrapper] Bound wl_seat: %p\n", g_ctx.seat);
        }
    } else if (strcmp(interface, "zwlr_screencopy_manager_v1") == 0) {
        g_ctx.screencopy_manager = wl_registry_bind(registry, name, 
                                                     &zwlr_screencopy_manager_v1_interface, 
                                                     version < 3 ? version : 3);
        fprintf(stderr, "[wayfire-wrapper] Bound screencopy_manager: %p\n", g_ctx.screencopy_manager);
    } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
        g_ctx.linux_dmabuf = wl_registry_bind(registry, name, 
                                               &zwp_linux_dmabuf_v1_interface, 
                                               version < 3 ? version : 3);
        fprintf(stderr, "[wayfire-wrapper] Bound linux_dmabuf: %p\n", g_ctx.linux_dmabuf);
    } else if (strcmp(interface, "zwlr_virtual_pointer_manager_v1") == 0) {
        g_ctx.vpointer_manager = wl_registry_bind(registry, name, 
                                                   &zwlr_virtual_pointer_manager_v1_interface, 1);
        fprintf(stderr, "[wayfire-wrapper] Bound virtual_pointer_manager: %p\n", g_ctx.vpointer_manager);
    } else if (strcmp(interface, "zwp_virtual_keyboard_manager_v1") == 0) {
        g_ctx.vkeyboard_manager = wl_registry_bind(registry, name, 
                                                    &zwp_virtual_keyboard_manager_v1_interface, 1);
        fprintf(stderr, "[wayfire-wrapper] Bound virtual_keyboard_manager: %p\n", g_ctx.vkeyboard_manager);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = { 
    .global = registry_handle_global, 
    .global_remove = registry_handle_global_remove 
};

// ============================================================================
// Virtual Input Initialization
// ============================================================================

static bool init_virtual_input(void) {
    // Create virtual pointer
    if (g_ctx.vpointer_manager) {
        g_ctx.vpointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
            g_ctx.vpointer_manager, g_ctx.seat);
        if (g_ctx.vpointer) {
            fprintf(stderr, "[wayfire-wrapper] Virtual pointer created: %p\n", g_ctx.vpointer);
        } else {
            fprintf(stderr, "[wayfire-wrapper] Failed to create virtual pointer\n");
        }
    } else {
        fprintf(stderr, "[wayfire-wrapper] No virtual pointer manager available\n");
    }
    
    // Create virtual keyboard
    if (g_ctx.vkeyboard_manager && g_ctx.seat) {
        g_ctx.vkeyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
            g_ctx.vkeyboard_manager, g_ctx.seat);
        if (g_ctx.vkeyboard) {
            fprintf(stderr, "[wayfire-wrapper] Virtual keyboard created: %p\n", g_ctx.vkeyboard);
            
            // Set up keymap (minimal US layout)
            // You may want to use the actual keymap from the compositor
            const char *keymap_str = 
                "xkb_keymap {\n"
                "  xkb_keycodes { include \"evdev+aliases(qwerty)\" };\n"
                "  xkb_types { include \"complete\" };\n"
                "  xkb_compat { include \"complete\" };\n"
                "  xkb_symbols { include \"pc+us\" };\n"
                "};\n";
            
            size_t keymap_size = strlen(keymap_str) + 1;
            int fd = my_memfd_create("keymap", MFD_CLOEXEC);
            if (fd >= 0) {
                if (ftruncate(fd, keymap_size) == 0) {
                    void *map = mmap(NULL, keymap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                    if (map != MAP_FAILED) {
                        memcpy(map, keymap_str, keymap_size);
                        munmap(map, keymap_size);
                        zwp_virtual_keyboard_v1_keymap(g_ctx.vkeyboard, 
                            WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, keymap_size);
                        fprintf(stderr, "[wayfire-wrapper] Virtual keyboard keymap set\n");
                    }
                }
                close(fd);
            }
        } else {
            fprintf(stderr, "[wayfire-wrapper] Failed to create virtual keyboard\n");
        }
    } else {
        fprintf(stderr, "[wayfire-wrapper] No virtual keyboard manager or seat available\n");
    }
    
    g_ctx.input_initialized = (g_ctx.vpointer != NULL);
    return g_ctx.input_initialized;
}

// ============================================================================
// Frame Request
// ============================================================================

static void request_frame(void) {
    if (!g_ctx.screencopy_manager) {
        static int warn_count = 0;
        if (warn_count++ < 3) fprintf(stderr, "[wayfire-wrapper] request_frame: No screencopy_manager\n");
        return;
    }
    if (!g_ctx.output) {
        static int warn_count = 0;
        if (warn_count++ < 3) fprintf(stderr, "[wayfire-wrapper] request_frame: No output\n");
        return;
    }
    if (!g_ctx.linux_dmabuf) {
        static int warn_count = 0;
        if (warn_count++ < 3) fprintf(stderr, "[wayfire-wrapper] request_frame: No linux_dmabuf\n");
        return;
    }
    if (g_ctx.current_frame) {
        return;
    }
    
    static int req_count = 0;
    if (req_count++ < 5) fprintf(stderr, "[wayfire-wrapper] Requesting frame capture #%d...\n", req_count);
    
    g_ctx.current_frame = zwlr_screencopy_manager_v1_capture_output(
        g_ctx.screencopy_manager, 1, g_ctx.output);
    zwlr_screencopy_frame_v1_add_listener(g_ctx.current_frame, &sc_listener, NULL);
}

// ============================================================================
// Wayfire Socket Discovery
// ============================================================================

static bool find_newest_wayland_socket(struct timespec *after_time, char *out_name, size_t out_size) {
    char runtime_dir[256];
    snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/%d", getuid());
    
    DIR *dir = opendir(runtime_dir);
    if (!dir) return false;
    
    struct timespec newest_time = {0, 0};
    char newest_name[128] = {0};
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "wayland-", 8) == 0 && 
            strstr(entry->d_name, ".lock") == NULL) {
            
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", runtime_dir, entry->d_name);
            
            struct stat st;
            if (stat(full_path, &st) == 0) {
                if (st.st_mtim.tv_sec > after_time->tv_sec ||
                    (st.st_mtim.tv_sec == after_time->tv_sec && 
                     st.st_mtim.tv_nsec > after_time->tv_nsec)) {
                    
                    if (st.st_mtim.tv_sec > newest_time.tv_sec ||
                        (st.st_mtim.tv_sec == newest_time.tv_sec && 
                         st.st_mtim.tv_nsec > newest_time.tv_nsec)) {
                        newest_time = st.st_mtim;
                        strncpy(newest_name, entry->d_name, sizeof(newest_name) - 1);
                    }
                }
            }
        }
    }
    closedir(dir);
    
    if (newest_name[0] != '\0') {
        strncpy(out_name, newest_name, out_size - 1);
        return true;
    }
    return false;
}

// ============================================================================
// Launch Headless Wayfire
// ============================================================================

static bool launch_wayfire_headless(int width, int height) {
    struct timespec launch_time;
    clock_gettime(CLOCK_REALTIME, &launch_time);
    
    fprintf(stderr, "[wayfire-wrapper] Launching Wayfire headless...\n");
    
    g_ctx.wayfire_pid = fork();
    
    if (g_ctx.wayfire_pid == 0) {
        setenv("WLR_BACKENDS", "headless", 1);
        setenv("WLR_HEADLESS_OUTPUTS", "1", 1);
        setenv("WLR_RENDERER", "gles2", 1);
        unsetenv("WAYLAND_DISPLAY");
        
        if (strlen(g_ctx.render_node) > 0) {
            setenv("WLR_RENDER_DRM_DEVICE", g_ctx.render_node, 1);
            setenv("WLR_DRM_DEVICES", g_ctx.render_node, 1);
        }

        if (access("./wayfire-headless.ini", R_OK) == 0) {
            setenv("WAYFIRE_CONFIG_FILE", "./wayfire-headless.ini", 1);
        }

        execl("/opt/wayfire/bin/wayfire", "wayfire", NULL);
        fprintf(stderr, "[wayfire-wrapper] Failed to exec wayfire: %s\n", strerror(errno));
        _exit(1);
    }
    
    fprintf(stderr, "[wayfire-wrapper] Wayfire PID: %d, scanning for new sockets...\n", g_ctx.wayfire_pid);
    
    for (int i = 0; i < 100; i++) {
        usleep(100000);
        
        if (find_newest_wayland_socket(&launch_time, g_ctx.wayfire_socket, sizeof(g_ctx.wayfire_socket))) {
            fprintf(stderr, "[wayfire-wrapper] Found Wayfire socket: %s (after %d ms)\n", 
                    g_ctx.wayfire_socket, (i + 1) * 100);
            usleep(500000);
            return true;
        }
    }
    
    fprintf(stderr, "[wayfire-wrapper] Timeout waiting for Wayfire socket\n");
    return false;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    fprintf(stderr, "[wayfire-wrapper] Starting (sizeof notification=%zu, sizeof input_event=%zu)...\n",
            sizeof(ipc_notification_t), sizeof(ipc_input_event_t));
    
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.notify_fd = -1; 
    g_ctx.event_fd = -1;
    
    signal(SIGINT, signal_handler); 
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    const char *efd = getenv("NANO_EVENT_FD");
    const char *nfd = getenv("NANO_NOTIFY_FD");
    if (efd) g_ctx.event_fd = atoi(efd);
    if (nfd) g_ctx.notify_fd = atoi(nfd);
    
    fprintf(stderr, "[wayfire-wrapper] IPC FDs: event=%d notify=%d\n", g_ctx.event_fd, g_ctx.notify_fd);

    if (!init_gbm()) return 1;
    if (!launch_wayfire_headless(1920, 1080)) return 1;

    // Connect to Wayfire
    setenv("WAYLAND_DISPLAY", g_ctx.wayfire_socket, 1);
    fprintf(stderr, "[wayfire-wrapper] Connecting to: %s\n", g_ctx.wayfire_socket);
    
    g_ctx.display = wl_display_connect(NULL);
    if (!g_ctx.display) {
        fprintf(stderr, "[wayfire-wrapper] Failed to connect to Wayfire: %s\n", strerror(errno));
        return 1;
    }
    fprintf(stderr, "[wayfire-wrapper] Connected to Wayfire display\n");

    g_ctx.registry = wl_display_get_registry(g_ctx.display);
    wl_registry_add_listener(g_ctx.registry, &registry_listener, NULL);
    
    wl_display_roundtrip(g_ctx.display);
    wl_display_roundtrip(g_ctx.display);
    
    fprintf(stderr, "[wayfire-wrapper] After roundtrip:\n");
    fprintf(stderr, "[wayfire-wrapper]   screencopy_manager: %p\n", g_ctx.screencopy_manager);
    fprintf(stderr, "[wayfire-wrapper]   linux_dmabuf: %p\n", g_ctx.linux_dmabuf);
    fprintf(stderr, "[wayfire-wrapper]   output: %p\n", g_ctx.output);
    fprintf(stderr, "[wayfire-wrapper]   seat: %p\n", g_ctx.seat);
    fprintf(stderr, "[wayfire-wrapper]   vpointer_manager: %p\n", g_ctx.vpointer_manager);
    fprintf(stderr, "[wayfire-wrapper]   vkeyboard_manager: %p\n", g_ctx.vkeyboard_manager);

    if (!g_ctx.screencopy_manager || !g_ctx.linux_dmabuf) {
        fprintf(stderr, "[wayfire-wrapper] Missing required protocols!\n");
        return 1;
    }
    
    if (!g_ctx.output) {
        fprintf(stderr, "[wayfire-wrapper] No output found, waiting...\n");
        for (int i = 0; i < 50 && !g_ctx.output; i++) {
            wl_display_roundtrip(g_ctx.display);
            usleep(100000);
        }
        if (!g_ctx.output) {
            fprintf(stderr, "[wayfire-wrapper] Still no output after waiting!\n");
            return 1;
        }
    }

    // Initialize virtual input devices
    init_virtual_input();

    // Send ready notification
    ipc_notification_t ready = {0};
    ready.type = IPC_NOTIFICATION_TYPE_PLUGIN_READY;
    strncpy(ready.data.plugin_ready.plugin_name, "wayfire-screencopy", 63);
    ready.data.plugin_ready.supports_dmabuf = true;
    send_notification(&ready);
    
    fprintf(stderr, "[wayfire-wrapper] Entering main loop\n");

    while (g_running) {
        request_frame();
        
        while (wl_display_prepare_read(g_ctx.display) != 0) {
            wl_display_dispatch_pending(g_ctx.display);
        }
        wl_display_flush(g_ctx.display);

        struct pollfd fds[2] = { 
            { wl_display_get_fd(g_ctx.display), POLLIN, 0 }, 
            { g_ctx.event_fd, POLLIN, 0 } 
        };
        
        int nfds = (g_ctx.event_fd >= 0) ? 2 : 1;
        
        if (poll(fds, nfds, 16) > 0) {  // ~60fps timeout
            if (fds[0].revents & POLLIN) {
                wl_display_read_events(g_ctx.display);
                wl_display_dispatch_pending(g_ctx.display);
            } else {
                wl_display_cancel_read(g_ctx.display);
            }
            
            // Handle input events from compositor
            if (nfds > 1 && (fds[1].revents & POLLIN)) {
                ipc_input_event_t input_event;
                ssize_t n = read(g_ctx.event_fd, &input_event, sizeof(input_event));
                if (n == sizeof(input_event)) {
                    process_input_event(&input_event);
                } else if (n > 0) {
                    fprintf(stderr, "[wayfire-wrapper] Partial input event read: %zd/%zu bytes\n",
                            n, sizeof(input_event));
                }
            }
        } else {
            wl_display_cancel_read(g_ctx.display);
        }
    }

    fprintf(stderr, "[wayfire-wrapper] Shutting down, captured %u frames, sent %u\n", 
            g_ctx.frames_captured, g_ctx.frames_sent);
    
    // Cleanup virtual input
    if (g_ctx.vpointer) zwlr_virtual_pointer_v1_destroy(g_ctx.vpointer);
    if (g_ctx.vkeyboard) zwp_virtual_keyboard_v1_destroy(g_ctx.vkeyboard);
    
    if (g_ctx.wayfire_pid > 0) {
        kill(g_ctx.wayfire_pid, SIGTERM);
        waitpid(g_ctx.wayfire_pid, NULL, 0);
    }
    
    return 0;
}
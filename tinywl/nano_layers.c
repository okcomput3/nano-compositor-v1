// nano_layers.c - Fixed with Debug Logging
#define _GNU_SOURCE
#include "nano_compositor.h"
#include "nano_layer_compositor.h"
// Note: nano_ipc.h is included via nano_compositor.h but has different struct sizes
// We define LOCAL structs that match the wrapper's definitions exactly

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <wlr/util/log.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/render/dmabuf.h>
#include <wlr/types/wlr_scene.h>

// ============================================================================
// LOCAL IPC DEFINITIONS - MUST MATCH wayfire_capture_wrapper.c EXACTLY
// These use different names to avoid conflicts with nano_ipc.h
// ============================================================================
typedef struct {
    uint32_t width; 
    uint32_t height; 
    uint32_t format; 
    uint64_t modifier;
    uint32_t num_planes; 
    uint32_t stride[4]; 
    uint32_t offset[4];
} layer_dmabuf_info_t;

typedef struct {
    char plugin_name[64]; 
    char plugin_version[32]; 
    uint32_t api_version;
    bool supports_dmabuf; 
    bool supports_window_management;
} layer_plugin_ready_t;

typedef struct {
    uint32_t type;  // 0=crashed, 1=frame, 2=dmabuf_frame, 20=ready
    uint64_t timestamp_ns;
    union {
        layer_dmabuf_info_t dmabuf;
        layer_plugin_ready_t plugin;
    } data;
} layer_notification_t;

// Notification types (matching wrapper)
#define LAYER_NOTIF_CRASHED           0
#define LAYER_NOTIF_FRAME_SUBMIT      1
#define LAYER_NOTIF_DMABUF_SUBMIT     2
#define LAYER_NOTIF_PLUGIN_READY      20

// Forward declarations
static int layer_notify_handler(int fd, uint32_t mask, void *data);

// ============================================================================
// Layer Manager
// ============================================================================

bool nano_layer_manager_init(layer_manager_t *manager, struct nano_server *server) {
    if (!manager || !server) return false;
    memset(manager, 0, sizeof(*manager));
    manager->server = server;
    
    manager->layer_tree = wlr_scene_tree_create(&server->scene->tree);
    if (!manager->layer_tree) return false;
    
    // Lower to bottom so it's a background layer
    wlr_scene_node_lower_to_bottom(&manager->layer_tree->node);
    
    manager->initialized = true;
    wlr_log(WLR_INFO, "Layer manager initialized");
    return true;
}

void nano_layer_manager_destroy(layer_manager_t *manager) {
    if (!manager) return;
    for (int i = 0; i < manager->layer_count; i++) {
        if (manager->layers[i]) {
            nano_layer_destroy(manager->layers[i]);
            manager->layers[i] = NULL;
        }
    }
}

// ============================================================================
// Layer Creation/Destruction
// ============================================================================

compositor_layer_t *nano_layer_create(layer_manager_t *manager, const char *name, const char *wrapper_cmd, int z_order) {
    if (!manager->initialized) return NULL;

    compositor_layer_t *layer = calloc(1, sizeof(compositor_layer_t));
    strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->server = manager->server;
    layer->visible = true;
    layer->dmabuf_fd = -1;

    // Socket pairs
    int event_pair[2], notify_pair[2];
    socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, event_pair);
    socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, notify_pair);
    
    layer->event_fd = event_pair[0];
    layer->notify_fd = notify_pair[0];
    
    // Scene Setup (Placeholder only)
    layer->scene_tree = wlr_scene_tree_create(manager->layer_tree);
    float placeholder_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    layer->placeholder = wlr_scene_rect_create(layer->scene_tree, 1920, 1080, placeholder_color);

    // Fork
    layer->pid = fork();
    if (layer->pid == 0) {
        close(event_pair[0]); close(notify_pair[0]);
        fcntl(event_pair[1], F_SETFD, 0);
        fcntl(notify_pair[1], F_SETFD, 0);
        
        char efd[16], nfd[16];
        snprintf(efd, 16, "%d", event_pair[1]);
        snprintf(nfd, 16, "%d", notify_pair[1]);
        
        setenv("NANO_EVENT_FD", efd, 1);
        setenv("NANO_NOTIFY_FD", nfd, 1);
        setenv("NANO_LAYER_NAME", name, 1);
        
        execl(wrapper_cmd, wrapper_cmd, NULL);
        _exit(1);
    }
    
    close(event_pair[1]); close(notify_pair[1]);
    
    layer->notify_source = wl_event_loop_add_fd(
        wl_display_get_event_loop(manager->server->wl_display),
        layer->notify_fd, WL_EVENT_READABLE, layer_notify_handler, layer
    );

    manager->layers[manager->layer_count++] = layer;
    wl_list_insert(&manager->server->layers, &layer->link);
    
    wlr_log(WLR_INFO, "Layer '%s' created with notify_fd=%d, expected notif size=%zu",
            name, layer->notify_fd, sizeof(layer_notification_t));
    
    return layer;
}

void nano_layer_destroy(compositor_layer_t *layer) {
    if (!layer) return;
    if (layer->notify_source) wl_event_source_remove(layer->notify_source);
    
    if (layer->pid > 0) {
        kill(layer->pid, SIGTERM);
        waitpid(layer->pid, NULL, 0);
    }
    
    if (layer->dmabuf_fd >= 0) close(layer->dmabuf_fd);
    if (layer->event_fd >= 0) close(layer->event_fd);
    if (layer->notify_fd >= 0) close(layer->notify_fd);
    if (layer->texture) wlr_texture_destroy(layer->texture);
    
    if (layer->scene_tree) wlr_scene_node_destroy(&layer->scene_tree->node);
    
    wl_list_remove(&layer->link);
    free(layer);
}

// ============================================================================
// IPC & Texture Import
// ============================================================================

static int receive_dmabuf_fds(int socket, int *fds, int max_fds, int *num_received) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    char buf[1];
    char control[CMSG_SPACE(sizeof(int) * 4)];
    
    iov[0].iov_base = buf; iov[0].iov_len = 1;
    msg.msg_iov = iov; msg.msg_iovlen = 1;
    msg.msg_control = control; msg.msg_controllen = sizeof(control);
    
    ssize_t n = recvmsg(socket, &msg, 0);
    if (n <= 0) { 
        wlr_log(WLR_ERROR, "recvmsg failed: n=%zd errno=%d (%s)", n, errno, strerror(errno));
        *num_received = 0; 
        return -1; 
    }
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        int fd_count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        if (fd_count > max_fds) fd_count = max_fds;
        memcpy(fds, CMSG_DATA(cmsg), sizeof(int) * fd_count);
        *num_received = fd_count;
        wlr_log(WLR_INFO, "Received %d FDs via SCM_RIGHTS", fd_count);
        return 0;
    }
    
    wlr_log(WLR_ERROR, "No SCM_RIGHTS in message (cmsg=%p)", (void*)cmsg);
    *num_received = 0;
    return -1;
}

static int layer_notify_handler(int fd, uint32_t mask, void *data) {
    wlr_log(WLR_INFO, "📬 layer_notify_handler: mask=0x%x", mask);
    
    compositor_layer_t *layer = data;
    if (!layer) {
        wlr_log(WLR_ERROR, "layer_notify_handler: layer is NULL!");
        return 0;
    }
    
    if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
        wlr_log(WLR_ERROR, "layer_notify_handler: HANGUP or ERROR on fd");
        return 0;
    }

    // Read the notification using LOCAL struct that matches wrapper
    layer_notification_t notif;
    ssize_t n = read(fd, &notif, sizeof(notif));
    
    wlr_log(WLR_INFO, "📬 read() returned %zd bytes (expected %zu)", n, sizeof(notif));
    
    if (n != sizeof(notif)) {
        wlr_log(WLR_ERROR, "📬 read size mismatch! Got %zd, expected %zu", n, sizeof(notif));
        return 0;
    }

    wlr_log(WLR_INFO, "📬 Notification type=%u (DMABUF_SUBMIT=%d)", 
            notif.type, LAYER_NOTIF_DMABUF_SUBMIT);

    if (notif.type == LAYER_NOTIF_DMABUF_SUBMIT) {
        wlr_log(WLR_INFO, "📬 Processing DMABUF frame submit...");
        
        int fds[4] = {-1, -1, -1, -1};
        int num_fds = 0;
        
        // Receive FDs
        if (receive_dmabuf_fds(fd, fds, 4, &num_fds) < 0 || num_fds == 0) {
            wlr_log(WLR_ERROR, "📬 Failed to receive DMA-BUF FDs");
            return 0;
        }
        
        wlr_log(WLR_INFO, "📬 Received %d FDs, first fd=%d", num_fds, fds[0]);

        // Get buffer info from notification (using local struct)
        uint32_t width = notif.data.dmabuf.width;
        uint32_t height = notif.data.dmabuf.height;
        uint32_t format = notif.data.dmabuf.format;
        uint64_t modifier = notif.data.dmabuf.modifier;
        uint32_t num_planes = notif.data.dmabuf.num_planes;
        
        wlr_log(WLR_INFO, "📬 Buffer info: %ux%u fmt=0x%x mod=0x%lx planes=%u",
                width, height, format, (unsigned long)modifier, num_planes);

        struct wlr_dmabuf_attributes attribs = {
            .width = width,
            .height = height,
            .format = format,
            .modifier = modifier,
            .n_planes = num_planes
        };

        // Populate plane attributes
        for (uint32_t i = 0; i < num_planes && i < 4; i++) {
            if (num_fds == 1) {
                attribs.fd[i] = fds[0]; // Reuse single FD for all planes
            } else {
                attribs.fd[i] = fds[i]; // Use corresponding FD
            }
            attribs.stride[i] = notif.data.dmabuf.stride[i];
            attribs.offset[i] = notif.data.dmabuf.offset[i];
            
            wlr_log(WLR_INFO, "📬 Plane %u: fd=%d stride=%u offset=%u",
                    i, attribs.fd[i], attribs.stride[i], attribs.offset[i]);
        }

        // Import texture
        wlr_log(WLR_INFO, "📬 Calling wlr_texture_from_dmabuf...");
        struct wlr_texture *new_texture = wlr_texture_from_dmabuf(layer->server->renderer, &attribs);

        // Clean up FDs - wlr_texture_from_dmabuf does NOT take ownership
        for (int i = 0; i < num_fds; i++) {
            if (fds[i] >= 0) {
                close(fds[i]);
            }
        }

        if (new_texture) {
            if (layer->texture) {
                wlr_texture_destroy(layer->texture);
            }
            
            layer->texture = new_texture;
            layer->width = width;
            layer->height = height;
            layer->format = format;
            layer->frame_ready = true;
            layer->frame_count++;
            
            // Hide placeholder since we have a real texture now
            if (layer->placeholder) {
                wlr_scene_node_set_enabled(&layer->placeholder->node, false);
            }
            
            wlr_log(WLR_INFO, "✅ Frame %u imported successfully! (%ux%u)", 
                    layer->frame_count, width, height);
        } else {
            wlr_log(WLR_ERROR, "❌ wlr_texture_from_dmabuf FAILED!");
        }
    } else if (notif.type == LAYER_NOTIF_PLUGIN_READY) {
        wlr_log(WLR_INFO, "📬 Layer '%s' plugin ready: %s", 
                layer->name, notif.data.plugin.plugin_name);
    } else {
        wlr_log(WLR_INFO, "📬 Unknown notification type: %u", notif.type);
    }
    
    return 0;
}

// Helpers
void nano_layer_set_visible(compositor_layer_t *layer, bool visible) {
    if (layer) layer->visible = visible;
    if (layer && layer->scene_tree) wlr_scene_node_set_enabled(&layer->scene_tree->node, visible);
}

void nano_layer_set_opacity(compositor_layer_t *layer, float opacity) {
    if (layer) layer->opacity = opacity; 
}

void nano_layer_set_position(compositor_layer_t *layer, int x, int y) {
    if (layer) {
        layer->x = x;
        layer->y = y;
        if (layer->scene_tree) wlr_scene_node_set_position(&layer->scene_tree->node, x, y);
    }
}
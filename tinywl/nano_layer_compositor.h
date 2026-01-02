// nano_layer_compositor.h - Compositor-as-layer support
// This enables running other Wayland compositors (like Wayfire) as layers
// that render to DMA-BUF and get composited by nano compositor

#ifndef NANO_LAYER_COMPOSITOR_H
#define NANO_LAYER_COMPOSITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <wayland-server-core.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>

// Forward declaration - full definition is in nano_compositor.h
struct nano_server;

// Maximum compositor layers supported
#define MAX_COMPOSITOR_LAYERS 8

// Single compositor layer (e.g., Wayfire running headless)
typedef struct compositor_layer {
    struct wl_list link;  // For server->layers list
    
    char name[64];
    pid_t pid;
    
    // The headless compositor's DMA-BUF output
    int dmabuf_fd;
    uint32_t width, height;
    uint32_t format;
    uint32_t stride;
    uint64_t modifier;
    
    // Texture created from the DMA-BUF
    struct wlr_texture *texture;
    struct wlr_buffer *buffer;
    
    // Scene graph node for this layer
    struct wlr_scene_buffer *scene_buffer;
    struct wlr_scene_tree *scene_tree;  // Parent tree for positioning
    struct wlr_scene_rect *placeholder; // Placeholder until first frame
    
    // IPC for frame synchronization
    int event_fd;       // Send events TO the layer
    int notify_fd;      // Receive notifications FROM the layer
    struct wl_event_source *notify_source;
    
    // Layer properties
    int z_order;
    float opacity;
    bool visible;
    int x, y;  // Position offset
    
    // Frame sync
    bool frame_ready;
    uint64_t last_frame_time;
    uint32_t frame_count;
    
    // Back-reference
    struct nano_server *server;
} compositor_layer_t;

// Manager for all compositor layers
typedef struct layer_manager {
    compositor_layer_t *layers[MAX_COMPOSITOR_LAYERS];
    int layer_count;
    
    // Scene tree for all layers (rendered before regular views)
    struct wlr_scene_tree *layer_tree;
    
    // Initialized flag
    bool initialized;
    
    struct nano_server *server;
} layer_manager_t;

// ============================================================================
// API Functions
// ============================================================================

// Initialize the layer manager
bool nano_layer_manager_init(layer_manager_t *manager, struct nano_server *server);

// Cleanup
void nano_layer_manager_destroy(layer_manager_t *manager);

// Create a new compositor layer
compositor_layer_t *nano_layer_create(
    layer_manager_t *manager,
    const char *name,
    const char *wrapper_cmd,
    int z_order
);

// Destroy a layer
void nano_layer_destroy(compositor_layer_t *layer);

// Layer control
void nano_layer_set_visible(compositor_layer_t *layer, bool visible);
void nano_layer_set_opacity(compositor_layer_t *layer, float opacity);
void nano_layer_set_position(compositor_layer_t *layer, int x, int y);
void nano_layer_set_z_order(compositor_layer_t *layer, int z_order);

// Frame handling - called when wrapper sends new DMA-BUF frame
void nano_layer_submit_frame(compositor_layer_t *layer, int dmabuf_fd,
                             uint32_t width, uint32_t height,
                             uint32_t format, uint32_t stride,
                             uint64_t modifier);


// Get layer by name
compositor_layer_t *nano_layer_find(layer_manager_t *manager, const char *name);

// Get layer by pid
compositor_layer_t *nano_layer_find_by_pid(layer_manager_t *manager, pid_t pid);

#endif // NANO_LAYER_COMPOSITOR_H
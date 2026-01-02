// nano_api.c - Plugin API implementation
#include "nano_compositor.h"
#include <stdlib.h>
#include <string.h>
#include <wlr/render/gles2.h>
#include <wlr/render/wlr_texture.h>

// Global server pointer for API functions
// In a real implementation, this should be thread-local or passed through context
static struct nano_server *g_server = NULL;

void nano_api_set_server(struct nano_server *server) {
    g_server = server;
}

// Plugin API implementation
static struct nano_server *api_get_server(void) {
    return g_server;
}

static struct wl_display *api_get_display(void) {
    return g_server ? g_server->wl_display : NULL;
}

static struct wlr_renderer *api_get_renderer(void) {
    return g_server ? g_server->renderer : NULL;
}

static void api_view_move(struct nano_view *view, double x, double y) {
    if (!view || !view->scene_tree) return;
    wlr_scene_node_set_position(&view->scene_tree->node, x, y);
}

static void api_view_resize(struct nano_view *view, int width, int height) {
    if (!view || !view->xdg_toplevel) return;
    wlr_xdg_toplevel_set_size(view->xdg_toplevel, width, height);
}

static void api_view_focus(struct nano_view *view) {
    if (!view || !g_server) return;
    
    struct nano_server *server = g_server;
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
    if (!view || !view->xdg_toplevel) return;
    wlr_xdg_toplevel_send_close(view->xdg_toplevel);
}

static struct wlr_scene_tree *api_get_scene_tree(void) {
    return g_server ? &g_server->scene->tree : NULL;
}

static struct wlr_scene_node *api_view_get_scene_node(struct nano_view *view) {
    return (view && view->scene_tree) ? &view->scene_tree->node : NULL;
}




static struct wlr_output_layout *api_get_output_layout(void) {
    return g_server ? g_server->output_layout : NULL;
}

static int api_register_service(const char *name, void *service_api) {
    if (!g_server || !name) return -1;
    
    struct nano_service *service = calloc(1, sizeof(struct nano_service));
    if (!service) return -1;
    
    service->name = strdup(name);
    service->api = service_api;
    service->provider = NULL; // TODO: track which plugin provides this
    wl_list_insert(&g_server->services, &service->link);
    
    return 0;
}

static void *api_get_service(const char *name) {
    if (!g_server || !name) return NULL;
    
    struct nano_service *service;
    wl_list_for_each(service, &g_server->services, link) {
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

// EGL context access for plugins
static struct wlr_egl *api_get_egl(void) {
    extern struct nano_server *g_server; // Assuming you have a global server pointer
    return g_server ? g_server->egl : NULL;
}


// Global array to store scene buffer info
static struct nano_scene_buffer_info scene_buffer_infos[32];
static int scene_buffer_count = 0;

// Scene buffer iterator callback (similar to your animation code)
static void scene_buffer_iterator(struct wlr_scene_buffer *scene_buffer, int sx, int sy, void *user_data) {
    if (scene_buffer_count >= 32) {
        return; // Array full
    }
    
    // Get the scene surface if available
    struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
    struct wlr_surface *surface = scene_surface ? scene_surface->surface : NULL;
    
    // Try to find associated nano_view
    struct nano_view *associated_view = NULL;
    if (surface && g_server) {
        struct nano_view *view;
        wl_list_for_each(view, &g_server->views, link) {
            if (view->xdg_toplevel && 
                view->xdg_toplevel->base && 
                view->xdg_toplevel->base->surface == surface) {
                associated_view = view;
                break;
            }
        }
    }
    
    // Store scene buffer info
    struct nano_scene_buffer_info *info = &scene_buffer_infos[scene_buffer_count];
    info->scene_buffer = scene_buffer;
    info->surface = surface;
    info->x = sx;
    info->y = sy;
    info->width = scene_buffer->buffer ? scene_buffer->buffer->width : 0;
    info->height = scene_buffer->buffer ? scene_buffer->buffer->height : 0;
    info->is_mapped = surface ? surface->mapped : true;
    info->view = associated_view;
    
    // Get title from associated view
    if (associated_view && associated_view->xdg_toplevel && associated_view->xdg_toplevel->title) {
        info->title = associated_view->xdg_toplevel->title;
    } else {
        info->title = "Unknown";
    }
    
    scene_buffer_count++;
    
    printf("Found scene buffer: %dx%d at (%d,%d) title='%s'\n", 
           info->width, info->height, info->x, info->y, info->title);
}

// API function to get scene buffer info
static struct nano_scene_buffer_info* api_get_scene_buffers(int *count) {
    scene_buffer_count = 0;
    
    if (!g_server || !g_server->scene) {
        *count = 0;
        return NULL;
    }
    
    printf("Collecting scene buffer information from compositor...\n");
    
    // Iterate through scene buffers (like your animation code does)
    wlr_scene_node_for_each_buffer(&g_server->scene->tree.node, scene_buffer_iterator, NULL);
    
    printf("Found %d scene buffers total\n", scene_buffer_count);
    *count = scene_buffer_count;
    return scene_buffer_infos;
}



// Window management handler implementations for nano_plugin.c
// Add these functions to your nano_plugin.c file

// Helper function to get view by window index
static struct nano_view* get_view_by_index(struct nano_server *server, int window_index) {
    if (!server || window_index < 0) {
        return NULL;
    }
    
    // Get scene buffer information to map index to view
    int buffer_count;
    struct nano_scene_buffer_info *buffers = server->plugin_api.get_scene_buffers(&buffer_count);
    
    if (!buffers || window_index >= buffer_count) {
        wlr_log(WLR_ERROR, "Invalid window index %d (available: %d)", window_index, buffer_count);
        return NULL;
    }
    
    // Return the view associated with this window index
    return buffers[window_index].view;
}

// Alternative helper: iterate through views list by index
static struct nano_view* get_view_by_index_simple(struct nano_server *server, int window_index) {
    if (!server || window_index < 0) {
        return NULL;
    }
    
    int current_index = 0;
    struct nano_view *view;
    wl_list_for_each(view, &server->views, link) {
        if (current_index == window_index) {
            return view;
        }
        current_index++;
    }
    
    wlr_log(WLR_ERROR, "Window index %d not found (total views: %d)", window_index, current_index);
    return NULL;
}

static void handle_window_move_request(struct nano_server *server, int window_index, int x, int y) {
    wlr_log(WLR_INFO, "Moving window %d to position (%d, %d)", window_index, x, y);
    
    // Get the view by window index
    struct nano_view *view = get_view_by_index_simple(server, window_index);
    if (!view) {
        wlr_log(WLR_ERROR, "Cannot find view for window index %d", window_index);
        return;
    }
    
    // Use your existing API function to move the window
    server->plugin_api.view_move(view, (double)x, (double)y);
    
    wlr_log(WLR_DEBUG, "Successfully moved window %d (%s) to (%d, %d)", 
            window_index, 
            (view->xdg_toplevel && view->xdg_toplevel->title) ? view->xdg_toplevel->title : "Unknown",
            x, y);
}

static void handle_window_focus_request(struct nano_server *server, int window_index) {
    wlr_log(WLR_INFO, "Focusing window %d", window_index);
    
    struct nano_view *view = get_view_by_index_simple(server, window_index);
    if (!view) {
        wlr_log(WLR_ERROR, "Cannot find view for window index %d", window_index);
        return;
    }
    
    // SUPER AGGRESSIVE RAISING - try multiple methods
    if (view->scene_tree) {
        wlr_log(WLR_DEBUG, "AGGRESSIVELY raising window %d to top", window_index);
        
        // Method 1: Multiple raise calls
        for (int i = 0; i < 5; i++) {
            wlr_scene_node_raise_to_top(&view->scene_tree->node);
        }
        
        // Method 2: Disable/enable to force redraw
        wlr_scene_node_set_enabled(&view->scene_tree->node, false);
        wlr_scene_node_set_enabled(&view->scene_tree->node, true);
        
        // Method 3: Move the view to end of compositor's view list
        wl_list_remove(&view->link);
        wl_list_insert(server->views.prev, &view->link);  // Insert at end (top)
        
        wlr_log(WLR_DEBUG, "Window %d raised using all methods", window_index);
    }
    
    // THEN call the normal focus function
    server->plugin_api.view_focus(view);
    
    wlr_log(WLR_DEBUG, "Successfully focused and raised window %d (%s)", 
            window_index,
            (view->xdg_toplevel && view->xdg_toplevel->title) ? view->xdg_toplevel->title : "Unknown");
}

static void handle_window_resize_request(struct nano_server *server, int window_index, int width, int height) {
    wlr_log(WLR_INFO, "Resizing window %d to %dx%d", window_index, width, height);
    
    // Get the view by window index
    struct nano_view *view = get_view_by_index_simple(server, window_index);
    if (!view) {
        wlr_log(WLR_ERROR, "Cannot find view for window index %d", window_index);
        return;
    }
    
    // Use your existing API function to resize the window
    server->plugin_api.view_resize(view, width, height);
    
    wlr_log(WLR_DEBUG, "Successfully resized window %d (%s) to %dx%d", 
            window_index,
            (view->xdg_toplevel && view->xdg_toplevel->title) ? view->xdg_toplevel->title : "Unknown",
            width, height);
}

// Enhanced version using scene buffer information (if you prefer more detailed mapping)
static void handle_window_move_request_enhanced(struct nano_server *server, int window_index, int x, int y) {
    wlr_log(WLR_INFO, "Moving window %d to position (%d, %d) [enhanced]", window_index, x, y);
    
    // Get scene buffer information
    int buffer_count;
    struct nano_scene_buffer_info *buffers = server->plugin_api.get_scene_buffers(&buffer_count);
    
    if (!buffers || window_index >= buffer_count || window_index < 0) {
        wlr_log(WLR_ERROR, "Invalid window index %d (available: %d)", window_index, buffer_count);
        return;
    }
    
    struct nano_scene_buffer_info *buffer_info = &buffers[window_index];
    struct nano_view *view = buffer_info->view;
    
    if (!view) {
        wlr_log(WLR_ERROR, "No view associated with window index %d", window_index);
        return;
    }
    
    // Log current and target positions
    wlr_log(WLR_DEBUG, "Moving window '%s' from (%d, %d) to (%d, %d)",
            buffer_info->title, buffer_info->x, buffer_info->y, x, y);
    
    // Use your existing API function to move the window
    server->plugin_api.view_move(view, (double)x, (double)y);
    
    wlr_log(WLR_DEBUG, "Successfully moved window %d (%s) to (%d, %d)", 
            window_index, buffer_info->title, x, y);
}


void nano_server_setup_plugin_api(struct nano_server *server) {
    nano_api_set_server(server);
    
    server->plugin_api.get_server = api_get_server;
    server->plugin_api.get_display = api_get_display;
    server->plugin_api.get_renderer = api_get_renderer;
    
    server->plugin_api.get_egl = api_get_egl;  // EGL context access

    server->plugin_api.view_move = api_view_move;
    server->plugin_api.view_resize = api_view_resize;
    server->plugin_api.view_focus = api_view_focus;
    server->plugin_api.view_close = api_view_close;
    server->plugin_api.get_scene_tree = api_get_scene_tree;
    server->plugin_api.view_get_scene_node = api_view_get_scene_node;

    server->plugin_api.get_output_layout = api_get_output_layout;
    server->plugin_api.register_service = api_register_service;
    server->plugin_api.get_service = api_get_service;
    server->plugin_api.get_config_string = api_get_config_string;
    server->plugin_api.get_config_int = api_get_config_int;

    server->plugin_api.get_scene_buffers = api_get_scene_buffers;

 
}
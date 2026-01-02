// Enhanced window_management with proper z-order handling
#define _GNU_SOURCE 
#include "../nano_compositor.h"
#include "../nano_ipc.h"
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
#include <sys/socket.h>
#include <errno.h>
#include <sys/un.h>
#include <limits.h>

#define TITLEBAR_HEIGHT 30
#define MAX_WINDOWS 32
#define MOUSE_HISTORY_SIZE 8
#define PREDICTION_FRAMES 2

// Mouse smoothing and prediction (unchanged)
struct mouse_state {
    double x, y;
    struct timeval timestamp;
    double velocity_x, velocity_y;
};

struct mouse_smoother {
    struct mouse_state history[MOUSE_HISTORY_SIZE];
    int current_index;
    int history_count;
    
    double predicted_x, predicted_y;
    bool prediction_valid;
    
    double smoothing_factor;
    double velocity_threshold;
};

struct managed_window {
    uint32_t id;
    double x, y;
    int width, height;
    char title[256];
    char app_id[256];
    
    bool mapped;
    bool focused;
    bool maximized;
    bool fullscreen;
    bool dragging;
    
    bool has_titlebar;
    uint32_t titlebar_color;
    uint32_t border_color;
    int border_width;
    
    // Enhanced animation state
    double target_x, target_y;
    double visual_x, visual_y;
    double animation_progress;
    bool animating;
    
    double predicted_x, predicted_y;
    bool prediction_active;
    
    int compositor_window_index;
    
    // NEW: Z-order management
    int z_order;        // Higher number = on top
    bool raise_pending; // Window needs to be raised
};

struct window_manager_state {
    int ipc_fd;
    void* shm_ptr;
    
    struct managed_window windows[MAX_WINDOWS];
    int window_count;
    int focused_window_index;
    int dragged_window_index;
    
    bool dragging;
    double drag_start_x, drag_start_y;
    double window_start_x, window_start_y;
    
    int screen_width;
    int screen_height;
    
    uint32_t next_window_id;
    int next_z_order;  // NEW: Next z-order value
    
    double last_mouse_x, last_mouse_y;
    struct mouse_smoother mouse_smoother;
    
    bool left_button_pressed;
    bool right_button_pressed;
    bool middle_button_pressed;
    struct timeval last_button_event;
    
    struct timeval last_ipc_send;
    struct timeval last_frame_time;
    int pending_moves;
    
    // Stats
    uint64_t frames_processed;
    uint64_t windows_managed;
    uint64_t drag_operations;
    uint64_t ipc_errors;
    uint64_t successful_moves;
    uint64_t successful_focus_changes;
    uint64_t mouse_events_processed;
    uint64_t predictions_made;
    uint64_t button_events_processed;
    uint64_t duplicate_events_filtered;
    uint64_t window_raises;  // NEW: Track window raises
};

// Initialize mouse smoother (unchanged)
static void init_mouse_smoother(struct mouse_smoother *smoother) {
    memset(smoother, 0, sizeof(*smoother));
    smoother->smoothing_factor = 0.0000000000000000000000008;
    smoother->velocity_threshold = 5.0;
}

// Mouse smoothing functions (unchanged)
static void update_mouse_smoother(struct mouse_smoother *smoother, double x, double y) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    int next_index = (smoother->current_index + 1) % MOUSE_HISTORY_SIZE;
    smoother->history[next_index].x = x;
    smoother->history[next_index].y = y;
    smoother->history[next_index].timestamp = now;
    
    if (smoother->history_count > 0) {
        int prev_index = smoother->current_index;
        struct mouse_state *prev = &smoother->history[prev_index];
        struct mouse_state *curr = &smoother->history[next_index];
        
        double dt = (now.tv_sec - prev->timestamp.tv_sec) + 
                   (now.tv_usec - prev->timestamp.tv_usec) / 1000000.0;
        
        if (dt > 0.001) {
            curr->velocity_x = (x - prev->x) / dt;
            curr->velocity_y = (y - prev->y) / dt;
        } else {
            curr->velocity_x = prev->velocity_x;
            curr->velocity_y = prev->velocity_y;
        }
        
        double speed = sqrt(curr->velocity_x * curr->velocity_x + curr->velocity_y * curr->velocity_y);
        if (speed > smoother->velocity_threshold) {
            double predict_time = 0.032;
            smoother->predicted_x = x + curr->velocity_x * predict_time;
            smoother->predicted_y = y + curr->velocity_y * predict_time;
            smoother->prediction_valid = true;
        } else {
            smoother->prediction_valid = false;
        }
    } else {
        smoother->history[next_index].velocity_x = 0;
        smoother->history[next_index].velocity_y = 0;
        smoother->prediction_valid = false;
    }
    
    smoother->current_index = next_index;
    if (smoother->history_count < MOUSE_HISTORY_SIZE) {
        smoother->history_count++;
    }
}

static void get_smoothed_mouse_pos(struct mouse_smoother *smoother, double *out_x, double *out_y) {
    if (smoother->history_count == 0) {
        *out_x = 0;
        *out_y = 0;
        return;
    }
    
    double smoothed_x = 0, smoothed_y = 0;
    double weight_sum = 0;
    
    for (int i = 0; i < smoother->history_count; i++) {
        int idx = (smoother->current_index - i + MOUSE_HISTORY_SIZE) % MOUSE_HISTORY_SIZE;
        double weight = pow(smoother->smoothing_factor, i);
        
        smoothed_x += smoother->history[idx].x * weight;
        smoothed_y += smoother->history[idx].y * weight;
        weight_sum += weight;
    }
    
    *out_x = smoothed_x / weight_sum;
    *out_y = smoothed_y / weight_sum;
}

// SIMPLIFIED: Just focus, no move tricks
static void raise_window(struct window_manager_state *state, int window_index) {
    if (window_index < 0 || window_index >= state->window_count) {
        return;
    }
    
    struct managed_window *win = &state->windows[window_index];
    
    // Update local z-order for our tracking
    win->z_order = state->next_z_order++;
    state->window_raises++;
    
    loggy_window_management( "[WM] 📈 Window %d (\"%s\") raised to z-order %d (local only)\n", 
            window_index, win->title, win->z_order);
}

static int find_window_at(struct window_manager_state *state, double x, double y) {
    loggy_window_management( "[WM] 🔍 CLICK DEBUG: Looking for window at (%.1f, %.1f)\n", x, y);
    
    // Print ALL windows and their bounds
    for (int i = 0; i < state->window_count; i++) {
        struct managed_window *win = &state->windows[i];
        if (!win->mapped) continue;
        
        double win_left = win->x;
        double win_right = win->x + win->width;
        double win_top = win->y - (win->has_titlebar ? TITLEBAR_HEIGHT : 0);
        double win_bottom = win->y + win->height;
        
        bool contains = (x >= win_left && x <= win_right && y >= win_top && y <= win_bottom);
        
        loggy_window_management( "[WM] Window %d (\"%s\"): (%.0f,%.0f)-(%.0f,%.0f) - %s\n",
                i, win->title, win_left, win_top, win_right, win_bottom,
                contains ? "CONTAINS CLICK ✅" : "doesn't contain");
    }
    
    // NOW try different selection methods
    int methods[3] = {-1, -1, -1};
    
    // Method 1: Forward iteration (first match)
    for (int i = 0; i < state->window_count; i++) {
        struct managed_window *win = &state->windows[i];
        if (!win->mapped) continue;
        
        double win_left = win->x;
        double win_right = win->x + win->width;
        double win_top = win->y - (win->has_titlebar ? TITLEBAR_HEIGHT : 0);
        double win_bottom = win->y + win->height;
        
        if (x >= win_left && x <= win_right && y >= win_top && y <= win_bottom) {
            methods[0] = i;
            break;
        }
    }
    
    // Method 2: Backward iteration (last match)
    for (int i = state->window_count - 1; i >= 0; i--) {
        struct managed_window *win = &state->windows[i];
        if (!win->mapped) continue;
        
        double win_left = win->x;
        double win_right = win->x + win->width;
        double win_top = win->y - (win->has_titlebar ? TITLEBAR_HEIGHT : 0);
        double win_bottom = win->y + win->height;
        
        if (x >= win_left && x <= win_right && y >= win_top && y <= win_bottom) {
            methods[1] = i;
            break;
        }
    }
    
    // Method 3: Focused window first
    if (state->focused_window_index >= 0 && state->focused_window_index < state->window_count) {
        struct managed_window *win = &state->windows[state->focused_window_index];
        if (win->mapped) {
            double win_left = win->x;
            double win_right = win->x + win->width;
            double win_top = win->y - (win->has_titlebar ? TITLEBAR_HEIGHT : 0);
            double win_bottom = win->y + win->height;
            
            if (x >= win_left && x <= win_right && y >= win_top && y <= win_bottom) {
                methods[2] = state->focused_window_index;
            }
        }
    }
    
    loggy_window_management( "[WM] SELECTION METHODS:\n");
    loggy_window_management( "[WM]   Forward (first):  %d (%s)\n", methods[0], 
            methods[0] >= 0 ? state->windows[methods[0]].title : "none");
    loggy_window_management( "[WM]   Backward (last):  %d (%s)\n", methods[1],
            methods[1] >= 0 ? state->windows[methods[1]].title : "none");
    loggy_window_management( "[WM]   Focused window:   %d (%s)\n", methods[2],
            methods[2] >= 0 ? state->windows[methods[2]].title : "none");
    
    // For now, try the backward method (last window that contains click)
    int selected = methods[1];
    
    loggy_window_management( "[WM] 🎯 SELECTED: %d (%s)\n", 
            selected, selected >= 0 ? state->windows[selected].title : "none");
    loggy_window_management( "[WM] ======================================\n");
    
    return selected;
}
// Enhanced move function (unchanged from original)
static void move_window(struct window_manager_state *state, int window_index, 
                       double x, double y, bool immediate) {
    if (window_index < 0 || window_index >= state->window_count) {
        return;
    }
    
    struct managed_window *win = &state->windows[window_index];
    struct timeval now;
    gettimeofday(&now, NULL);
    
    win->visual_x = x;
    win->visual_y = y;
    win->target_x = x;
    win->target_y = y;
    
    bool should_send = immediate;
    
    if (!should_send) {
        double time_since_last = (now.tv_sec - state->last_ipc_send.tv_sec) + 
                               (now.tv_usec - state->last_ipc_send.tv_usec) / 1000000.0;
        should_send = (time_since_last >= 0.008);
    }
    
    if (!should_send) {
        double dx = x - win->x;
        double dy = y - win->y;
        should_send = (dx*dx + dy*dy) >= 1;
    }
    
    if (should_send) {
        win->x = x;
        win->y = y;
        
        double predicted_x = x, predicted_y = y;
        if (state->mouse_smoother.prediction_valid) {
            double mouse_dx = state->mouse_smoother.predicted_x - state->last_mouse_x;
            double mouse_dy = state->mouse_smoother.predicted_y - state->last_mouse_y;
            predicted_x += mouse_dx;
            predicted_y += mouse_dy;
            state->predictions_made++;
        }

        ipc_notification_t notification = {0};
        notification.type = IPC_NOTIFICATION_TYPE_WINDOW_MOVE_REQUEST;
        notification.data.window_move_request.window_index = win->compositor_window_index;
        notification.data.window_move_request.x = (int)predicted_x;
        notification.data.window_move_request.y = (int)predicted_y;
        
        ssize_t written = write(state->ipc_fd, &notification, sizeof(notification));
        if (written == sizeof(notification)) {
            state->successful_moves++;
            state->last_ipc_send = now;
            state->pending_moves = 0;
        } else {
            state->ipc_errors++;
            loggy_window_management( "[WM] ❌ Failed to send move notification: %s\n", strerror(errno));
        }
    } else {
        state->pending_moves++;
    }
}

// CORRECTED: Focus window with raise
static void focus_window(struct window_manager_state *state, int window_index) {
    if (window_index < 0 || window_index >= state->window_count) {
        return;
    }
    
    // Update local state
    if (state->focused_window_index >= 0 && state->focused_window_index < state->window_count) {
        state->windows[state->focused_window_index].focused = false;
    }
    state->windows[window_index].focused = true;
    state->focused_window_index = window_index;
    
    // Raise the window to top when focusing
    raise_window(state, window_index);
    
    // Send focus notification to compositor
    ipc_notification_t notification = {0};
    notification.type = IPC_NOTIFICATION_TYPE_WINDOW_FOCUS_REQUEST;
    notification.data.window_focus_request.window_index = state->windows[window_index].compositor_window_index;
    
    ssize_t written = write(state->ipc_fd, &notification, sizeof(notification));
    if (written == sizeof(notification)) {
        state->successful_focus_changes++;
        loggy_window_management( "[WM] 🎯 Window focus request sent: %s\n", state->windows[window_index].title);
    } else {
        state->ipc_errors++;
        loggy_window_management( "[WM] ❌ Failed to send focus notification: %s\n", strerror(errno));
    }
}

static void handle_window_list_update(struct window_manager_state *state, 
                                     const ipc_window_list_t *window_list) {
    int new_count = window_list->count;
    if (new_count > MAX_WINDOWS) new_count = MAX_WINDOWS;
    
    fprintf(stderr, "[WM] 📋 Window list update received: %d windows\n", new_count);
    
    state->window_count = new_count;
    int compositor_focused_window = -1;  // Track what compositor says is focused
    
    for (int i = 0; i < new_count; i++) {
        const ipc_window_info_t *info = &window_list->windows[i];
        struct managed_window *win = &state->windows[i];
        
        win->id = state->next_window_id++;
        win->x = info->x;
        win->y = info->y;
        win->visual_x = info->x;
        win->visual_y = info->y;
        win->target_x = info->x;
        win->target_y = info->y;
        win->width = info->width;
        win->height = info->height;
        win->mapped = true;
        win->focused = info->is_focused;
        win->has_titlebar = true;
        win->titlebar_color = 0xFF2D3748;
        win->border_color = 0xFF4299E1;
        win->border_width = 2;
        win->compositor_window_index = i;
        
        // NEW: Initialize z-order (higher index = created later = on top by default)
        win->z_order = state->next_z_order++;
        win->raise_pending = false;
        
        if (info->is_focused) {
            compositor_focused_window = i;
            // Give focused window highest z-order
            win->z_order = state->next_z_order++;
        }

        strncpy(win->title, info->title, sizeof(win->title) - 1);
        win->title[sizeof(win->title) - 1] = '\0';
        
        fprintf(stderr, "[WM]   -> Window %d: \"%s\" at (%.0f,%.0f) size %dx%d z=%d\n", 
                i, win->title, info->x, info->y, info->width, info->height, win->z_order);
    }
    
    state->windows_managed += new_count;
    
    // FIXED: Don't auto-focus if we have a valid manually focused window
    if (state->focused_window_index >= 0 && state->focused_window_index < state->window_count) {
        fprintf(stderr, "[WM] 💾 Keeping manual focus on window %d (\"%s\")\n", 
                state->focused_window_index, state->windows[state->focused_window_index].title);
        
        // Keep our manual focus, don't override with compositor's focus
        state->windows[state->focused_window_index].focused = true;
        
        // Clear focus from other windows
        for (int i = 0; i < state->window_count; i++) {
            if (i != state->focused_window_index) {
                state->windows[i].focused = false;
            }
        }
    } else {
        // No manual focus set, use compositor's focus or default
        if (compositor_focused_window >= 0) {
            state->focused_window_index = compositor_focused_window;
            fprintf(stderr, "[WM] 🎯 Using compositor focus: window %d\n", compositor_focused_window);
        } else if (state->window_count > 0) {
            fprintf(stderr, "[WM] 🎯 No focus set, defaulting to window 0\n");
            focus_window(state, 0);
        }
    }
    
    fprintf(stderr, "[WM] ✅ Window list updated. Final focus: window %d\n", state->focused_window_index);
}

static void handle_mouse_motion(struct window_manager_state *state, double x, double y) {
    static int debug_count = 0;
    if (debug_count++ < 20) { // Log first 20 mouse events
        fprintf(stderr, "[WM] 🐭 Mouse motion %d: (%.1f, %.1f) dragging=%s\n", 
                debug_count, x, y, state->dragging ? "YES" : "NO");
    }
    static int motion_log_count = 0;
    if (++motion_log_count <= 5) {
         loggy_window_management( "[WM] Mouse motion: (%.1f, %.1f)\n", x, y);
    }
    state->mouse_events_processed++;
    
    update_mouse_smoother(&state->mouse_smoother, x, y);
    
    double smoothed_x, smoothed_y;
    get_smoothed_mouse_pos(&state->mouse_smoother, &smoothed_x, &smoothed_y);
    
    state->last_mouse_x = smoothed_x;
    state->last_mouse_y = smoothed_y;
    
    if (state->dragging && state->dragged_window_index >= 0) {
        double window_dx = smoothed_x - state->drag_start_x;
        double window_dy = smoothed_y - state->drag_start_y;
        
        double new_x = state->window_start_x + window_dx;
        double new_y = state->window_start_y + window_dy;
        
        move_window(state, state->dragged_window_index, new_x, new_y, false);
        state->drag_operations++;
    }
}

static void handle_pointer_button(struct window_manager_state *state, 
                                 uint32_t button, uint32_t pressed) {
    double x = state->last_mouse_x;
    double y = state->last_mouse_y;
    struct timeval now;
    gettimeofday(&now, NULL);
    
    state->button_events_processed++;
    
    double time_since_last = (now.tv_sec - state->last_button_event.tv_sec) + 
                           (now.tv_usec - state->last_button_event.tv_usec) / 1000000.0;
    
    bool *button_state = NULL;
    const char *button_name = "UNKNOWN";
    
    switch (button) {
        case BTN_LEFT:
            button_state = &state->left_button_pressed;
            button_name = "LEFT";
            break;
        case BTN_RIGHT:
            button_state = &state->right_button_pressed;
            button_name = "RIGHT";
            break;
        case BTN_MIDDLE:
            button_state = &state->middle_button_pressed;
            button_name = "MIDDLE";
            break;
        default:
            return;
    }
    
    bool current_state = (pressed != 0);
    if (*button_state == current_state && time_since_last < 0.050) {
        return; // Filter duplicates
    }
    
    *button_state = current_state;
    state->last_button_event = now;
    
    fprintf(stderr, "[WM] 🖱️ %s button %s at (%.0f,%.0f)\n", 
            button_name, pressed ? "PRESSED" : "RELEASED", x, y);

    if (button != BTN_LEFT) return;
    
    if (pressed && !state->dragging) {
        // BUTTON PRESSED - Find and focus window, prepare for dragging
        fprintf(stderr, "[WM] 🔍 Looking for window at click...\n");
        int window_index = find_window_at(state, x, y);
        if (window_index >= 0) {
    struct managed_window *win = &state->windows[window_index];
    fprintf(stderr, "[WM] ✅ Found window %d (\"%s\") - focusing and preparing drag\n", 
            window_index, win->title);
    
    // Focus the clicked window immediately
    focus_window(state, window_index);
    
    // FIXED: Better draggable area detection
    double titlebar_top = win->visual_y - (win->has_titlebar ? TITLEBAR_HEIGHT : 0);
    double titlebar_bottom = win->visual_y;
    double content_drag_bottom = win->visual_y + 100; // Increased from 50 to 100 pixels
    
    bool in_titlebar = (win->has_titlebar && y >= titlebar_top && y <= titlebar_bottom);
    bool in_draggable_content = (y >= win->visual_y && y <= content_drag_bottom);
    
    fprintf(stderr, "[WM] 📏 Draggable area check:\n");
    fprintf(stderr, "[WM]   Window visual_y: %.1f\n", win->visual_y);
    fprintf(stderr, "[WM]   Titlebar: %.1f to %.1f (click in titlebar: %s)\n", 
            titlebar_top, titlebar_bottom, in_titlebar ? "YES" : "NO");
    fprintf(stderr, "[WM]   Draggable content: %.1f to %.1f (click in content: %s)\n", 
            win->visual_y, content_drag_bottom, in_draggable_content ? "YES" : "NO");
    fprintf(stderr, "[WM]   Click Y position: %.1f\n", y);
    
    if (in_titlebar || in_draggable_content) {
        // START DRAGGING IMMEDIATELY for real-time feedback
        fprintf(stderr, "[WM] 🚀 Starting immediate drag on window %d (%s area)\n", 
                window_index, in_titlebar ? "titlebar" : "content");
        
        state->dragging = true;
        state->dragged_window_index = window_index;
        state->drag_start_x = x;
        state->drag_start_y = y;
        state->window_start_x = win->visual_x;
        state->window_start_y = win->visual_y;
        win->dragging = true;
        
        // Reset mouse smoother for new drag
        init_mouse_smoother(&state->mouse_smoother);
    } else {
        fprintf(stderr, "[WM] 👆 Click not in draggable area - just focusing\n");
    }
}else {
            fprintf(stderr, "[WM] ❌ No window found at click position\n");
        }
        
    } else if (!pressed && state->dragging) {
        // BUTTON RELEASED - End active drag
        fprintf(stderr, "[WM] 🛑 DRAG END for window %d\n", state->dragged_window_index);
        
        if (state->dragged_window_index >= 0) {
            struct managed_window *win = &state->windows[state->dragged_window_index];
            win->dragging = false;
            
            // Send final position
            move_window(state, state->dragged_window_index, win->target_x, win->target_y, true);
            fprintf(stderr, "[WM] ✅ Final position sent: (%.0f, %.0f)\n", 
                    win->target_x, win->target_y);
        }
        
        state->dragging = false;
        state->dragged_window_index = -1;
        
    } else if (!pressed && !state->dragging) {
        // Just a normal click end
        fprintf(stderr, "[WM] 👆 Normal click end (no drag)\n");
        
    } else if (pressed && state->dragging) {
        // Already dragging, ignore
        fprintf(stderr, "[WM] 🚫 Ignoring button press while already dragging\n");
    }
}
static void flush_pending_moves(struct window_manager_state *state) {
    if (state->pending_moves > 0 && state->dragged_window_index >= 0) {
        struct managed_window *win = &state->windows[state->dragged_window_index];
        move_window(state, state->dragged_window_index, win->target_x, win->target_y, true);
    }
}

int main(int argc, char *argv[]) {
    loggy_window_management( "[WM] === Enhanced Window Manager with Proper Z-Order ===\n");
    
    int ipc_fd = -1;
    const char* shm_name = NULL;
    
    if (argc >= 3) {
        if (strncmp(argv[1], "--", 2) != 0) {
            ipc_fd = atoi(argv[1]);
            shm_name = argv[2];
        } else {
            for (int i = 1; i < argc; i++) {
                if (strcmp(argv[i], "--ipc-fd") == 0 && i + 1 < argc) {
                    ipc_fd = atoi(argv[++i]);
                } else if (strcmp(argv[i], "--shm-name") == 0 && i + 1 < argc) {
                    shm_name = argv[++i];
                }
            }
        }
    }
    
    if (ipc_fd < 0 || !shm_name) {
        loggy_window_management( "[WM] ❌ CRITICAL ERROR: Missing arguments.\n");
        loggy_window_management( "[WM] Usage: %s --ipc-fd <fd> --shm-name <name>\n", argv[0]);
        return 1;
    }
    
    loggy_window_management( "[WM] 🚀 Starting with IPC fd=%d and SHM=%s\n", ipc_fd, shm_name);
    
    struct window_manager_state state = {0};
    state.ipc_fd = ipc_fd;
    state.focused_window_index = -1;
    state.dragged_window_index = -1;
    state.screen_width = SHM_WIDTH;
    state.screen_height = SHM_HEIGHT;
    state.next_window_id = 1;
    state.next_z_order = 1;  // NEW: Initialize z-order counter
    
    init_mouse_smoother(&state.mouse_smoother);
    gettimeofday(&state.last_frame_time, NULL);

    int shm_fd = shm_open(shm_name, O_RDWR, 0600);
    if (shm_fd < 0) {
        loggy_window_management( "[WM] ❌ CRITICAL ERROR: Failed to open SHM file descriptor '%s': %s\n", shm_name, strerror(errno));
        return 1;
    }
    
    state.shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    
    if (state.shm_ptr == MAP_FAILED) {
        loggy_window_management( "[WM] ❌ CRITICAL ERROR: Failed to mmap SHM buffer: %s\n", strerror(errno));
        return 1;
    }
    
    loggy_window_management( "[WM] ✅ SHM connected successfully.\n");
    loggy_window_management( "[WM] ✅ Enhanced Window Manager with Z-order is ready.\n");
    
    // Main event loop
    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(state.ipc_fd, &read_fds);
        
        struct timeval timeout = {0, 4166}; 
        int select_result = select(state.ipc_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result < 0) {
            loggy_window_management( "[WM] ❌ select() error: %s\n", strerror(errno));
            break;
        } 
        
        if (select_result > 0) {
            ipc_event_t event;
            ssize_t n = read(state.ipc_fd, &event, sizeof(event));
            if (n <= 0) {
                if (n == 0) loggy_window_management( "[WM] 🔌 IPC connection closed by compositor.\n");
                else loggy_window_management( "[WM] ❌ IPC read error: %s\n", strerror(errno));
                break;
            }
            
            if (n != sizeof(event)) {
                loggy_window_management( "[WM] ⚠️ Partial read: %zd bytes, expected %zu\n", n, sizeof(event));
                continue;
            }
            
            switch (event.type) {
                case IPC_EVENT_TYPE_WINDOW_LIST_UPDATE:
                    handle_window_list_update(&state, &event.data.window_list);
                    break;
                case IPC_EVENT_TYPE_POINTER_MOTION:
                    handle_mouse_motion(&state, event.data.motion.x, event.data.motion.y);
                    break;
                case IPC_EVENT_TYPE_POINTER_BUTTON:
                     loggy_window_management( "[WM] 🖱️ Received POINTER_BUTTON: button=%u, state=%u\n", 
                event.data.button.button, event.data.button.state);
                    handle_pointer_button(&state, event.data.button.button, event.data.button.state);
                    break;
                default:
                    loggy_window_management( "[WM] ❓ Unknown IPC event type: %d\n", event.type);
                    break;
            }
        }

        flush_pending_moves(&state);
        
        state.frames_processed++;
        if (state.frames_processed % 4 == 0) {
            ipc_notification_t frame_notification = {0};
            frame_notification.type = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
            ssize_t written = write(state.ipc_fd, &frame_notification, sizeof(frame_notification));
            if (written < 0) {
                loggy_window_management( "[WM] ❌ Failed to send frame notification: %s\n", strerror(errno));
                break;
            }
        }
        
        if (state.frames_processed % 600 == 0) {
            loggy_window_management( "[WM] 📊 Enhanced Stats with Z-Order:\n");
            loggy_window_management( "[WM]   - Mouse events: %llu\n", (unsigned long long)state.mouse_events_processed);
            loggy_window_management( "[WM]   - Button events: %llu (filtered: %llu)\n", 
                    (unsigned long long)state.button_events_processed,
                    (unsigned long long)state.duplicate_events_filtered);
            loggy_window_management( "[WM]   - Window raises: %llu\n", (unsigned long long)state.window_raises);
            loggy_window_management( "[WM]   - Predictions made: %llu\n", (unsigned long long)state.predictions_made);
            loggy_window_management( "[WM]   - Successful moves: %llu\n", (unsigned long long)state.successful_moves);
            loggy_window_management( "[WM]   - Pending moves: %d\n", state.pending_moves);
            loggy_window_management( "[WM]   - Next z-order: %d\n", state.next_z_order);
        }
    }
    
    // Print final stats before shutdown
    loggy_window_management( "[WM] 📊 Final Enhanced Stats:\n");
    loggy_window_management( "[WM]   - Mouse events processed: %llu\n", (unsigned long long)state.mouse_events_processed);
    loggy_window_management( "[WM]   - Button events processed: %llu\n", (unsigned long long)state.button_events_processed);
    loggy_window_management( "[WM]   - Duplicate events filtered: %llu\n", (unsigned long long)state.duplicate_events_filtered);
    loggy_window_management( "[WM]   - Window raises: %llu\n", (unsigned long long)state.window_raises);
    loggy_window_management( "[WM]   - Predictions made: %llu\n", (unsigned long long)state.predictions_made);
    loggy_window_management( "[WM]   - Successful moves: %llu\n", (unsigned long long)state.successful_moves);
    loggy_window_management( "[WM]   - Successful focus changes: %llu\n", (unsigned long long)state.successful_focus_changes);
    loggy_window_management( "[WM]   - IPC errors: %llu\n", (unsigned long long)state.ipc_errors);
    loggy_window_management( "[WM]   - Drag operations: %llu\n", (unsigned long long)state.drag_operations);
    
    loggy_window_management( "[WM] 🛑 Shutting down...\n");
    munmap(state.shm_ptr, SHM_BUFFER_SIZE);
    close(state.ipc_fd);
    
    loggy_window_management( "[WM] ✅ Shutdown complete.\n");
    return 0;
}

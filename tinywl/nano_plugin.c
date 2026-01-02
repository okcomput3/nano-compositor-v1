// nano_plugin.c - FINAL STABLE SEMI-HYBRID PLUGIN MANAGEMENT WITH WINDOW MANAGEMENT
#include "nano_compositor.h"
#include "nano_ipc.h"
#include "concurrent_queue.h"
#include "logs.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> 


// Forward declaration for the thread function
static void* plugin_comm_thread_func(void* arg);

// Forward declarations for window management functions
static void handle_window_move_request(struct nano_server *server, int window_index, int x, int y);
static void handle_window_focus_request(struct nano_server *server, int window_index);
static void handle_window_resize_request(struct nano_server *server, int window_index, int width, int height);

// Initializes the plugin manager and starts the communication thread.
void nano_plugin_manager_init(struct nano_server *server) {
    server->render_queue = queue_create();
    if (!server->render_queue) {
        loggy_plugins(  "Failed to create render notification queue");
        return;
    }

    pthread_t comm_thread;
    if (pthread_create(&comm_thread, NULL, plugin_comm_thread_func, server) != 0) {
        loggy_plugins(  "Failed to create plugin communication thread");
    }
    pthread_detach(comm_thread);
}
// The communication thread: listens for notifications from plugins with DMA-BUF and window management support
static void* plugin_comm_thread_func(void* arg) {
    struct nano_server *server = (struct nano_server*)arg;
    loggy_plugins("Plugin communication thread started with DMA-BUF and window management support");

    while (true) {
        struct pollfd fds[64];
        struct spawned_plugin* plugins[64];
        int count = 0;

        pthread_mutex_lock(&server->plugin_list_mutex);
        struct spawned_plugin *p;
        wl_list_for_each(p, &server->plugins, link) {
            
            // *** CRITICAL FIX: Skip async plugins - they have their own IPC thread ***
            if (count < 64 && p->ipc_fd >= 0 && !p->is_async) {
                fds[count].fd = p->ipc_fd;
                fds[count].events = POLLIN;
                plugins[count] = p;
                count++;
                loggy_plugins("Regular comm thread monitoring plugin '%s' (PID: %d, FD: %d)", p->name, p->pid, p->ipc_fd);
            } else if (p->is_async) {
                loggy_plugins("Skipping async plugin '%s' from regular comm thread (has dedicated thread)", p->name);
            }
        }
        pthread_mutex_unlock(&server->plugin_list_mutex);

        if (count == 0) {
            usleep(100000); // Wait a bit if no regular plugins to monitor
            continue;
        }

        loggy_plugins("Regular comm thread polling %d non-async plugins", count);
        int ret = poll(fds, count, 1000); // Use timeout instead of blocking indefinitely
        if (ret <= 0) continue;

        for (int i = 0; i < count; i++) {
            if (fds[i].revents & POLLIN) {
                // Try to read a full notification structure first
                ipc_notification_t notification;
                // Try to read with potential FD first

                struct msghdr msg = {0};
                struct iovec iov = { .iov_base = &notification, .iov_len = sizeof(notification) };
                char cmsg_buf[CMSG_SPACE(sizeof(int))];
                int received_fd = -1;

                msg.msg_iov = &iov;
                msg.msg_iovlen = 1;
                msg.msg_control = cmsg_buf;
                msg.msg_controllen = sizeof(cmsg_buf);

                ssize_t n = recvmsg(fds[i].fd, &msg, MSG_DONTWAIT);

                if (n <= 0) {
                    // Plugin crashed or disconnected
                    loggy_plugins("Regular plugin %d disconnected", plugins[i]->pid);
                    render_notification_t notif = {
                        .type = RENDER_NOTIFICATION_TYPE_PLUGIN_DIED,
                        .plugin_pid = plugins[i]->pid
                    };
                    queue_push(server->render_queue, &notif);
                    plugins[i]->ipc_fd = -1;
                    continue;
                }

                // Extract file descriptor if present
                struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
                if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                    memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
                }
                
                if (n == sizeof(ipc_notification_type_t)) {
                    // Old-style simple notification (just the type)
                    ipc_notification_type_t simple_type = *((ipc_notification_type_t*)&notification);
                    
                    if (simple_type == IPC_NOTIFICATION_TYPE_FRAME_SUBMIT) {
                        render_notification_t notif = {
                            .type = RENDER_NOTIFICATION_TYPE_FRAME_READY,
                            .plugin_pid = plugins[i]->pid
                        };
                        queue_push(server->render_queue, &notif);
                        loggy_plugins("Regular plugin %d sent SHM frame notification (simple)", plugins[i]->pid);
                    } else {
                        loggy_plugins("Regular plugin %d sent unknown notification type: %d", plugins[i]->pid, simple_type);
                    }
                } else if (n == sizeof(notification)) {
                    // Full notification structure received
                    switch (notification.type) {
                        case IPC_NOTIFICATION_TYPE_FRAME_SUBMIT:
                            {
                                render_notification_t notif = {
                                    .type = RENDER_NOTIFICATION_TYPE_FRAME_READY,
                                    .plugin_pid = plugins[i]->pid
                                };
                                queue_push(server->render_queue, &notif);
                                loggy_plugins("Regular plugin %d sent SHM frame notification", plugins[i]->pid);
                            }
                            break;
                            
                        case IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT:
                            if (received_fd >= 0) {
                                // Store the DMA-BUF info in the plugin
                                if (plugins[i]->dmabuf_fd >= 0) {
                                    close(plugins[i]->dmabuf_fd);  // ✅ Close old FD
                                }
                                
                                plugins[i]->dmabuf_fd = received_fd;
                                plugins[i]->dmabuf_width = notification.data.dmabuf_frame_info.width;
                                plugins[i]->dmabuf_height = notification.data.dmabuf_frame_info.height;
                                plugins[i]->dmabuf_format = notification.data.dmabuf_frame_info.format;
                                plugins[i]->dmabuf_stride = notification.data.dmabuf_frame_info.stride;
                                plugins[i]->dmabuf_modifier = notification.data.dmabuf_frame_info.modifier;
                                plugins[i]->has_new_frame = true;
                                
                                loggy_plugins("✅ Received regular plugin DMA-BUF frame: %dx%d, fd=%d", 
                                        plugins[i]->dmabuf_width, plugins[i]->dmabuf_height, received_fd);
                            } else {
                                loggy_plugins("❌ Regular plugin sent DMA-BUF notification but no FD received");
                            }
                            break;
                            
                        case IPC_NOTIFICATION_TYPE_WINDOW_MOVE_REQUEST:
                            {
                                int window_index = notification.data.window_move_request.window_index;
                                int x = notification.data.window_move_request.x;
                                int y = notification.data.window_move_request.y;
                                
                                loggy_plugins("Regular plugin %d requests window %d move to (%d, %d)", 
                                       plugins[i]->pid, window_index, x, y);
                                       
                                handle_window_move_request(server, window_index, x, y);
                            }
                            break;
                            
                        case IPC_NOTIFICATION_TYPE_WINDOW_FOCUS_REQUEST:
                            {
                                int window_index = notification.data.window_focus_request.window_index;
                                
                                loggy_plugins("Regular plugin %d requests window %d focus", 
                                       plugins[i]->pid, window_index);
                                       
                                handle_window_focus_request(server, window_index);
                            }
                            break;
                            
                        case IPC_NOTIFICATION_TYPE_WINDOW_RESIZE_REQUEST:
                            {
                                int window_index = notification.data.window_resize_request.window_index;
                                int width = notification.data.window_resize_request.width;
                                int height = notification.data.window_resize_request.height;
                                
                                loggy_plugins("Regular plugin %d requests window %d resize to %dx%d", 
                                       plugins[i]->pid, window_index, width, height);
                                       
                                handle_window_resize_request(server, window_index, width, height);
                            }
                            break;
                            
                        default:
                            loggy_plugins("Regular plugin %d sent unknown notification type: %d", 
                                   plugins[i]->pid, notification.type);
                            break;
                    }
                } else {
                    loggy_plugins("Regular plugin %d sent partial notification: %zd bytes", plugins[i]->pid, n);
                }
            }
        }
    }
    return NULL;
}
// Helper function to get view by window index
static struct nano_view* get_view_by_index(struct nano_server *server, int window_index) {
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
    
    loggy_plugins(  "Window index %d not found", window_index);
    return NULL;
}

// Window management functions using your existing API
// In nano_plugin.c

static void handle_window_move_request(struct nano_server *server, int window_index, int x, int y) {
    // Queue the move for safe processing on the main thread.
    render_notification_t notification = {0};
    
    // Set the type of the notification.
    notification.type = RENDER_NOTIFICATION_TYPE_WINDOW_MOVE;
    
    // **THIS IS THE CORRECT WAY FOR YOUR PROJECT**
    // Populate the data by accessing the fields through the 'move_data' member of the union.
    notification.move_data.window_index = window_index;
    notification.move_data.move_x = (double)x;  // The cast to double is important if the struct defines it as such
    notification.move_data.move_y = (double)y;  // The cast to double is important if the struct defines it as such
    
    // Push the correctly formatted notification onto the queue.
    queue_push(server->render_queue, &notification);
    
    wlr_log(WLR_DEBUG, "Queued window move request: window %d to (%d, %d)", 
            window_index, x, y);
}
static void handle_window_focus_request(struct nano_server *server, int window_index) {
    loggy_plugins("📨 Focus request for window %d (with aggressive raising)", window_index);
    
    int current_index = 0;
    struct nano_view *view;
    wl_list_for_each(view, &server->views, link) {
        if (current_index == window_index) {
            if (view && view->scene_tree) {
                loggy_plugins("🔧 AGGRESSIVELY raising window %d to top", window_index);
                
                // SUPER AGGRESSIVE RAISING - multiple methods
                // Method 1: Multiple wlroots raise calls
                for (int i = 0; i < 5; i++) {
                    wlr_scene_node_raise_to_top(&view->scene_tree->node);
                }
                
                // Method 2: Disable/enable to force scene update
                wlr_scene_node_set_enabled(&view->scene_tree->node, false);
                wlr_scene_node_set_enabled(&view->scene_tree->node, true);
                
                // Method 3: Move to end of view list (visual top)
                wl_list_remove(&view->link);
                wl_list_insert(server->views.prev, &view->link);
                
                loggy_plugins("✅ Window %d raised using all methods", window_index);
            }
            
            // THEN call normal focus
            if (view && server->plugin_api.view_focus) {
                server->plugin_api.view_focus(view);
                loggy_plugins("✅ Successfully focused window %d", window_index);
            }
            return;
        }
        current_index++;
    }
    
    loggy_plugins("❌ Window index %d not found", window_index);
}

static void handle_window_resize_request(struct nano_server *server, int window_index, int width, int height) {
    loggy_plugins(  "Resizing window %d to %dx%d", window_index, width, height);
    
    // Get the view by window index
    struct nano_view *view = get_view_by_index(server, window_index);
    if (!view) {
        loggy_plugins(  "Cannot find view for window index %d", window_index);
        return;
    }
    
    // Use your existing API function to resize the window
    server->plugin_api.view_resize(view, width, height);
    
    loggy_plugins(  "Successfully resized window %d (%s) to %dx%d", 
            window_index,
            (view->xdg_toplevel && view->xdg_toplevel->title) ? view->xdg_toplevel->title : "Unknown",
            width, height);
}

/*
// Spawns a plugin as standalone, creating an IPC socket and a shared memory buffer.
void nano_plugin_spawn(struct nano_server *server, const char *path, const char* name) {
    int ipc_sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ipc_sockets) < 0) {
        loggy_plugins(  "Failed to create IPC socket pair: %s", strerror(errno));
        return;
    }

    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/nano-plugin-%s-%d", name, getpid());

    shm_unlink(shm_name);
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (shm_fd < 0) { 
        loggy_plugins(  "Failed to create SHM %s: %s", shm_name, strerror(errno));
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }
    if (ftruncate(shm_fd, SHM_BUFFER_SIZE) < 0) { 
        loggy_plugins(  "Failed to resize SHM %s: %s", shm_name, strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }

    // DEBUG: Show SHM creation
    loggy_plugins(  "Created SHM: %s (size: %d bytes)", shm_name, SHM_BUFFER_SIZE);

    // Find available display socket for standalone mode
    char display_socket[256];
    const char *display_name = NULL;
    
    for (int i = 20; i < 64; i++) {
        snprintf(display_socket, sizeof(display_socket), "wayland-%d", i);
        char socket_path[512];
        snprintf(socket_path, sizeof(socket_path), "%s/%s", 
                getenv("XDG_RUNTIME_DIR") ?: "/tmp", display_socket);
        
        if (access(socket_path, F_OK) != 0) {
            display_name = display_socket;
            break;
        }
    }
    
    if (!display_name) {
        // Fallback to a unique name
        snprintf(display_socket, sizeof(display_socket), "nano-%s-%d", name, getpid());
        display_name = display_socket;
    }

    loggy_plugins(  "Launching standalone nano compositor %s on display %s", name, display_name);

    // Check if the path exists, if not try some common locations
    char resolved_path[1024];
    char absolute_path[1024];
    
    if (access(path, X_OK) == 0) {
        // Convert to absolute path before we change directories
        if (path[0] == '/') {
            // Already absolute
            strncpy(resolved_path, path, sizeof(resolved_path) - 1);
        } else {
            // Make it absolute
            char *cwd = getcwd(NULL, 0);
            snprintf(resolved_path, sizeof(resolved_path), "%s/%s", cwd, path);
            free(cwd);
        }
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    } else {
        // Extract filename from path manually
        const char *filename = strrchr(path, '/');
        if (filename) {
            filename++; // Skip the '/'
        } else {
            filename = path; // No '/' found, use whole string
        }
        
        char *cwd = getcwd(NULL, 0);
        
        // Try current directory
        snprintf(absolute_path, sizeof(absolute_path), "%s/%s", cwd, filename);
        if (access(absolute_path, X_OK) == 0) {
            strncpy(resolved_path, absolute_path, sizeof(resolved_path) - 1);
        } else {
            // Try plugins subdirectory
            snprintf(absolute_path, sizeof(absolute_path), "%s/plugins/%s", cwd, filename);
            if (access(absolute_path, X_OK) == 0) {
                strncpy(resolved_path, absolute_path, sizeof(resolved_path) - 1);
            } else {
                loggy_plugins(  "Cannot find executable: %s (tried %s/%s, %s/plugins/%s, %s)", 
                       path, cwd, filename, cwd, filename, path);
                free(cwd);
                close(shm_fd);
                close(ipc_sockets[0]);
                close(ipc_sockets[1]);
                return;
            }
        }
        free(cwd);
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    }
    
    loggy_plugins(  "Using resolved path: %s", resolved_path);
    
    // Debug: verify the file exists and is executable
    struct stat st;
    if (stat(resolved_path, &st) == 0) {
        loggy_plugins(  "File exists: %s (mode: %o, size: %ld)", resolved_path, st.st_mode, st.st_size);
        if (st.st_mode & S_IXUSR) {
            loggy_plugins(  "File is executable");
        } else {
            loggy_plugins(  "File is NOT executable - fixing permissions");
            chmod(resolved_path, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
        }
    } else {
        loggy_plugins(  "File does not exist: %s", resolved_path);
    }

    pid_t pid = fork();
    if (pid < 0) { 
        loggy_plugins(  "Failed to fork: %s", strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }

    if (pid == 0) { // Child - make it standalone
        close(ipc_sockets[0]);
        close(shm_fd);
        
        // Get original working directory BEFORE changing it
        char *original_cwd = getcwd(NULL, 0);
        
        // Become session leader for independence
        if (setsid() < 0) {
            loggy_plugins(  "Failed to create session: %s", strerror(errno));
            exit(1);
        }
        
        // Change to safe directory
        if (chdir("/tmp") < 0) {
            loggy_plugins(  "Failed to change directory: %s", strerror(errno));
            exit(1);
        }
        
        // Create new process group
        setpgid(0, 0);
        
        char socket_str[16];
        snprintf(socket_str, sizeof(socket_str), "%d", ipc_sockets[1]);
        
        // Set up ISOLATED environment for standalone operation
        char *orig_wayland_display = getenv("WAYLAND_DISPLAY");
        char *orig_wlr_backends = getenv("WLR_BACKENDS");
        char *orig_wlr_drm = getenv("WLR_DRM_DEVICES");
        
        // CRITICAL: Force isolated display BEFORE any other setup
        setenv("WAYLAND_DISPLAY", display_name, 1);
        
        // Force headless backend to prevent any hardware conflicts
        setenv("WLR_BACKENDS", "headless", 1);
        
        // If you want it to display in a window, use wayland backend instead:
        // setenv("WLR_BACKENDS", "wayland", 1);
        
        // Prevent DRM device conflicts completely
        unsetenv("WLR_DRM_DEVICES");
        unsetenv("WLR_DRM_NO_ATOMIC");
        unsetenv("WLR_DIRECT_TTY");
        
        
        // Disable hardware features that might conflict
        setenv("WLR_NO_HARDWARE_CURSORS", "1", 1);
        setenv("WLR_LIBINPUT_NO_DEVICES", "1", 1);
        
        // Create isolated XDG_RUNTIME_DIR to prevent socket conflicts
        char isolated_runtime[512];
        snprintf(isolated_runtime, sizeof(isolated_runtime), "/tmp/nano_runtime_%s_%d", name, getpid());
        mkdir(isolated_runtime, 0700);
        setenv("XDG_RUNTIME_DIR", isolated_runtime, 1);
        
        // Set up library path for shared DMA-BUF library using original directory
        if (original_cwd) {
            char lib_path[2048];
            snprintf(lib_path, sizeof(lib_path), "%s/libdmabuf_share", original_cwd);
            
            char *existing_path = getenv("LD_LIBRARY_PATH");
            if (existing_path) {
                char new_path[4096];
                snprintf(new_path, sizeof(new_path), "%s:%s", lib_path, existing_path);
                setenv("LD_LIBRARY_PATH", new_path, 1);
            } else {
                setenv("LD_LIBRARY_PATH", lib_path, 1);
            }
            
            loggy_plugins(  "Set standalone plugin LD_LIBRARY_PATH to include: %s", lib_path);
            free(original_cwd);
        }
        
        loggy_plugins(  "Executing standalone plugin with SHM: %s, Display: %s, Runtime: %s", 
               shm_name, display_name, isolated_runtime);
        
        // Debug: Show what we're actually trying to execute
        loggy_plugins(  "Attempting to execute: %s", resolved_path);
        loggy_plugins(  "Current working directory: %s", getcwd(NULL, 0));
        
        // Try different argument combinations for standalone mode
        
        // First try: with standalone flags
        loggy_plugins(  "Exec attempt 1: standalone mode with full flags");
        execl(resolved_path, resolved_path, 
              "--standalone",
              "--display", display_name,
              "--ipc-fd", socket_str, 
              "--shm-name", shm_name,
              "--headless",
              (char*)NULL);
        
        // Second try: with basic standalone args
        loggy_plugins(  "Exec attempt 2: standalone mode with basic args");
        execl(resolved_path, resolved_path,
              "--standalone",
              socket_str, 
              shm_name, 
              display_name,
              (char*)NULL);
              
        // Third try: original format but with display
        loggy_plugins(  "Exec attempt 3: original format with display");
        execl(resolved_path, resolved_path, 
              socket_str, 
              shm_name, 
              display_name,
              (char*)NULL);
        
        // Fourth try: just the basics (your original working format)
        loggy_plugins(  "Exec attempt 4: basic format");
        execl(resolved_path, resolved_path, socket_str, shm_name, (char*)NULL);
        
        loggy_plugins(  "All exec attempts failed for %s: %s", resolved_path, strerror(errno));
        exit(1);
    }

    // Parent
    close(ipc_sockets[1]);

    struct spawned_plugin *p = calloc(1, sizeof(struct spawned_plugin));
    if (!p) {
        loggy_plugins(  "Failed to allocate memory for plugin struct");
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return;
    }

    p->pid = pid;
    p->ipc_fd = ipc_sockets[0];
    strncpy(p->name, name, sizeof(p->name) - 1);
    strncpy(p->shm_name, shm_name, sizeof(p->shm_name) - 1);
    
    // Store display info in the name for tracking (ensure it fits in 64 bytes)
    // Use a safer approach that the compiler can verify
    size_t name_len = strnlen(name, sizeof(p->name) - 1);
    size_t display_len = strnlen(display_name, sizeof(p->name) - 1);
    
    // Check if we can fit "name@display" in the buffer
    if (name_len + 1 + display_len < sizeof(p->name)) {
        // Safe to combine - compiler can see the bounds
        int written = snprintf(p->name, sizeof(p->name), "%.*s@%.*s", 
                              (int)name_len, name, 
                              (int)(sizeof(p->name) - name_len - 2), display_name);
        if (written >= sizeof(p->name)) {
            // Fallback if somehow it didn't fit
            strncpy(p->name, name, sizeof(p->name) - 1);
            p->name[sizeof(p->name) - 1] = '\0';
        }
    } else {
        // Just use the name, safely truncated
        strncpy(p->name, name, sizeof(p->name) - 1);
        p->name[sizeof(p->name) - 1] = '\0';
    }
    
    // Map SHM as READ-WRITE for both compositor and plugin access
    p->shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (p->shm_ptr == MAP_FAILED) { 
        loggy_plugins(  "Failed to mmap SHM %s: %s", shm_name, strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return; 
    }
    close(shm_fd);
    
    // DEBUG: Show compositor SHM mapping
    loggy_plugins(  "Compositor mapped SHM: %s at %p", shm_name, p->shm_ptr);
    
    // Write test pattern to verify shared memory works
    uint32_t* test_ptr = (uint32_t*)p->shm_ptr;
    test_ptr[0] = 0x57A4DA10; // STANDALONE (in leetspeak-ish hex)
    test_ptr[1] = 0x87654321;
    loggy_plugins(  "Compositor wrote standalone test pattern: 0x%08x 0x%08x", test_ptr[0], test_ptr[1]);
    
    // Initialize texture and frame state
    p->texture = NULL;
    p->has_new_frame = false;
    
    // Initialize DMA-BUF fields
    p->dmabuf_fd = -1;
    p->dmabuf_width = 0;
    p->dmabuf_height = 0;
    p->dmabuf_format = 0;
    p->dmabuf_stride = 0;
    p->dmabuf_modifier = 0;

    // Add to server's plugin list
    pthread_mutex_lock(&server->plugin_list_mutex);
    wl_list_insert(&server->plugins, &p->link);
    pthread_mutex_unlock(&server->plugin_list_mutex);
    
    loggy_plugins(  "Spawned standalone plugin: %s (PID %d) on display %s with DMA-BUF support", 
           name, p->pid, display_name);
}*/

void nano_plugin_spawn(struct nano_server *server, const char *path, const char* name) {
    
    int ipc_sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ipc_sockets) < 0) {
        loggy_plugins(  "Failed to create IPC socket pair: %s", strerror(errno));
        return;
    }

    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/nano-plugin-%s-%d", name, getpid());

    shm_unlink(shm_name);
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (shm_fd < 0) { 
        loggy_plugins(  "Failed to create SHM %s: %s", shm_name, strerror(errno));
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }
    if (ftruncate(shm_fd, SHM_BUFFER_SIZE) < 0) { 
        loggy_plugins(  "Failed to resize SHM %s: %s", shm_name, strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }

    // DEBUG: Show SHM creation
    loggy_plugins(  "Created SHM: %s (size: %d bytes)", shm_name, SHM_BUFFER_SIZE);

    // Find available display socket for standalone mode
    char display_socket[256];
    const char *display_name = NULL;
    
    for (int i = 20; i < 64; i++) {
        snprintf(display_socket, sizeof(display_socket), "wayland-%d", i);
        char socket_path[512];
        snprintf(socket_path, sizeof(socket_path), "%s/%s", 
                getenv("XDG_RUNTIME_DIR") ?: "/tmp", display_socket);
        
        if (access(socket_path, F_OK) != 0) {
            display_name = display_socket;
            break;
        }
    }
    
    if (!display_name) {
        // Fallback to a unique name
        snprintf(display_socket, sizeof(display_socket), "nano-%s-%d", name, getpid());
        display_name = display_socket;
    }

    loggy_plugins(  "Launching standalone nano compositor %s on display %s", name, display_name);

    // Check if the path exists, if not try some common locations
    char resolved_path[1024];
    char absolute_path[1024];
    
    if (access(path, X_OK) == 0) {
        // Convert to absolute path before we change directories
        if (path[0] == '/') {
            // Already absolute
            strncpy(resolved_path, path, sizeof(resolved_path) - 1);
        } else {
            // Make it absolute
            char *cwd = getcwd(NULL, 0);
            snprintf(resolved_path, sizeof(resolved_path), "%s/%s", cwd, path);
            free(cwd);
        }
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    } else {
        // Extract filename from path manually
        const char *filename = strrchr(path, '/');
        if (filename) {
            filename++; // Skip the '/'
        } else {
            filename = path; // No '/' found, use whole string
        }
        
        char *cwd = getcwd(NULL, 0);
        
        // Try current directory
        snprintf(absolute_path, sizeof(absolute_path), "%s/%s", cwd, filename);
        if (access(absolute_path, X_OK) == 0) {
            strncpy(resolved_path, absolute_path, sizeof(resolved_path) - 1);
        } else {
            // Try plugins subdirectory
            snprintf(absolute_path, sizeof(absolute_path), "%s/plugins/%s", cwd, filename);
            if (access(absolute_path, X_OK) == 0) {
                strncpy(resolved_path, absolute_path, sizeof(resolved_path) - 1);
            } else {
                loggy_plugins(  "Cannot find executable: %s (tried %s/%s, %s/plugins/%s, %s)", 
                       path, cwd, filename, cwd, filename, path);
                free(cwd);
                close(shm_fd);
                close(ipc_sockets[0]);
                close(ipc_sockets[1]);
                return;
            }
        }
        free(cwd);
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    }
    
    loggy_plugins(  "Using resolved path: %s", resolved_path);
    
    // Debug: verify the file exists and is executable
    struct stat st;
    if (stat(resolved_path, &st) == 0) {
        loggy_plugins(  "File exists: %s (mode: %o, size: %ld)", resolved_path, st.st_mode, st.st_size);
        if (st.st_mode & S_IXUSR) {
            loggy_plugins(  "File is executable");
        } else {
            loggy_plugins(  "File is NOT executable - fixing permissions");
            chmod(resolved_path, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
        }
    } else {
        loggy_plugins(  "File does not exist: %s", resolved_path);
    }

    pid_t pid = fork();
    if (pid < 0) { 
        loggy_plugins(  "Failed to fork: %s", strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }

if (pid == 0) { 
    // ZERO ISOLATION but preserve library path
    close(ipc_sockets[0]);
    close(shm_fd);
    
    // Get current working directory before any changes
    char *original_cwd = getcwd(NULL, 0);
    
    char socket_str[16];
    snprintf(socket_str, sizeof(socket_str), "%d", ipc_sockets[1]);
    
    // Only set the display and library path - NOTHING ELSE
    setenv("WAYLAND_DISPLAY", display_name, 1);
    
    // Set up library path for shared DMA-BUF library
    if (original_cwd) {
        char lib_path[2048];
        snprintf(lib_path, sizeof(lib_path), "%s/libdmabuf_share", original_cwd);
        
        char *existing_path = getenv("LD_LIBRARY_PATH");
        if (existing_path) {
            char new_path[4096];
            snprintf(new_path, sizeof(new_path), "%s:%s", lib_path, existing_path);
            setenv("LD_LIBRARY_PATH", new_path, 1);
        } else {
            setenv("LD_LIBRARY_PATH", lib_path, 1);
        }
        
        loggy_plugins(  "Set plugin LD_LIBRARY_PATH to include: %s", lib_path);
        free(original_cwd);
    }
    
    loggy_plugins(  "Zero isolation exec: %s %s %s", resolved_path, socket_str, shm_name);
    
    // Single exec attempt
    execl(resolved_path, resolved_path, socket_str, shm_name, (char*)NULL);
    
    loggy_plugins(  "Exec failed: %s", strerror(errno));
    exit(1);
}
    // Parent
    close(ipc_sockets[1]);

    struct spawned_plugin *p = calloc(1, sizeof(struct spawned_plugin));
    if (!p) {
        loggy_plugins(  "Failed to allocate memory for plugin struct");
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return;
    }
p->is_async = false; 


    p->pid = pid;
    p->ipc_fd = ipc_sockets[0];
    strncpy(p->name, name, sizeof(p->name) - 1);
    strncpy(p->shm_name, shm_name, sizeof(p->shm_name) - 1);
    
    // Store display info in the name for tracking (ensure it fits in 64 bytes)
    size_t name_len = strnlen(name, sizeof(p->name) - 1);
    size_t display_len = strnlen(display_name, sizeof(p->name) - 1);
    
    if (name_len + 1 + display_len < sizeof(p->name)) {
        int written = snprintf(p->name, sizeof(p->name), "%.*s@%.*s", 
                              (int)name_len, name, 
                              (int)(sizeof(p->name) - name_len - 2), display_name);
        if (written >= sizeof(p->name)) {
            strncpy(p->name, name, sizeof(p->name) - 1);
            p->name[sizeof(p->name) - 1] = '\0';
        }
    } else {
        strncpy(p->name, name, sizeof(p->name) - 1);
        p->name[sizeof(p->name) - 1] = '\0';
    }
    
    // Map SHM as READ-WRITE for both compositor and plugin access
    p->shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (p->shm_ptr == MAP_FAILED) { 
        loggy_plugins(  "Failed to mmap SHM %s: %s", shm_name, strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return; 
    }
    close(shm_fd);
    
    // DEBUG: Show compositor SHM mapping
    loggy_plugins(  "Compositor mapped SHM: %s at %p", shm_name, p->shm_ptr);
    
    // Write test pattern to verify shared memory works
    uint32_t* test_ptr = (uint32_t*)p->shm_ptr;
    test_ptr[0] = 0x57A4DA10; // STANDALONE (in leetspeak-ish hex)
    test_ptr[1] = 0x87654321;
    loggy_plugins(  "Compositor wrote standalone test pattern: 0x%08x 0x%08x", test_ptr[0], test_ptr[1]);
    
    // Initialize texture and frame state
    p->texture = NULL;
    p->has_new_frame = false;

    
    
    // Initialize DMA-BUF fields
    p->dmabuf_fd = -1;
    p->dmabuf_width = 0;
    p->dmabuf_height = 0;
    p->dmabuf_format = 0;
    p->dmabuf_stride = 0;
    p->dmabuf_modifier = 0;

    // Add to server's plugin list
    pthread_mutex_lock(&server->plugin_list_mutex);
    wl_list_insert(&server->plugins, &p->link);
    pthread_mutex_unlock(&server->plugin_list_mutex);
    
    loggy_plugins(  "Spawned standalone plugin: %s (PID %d) on display %s with GPU environment preserved", 
           name, p->pid, display_name);
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


// In nano_server.c (or nano_plugin.c where you placed the function)

/**
 * LEGACY FALLBACK for allocating a DMA-BUF.
 * This version avoids the newer wlr_renderer_get_dmabuf_render_formats function
 * and instead manually requests a widely-supported format. This is compatible
 * with older versions of wlroots.
 *
 * @return A valid wlr_buffer on success, or NULL on failure.
 */
static struct wlr_buffer *nano_server_alloc_buffer_legacy(
    struct nano_server *server, int width, int height) {

    // 1. Manually create a format specification. We are making a safe assumption
    //    that the hardware supports ARGB8888 with a LINEAR memory layout.
    //    This is the most common and compatible format.
    struct wlr_drm_format format_spec = {0};
    uint64_t linear_modifier = DRM_FORMAT_MOD_LINEAR;

    format_spec.format = DRM_FORMAT_ARGB8888;
    format_spec.len = 1; // THIS IS THE KEY FIX: We state that we are providing ONE valid modifier.
    format_spec.modifiers = &linear_modifier; // Point to the LINEAR modifier.

    wlr_log(WLR_INFO, "Attempting to allocate plugin buffer with legacy method (format 0x%X, LINEAR)", format_spec.format);

    // 2. Allocate the buffer using this manually-constructed, valid format spec.
    //    Because format_spec.len is now 1 (which is > 0), the assertion will not fail.
    struct wlr_buffer *buffer = wlr_allocator_create_buffer(server->allocator, width, height, &format_spec);

    if (!buffer) {
        wlr_log(WLR_ERROR, "Legacy buffer allocation failed. The hardware might not support linear ARGB8888.");
        return NULL;
    }

    wlr_log(WLR_INFO, "✅ Successfully allocated a buffer for the plugin using the legacy method.");
    return buffer;
}

// In nano_server.c (or nano_plugin.c)

// NOTE: Make sure the nano_server_alloc_buffer_legacy function from step 1 is in this file, above this function.
/*
void nano_plugin_spawn_python(struct nano_server *server, const char *path, const char* name) {
    int ipc_sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ipc_sockets) < 0) {
        loggy_plugins("Failed to create IPC socket pair: %s", strerror(errno));
        return;
    }

    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/nano-plugin-%s-%d", name, getpid());
    shm_unlink(shm_name);
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (shm_fd < 0 || ftruncate(shm_fd, SHM_BUFFER_SIZE) < 0) {
        loggy_plugins("Failed to create SHM %s: %s", shm_name, strerror(errno));
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        if (shm_fd >= 0) close(shm_fd);
        return;
    }

    char resolved_path[1024];
    if (access(path, X_OK) == 0) {
        strncpy(resolved_path, path, sizeof(resolved_path) - 1);
    } else {
        loggy_plugins("Cannot find or execute plugin: %s", path);
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        loggy_plugins("Failed to fork: %s", strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return;
    }

    if (pid == 0) { // Child process
        close(ipc_sockets[0]);
        close(shm_fd);

        char socket_str[16];
        snprintf(socket_str, sizeof(socket_str), "%d", ipc_sockets[1]);

        const char *python_script_name = "plugins/hosted_mouse_plugin.py";
        loggy_plugins("Preparing to exec: %s %s %s", resolved_path, socket_str, python_script_name);

        execl(resolved_path, resolved_path, socket_str, python_script_name, (char*)NULL);

        loggy_plugins("Exec failed: %s", strerror(errno));
        exit(1);
    }

    // ===== PARENT PROCESS =====
    close(ipc_sockets[1]);

    struct spawned_plugin *p = calloc(1, sizeof(struct spawned_plugin));
    if (!p) {
        loggy_plugins("Failed to allocate memory for plugin struct");
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return;
    }

    p->pid = pid;
    p->ipc_fd = ipc_sockets[0];
    strncpy(p->name, name, sizeof(p->name) - 1);
    strncpy(p->shm_name, shm_name, sizeof(p->shm_name) - 1);
    p->shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (p->shm_ptr == MAP_FAILED) {
        loggy_plugins("Failed to mmap SHM: %s", strerror(errno));
        close(p->ipc_fd);
        kill(pid, SIGTERM);
        free(p);
        return;
    }

    // ======================================================================
    // +++ START: LINKER-SAFE BUFFER ALLOCATION LOGIC +++
    // ======================================================================

    int buffer_width = 1920;
    int buffer_height = 1080;

    // *** THE FIX IS HERE: Call the legacy function instead of the new one. ***
    p->compositor_buffer = nano_server_alloc_buffer_legacy(server, buffer_width, buffer_height);

    if (!p->compositor_buffer) {
        loggy_plugins("❌ Failed to create DMA-BUF for plugin '%s' using the legacy allocator.", p->name);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(p->ipc_fd);
        kill(pid, SIGTERM);
        free(p);
        return;
    }

    if (!wlr_buffer_get_dmabuf(p->compositor_buffer, &p->dmabuf_attrs)) {
        loggy_plugins("❌ Failed to export DMA-BUF attributes for plugin buffer");
        wlr_buffer_drop(p->compositor_buffer);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(p->ipc_fd);
        kill(pid, SIGTERM);
        free(p);
        return;
    }

    loggy_plugins("✅ Allocated buffer for plugin '%s': fd=%d, stride=%u, format=0x%x, modifier=0x%llx",
        p->name, p->dmabuf_attrs.fd[0], p->dmabuf_attrs.stride[0], p->dmabuf_attrs.format,
        (unsigned long long)p->dmabuf_attrs.modifier);

    ipc_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = IPC_EVENT_TYPE_BUFFER_ALLOCATED;
    event.data.buffer_info = p->dmabuf_attrs;

    int fd_to_send = dup(p->dmabuf_attrs.fd[0]);
    if (fd_to_send >= 0) {
        send_ipc_with_fds(p->ipc_fd, &event, &fd_to_send, 1);
        close(fd_to_send);
        loggy_plugins("✅ Sent buffer info to plugin '%s'.", p->name);
    } else {
        loggy_plugins("❌ Failed to dup FD for sending to plugin: %s", strerror(errno));
        wlr_buffer_drop(p->compositor_buffer);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(p->ipc_fd);
        kill(pid, SIGTERM);
        free(p);
        return;
    }
    // ======================================================================
    // +++ END: LINKER-SAFE BUFFER ALLOCATION LOGIC +++
    // ======================================================================

    p->has_new_frame = false;

    pthread_mutex_lock(&server->plugin_list_mutex);
    wl_list_insert(&server->plugins, &p->link);
    pthread_mutex_unlock(&server->plugin_list_mutex);

    loggy_plugins("Spawned plugin: %s (PID %d)", p->name, p->pid);
}*/

// In nano_server.c

// Make sure you still have the `nano_server_alloc_buffer_legacy` function from the previous step.
// In nano_server.c

// Make sure you still have the `nano_server_alloc_buffer_legacy` function from the previous step.

#include <errno.h>   // Required for the 'errno' variable
#include <string.h>  // Required for strerror()

// ... (other necessary includes)

static void *plugin_ipc_thread_func(void *data) {
    struct spawned_plugin *p = (struct spawned_plugin *)data;
    struct nano_server *server = p->server;

    loggy_plugins("✅ IPC listener thread started for plugin '%s' (PID: %d, FD: %d)", p->name, p->pid, p->ipc_fd);
    p->ipc_thread_running = true;

    while (p->ipc_thread_running) {
        // This struct must exactly match what Python sends with struct.pack('=Ii')
        struct {
            uint32_t type;
            int32_t index;
        } notif_msg;

        loggy_plugins("IPC-THREAD '%s': Waiting for message from plugin...", p->name);
        // Block and wait for a message from the Python plugin
        ssize_t n = read(p->ipc_fd, &notif_msg, sizeof(notif_msg));

        // This is the most critical part for debugging the disconnection.
        if (n <= 0) {
            if (n == 0) {
                // An n of 0 means the other end (the Python script) has closed the connection cleanly.
                loggy_plugins("🔌 IPC-THREAD '%s': Connection closed by plugin (read returned 0). Listener exiting.", p->name);
            } else {
                // An n of -1 means a critical error occurred. We check `errno` to see what it was.
                loggy_plugins("❌ IPC-THREAD '%s': Read error on socket: %s (errno %d). Listener exiting.", p->name, strerror(errno), errno);
            }
            p->ipc_thread_running = false; // Ensure the loop terminates
            // (Consider adding a mechanism here to notify the main thread that the plugin has died)
            break;
        }

        if (n != sizeof(notif_msg)) {
            // This is a serious issue, as it implies a protocol mismatch or data corruption.
            loggy_plugins("⚠️ IPC-THREAD '%s': Received incomplete IPC message. Expected %zu bytes, got %zd. Discarding.", p->name, sizeof(notif_msg), n);
            continue; // Try to recover by waiting for the next message
        }

        // We have a full message, let's log what we received before processing it.
        loggy_plugins("IPC-THREAD '%s': Received message. Type: %u, Index: %d", p->name, notif_msg.type, notif_msg.index);

        // Process the valid message
        if (notif_msg.type == IPC_NOTIFICATION_TYPE_FRAME_SUBMIT) {
            loggy_plugins("IPC-THREAD '%s': Message is FRAME_SUBMIT for buffer %d. Pushing to render queue.", p->name, notif_msg.index);
            render_notification_t render_notif = {
                .plugin_pid = p->pid,
                .type = RENDER_NOTIFICATION_TYPE_FRAME_READY,
                .ready_buffer_index = notif_msg.index,
            };
            // Push it onto the thread-safe queue that process_render_queue_timer reads from
            queue_push(server->render_queue, &render_notif);
        } else {
            // It's good practice to log when you receive a message type you don't understand.
            loggy_plugins("❓ IPC-THREAD '%s': Received unknown message type: %u", p->name, notif_msg.type);
        }
    }

    loggy_plugins("🛑 IPC listener thread for plugin '%s' is now shutting down.", p->name);
    // (You might want to set a flag here like `p->is_dead = true;` for the main loop to discover)

    return NULL;
}

// Add this thread function to monitor the Python process
static void *plugin_process_monitor_thread(void *data) {
    struct spawned_plugin *p = (struct spawned_plugin *)data;
    
    loggy_plugins("🔍 Process monitor started for plugin '%s' (PID: %d)", p->name, p->pid);
    
    while (p->process_monitor_running) {
        int status;
        pid_t result = waitpid(p->pid, &status, WNOHANG);
        
        if (result > 0) {
            // Process has exited
            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                loggy_plugins("💀 PROCESS MONITOR '%s': Python process %d exited with code %d", p->name, p->pid, exit_code);
                if (exit_code == 0) {
                    loggy_plugins("✅ Process exited normally (code 0)");
                } else {
                    loggy_plugins("❌ Process exited with error code %d", exit_code);
                }
            } else if (WIFSIGNALED(status)) {
                int signal = WTERMSIG(status);
                loggy_plugins("💀 PROCESS MONITOR '%s': Python process %d killed by signal %d (%s)", 
                             p->name, p->pid, signal, strsignal(signal));
                if (signal == SIGSEGV) {
                    loggy_plugins("💥 Segmentation fault - plugin crashed");
                } else if (signal == SIGKILL) {
                    loggy_plugins("🔪 Process was killed (probably by system/OOM)");
                } else if (signal == SIGTERM) {
                    loggy_plugins("🛑 Process was terminated");
                }
            }
            
            // Process is dead, stop monitoring
            p->process_monitor_running = false;
            p->ipc_thread_running = false; // Also stop the IPC thread
            break;
        } else if (result == 0) {
            // Process is still running, check again in a bit
            usleep(500000); // Check every 500ms
        } else {
            // Error in waitpid
            if (errno == ECHILD) {
                loggy_plugins("⚠️ PROCESS MONITOR '%s': No child process (PID %d) - may have been reaped elsewhere", p->name, p->pid);
            } else {
                loggy_plugins("❌ PROCESS MONITOR '%s': waitpid error: %s", p->name, strerror(errno));
            }
            break;
        }
    }
    
    loggy_plugins("🛑 Process monitor for plugin '%s' shutting down", p->name);
    return NULL;
}

void nano_plugin_spawn_python(struct nano_server *server, const char *path, const char* name) {
    int ipc_sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ipc_sockets) < 0) {
        loggy_plugins("Failed to create IPC socket pair: %s", strerror(errno));
        return;
    }

    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/nano-plugin-%s-%d", name, getpid());
    
    shm_unlink(shm_name);
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (shm_fd < 0) { 
        loggy_plugins("Failed to create SHM %s: %s", shm_name, strerror(errno));
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }
    if (ftruncate(shm_fd, SHM_BUFFER_SIZE) < 0) { 
        loggy_plugins("Failed to resize SHM %s: %s", shm_name, strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }

    // DEBUG: Show SHM creation
    loggy_plugins("Created SHM: %s (size: %d bytes)", shm_name, SHM_BUFFER_SIZE);

    // Find available display socket for standalone mode
    char display_socket[256];
    const char *display_name = NULL;
    
    for (int i = 20; i < 64; i++) {
        snprintf(display_socket, sizeof(display_socket), "wayland-%d", i);
        char socket_path[512];
        snprintf(socket_path, sizeof(socket_path), "%s/%s", 
                getenv("XDG_RUNTIME_DIR") ?: "/tmp", display_socket);
        
        if (access(socket_path, F_OK) != 0) {
            display_name = display_socket;
            break;
        }
    }
    
    if (!display_name) {
        // Fallback to a unique name
        snprintf(display_socket, sizeof(display_socket), "nano-%s-%d", name, getpid());
        display_name = display_socket;
    }

    loggy_plugins("Launching Python plugin %s on display %s", name, display_name);

    // Check if the path exists, if not try some common locations
    char resolved_path[1024];
    char absolute_path[1024];
    
    if (access(path, X_OK) == 0) {
        // Convert to absolute path before we change directories
        if (path[0] == '/') {
            // Already absolute
            strncpy(resolved_path, path, sizeof(resolved_path) - 1);
        } else {
            // Make it absolute
            char *cwd = getcwd(NULL, 0);
            snprintf(resolved_path, sizeof(resolved_path), "%s/%s", cwd, path);
            free(cwd);
        }
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    } else {
        // Extract filename from path manually
        const char *filename = strrchr(path, '/');
        if (filename) {
            filename++; // Skip the '/'
        } else {
            filename = path; // No '/' found, use whole string
        }
        
        char *cwd = getcwd(NULL, 0);
        
        // Try current directory
        snprintf(absolute_path, sizeof(absolute_path), "%s/%s", cwd, filename);
        if (access(absolute_path, X_OK) == 0) {
            strncpy(resolved_path, absolute_path, sizeof(resolved_path) - 1);
        } else {
            // Try plugins subdirectory
            snprintf(absolute_path, sizeof(absolute_path), "%s/plugins/%s", cwd, filename);
            if (access(absolute_path, X_OK) == 0) {
                strncpy(resolved_path, absolute_path, sizeof(resolved_path) - 1);
            } else {
                loggy_plugins("Cannot find executable: %s (tried %s/%s, %s/plugins/%s, %s)", 
                       path, cwd, filename, cwd, filename, path);
                free(cwd);
                close(shm_fd);
                close(ipc_sockets[0]);
                close(ipc_sockets[1]);
                return;
            }
        }
        free(cwd);
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    }
    
    loggy_plugins("Using resolved path: %s", resolved_path);
    
    // Debug: verify the file exists and is executable
    struct stat st;
    if (stat(resolved_path, &st) == 0) {
        loggy_plugins("File exists: %s (mode: %o, size: %ld)", resolved_path, st.st_mode, st.st_size);
        if (st.st_mode & S_IXUSR) {
            loggy_plugins("File is executable");
        } else {
            loggy_plugins("File is NOT executable - fixing permissions");
            chmod(resolved_path, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
        }
    } else {
        loggy_plugins("File does not exist: %s", resolved_path);
    }

    pid_t pid = fork();
    if (pid < 0) { 
        loggy_plugins("Failed to fork: %s", strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }

    if (pid == 0) { 
        // ZERO ISOLATION but preserve library path
        close(ipc_sockets[0]);
        close(shm_fd);
        
        // Get current working directory before any changes
        char *original_cwd = getcwd(NULL, 0);
        
        char socket_str[16];
        snprintf(socket_str, sizeof(socket_str), "%d", ipc_sockets[1]);
        
        // Only set the display and library path - NOTHING ELSE
        setenv("WAYLAND_DISPLAY", display_name, 1);
        
        // Set up library path for shared DMA-BUF library
        if (original_cwd) {
            char lib_path[2048];
            snprintf(lib_path, sizeof(lib_path), "%s/libdmabuf_share", original_cwd);
            
            char *existing_path = getenv("LD_LIBRARY_PATH");
            if (existing_path) {
                char new_path[4096];
                snprintf(new_path, sizeof(new_path), "%s:%s", lib_path, existing_path);
                setenv("LD_LIBRARY_PATH", new_path, 1);
            } else {
                setenv("LD_LIBRARY_PATH", lib_path, 1);
            }
            
            loggy_plugins("Set plugin LD_LIBRARY_PATH to include: %s", lib_path);
            free(original_cwd);
        }
        
        const char *python_script_name = "plugins/hosted_mouse_plugin.py";
        loggy_plugins("CHILD PROCESS: About to exec: %s %s %s", resolved_path, socket_str, python_script_name);
        loggy_plugins("CHILD PROCESS: Socket FD being passed: %s", socket_str);
        loggy_plugins("CHILD PROCESS: WAYLAND_DISPLAY=%s", getenv("WAYLAND_DISPLAY"));
        loggy_plugins("CHILD PROCESS: LD_LIBRARY_PATH=%s", getenv("LD_LIBRARY_PATH"));
        
        // Single exec attempt
        execl(resolved_path, resolved_path, socket_str, python_script_name, (char*)NULL);
        
        loggy_plugins("Exec failed: %s", strerror(errno));
        exit(1);
    }

    // ===== PARENT PROCESS =====
    close(ipc_sockets[1]);

    struct spawned_plugin *p = calloc(1, sizeof(struct spawned_plugin));
    if (!p) {
        loggy_plugins("Failed to allocate memory for plugin struct");
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return;
    }

    p->pid = pid;
    p->ipc_fd = ipc_sockets[0];
    strncpy(p->shm_name, shm_name, sizeof(p->shm_name) - 1);
    p->shm_name[sizeof(p->shm_name) - 1] = '\0';
    
    // Set the name first - this is critical for logging
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    
    loggy_plugins("✅ PARENT: Set plugin name to '%s'", p->name);
    
    // Store display info in the name for tracking (ensure it fits in 64 bytes)
    size_t name_len = strnlen(name, sizeof(p->name) - 1);
    size_t display_len = strnlen(display_name, sizeof(p->name) - 1);
    
    if (name_len + 1 + display_len < sizeof(p->name)) {
        int written = snprintf(p->name, sizeof(p->name), "%.*s@%.*s", 
                              (int)name_len, name, 
                              (int)(sizeof(p->name) - name_len - 2), display_name);
        if (written >= sizeof(p->name)) {
            // If it doesn't fit, just use the basic name
            strncpy(p->name, name, sizeof(p->name) - 1);
            p->name[sizeof(p->name) - 1] = '\0';
        }
    }
    
    loggy_plugins("✅ PARENT: Final plugin name set to '%s'", p->name);
    
    loggy_plugins("PARENT: Child process PID: %d, IPC FD: %d", pid, p->ipc_fd);
    
    // Check if child process is still alive before proceeding
    usleep(50000); // 50ms delay to let child start
    int child_status;
    pid_t wait_result = waitpid(pid, &child_status, WNOHANG);
    if (wait_result > 0) {
        if (WIFEXITED(child_status)) {
            loggy_plugins("❌ PARENT: Child process %d exited immediately with status %d", pid, WEXITSTATUS(child_status));
        } else if (WIFSIGNALED(child_status)) {
            loggy_plugins("❌ PARENT: Child process %d killed by signal %d", pid, WTERMSIG(child_status));
        }
        // Child died, clean up and return
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        free(p);
        return;
    } else if (wait_result == 0) {
        loggy_plugins("✅ PARENT: Child process %d is still running", pid);
    } else {
        loggy_plugins("⚠️ PARENT: waitpid error: %s", strerror(errno));
    }

    // Map SHM as READ-WRITE for both compositor and plugin access
    p->shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (p->shm_ptr == MAP_FAILED) { 
        loggy_plugins("Failed to mmap SHM %s: %s", shm_name, strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return; 
    }
    close(shm_fd);
    
    // DEBUG: Show compositor SHM mapping
    loggy_plugins("Compositor mapped SHM: %s at %p", shm_name, p->shm_ptr);
    
    // Write test pattern to verify shared memory works
    uint32_t* test_ptr = (uint32_t*)p->shm_ptr;
    test_ptr[0] = 0x57A4DA10; // STANDALONE (in leetspeak-ish hex)
    test_ptr[1] = 0x87654321;
    loggy_plugins("Compositor wrote standalone test pattern: 0x%08x 0x%08x", test_ptr[0], test_ptr[1]);
    
    // Initialize texture and frame state
    p->texture = NULL;
    p->has_new_frame = false;
    
    // Initialize DMA-BUF fields (for regular IPC compatibility)
    p->dmabuf_fd = -1;
    p->dmabuf_width = 0;
    p->dmabuf_height = 0;
    p->dmabuf_format = 0;
    p->dmabuf_stride = 0;
    p->dmabuf_modifier = 0;

    // ======================================================================
    // +++ ASYNC-READY DOUBLE BUFFER ALLOCATION +++
    // ======================================================================
    int buffer_width = 1920;
    int buffer_height = 1080;
    bool allocation_ok = true;

    // Allocate TWO buffers for asynchronous rendering.
    for (int i = 0; i < 2; i++) {
        p->compositor_buffers[i] = nano_server_alloc_buffer_legacy(server, buffer_width, buffer_height);
        if (!p->compositor_buffers[i]) {
            loggy_plugins("❌ Failed to create DMA-BUF %d for plugin '%s'", i, p->name);
            allocation_ok = false;
            break;
        }

        if (!wlr_buffer_get_dmabuf(p->compositor_buffers[i], &p->dmabuf_attrs[i])) {
            loggy_plugins("❌ Failed to export DMA-BUF attributes for buffer %d", i);
            allocation_ok = false;
            break;
        }
        loggy_plugins("✅ Allocated buffer %d for plugin '%s': fd=%d", i, p->name, p->dmabuf_attrs[i].fd[0]);
    }

    if (!allocation_ok) {
        // Cleanup on failure
        if (p->compositor_buffers[0]) wlr_buffer_drop(p->compositor_buffers[0]);
        if (p->compositor_buffers[1]) wlr_buffer_drop(p->compositor_buffers[1]);
        // Clean up SHM and other resources
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return;
    }

    // Send BOTH buffers to the plugin in a single message.
    ipc_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = IPC_EVENT_TYPE_BUFFER_ALLOCATED;
    event.data.multi_buffer_info.count = 2;

    // Manually copy attributes for the first buffer (index 0)
    event.data.multi_buffer_info.buffers[0].width    = p->dmabuf_attrs[0].width;
    event.data.multi_buffer_info.buffers[0].height   = p->dmabuf_attrs[0].height;
    event.data.multi_buffer_info.buffers[0].format   = p->dmabuf_attrs[0].format;
    event.data.multi_buffer_info.buffers[0].modifier = p->dmabuf_attrs[0].modifier;
    event.data.multi_buffer_info.buffers[0].stride   = p->dmabuf_attrs[0].stride[0]; // Use the first plane's stride
    event.data.multi_buffer_info.buffers[0].offset   = p->dmabuf_attrs[0].offset[0]; // Use the first plane's offset

    // Manually copy attributes for the second buffer (index 1)
    event.data.multi_buffer_info.buffers[1].width    = p->dmabuf_attrs[1].width;
    event.data.multi_buffer_info.buffers[1].height   = p->dmabuf_attrs[1].height;
    event.data.multi_buffer_info.buffers[1].format   = p->dmabuf_attrs[1].format;
    event.data.multi_buffer_info.buffers[1].modifier = p->dmabuf_attrs[1].modifier;
    event.data.multi_buffer_info.buffers[1].stride   = p->dmabuf_attrs[1].stride[0]; // Use the first plane's stride
    event.data.multi_buffer_info.buffers[1].offset   = p->dmabuf_attrs[1].offset[0]; // Use the first plane's offset

    int fds_to_send[2];
    fds_to_send[0] = dup(p->dmabuf_attrs[0].fd[0]);
    fds_to_send[1] = dup(p->dmabuf_attrs[1].fd[0]);

    if (fds_to_send[0] >= 0 && fds_to_send[1] >= 0) {
        send_ipc_with_fds(p->ipc_fd, &event, fds_to_send, 2);
        loggy_plugins("✅ Sent 2 buffers to plugin '%s'.", p->name);
    } else {
        loggy_plugins("❌ Failed to dup FDs for sending to plugin");
        // Clean up on failure
        if (p->compositor_buffers[0]) wlr_buffer_drop(p->compositor_buffers[0]);
        if (p->compositor_buffers[1]) wlr_buffer_drop(p->compositor_buffers[1]);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return;
    }
    close(fds_to_send[0]);
    close(fds_to_send[1]);

    // +++ FINAL STEP: START THE IPC LISTENER THREAD +++
    // ======================================================================

    p->is_async = true; // Mark this plugin as using the async model
    p->has_new_frame = false;
    p->last_ready_buffer = -1;
    p->server = server; // Pass a reference to the server for the thread
    p->ipc_thread_running = false; // Initialize to false, thread will set to true
    
    // Debug: Check if the socket is still valid before creating thread
    int sock_flags = fcntl(p->ipc_fd, F_GETFL);
    if (sock_flags == -1) {
        loggy_plugins("❌ PARENT: IPC socket is invalid before thread creation: %s", strerror(errno));
        // Clean up and return
        wlr_buffer_drop(p->compositor_buffers[0]);
        wlr_buffer_drop(p->compositor_buffers[1]);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return;
    } else {
        loggy_plugins("✅ PARENT: IPC socket FD %d is valid (flags: %d)", p->ipc_fd, sock_flags);
    }
    
    // Add process monitoring
    p->process_monitor_running = true;
    
    // Give the Python process a moment to start up and connect
    usleep(100000); // 100ms delay
    
    if (pthread_create(&p->ipc_thread, NULL, plugin_ipc_thread_func, p) != 0) {
        loggy_plugins("❌ Failed to create IPC listener thread for plugin '%s'", p->name);
        // If the thread fails to start, the plugin is useless. Clean everything up.
        wlr_buffer_drop(p->compositor_buffers[0]);
        wlr_buffer_drop(p->compositor_buffers[1]);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return;
    }

    // Start the process monitor thread
    if (pthread_create(&p->process_monitor_thread, NULL, plugin_process_monitor_thread, p) != 0) {
        loggy_plugins("❌ Failed to create process monitor thread for plugin '%s'", p->name);
        // Continue anyway - this is just for debugging
    } else {
        loggy_plugins("✅ Started process monitor thread for plugin '%s'", p->name);
    }

    // Add to server's plugin list
    pthread_mutex_lock(&server->plugin_list_mutex);
    wl_list_insert(&server->plugins, &p->link);
    pthread_mutex_unlock(&server->plugin_list_mutex);

    loggy_plugins("Spawned async Python plugin: %s (PID %d) on display %s with dual IPC (SHM + threaded async).", 
           p->name, p->pid, display_name);
}



/**
 * Spawns a JavaScript/Node.js plugin process.
 * Works identically to Python plugin spawning - allocates DMA-BUFs,
 * creates IPC socket, forks, and execs the JS loader with the script.
 *
 * @param server The nano compositor server
 * @param loader_path Path to js_loader_plugin executable (e.g., "plugins/js_loader_plugin")
 * @param script_path Path to the JavaScript file (e.g., "plugins/webgl_effects.js")
 * @param name Plugin name for identification
 */
void nano_plugin_spawn_javascript(struct nano_server *server, 
                                   const char *loader_path, 
                                   const char *script_path,
                                   const char *name) {
    int ipc_sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ipc_sockets) < 0) {
        loggy_plugins("Failed to create IPC socket pair: %s", strerror(errno));
        return;
    }

    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/nano-js-plugin-%s-%d", name, getpid());
    
    shm_unlink(shm_name);
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (shm_fd < 0) { 
        loggy_plugins("Failed to create SHM %s: %s", shm_name, strerror(errno));
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }
    if (ftruncate(shm_fd, SHM_BUFFER_SIZE) < 0) { 
        loggy_plugins("Failed to resize SHM %s: %s", shm_name, strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }

    loggy_plugins("Created SHM for JS plugin: %s (size: %d bytes)", shm_name, SHM_BUFFER_SIZE);

    // Find available display socket
    char display_socket[256];
    const char *display_name = NULL;
    
    for (int i = 30; i < 64; i++) {  // Start at 30 to avoid conflict with Python plugins
        snprintf(display_socket, sizeof(display_socket), "wayland-%d", i);
        char socket_path[512];
        snprintf(socket_path, sizeof(socket_path), "%s/%s", 
                getenv("XDG_RUNTIME_DIR") ?: "/tmp", display_socket);
        
        if (access(socket_path, F_OK) != 0) {
            display_name = display_socket;
            break;
        }
    }
    
    if (!display_name) {
        snprintf(display_socket, sizeof(display_socket), "nano-js-%s-%d", name, getpid());
        display_name = display_socket;
    }

    loggy_plugins("Launching JavaScript plugin %s on display %s", name, display_name);

    // Resolve loader path
    char resolved_loader[1024];
    char resolved_script[1024];
    
    // Resolve loader executable
    if (access(loader_path, X_OK) == 0) {
        if (loader_path[0] == '/') {
            strncpy(resolved_loader, loader_path, sizeof(resolved_loader) - 1);
        } else {
            char *cwd = getcwd(NULL, 0);
            snprintf(resolved_loader, sizeof(resolved_loader), "%s/%s", cwd, loader_path);
            free(cwd);
        }
    } else {
        // Try plugins subdirectory
        char *cwd = getcwd(NULL, 0);
        snprintf(resolved_loader, sizeof(resolved_loader), "%s/plugins/js_loader_plugin", cwd);
        if (access(resolved_loader, X_OK) != 0) {
            loggy_plugins("Cannot find JS loader executable: %s", loader_path);
            free(cwd);
            close(shm_fd);
            close(ipc_sockets[0]);
            close(ipc_sockets[1]);
            return;
        }
        free(cwd);
    }
    resolved_loader[sizeof(resolved_loader) - 1] = '\0';

    // Resolve script path
    if (script_path[0] == '/') {
        strncpy(resolved_script, script_path, sizeof(resolved_script) - 1);
    } else {
        char *cwd = getcwd(NULL, 0);
        snprintf(resolved_script, sizeof(resolved_script), "%s/%s", cwd, script_path);
        free(cwd);
    }
    resolved_script[sizeof(resolved_script) - 1] = '\0';

    if (access(resolved_script, R_OK) != 0) {
        loggy_plugins("Cannot find JS script: %s", resolved_script);
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return;
    }

    loggy_plugins("Resolved JS loader: %s", resolved_loader);
    loggy_plugins("Resolved JS script: %s", resolved_script);

    pid_t pid = fork();
    if (pid < 0) { 
        loggy_plugins("Failed to fork for JS plugin: %s", strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        close(ipc_sockets[1]);
        return; 
    }

    if (pid == 0) { 
        // ===== CHILD PROCESS =====
        close(ipc_sockets[0]);
        close(shm_fd);
        
        char *original_cwd = getcwd(NULL, 0);
        
        char socket_str[16];
        snprintf(socket_str, sizeof(socket_str), "%d", ipc_sockets[1]);
        
        setenv("WAYLAND_DISPLAY", display_name, 1);
        
        // Set up library path for shared DMA-BUF library
        if (original_cwd) {
            char lib_path[2048];
            snprintf(lib_path, sizeof(lib_path), "%s/libdmabuf_share", original_cwd);
            
            char *existing_path = getenv("LD_LIBRARY_PATH");
            if (existing_path) {
                char new_path[4096];
                snprintf(new_path, sizeof(new_path), "%s:%s", lib_path, existing_path);
                setenv("LD_LIBRARY_PATH", new_path, 1);
            } else {
                setenv("LD_LIBRARY_PATH", lib_path, 1);
            }
            
            loggy_plugins("Set JS plugin LD_LIBRARY_PATH to include: %s", lib_path);
            free(original_cwd);
        }
        
        loggy_plugins("CHILD PROCESS: Execing JS loader: %s %s %s", 
                     resolved_loader, socket_str, resolved_script);
        
        // Exec: js_loader_plugin <ipc_fd> <script.js>
        execl(resolved_loader, resolved_loader, socket_str, resolved_script, (char*)NULL);
        
        loggy_plugins("Exec failed for JS plugin: %s", strerror(errno));
        exit(1);
    }

    // ===== PARENT PROCESS =====
    close(ipc_sockets[1]);

    struct spawned_plugin *p = calloc(1, sizeof(struct spawned_plugin));
    if (!p) {
        loggy_plugins("Failed to allocate memory for JS plugin struct");
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return;
    }

    p->pid = pid;
    p->ipc_fd = ipc_sockets[0];
    strncpy(p->shm_name, shm_name, sizeof(p->shm_name) - 1);
    p->shm_name[sizeof(p->shm_name) - 1] = '\0';
    
    // Set plugin name with JS prefix for identification
    snprintf(p->name, sizeof(p->name), "js:%s@%s", name, display_name);
    
    loggy_plugins("✅ PARENT: Created JS plugin '%s' (PID: %d, IPC FD: %d)", p->name, pid, p->ipc_fd);
    
    // Check if child process is still alive
    usleep(50000); // 50ms delay
    int child_status;
    pid_t wait_result = waitpid(pid, &child_status, WNOHANG);
    if (wait_result > 0) {
        if (WIFEXITED(child_status)) {
            loggy_plugins("❌ PARENT: JS child process %d exited with status %d", pid, WEXITSTATUS(child_status));
        } else if (WIFSIGNALED(child_status)) {
            loggy_plugins("❌ PARENT: JS child process %d killed by signal %d", pid, WTERMSIG(child_status));
        }
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        free(p);
        return;
    }

    // Map SHM
    p->shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (p->shm_ptr == MAP_FAILED) { 
        loggy_plugins("Failed to mmap SHM for JS plugin %s: %s", shm_name, strerror(errno));
        close(shm_fd);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return; 
    }
    close(shm_fd);

    // Initialize state
    p->texture = NULL;
    p->has_new_frame = false;
    p->dmabuf_fd = -1;
    p->dmabuf_width = 0;
    p->dmabuf_height = 0;
    p->dmabuf_format = 0;
    p->dmabuf_stride = 0;
    p->dmabuf_modifier = 0;

    // ======================================================================
    // ASYNC DOUBLE BUFFER ALLOCATION (same as Python plugins)
    // ======================================================================
    int buffer_width = 1920;
    int buffer_height = 1080;
    bool allocation_ok = true;

    for (int i = 0; i < 2; i++) {
        p->compositor_buffers[i] = nano_server_alloc_buffer_legacy(server, buffer_width, buffer_height);
        if (!p->compositor_buffers[i]) {
            loggy_plugins("❌ Failed to create DMA-BUF %d for JS plugin '%s'", i, p->name);
            allocation_ok = false;
            break;
        }

        if (!wlr_buffer_get_dmabuf(p->compositor_buffers[i], &p->dmabuf_attrs[i])) {
            loggy_plugins("❌ Failed to export DMA-BUF attributes for buffer %d", i);
            allocation_ok = false;
            break;
        }
        loggy_plugins("✅ Allocated buffer %d for JS plugin '%s': fd=%d", i, p->name, p->dmabuf_attrs[i].fd[0]);
    }

    if (!allocation_ok) {
        if (p->compositor_buffers[0]) wlr_buffer_drop(p->compositor_buffers[0]);
        if (p->compositor_buffers[1]) wlr_buffer_drop(p->compositor_buffers[1]);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return;
    }

    // Send BOTH buffers to the JS plugin
    ipc_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = IPC_EVENT_TYPE_BUFFER_ALLOCATED;
    event.data.multi_buffer_info.count = 2;

    for (int i = 0; i < 2; i++) {
        event.data.multi_buffer_info.buffers[i].width    = p->dmabuf_attrs[i].width;
        event.data.multi_buffer_info.buffers[i].height   = p->dmabuf_attrs[i].height;
        event.data.multi_buffer_info.buffers[i].format   = p->dmabuf_attrs[i].format;
        event.data.multi_buffer_info.buffers[i].modifier = p->dmabuf_attrs[i].modifier;
        event.data.multi_buffer_info.buffers[i].stride   = p->dmabuf_attrs[i].stride[0];
        event.data.multi_buffer_info.buffers[i].offset   = p->dmabuf_attrs[i].offset[0];
    }

    int fds_to_send[2];
    fds_to_send[0] = dup(p->dmabuf_attrs[0].fd[0]);
    fds_to_send[1] = dup(p->dmabuf_attrs[1].fd[0]);

    if (fds_to_send[0] >= 0 && fds_to_send[1] >= 0) {
        send_ipc_with_fds(p->ipc_fd, &event, fds_to_send, 2);
        loggy_plugins("✅ Sent 2 buffers to JS plugin '%s'.", p->name);
    } else {
        loggy_plugins("❌ Failed to dup FDs for JS plugin");
        if (p->compositor_buffers[0]) wlr_buffer_drop(p->compositor_buffers[0]);
        if (p->compositor_buffers[1]) wlr_buffer_drop(p->compositor_buffers[1]);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return;
    }
    close(fds_to_send[0]);
    close(fds_to_send[1]);

    // Start async IPC thread (same as Python)
    p->is_async = true;
    p->has_new_frame = false;
    p->last_ready_buffer = -1;
    p->server = server;
    p->ipc_thread_running = false;
    
    int sock_flags = fcntl(p->ipc_fd, F_GETFL);
    if (sock_flags == -1) {
        loggy_plugins("❌ PARENT: JS plugin IPC socket invalid: %s", strerror(errno));
        wlr_buffer_drop(p->compositor_buffers[0]);
        wlr_buffer_drop(p->compositor_buffers[1]);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return;
    }
    
    p->process_monitor_running = true;
    usleep(100000); // 100ms delay for JS process to start
    
    if (pthread_create(&p->ipc_thread, NULL, plugin_ipc_thread_func, p) != 0) {
        loggy_plugins("❌ Failed to create IPC thread for JS plugin '%s'", p->name);
        wlr_buffer_drop(p->compositor_buffers[0]);
        wlr_buffer_drop(p->compositor_buffers[1]);
        munmap(p->shm_ptr, SHM_BUFFER_SIZE);
        close(ipc_sockets[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        free(p);
        return;
    }

    if (pthread_create(&p->process_monitor_thread, NULL, plugin_process_monitor_thread, p) != 0) {
        loggy_plugins("❌ Failed to create process monitor for JS plugin '%s'", p->name);
        // Continue anyway
    }

    // Add to server's plugin list
    pthread_mutex_lock(&server->plugin_list_mutex);
    wl_list_insert(&server->plugins, &p->link);
    pthread_mutex_unlock(&server->plugin_list_mutex);

    loggy_plugins("✅ Spawned async JavaScript plugin: %s (PID %d) with dual DMA-BUF.", p->name, p->pid);
}


// Kills a running plugin process.
void nano_plugin_kill(struct nano_server *server, struct spawned_plugin *plugin) {
    pthread_mutex_lock(&server->plugin_list_mutex);
    
    loggy_plugins(  "Killing plugin %s (PID %d)", plugin->name, plugin->pid);
    kill(plugin->pid, SIGTERM);
    waitpid(plugin->pid, NULL, 0);

    if (plugin->ipc_fd >= 0) close(plugin->ipc_fd);
    if (plugin->texture) wlr_texture_destroy(plugin->texture);
    if (plugin->shm_ptr) munmap(plugin->shm_ptr, SHM_BUFFER_SIZE);
    if (plugin->compositor_buffer) {
        wlr_buffer_drop(plugin->compositor_buffer);
        plugin->compositor_buffer = NULL;
    }
    // NEW: Clean up DMA-BUF resources
    if (plugin->dmabuf_fd >= 0) {
        close(plugin->dmabuf_fd);
    }
    
    shm_unlink(plugin->shm_name);
    
    wl_list_remove(&plugin->link);
    free(plugin);

    pthread_mutex_unlock(&server->plugin_list_mutex);
}

// Helper to find a plugin by its process ID.
struct spawned_plugin *nano_plugin_get_by_pid(struct nano_server *server, pid_t pid) {
    struct spawned_plugin *p;
    pthread_mutex_lock(&server->plugin_list_mutex);
    wl_list_for_each(p, &server->plugins, link) {
        if (p->pid == pid) {
            pthread_mutex_unlock(&server->plugin_list_mutex);
            return p;
        }
    }
    pthread_mutex_unlock(&server->plugin_list_mutex);
    return NULL;
}
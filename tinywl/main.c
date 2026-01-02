// main.c - Entry point for nano compositor
//to compile plugins 

//gcc -shared -fPIC -I. -DWLR_USE_UNSTABLE -I/usr/include/libdrm -I/usr/include/pixman-1 -I/usr/include/libpng16 plugins/opengl_desktop_plugin.c -lGLESv2 -lEGL -o plugins/opengl_desktop.so
//gcc -shared -fPIC -I. -DWLR_USE_UNSTABLE -I/usr/include/libdrm -I/usr/include/pixman-1 -I/usr/include/libpng16 plugins/window_management_plugin.c -lGLESv2 -lEGL -o plugins/window_management.so

//gcc -shared -fPIC -I. -DWLR_USE_UNSTABLE -I/usr/include/libdrm -I/usr/include/pixman-1 -I/usr/include/libpng16 plugins/show_mouse.c -lGLESv2 -lEGL -o plugins/show_mouse.so

//opengl_desktop_plugin.c 

// main.c - Entry point for nano compositor with EGL context access
#define _GNU_SOURCE
#include "nano_compositor.h"
#include <stdlib.h>
#include <string.h>
#include <wlr/render/gles2.h>
#include <wlr/render/egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <signal.h> 




int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    wlr_log_init(WLR_DEBUG, NULL);
   
    // Save original display for nested Wayland
    char *original_wayland_display = NULL;
    if (getenv("WAYLAND_DISPLAY")) {
        original_wayland_display = strdup(getenv("WAYLAND_DISPLAY"));
        // Temporarily unset to avoid conflict with nested compositor
      //  unsetenv("WAYLAND_DISPLAY");
        setenv("WLR_BACKENDS", "wayland", 1);
        wlr_log(WLR_INFO, "Using Wayland backend for nested session");
    } else if (getenv("DISPLAY")) {
        setenv("WLR_BACKENDS", "x11", 1);
        wlr_log(WLR_INFO, "Using X11 backend for windowed testing");
    } else {
        setenv("WLR_BACKENDS", "headless", 1);
        wlr_log(WLR_INFO, "Using headless backend");
    }
    
    struct nano_server server = {0};
    
    if (!nano_server_init(&server)) {
        wlr_log(WLR_ERROR, "Failed to initialize server");
        if (original_wayland_display) {
            free(original_wayland_display);
        }
        return 1;
    }
    
    // Initialize EGL context for plugins after server init
    if (server.renderer && wlr_renderer_is_gles2(server.renderer)) {
        struct wlr_egl *egl = wlr_gles2_renderer_get_egl(server.renderer);
        if (egl) {
            // Try to make context current for initialization
            EGLDisplay display = wlr_egl_get_display(egl);
            EGLContext context = wlr_egl_get_context(egl);
            
            if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT) {
                if (eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
                    // Query OpenGL information for plugins
                    const char *vendor = (const char *)glGetString(GL_VENDOR);
                    const char *renderer = (const char *)glGetString(GL_RENDERER);
                    const char *version = (const char *)glGetString(GL_VERSION);
                    const char *shading_lang = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
                    
                    wlr_log(WLR_INFO, "OpenGL Context for Plugins:");
                    wlr_log(WLR_INFO, "  GL_VENDOR: %s", vendor ? vendor : "(null)");
                    wlr_log(WLR_INFO, "  GL_RENDERER: %s", renderer ? renderer : "(null)");
                    wlr_log(WLR_INFO, "  GL_VERSION: %s", version ? version : "(null)");
                    wlr_log(WLR_INFO, "  GL_SHADING_LANGUAGE_VERSION: %s", shading_lang ? shading_lang : "(null)");
                    
                    // Store EGL context in server for plugin access
                    server.egl = egl;
                    
                    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    wlr_log(WLR_INFO, "EGL context available for plugins");
                } else {
                    wlr_log(WLR_ERROR, "Failed to make EGL context current - plugins may have limited OpenGL access");
                }
            } else {
                wlr_log(WLR_ERROR, "Invalid EGL display or context");
            }
        } else {
            wlr_log(WLR_ERROR, "Failed to get EGL context from renderer");
        }
    } else {
        wlr_log(WLR_ERROR, "Renderer is not GLES2 - OpenGL plugins may not work");
    }
    
    // Restore original display for parent session
    if (original_wayland_display) {
        setenv("WAYLAND_DISPLAY", original_wayland_display, 1);
        free(original_wayland_display);
    }
    

// ADD: Create Wayfire as a background layer
    compositor_layer_t *wayfire_layer = nano_layer_create(
        &server.layer_manager,
        "wayfire",
        "./wayfire_capture_wrapper",
        0  // z-order 0 = background
    );
    
    if (wayfire_layer) {
        wlr_log(WLR_INFO, "Wayfire layer created");
    }




/*
nano_plugin_spawn_javascript(&server, 
    "./plugins/js_loader_plugin", 
    "standalone",   // <-- Use standalone mode
    "webgl");
*/
//nano_plugin_spawn(&server, "./plugins/window_management_plugin", "wm");
//    nano_plugin_spawn(&server, "./plugins/opengl_desktop_plugin", "desktop");
//nano_plugin_spawn(&server, "./plugins/dockx_plugin", "desktop");
nano_plugin_spawn(&server, "./plugins/real_mouse_plugin", "desktop");    

//nano_plugin_spawn(&server,"dmabuf_creator_plugin","mouse" );
// nano_plugin_spawn(&server, "./plugins/opengl_desktop_plugin", "desktop");
nano_plugin_spawn_python(&server, "./python4_plugin", "mouse");
nano_plugin_spawn(&server, "./plugins/dockx_plugin", "desktop");
//nano_plugin_spawn(&server, "./plugins/opengl_desktop2_plugin", "desktop");
nano_plugin_spawn_python(&server, "./python_plugin", "mouse");
nano_plugin_spawn_python(&server, "./python3_plugin", "mouse");

nano_plugin_spawn(&server, "./plugins/wobbly_plugin", "desktop");
//nano_plugin_spawn_python(&server, "./python2_plugin", "mouse");

//nano_plugin_spawn(&server, "./plugins/show_mouse_plugin", "mouse");
nano_plugin_spawn(&server, "./plugins/window_management_plugin", "wm");
//nano_plugin_spawn(&server, "./plugins/shader_background_plugin", "desktop");

nano_plugin_spawn_javascript(&server, 
    "./plugins/js_loader_plugin", 
    "./plugins/webgl_desktop_plugin.js", 
    "webgl");

  
//    nano_plugin_load(&server, "./plugins/input_handler.so");
//    nano_plugin_load(&server, "./plugins/workspace.so");
    
    wlr_log(WLR_INFO, "Plugin loading complete");
    
    // Run the server
    wlr_log(WLR_INFO, "Starting nano compositor...");
    int ret = nano_server_run(&server);
   // nano_server_send_window_updates() 
    // Cleanup
    nano_server_destroy(&server);
    
    return ret;
} 










/*
#define _GNU_SOURCE
#include "nano_compositor.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_DEBUG, NULL);
    
    // Save original display for nested Wayland
    char *original_wayland_display = NULL;
    if (getenv("WAYLAND_DISPLAY")) {
        original_wayland_display = strdup(getenv("WAYLAND_DISPLAY"));
        // Temporarily unset to avoid conflict with nested compositor
        unsetenv("WAYLAND_DISPLAY");
        setenv("WLR_BACKENDS", "wayland", 1);
        wlr_log(WLR_INFO, "Using Wayland backend for nested session");
    } else if (getenv("DISPLAY")) {
        setenv("WLR_BACKENDS", "x11", 1);
        wlr_log(WLR_INFO, "Using X11 backend for windowed testing");
    } else {
        setenv("WLR_BACKENDS", "headless", 1);
        wlr_log(WLR_INFO, "Using headless backend");
    }
    
    struct nano_server server = {0};
    
    if (!nano_server_init(&server)) {
        wlr_log(WLR_ERROR, "Failed to initialize server");
        if (original_wayland_display) {
            free(original_wayland_display);
        }
        return 1;
    }
    
    // Restore original display for parent session
    if (original_wayland_display) {
        setenv("WAYLAND_DISPLAY", original_wayland_display, 1);
        free(original_wayland_display);
    }
    
    // Load core plugins (skip if they don't exist)
    nano_plugin_load(&server, "./plugins/opengl_desktop.so");
//    nano_plugin_load(&server, "./plugins/opengl_renderer.so");
//    nano_plugin_load(&server, "./plugins/window_management.so");
//    nano_plugin_load(&server, "./plugins/input_handler.so");
 //   nano_plugin_load(&server, "./plugins/workspace.so");
    
    // Run the server
    int ret = nano_server_run(&server);
    
    // Cleanup
    nano_server_destroy(&server);
    
    return ret;
}*/
/*
int main(int argc, char *argv[]) {
    wlr_log_init(WLR_DEBUG, NULL);
    
    // Save original display for nested Wayland
    char *original_wayland_display = NULL;
    if (getenv("WAYLAND_DISPLAY")) {
        original_wayland_display = strdup(getenv("WAYLAND_DISPLAY"));
        // Temporarily unset to avoid conflict with nested compositor
        unsetenv("WAYLAND_DISPLAY");
        setenv("WLR_BACKENDS", "wayland", 1);
        wlr_log(WLR_INFO, "Using Wayland backend for nested session");
    } else if (getenv("DISPLAY")) {
        setenv("WLR_BACKENDS", "x11", 1);
        wlr_log(WLR_INFO, "Using X11 backend for windowed testing");
    } else {
        setenv("WLR_BACKENDS", "headless", 1);
        wlr_log(WLR_INFO, "Using headless backend");
    }
    
    struct nano_server server = {0};
    
    if (!nano_server_init(&server)) {
        wlr_log(WLR_ERROR, "Failed to initialize server");
        if (original_wayland_display) {
            free(original_wayland_display);
        }
        return 1;
    }
    
    // Restore original display for parent session
    if (original_wayland_display) {
        setenv("WAYLAND_DISPLAY", original_wayland_display, 1);
        free(original_wayland_display);
    }
    
    // Load core plugins (skip if they don't exist)
    nano_plugin_load(&server, "./plugins/window_management.so");
    nano_plugin_load(&server, "./plugins/input_handler.so");
    nano_plugin_load(&server, "./plugins/workspace.so");
    
    // Run the server
    int ret = nano_server_run(&server);
    
    // Cleanup
    nano_server_destroy(&server);
    
    return ret;
}*/

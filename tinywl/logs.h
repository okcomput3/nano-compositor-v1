#ifndef LOGS_H
#define LOGS_H

#include <stdio.h>

#define COMPOSITOR_DEBUG_LOGS 1
#define PLUGINS_DEBUG_LOGS 0

#define OPENGL_DESKTOP_DEBUG_LOGS 0
#define OPENGL_DESKTOP2_DEBUG_LOGS 0
#define REAL_MOUSE_DEBUG_LOGS 0
#define DOCKX_DEBUG_LOGS 0
#define SHADER_BACKGROUND_DEBUG_LOGS 0
#define WOBBLY_DEBUG_LOGS 0
#define WINDOW_MANAGEMENT_DEBUG_LOGS 0
#define SHOW_MOUSE_DEBUG_LOGS 0


static inline void loggy_compositor(const char* format, ...) {
    if (COMPOSITOR_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static inline void loggy_plugins(const char* format, ...) {
    if (PLUGINS_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}





static inline void loggy_opengl_desktop(const char* format, ...) {
    if (OPENGL_DESKTOP_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static inline void loggy_opengl_desktop2(const char* format, ...) {
    if (OPENGL_DESKTOP2_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static inline void loggy_real_mouse(const char* format, ...) {
    if (REAL_MOUSE_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static inline void loggy_dockx(const char* format, ...) {
    if (DOCKX_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static inline void loggy_shader_background(const char* format, ...) {
    if (SHADER_BACKGROUND_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static inline void loggy_wobbly(const char* format, ...) {
    if (WOBBLY_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static inline void loggy_window_management(const char* format, ...) {
    if (WINDOW_MANAGEMENT_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static inline void loggy_show_mouse(const char* format, ...) {
    if (SHOW_MOUSE_DEBUG_LOGS) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}



#endif // LOGS_H
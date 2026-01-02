// opengl_desktop_plugin.c - FIXED STANDALONE EXECUTABLE WITH REAL SCENE BUFFERS
#define _GNU_SOURCE 
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
#include <sys/un.h>      // for ancillary data

#include <wlr/render/gles2.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_drm.h> // For wlr_drm_format
// Add these includes at the top of your plugin file
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

// Headers from Wobbly Windows
#include <stdlib.h>
#include <string.h>
#include <limits.h> // For SHRT_MAX/SHRT_MIN instead of values.h
#include <math.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <GLES2/gl2.h>
#include <time.h>

#define GRID_PADDING 10.0f
#define GRID_ANIMATION_SPEED 0.1f 

float get_current_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f;
}

static pthread_mutex_t dmabuf_mutex = PTHREAD_MUTEX_INITIALIZER;


#ifndef PFNGLEGLIMAGETARGETTEXTURE2DOESPROC
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
#endif

// 1. CRITICAL: Fix the window structure in wobbly version
// The working version has a simpler window structure:
struct plugin_window {
    GLuint gl_texture;
    float x, y, width, height;
    bool is_valid;
    bool is_focused;
    float alpha;
    uint64_t buffer_id;

    // Wobbly additions
    struct surface *wobbly_surface;
    struct timeval last_move_time;
    float last_x, last_y;
    // FIXED: Grid animation state
    float original_x, original_y, original_width, original_height;
    float target_x, target_y, target_width, target_height;
    bool grid_animation_active;  // NEW: Track if this window is animating
    float grid_animation_progress;  // NEW: 0.0 to 1.0 animation progress


   // ADD THIS - SSD support
    struct ssd_elements {
        bool enabled;
        bool needs_close_button;
        bool needs_title_bar;
        char title[256];
    } ssd;

};
// The plugin's internal state
// The plugin's internal states
struct plugin_state {
    // Core file descriptors and memory
    int drm_fd; 
    int ipc_fd;
    void* shm_ptr;
    int dmabuf_fd;
    void* dmabuf_ptr; // Can still be useful for debugging
    EGLImage egl_image;
    
    // Headless rendering context
    struct gbm_device *gbm;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    
    // Off-screen rendering resources
    GLuint fbo;
    GLuint fbo_texture;
    int width, height; // Dimensions of our off-screen buffer
    
    // OpenGL resources - matching the original plugin
    GLuint window_program;
    GLuint quad_vao;
    GLuint quad_vbo;
    GLuint quad_ibo;
    
    // NEW: Wobbly window shader
    GLuint wobbly_program;
    GLint wobbly_mvp_uniform;
    
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
    
    // Window data - using struct plugin_window
    struct plugin_window windows[10];
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
    
    // DMA-BUF and memory management
    dmabuf_share_context_t dmabuf_ctx;
    int cached_memfd;
    void *cached_memfd_ptr;
    size_t cached_memfd_size;
    
    struct wlr_allocator *allocator;
    struct wlr_buffer *export_buffer; 
    
    // Wobbly window shader and resources
    GLuint wobbly_vbo;
    GLuint wobbly_uv_bo;
    GLuint wobbly_ibo;
    int wobbly_index_count; 
    
    // Content change tracking
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
    
    // DMA-BUF export control
    bool use_egl_export; 
    bool dmabuf_sent_to_compositor;      // Track if we sent the DMA-BUF already
    uint32_t last_frame_sequence;       // Track frame sequence
    struct timeval dmabuf_send_time; 
    
    // Instance identification
    int instance_id;  // Unique per plugin
    char instance_name[64];  // For debugging

    bool grid_mode_active;

       // SSD rendering resources
    GLuint rect_program;
    GLuint rect_vao;
    GLuint rect_vbo;
    GLuint rect_ibo;
    GLint rect_transform_uniform;
    GLint rect_color_uniform;

      GLint rect_size_uniform;
    GLint rect_origin_uniform;
     GLint rect_resolution_uniform;
         GLint rect_time_uniform;
    GLint rect_iresolution_uniform;

        GLint iresolution_uniform;
    GLint corner_radius_uniform;
    GLint bevel_color_uniform;
    GLint time_uniform;


    GLint wobbly_iresolution_uniform;
    GLint wobbly_corner_radius_uniform;
    GLint wobbly_bevel_color_uniform;
    GLint wobbly_time_uniform;

};


// SSD Configuration (same as before)
#define SSD_TITLE_BAR_HEIGHT 10
#define SSD_BORDER_WIDTH 10
#define  SSD_BORDER_BOTTOM_WIDTH 8
#define SSD_BUTTON_SIZE 15
#define SSD_CLOSE_BUTTON_MARGIN 5

// SSD Colors
static const float SSD_TITLE_BAR_COLOR[4] = {0.2f, 0.2f, 0.2f, 1.0f};
static const float SSD_BORDER_COLOR[4] = {0.15f, 0.15f, 0.15f, 1.0f};
static const float SSD_CLOSE_BUTTON_COLOR[4] = {0.8f, 0.2f, 0.2f, 1.0f};
static const float SSD_TEXT_COLOR[4] = {0.9f, 0.9f, 0.9f, 1.0f};

static const char* rect_vertex_shader =
    "#version 100\n"
    "precision mediump float;\n"
    "attribute vec2 position;\n"         // Input vertex position (0.0 to 1.0 for the quad)
    "uniform mat3 transform;\n"
    "uniform vec2 u_resolution;\n"       // NEW: The FBO/Screen resolution (e.g., 1920, 1080)
    "varying vec2 v_texcoord;\n"         // NEW: Pass texture coordinates to fragment shader
    "void main() {\n"
    "    // 1. Calculate the vertex's final position in Normalized Device Coordinates (-1 to 1)\n"
    "    vec3 pos_ndc = transform * vec3(position, 1.0);\n"
    "    gl_Position = vec4(pos_ndc.xy, 0.0, 1.0);\n\n"
    "    // 2. Pass the original position as texture coordinates (0.0 to 1.0)\n"
    "    v_texcoord = position;\n"
    "}\n";

static const char* rect_fragment_shader =
    "#version 100\n"
    "precision highp float;\n"
    "\n"
    "varying vec2 v_texcoord;\n"
    "\n"
    "uniform float time;\n"

    "\n"
    "// --- Effect Parameters ---\n"
    "const float cornerRadius = 40.0;\n"
    "const float borderWidth = 4.0;\n"
    "const float aa = 1.5;\n"
    "\n"
    "// --- Helper functions ---\n"
    "float sdRoundedBox(vec2 p, vec2 b, float r) {\n"
    "    vec2 q = abs(p) - b + r;\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;\n"
    "}\n"
    "\n"
    "vec3 hsv2rgb(vec3 c) {\n"
    "    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);\n"
    "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
    "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
    "}\n"
    "\n"
    "void main() {\n"
        "vec2 iResolution =vec2(720.0,40.0); // Resolution of the rect being drawn\n"
    "    // --- 1. Shape Calculation ---\n"
    "    vec2 p = (v_texcoord - 0.5) * iResolution;\n"
    "    float d = sdRoundedBox(p, iResolution * 0.5, cornerRadius);\n"
    "\n"
    "    // --- 2. Horizontal Gradient Color Calculation ---\n"
    "    float hue_base = v_texcoord.x;\n"
    "\n"
    "    // Animate the hue by shifting it over time\n"
    "    float hue = fract(hue_base - time * 0.15);\n"
    "\n"
    "    // Create the two colors from the same hue\n"
    "    vec3 border_color = hsv2rgb(vec3(hue, 0.9, 1.0)); // Bright, saturated border\n"
    "    vec3 fill_color   = hsv2rgb(vec3(hue, 0.8, 0.5)); // Dimmer, less saturated fill\n"
    "\n"
    "    // --- 3. Final Composition ---\n"
    "    float border_mix = smoothstep(-borderWidth, -borderWidth + aa, d);\n"
    "    vec3 final_rgb = mix(border_color, fill_color, border_mix);\n"
    "\n"
    "    float border_alpha = 0.95;\n"
    "    float fill_alpha = 0.0;\n"
    "    float final_alpha = mix(border_alpha, fill_alpha, border_mix);\n"
    "\n"
    "    final_alpha *= (1.0 - smoothstep(-aa, aa, d));\n"
    "\n"
    "    // GLSL ES 1.00 uses gl_FragColor instead of out variables\n"
    "    // Manual swizzle to achieve .bgra effect\n"
    "    gl_FragColor = vec4(final_rgb.b, final_rgb.g, final_rgb.r, final_alpha);\n"
    "}\n";





/******************************************************************************/
/* START: WOBBLY WINDOWS - CODE FROM FIRST FILE                               */
/******************************************************************************/

#define WOBBLY_FRICTION 2
#define WOBBLY_SPRING_K 2

#define GRID_WIDTH  4
#define GRID_HEIGHT 4

#define MODEL_MAX_SPRINGS (GRID_WIDTH * GRID_HEIGHT * 2)
#define MASS 100.0f

typedef struct _xy_pair {
    float x, y;
} Point, Vector;

typedef struct _Edge {
    float next, prev;
    float start;
    float end;
    float attract;
    float velocity;
} Edge;

typedef struct _Object {
    Vector   force;
    Point    position;
    Vector   velocity;
    float    theta;
    int      immobile;
    Edge     vertEdge;
    Edge     horzEdge;
} Object;

typedef struct _Spring {
    Object *a;
    Object *b;
    Vector offset;
} Spring;

typedef struct _Model {
    Object   *objects;
    int      numObjects;
    Spring   springs[MODEL_MAX_SPRINGS];
    int      numSprings;
    Object   *anchorObject;
    float    steps;
    Point    topLeft;
    Point    bottomRight;
} Model;

typedef struct _WobblyWindow {
    Model        *model;
    int          wobbly;
    int         grabbed;
    int        velocity;
    unsigned int  state;
} WobblyWindow;

// Wobbly state flags
#define WobblyInitial  (1L << 0)
#define WobblyForce    (1L << 1)
#define WobblyVelocity (1L << 2)


// Forward declaration of the surface struct used by wobbly logic
struct surface;

// All wobbly_* functions and their helpers (objectInit, modelStep, etc.)
// are placed here, unchanged from the source file.

static void objectInit (Object *object, float positionX, float positionY, float velocityX, float velocityY) {
    object->force.x = 0;
    object->force.y = 0;
    object->position.x = positionX;
    object->position.y = positionY;
    object->velocity.x = velocityX;
    object->velocity.y = velocityY;
    object->theta    = 0;
    object->immobile = 0;
    object->vertEdge.next = 0.0f;
    object->horzEdge.next = 0.0f;
}

static void springInit (Spring *spring, Object *a, Object *b, float offsetX, float offsetY) {
    spring->a = a;
    spring->b = b;
    spring->offset.x = offsetX;
    spring->offset.y = offsetY;
}

static void modelCalcBounds (Model *model) {
    model->topLeft.x = SHRT_MAX;
    model->topLeft.y = SHRT_MAX;
    model->bottomRight.x = SHRT_MIN;
    model->bottomRight.y = SHRT_MIN;
    for (int i = 0; i < model->numObjects; i++) {
        if (model->objects[i].position.x < model->topLeft.x) model->topLeft.x = model->objects[i].position.x;
        else if (model->objects[i].position.x > model->bottomRight.x) model->bottomRight.x = model->objects[i].position.x;
        if (model->objects[i].position.y < model->topLeft.y) model->topLeft.y = model->objects[i].position.y;
        else if (model->objects[i].position.y > model->bottomRight.y) model->bottomRight.y = model->objects[i].position.y;
    }
}

static void modelAddSpring (Model *model, Object *a, Object *b, float offsetX, float offsetY) {
    Spring *spring = &model->springs[model->numSprings++];
    springInit (spring, a, b, offsetX, offsetY);
}

static void modelSetMiddleAnchor (Model *model, int x, int y, int width, int height) {
    float gx = ((GRID_WIDTH - 1) / 2.0f * width) / (float)(GRID_WIDTH - 1);
    float gy = ((GRID_HEIGHT - 1) / 2.0f * height) / (float)(GRID_HEIGHT - 1);
    if (model->anchorObject) model->anchorObject->immobile = 0;
    model->anchorObject = &model->objects[GRID_WIDTH * ((GRID_HEIGHT - 1) / 2) + (GRID_WIDTH - 1) / 2];
    model->anchorObject->position.x = x + gx;
    model->anchorObject->position.y = y + gy;
    model->anchorObject->immobile = 1;
}

static void modelInitObjects (Model *model, int x, int y, int width, int height) {
    float gw = GRID_WIDTH - 1;
    float gh = GRID_HEIGHT - 1;
    int i = 0;
    for (int gridY = 0; gridY < GRID_HEIGHT; gridY++) {
        for (int gridX = 0; gridX < GRID_WIDTH; gridX++) {
            objectInit(&model->objects[i], x + (gridX * width) / gw, y + (gridY * height) / gh, 0, 0);
            i++;
        }
    }
    modelSetMiddleAnchor(model, x, y, width, height);
}

static void modelInitSprings (Model *model, int width, int height) {
    float hpad = ((float)width) / (GRID_WIDTH - 1);
    float vpad = ((float)height) / (GRID_HEIGHT - 1);
    model->numSprings = 0;
    int i = 0;
    for (int gridY = 0; gridY < GRID_HEIGHT; gridY++) {
        for (int gridX = 0; gridX < GRID_WIDTH; gridX++) {
            if (gridX > 0) modelAddSpring(model, &model->objects[i - 1], &model->objects[i], hpad, 0);
            if (gridY > 0) modelAddSpring(model, &model->objects[i - GRID_WIDTH], &model->objects[i], 0, vpad);
            i++;
        }
    }
}

static Model* createModel (int x, int y, int width, int height) {
    Model *model = malloc(sizeof(Model));
    if (!model) return 0;
    model->numObjects = GRID_WIDTH * GRID_HEIGHT;
    model->objects = malloc(sizeof(Object) * model->numObjects);
    if (!model->objects) {
        free(model);
        return 0;
    }
    model->anchorObject = 0;
    model->numSprings = 0;
    model->steps = 0;
    modelInitObjects(model, x, y, width, height);
    modelInitSprings(model, width, height);
    modelCalcBounds(model);
    return model;
}

static void objectApplyForce (Object *object, float fx, float fy) {
    object->force.x += fx;
    object->force.y += fy;
}

static void springExertForces (Spring *spring, float k) {
    Vector da, db;
    Vector a = spring->a->position;
    Vector b = spring->b->position;
    da.x = 0.5f * (b.x - a.x - spring->offset.x);
    da.y = 0.5f * (b.y - a.y - spring->offset.y);
    db.x = 0.5f * (a.x - b.x + spring->offset.x);
    db.y = 0.5f * (a.y - b.y + spring->offset.y);
    objectApplyForce(spring->a, k * da.x, k * da.y);
    objectApplyForce(spring->b, k * db.x, k * db.y);
}

static float modelStepObject (Object *object, float friction, float *force) {
    if (object->immobile) {
        object->velocity.x = 0.0f; object->velocity.y = 0.0f;
        object->force.x = 0.0f; object->force.y = 0.0f;
        *force = 0.0f;
        return 0.0f;
    }
    object->force.x -= friction * object->velocity.x;
    object->force.y -= friction * object->velocity.y;
    object->velocity.x += object->force.x / MASS;
    object->velocity.y += object->force.y / MASS;
    object->position.x += object->velocity.x;
    object->position.y += object->velocity.y;
    *force = fabs(object->force.x) + fabs(object->force.y);
    object->force.x = 0.0f; object->force.y = 0.0f;
    return fabs(object->velocity.x) + fabs(object->velocity.y);
}

static int modelStep (Model *model, float friction, float k, float time) {
    int wobbly = 0;
    model->steps += time / 15.0f;
    int steps = floor(model->steps);
    model->steps -= steps;
    if (!steps) return 1;
    for (int j = 0; j < steps; j++) {
        float velocitySum = 0.0f, forceSum = 0.0f, force;
        for (int i = 0; i < model->numSprings; i++) springExertForces(&model->springs[i], k);
        for (int i = 0; i < model->numObjects; i++) {
            velocitySum += modelStepObject(&model->objects[i], friction, &force);
            forceSum += force;
        }
        if (velocitySum > 0.5f) wobbly |= WobblyVelocity;
        if (forceSum > 20.0f) wobbly |= WobblyForce;
    }
    modelCalcBounds(model);
    return wobbly;
}

static void bezierPatchEvaluate (Model *model, float u, float v, float *patchX, float *patchY) {
    float coeffsU[4], coeffsV[4];
    coeffsU[0] = (1 - u) * (1 - u) * (1 - u);
    coeffsU[1] = 3 * u * (1 - u) * (1 - u);
    coeffsU[2] = 3 * u * u * (1 - u);
    coeffsU[3] = u * u * u;
    coeffsV[0] = (1 - v) * (1 - v) * (1 - v);
    coeffsV[1] = 3 * v * (1 - v) * (1 - v);
    coeffsV[2] = 3 * v * v * (1 - v);
    coeffsV[3] = v * v * v;
    *patchX = *patchY = 0.0f;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            *patchX += coeffsU[i] * coeffsV[j] * model->objects[j * GRID_WIDTH + i].position.x;
            *patchY += coeffsU[i] * coeffsV[j] * model->objects[j * GRID_HEIGHT + i].position.y;
        }
    }
}

static float objectDistance (Object *object, float x, float y) {
    float dx = object->position.x - x;
    float dy = object->position.y - y;
    return sqrt (dx * dx + dy * dy);
}

static Object* modelFindNearestObject (Model *model, float x, float y) {
    Object *object = &model->objects[0];
    float minDistance = -1.0;
    for (int i = 0; i < model->numObjects; i++) {
        float distance = objectDistance(&model->objects[i], x, y);
        if (minDistance < 0.0 || distance < minDistance) {
            minDistance = distance;
            object = &model->objects[i];
        }
    }
    return object;
}

struct surface {
   WobblyWindow *ww;
   int x, y, width, height;
   int x_cells, y_cells;
   int grabbed, synced;
   int vertex_count;
   GLfloat *v;
   GLfloat *uv; // Renamed from tex.uv for simplicity
};


static int wobblyEnsureModel(struct surface *surface) {
    if (!surface->ww->model) {
        surface->ww->model = createModel(surface->x, surface->y, surface->width, surface->height);
        if (!surface->ww->model) return 0;
    }
    return 1;
}

void wobbly_prepare_paint(struct surface *surface, int msSinceLastPaint) {
    WobblyWindow *ww = surface->ww;
    if (ww->wobbly) {
        if (ww->wobbly & (WobblyInitial | WobblyVelocity | WobblyForce)) {
            ww->wobbly = modelStep(ww->model, WOBBLY_FRICTION, WOBBLY_SPRING_K,
                                   (ww->wobbly & WobblyVelocity) ? msSinceLastPaint : 16);
            if (ww->wobbly) {
                modelCalcBounds(ww->model);
            } else {
                surface->x = ww->model->topLeft.x;
                surface->y = ww->model->topLeft.y;
                surface->synced = 1;
            }
        }
    }
}

void wobbly_done_paint(struct surface *surface) {
    WobblyWindow *ww = surface->ww;
    if (ww->wobbly) {
        surface->x = ww->model->topLeft.x;
        surface->y = ww->model->topLeft.y;
    }
}

void wobbly_add_geometry(struct surface *surface) {
    WobblyWindow *ww = surface->ww;
    if (ww->wobbly) {
        int iw = surface->x_cells + 1;
        int ih = surface->y_cells + 1;
        surface->vertex_count = iw * ih;

        GLfloat *v_ptr = realloc(surface->v, sizeof(GLfloat) * 2 * surface->vertex_count);
        GLfloat *uv_ptr = realloc(surface->uv, sizeof(GLfloat) * 2 * surface->vertex_count);
        if (!v_ptr || !uv_ptr) { /* handle allocation failure */ return; }

        surface->v = v_ptr;
        surface->uv = uv_ptr;
        
        float cell_w = (float)surface->width / surface->x_cells;
        float cell_h = (float)surface->height / surface->y_cells;

        for (int y = 0; y < ih; y++) {
            for (int x = 0; x < iw; x++) {
                float deformedX, deformedY;
                bezierPatchEvaluate(ww->model, (x * cell_w) / surface->width, (y * cell_h) / surface->height, &deformedX, &deformedY);
                *v_ptr++ = deformedX;
                *v_ptr++ = deformedY;
                *uv_ptr++ = (x * cell_w) / surface->width;
                *uv_ptr++ = 1.0 - ((y * cell_h) / surface->height);
            }
        }
    }
}

void wobbly_resize_notify(struct surface *surface) {
    WobblyWindow *ww = surface->ww;
    if (ww->model) {
        if (!ww->wobbly) {
            modelInitObjects(ww->model, surface->x, surface->y, surface->width, surface->height);
        }
        modelInitSprings(ww->model, surface->width, surface->height);
    }
}

void wobbly_move_notify(struct surface *surface, int dx, int dy) {
    WobblyWindow *ww = surface->ww;
    if (ww->grabbed && ww->model && ww->model->anchorObject) {
        ww->model->anchorObject->position.x += dx;
        ww->model->anchorObject->position.y += dy;
        ww->wobbly |= WobblyInitial;
        surface->synced = 0;
    }
}

void wobbly_grab_notify(struct surface *surface, int x, int y) {
    WobblyWindow *ww = surface->ww;
    if (wobblyEnsureModel(surface)) {
        if (ww->model->anchorObject) ww->model->anchorObject->immobile = 0;
        ww->model->anchorObject = modelFindNearestObject(ww->model, x, y);
        ww->model->anchorObject->immobile = 1;
        ww->grabbed = 1;
        ww->wobbly |= WobblyInitial;
    }
}

void wobbly_ungrab_notify(struct surface *surface) {
    WobblyWindow *ww = surface->ww;
    if (ww->grabbed) {
        if (ww->model && ww->model->anchorObject) {
            ww->model->anchorObject->immobile = 0;
            ww->model->anchorObject = NULL;
            ww->wobbly |= WobblyInitial;
        }
        ww->grabbed = 0;
    }
}

int wobbly_init(struct surface *surface) {
    WobblyWindow *ww = malloc(sizeof(WobblyWindow));
    if (!ww) return 0;
    ww->model = 0;
    ww->wobbly = 0;
    ww->grabbed = 0;
    ww->state = 0;
    surface->ww = ww;
    if (!wobblyEnsureModel(surface)) {
         free(ww);
         return 0;
    }
    return 1;
}

void wobbly_fini(struct surface *surface) {
    WobblyWindow *ww = surface->ww;
    if (ww) {
        if (ww->model) {
            free(ww->model->objects);
            free(ww->model);
        }
        free(ww);
    }
    free(surface->v);
    free(surface->uv); // FIX: Free UV buffer to prevent memory leak
}

/******************************************************************************/
/* END: WOBBLY WINDOWS - CODE FROM FIRST FILE                                 */
/******************************************************************************/

// Add this forward declaration near the top with other declarations
// Add these function declarations
static GLuint create_texture_hybrid(struct plugin_state *state, const ipc_window_info_t *info, int window_index);
static GLuint create_texture_from_dmabuf_manual(struct plugin_state *state, const ipc_window_info_t *info);
static GLuint create_texture_with_stride_fix_optimized(struct plugin_state *state, const ipc_window_info_t *info);
static void handle_window_list_hybrid(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count);
static void render_frame_hybrid(struct plugin_state *state, int frame_count);
static GLuint texture_from_dmabuf(struct plugin_state *state, const ipc_window_info_t *info);
static GLuint create_texture_with_stride_fix(struct plugin_state *state, const ipc_window_info_t *info);
// Add these declarations near the top of your plugin file
static void handle_window_list_with_textures(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count);
static void render_frame_opengl_with_windows(struct plugin_state *state, int frame_count);
static void render_window(struct plugin_state *state, int index) ;
static int export_dmabuf_fallback(struct plugin_state *state) ;
static void send_frame_ready(struct plugin_state *state) ;
static void render_frame_with_wobbly(struct plugin_state *state, int frame_count);
// Add these function declarations

// Quad vertices with flipped texture coordinates (from original)
// Updated quad vertices with corrected texture coordinates (flipped Y)
// Fixed quad vertices - flip the texture Y coordinates
static const GLfloat quad_vertices[] = {
    0.0f, 0.0f, 0.0f, 1.0f,  // Bottom-left  - flipped texture Y from 0.0f to 1.0f
    1.0f, 0.0f, 1.0f, 1.0f,  // Bottom-right - flipped texture Y from 0.0f to 1.0f
    0.0f, 1.0f, 0.0f, 0.0f,  // Top-left     - flipped texture Y from 1.0f to 0.0f
    1.0f, 1.0f, 1.0f, 0.0f,  // Top-right    - flipped texture Y from 1.0f to 0.0f
};
static const GLuint quad_indices[] = {0, 1, 2, 1, 3, 2};

// Vertex shader (from original)
static const char* window_vertex_shader = 
    "#version 100\n"
    "precision mediump float;\n"
    "attribute vec2 position;\n"
    "attribute vec2 texcoord;\n"
    "uniform mat3 transform;\n"
    "uniform vec2 window_size;\n"  // Keep this for compatibility but don't use it
    "varying vec2 v_texcoord;\n"
    "\n"
    "void main() {\n"
    "    v_texcoord = texcoord;\n"
    "    // FIXED: Don't multiply by window_size since transform already handles scaling\n"
    "    vec3 pos = transform * vec3(position, 1.0);\n"
    "    gl_Position = vec4(pos.xy, 0.0, 1.0);\n"
    "}\n";



// Fragment shader (from original)
static const char* window_fragment_shader =
    "#version 100\n"
    "precision mediump float;\n"
    "uniform sampler2D window_texture;\n"
    "uniform float alpha;\n"
    "uniform vec2 iResolution;\n"
    "uniform float cornerRadius;\n"
    "uniform vec4 bevelColor;\n"
    "uniform float time;\n"
    "varying vec2 v_texcoord;\n"
    "\n"
    "const float bevelWidth = 4.0;\n"
    "const float aa = 1.5;\n"
    "\n"
    "// SDF function\n"
    "float sdRoundedBox(vec2 p, vec2 b, float r) {\n"
    "    vec2 q = abs(p) - b + r;\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    // --- 1. Shape Calculation ---\n"
    "    vec2 p = (v_texcoord - 0.5) * iResolution;\n"
    "    float d = sdRoundedBox(p, iResolution * 0.5, cornerRadius);\n"
    "\n"
    "    // --- 2. Alpha Mask ---\n"
    "    float shape_alpha = 1.0 - smoothstep(-aa, aa, d);\n"
    "\n"
    "    // --- 3. Bevel Intensity ---\n"
    "    float bevel_intensity = smoothstep(-bevelWidth, 0.0, d);\n"
    "    bevel_intensity -= smoothstep(0.0, aa, d);\n"
    "\n"
    "    // --- 4. Moving Gradient Calculation ---\n"
    "    float angle = atan(p.y, p.x);\n"
    "    float gradient_wave = sin(angle * 2.0 - time * 2.5) * 0.5 + 0.5;\n"
    "    float highlight_factor = pow(gradient_wave, 8.0);\n"
    "    float brightness_modulator = 0.7 + highlight_factor * 0.6;\n"
    "\n"
    "    // --- 5. Color Composition ---\n"
    "    vec4 tex_color = texture2D(window_texture, v_texcoord);\n"
    "    \n"
    "    // Swap R and B channels: BGRA -> RGBA\n"
    "    vec4 corrected = vec4(tex_color.b, tex_color.g, tex_color.r, tex_color.a);\n"
    "    \n"
    "    if (corrected.a < 0.01) {\n"
    "        discard;\n"
    "    }\n"
    "\n"
    "    // Modulate the base bevel color with our moving brightness gradient\n"
    "    vec3 final_bevel_color = bevelColor.rgb * brightness_modulator;\n"
    "\n"
    "    // Mix the texture color with the final, highlighted bevel color\n"
    "    vec3 final_rgb = mix(corrected.rgb, final_bevel_color, bevel_intensity * bevelColor.a);\n"
    "\n"
    "    // --- 6. Final Output ---\n"
    "    float final_alpha = corrected.a * shape_alpha * alpha;\n"
    "    gl_FragColor = vec4(final_rgb, final_alpha);\n"
    "}\n";

// NEW: Vertex shader for wobbly windows (from wobbly demo)
static const char *wobbly_vertex_shader =
    "#version 100\n"
    "uniform mat4 modelviewProjection;\n"
    "attribute vec4 pos;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   gl_Position = modelviewProjection * pos;\n"
    "   v_texcoord = texcoord;\n"
    "}\n";

// The wobbly fragment shader can be the same as the standard one.
// We just need to handle the BGRA->RGBA swizzle.
// A "uber-shader" that can render window content or procedural decorations
// Corrected wobbly_fragment_shader
static const char* wobbly_fragment_shader =
    "#version 100\n"
    "precision mediump float;\n"
    "\n"
    "// Input from vertex shader\n"
    "varying vec2 v_texcoord;\n"
    "\n"
    "// Uniforms for texturing and effects\n"
    "uniform sampler2D window_texture;\n"
    "uniform float alpha;\n"
    "uniform float time;\n"
    "uniform vec2 iResolution;\n"
    "uniform float cornerRadius;\n"
    "uniform vec4 bevelColor;\n"
    "\n"
    "// Uniform to switch between rendering modes\n"
    "uniform bool u_use_procedural_color;\n"
    "\n"
    "// Constants for effects\n"
    "const float bevelWidth = 4.0;\n"
    "const float aa = 1.5;\n"
    "\n"
    "// --- Helper functions ---\n"
    "float sdRoundedBox(vec2 p, vec2 b, float r) {\n"
    "    vec2 q = abs(p) - b + r;\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;\n"
    "}\n"
    "\n"
    "vec3 hsv2rgb(vec3 c) {\n"
    "    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);\n"
    "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
    "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    if (u_use_procedural_color) {\n"
    "        // --- Mode 1: Procedural color for decorations ---\n"
    "        vec2 p_deco = (v_texcoord - 0.5) * vec2(720.0, 40.0);\n"
    "        float d_deco = sdRoundedBox(p_deco, vec2(720.0, 40.0) * 0.5, 40.0);\n"
    "        float hue_base = v_texcoord.x;\n"
    "        float hue = fract(hue_base - time * 0.15);\n"
    "        vec3 border_color = hsv2rgb(vec3(hue, 0.9, 1.0));\n"
    "        vec3 fill_color   = hsv2rgb(vec3(hue, 0.8, 0.5));\n"
    "        float border_mix = smoothstep(-4.0, -4.0 + aa, d_deco);\n"
    "        vec3 final_rgb_deco = mix(border_color, fill_color, border_mix);\n"
    "        float final_alpha_deco = mix(0.95, 0.0, border_mix) * (1.0 - smoothstep(-aa, aa, d_deco));\n"
    "        gl_FragColor = vec4(final_rgb_deco.b, final_rgb_deco.g, final_rgb_deco.r, final_alpha_deco);\n"
    "    } else {\n"
    "        // --- FIX: Mode 2: Apply rounded corners and bevel to the window texture ---\n"
    "        // 1. Shape Calculation\n"
    "        vec2 p = (v_texcoord - 0.5) * iResolution;\n"
    "        float d = sdRoundedBox(p, iResolution * 0.5, cornerRadius);\n"
    "\n"
    "        // 2. Alpha Mask\n"
    "        float shape_alpha = 1.0 - smoothstep(-aa, aa, d);\n"
    "\n"
    "        // 3. Bevel Intensity\n"
    "        float bevel_intensity = smoothstep(-bevelWidth, 0.0, d);\n"
    "        bevel_intensity -= smoothstep(0.0, aa, d);\n"
    "\n"
    "        // 4. Moving Gradient Calculation\n"
    "        float angle = atan(p.y, p.x);\n"
    "        float gradient_wave = sin(angle * 2.0 - time * 2.5) * 0.5 + 0.5;\n"
    "        float highlight_factor = pow(gradient_wave, 8.0);\n"
    "        float brightness_modulator = 0.7 + highlight_factor * 0.6;\n"
    "\n"
    "        // 5. Color Composition\n"
    "        vec4 tex_color = texture2D(window_texture, v_texcoord);\n"
    "        vec4 corrected = vec4(tex_color.b, tex_color.g, tex_color.r, tex_color.a);\n"
    "        if (corrected.a < 0.01) {\n"
    "            discard;\n"
    "        }\n"
    "\n"
    "        vec3 final_bevel_color = bevelColor.rgb * brightness_modulator;\n"
    "        vec3 final_rgb = mix(corrected.rgb, final_bevel_color, bevel_intensity * bevelColor.a);\n"
    "\n"
    "        // 6. Final Output\n"
    "        float final_alpha = corrected.a * shape_alpha * alpha;\n"
    "        gl_FragColor = vec4(final_rgb, final_alpha);\n"
    "    }\n"
    "}\n";


static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char* log = malloc(length);
        glGetShaderInfoLog(shader, length, NULL, log);
        loggy_wobbly( "Shader compile error: %s\n", log);
        free(log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Utility to create a shader program
static GLuint create_program(const char* vs_src, const char* fs_src) {
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!vertex || !fragment) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLchar log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        loggy_wobbly( "Program link error: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}


// REPLACE your init_headless_renderer function
static bool init_headless_renderer(struct plugin_state *state) {
    int drm_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
        perror("[PLUGIN] Failed to open DRM render node");
        return false;
    }

    state->renderer = wlr_gles2_renderer_create_with_drm_fd(drm_fd);
    if (!state->renderer) {
        loggy_wobbly( "[PLUGIN] Failed to create wlr_gles2_renderer\n");
        close(drm_fd);
        return false;
    }

    state->allocator = wlr_allocator_autocreate(NULL, state->renderer);
    if (!state->allocator) {
        loggy_wobbly( "[PLUGIN] Failed to create wlr_allocator\n");
        wlr_renderer_destroy(state->renderer);
        return false;
    }

    loggy_wobbly( "[PLUGIN] ✅ Successfully initialized headless wlroots context\n");
    return true;
}
static int create_dmabuf(size_t size, int instance_id) {
    int result = -1;
    int lock_fd = -1;
    pid_t pid = getpid();
    
    // FIXED: Instance-specific lock file
    char lock_file[256];
    snprintf(lock_file, sizeof(lock_file), "/tmp/dmabuf_create_%d.lock", instance_id);
    
    lock_fd = open(lock_file, O_CREAT|O_RDWR, 0666);
    if (lock_fd < 0) {
        loggy_wobbly( "[PLUGIN-%d] Failed to create lock file: %s\n", instance_id, strerror(errno));
        return -1;
    }
    
    // Lock for multi-process safety
    if (flock(lock_fd, LOCK_EX) < 0) {
        loggy_wobbly( "[PLUGIN] PID=%d Failed to acquire lock: %s\n", pid, strerror(errno));
        close(lock_fd);
        return -1;
    }
    
    loggy_wobbly( "[PLUGIN] PID=%d Creating DMA-BUF of size %zu bytes\n", 
        pid, size);
    
    // Method 1: Try DMA heap allocation
    const char* heap_path = "/dev/dma_heap/system";
    loggy_wobbly( "[PLUGIN] PID=%d Attempting to open: %s\n", pid, heap_path);
    
    int heap_fd = open(heap_path, O_RDONLY | O_CLOEXEC);
    if (heap_fd < 0) {
        loggy_wobbly( "[PLUGIN] PID=%d Failed to open %s: %s (errno=%d)\n", 
                pid, heap_path, strerror(errno), errno);
        goto try_fallback;
    }
    
    loggy_wobbly( "[PLUGIN] PID=%d ✅ Successfully opened %s (fd=%d)\n", 
            pid, heap_path, heap_fd);
    
    struct dma_heap_allocation_data heap_data = {
        .len = size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };
    
    int ioctl_result = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
    if (ioctl_result == 0) {
        loggy_wobbly( "[PLUGIN] PID=%d ✅ DMA heap allocation SUCCESS: fd=%d\n", 
                pid, heap_data.fd);
        
        // Test the returned fd
        struct stat stat_buf;
        if (fstat(heap_data.fd, &stat_buf) == 0) {
            loggy_wobbly( "[PLUGIN] PID=%d DMA-BUF fd stats: size=%ld, mode=0%o\n", 
                    pid, stat_buf.st_size, stat_buf.st_mode);
        }
        
        result = heap_data.fd;
        goto cleanup_heap_fd;
    }
    
    loggy_wobbly( "[PLUGIN] PID=%d ioctl DMA_HEAP_IOCTL_ALLOC FAILED: %s (errno=%d)\n", 
            pid, strerror(errno), errno);
    
    // Method 2: Try smaller allocation if the original was large
    if (size > 1024 * 1024) {  // If > 1MB
        loggy_wobbly( "[PLUGIN] PID=%d Large allocation failed, trying smaller test allocation...\n", pid);
        
        struct dma_heap_allocation_data test_data = {
            .len = 4096,  // Just 4KB test
            .fd_flags = O_RDWR | O_CLOEXEC,
        };
        
        if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &test_data) == 0) {
            loggy_wobbly( "[PLUGIN] PID=%d ✅ Small test allocation worked! The issue might be size limits.\n", pid);
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
                loggy_wobbly( "[PLUGIN] PID=%d ✅ Medium allocation worked: fd=%d\n", 
                        pid, medium_data.fd);
                result = medium_data.fd;
                goto cleanup_heap_fd;
            } else {
                loggy_wobbly( "[PLUGIN] PID=%d Medium allocation failed: %s\n", 
                        pid, strerror(errno));
            }
        } else {
            loggy_wobbly( "[PLUGIN] PID=%d Even small test allocation failed: %s\n", 
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
    loggy_wobbly( "[PLUGIN] PID=%d DMA heap failed, falling back to memfd (won't work for zero-copy)\n", pid);
    
    int memfd = memfd_create("plugin-fbo-fallback", MFD_CLOEXEC);
    if (memfd < 0) {
        loggy_wobbly( "[PLUGIN] PID=%d memfd_create failed: %s\n", pid, strerror(errno));
        goto error;
    }
    
    if (ftruncate(memfd, size) != 0) {
        loggy_wobbly( "[PLUGIN] PID=%d memfd ftruncate failed: %s\n", pid, strerror(errno));
        close(memfd);
        goto error;
    }
    
    loggy_wobbly( "[PLUGIN] PID=%d ⚠️  Created memfd fallback: fd=%d (THIS WON'T WORK FOR ZERO-COPY)\n", 
            pid, memfd);
    result = memfd;
    goto success;

error:
    loggy_wobbly( "[PLUGIN] PID=%d ❌ All allocation methods failed\n", pid);
    result = -1;

success:
    // Unlock and cleanup
    if (lock_fd >= 0) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
    return result;
}


// Helper function to properly close dmabuf
static void close_dmabuf(int fd) {
    if (fd >= 0) {
        pid_t pid = getpid();
        loggy_wobbly( "[PLUGIN] PID=%d Closing dmabuf fd=%d\n", pid, fd);
        close(fd);
    }
}



static bool init_dmabuf_fbo_robust(struct plugin_state *state, int width, int height) {
    loggy_wobbly( "[PLUGIN] Attempting robust DMA-BUF backed FBO (%dx%d)\n", width, height);
    
    state->width = width;
    state->height = height;
    
    // Try multiple DMA-BUF creation methods
    for (int attempt = 0; attempt < 3; attempt++) {
        state->dmabuf_fd = create_dmabuf(width * height * 4, state->instance_id);
        if (state->dmabuf_fd < 0) {
            continue;
        }
        
        loggy_wobbly( "[PLUGIN] Attempt %d: Created DMA-BUF fd: %d\n", attempt + 1, state->dmabuf_fd);
        
        // ✅ FIXED: Better DMA-BUF validation that doesn't reject real DMA-BUFs
        struct dma_buf_sync sync = { .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ };
        bool is_real_dmabuf = (ioctl(state->dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync) == 0);
        
        if (is_real_dmabuf) {
            loggy_wobbly( "[PLUGIN] ✅ Confirmed real DMA-BUF, proceeding with EGL import\n");
        } else {
            // Instead of rejecting, let's check if it's a usable memfd
            struct stat fd_stat;
            if (fstat(state->dmabuf_fd, &fd_stat) == 0 && fd_stat.st_size == (width * height * 4)) {
                loggy_wobbly( "[PLUGIN] ⚠️  Not a real DMA-BUF but valid memfd, proceeding\n");
            } else {
                loggy_wobbly( "[PLUGIN] ❌ FD %d is invalid, trying next attempt\n", state->dmabuf_fd);
                close(state->dmabuf_fd);
                state->dmabuf_fd = -1;
                continue;
            }
        }
        
        // Try EGL import
        PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = 
            (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 
            (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        
        if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
            loggy_wobbly( "[PLUGIN] EGL extensions not available\n");
            close(state->dmabuf_fd);
            state->dmabuf_fd = -1;
            return false;
        }
        
        EGLint attribs[] = {
            EGL_WIDTH, width,
            EGL_HEIGHT, height,
            EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ABGR8888,
            EGL_DMA_BUF_PLANE0_FD_EXT, state->dmabuf_fd,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, width * 4,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
            EGL_NONE
        };
        
        state->egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, 
                                           EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
        if (state->egl_image == EGL_NO_IMAGE_KHR) {
            EGLint error = eglGetError();
            loggy_wobbly( "[PLUGIN] EGL image creation failed (attempt %d): 0x%x\n", 
                    attempt + 1, error);
            close(state->dmabuf_fd);
            state->dmabuf_fd = -1;
            continue;
        }
        
        // Success! Continue with texture and FBO creation
        glGenTextures(1, &state->fbo_texture);
        glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
        
        // ✅ CRITICAL: Set texture parameters BEFORE calling glEGLImageTargetTexture2DOES
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image);
        
        // ✅ Add error checking like the working version
        GLenum gl_error = glGetError();
        if (gl_error != GL_NO_ERROR) {
            loggy_wobbly( "[PLUGIN] Failed to create texture from EGL image: 0x%x\n", gl_error);
            
            switch (gl_error) {
                case GL_INVALID_VALUE:
                    loggy_wobbly( "[PLUGIN] GL_INVALID_VALUE - Invalid texture target or EGL image\n");
                    break;
                case GL_INVALID_OPERATION:
                    loggy_wobbly( "[PLUGIN] GL_INVALID_OPERATION - Context/texture state issue\n");
                    break;
                default:
                    loggy_wobbly( "[PLUGIN] Unknown OpenGL error\n");
                    break;
            }
            
            glDeleteTextures(1, &state->fbo_texture);
            PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
                (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
            if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
            close(state->dmabuf_fd);
            state->dmabuf_fd = -1;
            continue;
        }
        
        loggy_wobbly( "[PLUGIN] ✅ Created GL texture %u from EGL image\n", state->fbo_texture);
        
        glGenFramebuffers(1, &state->fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                              GL_TEXTURE_2D, state->fbo_texture, 0);
        
        GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fb_status == GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
            // ✅ Report the actual status
            if (is_real_dmabuf) {
                loggy_wobbly( "[PLUGIN] ✅ TRUE ZERO-COPY: Real DMA-BUF FBO created on attempt %d!\n", attempt + 1);
            } else {
                loggy_wobbly( "[PLUGIN] ✅ PSEUDO ZERO-COPY: MemFD FBO created on attempt %d!\n", attempt + 1);
            }
            return true;
        } else {
            loggy_wobbly( "[PLUGIN] Framebuffer incomplete on attempt %d: 0x%x\n", 
                    attempt + 1, fb_status);
            // Clean up and try again
            glDeleteFramebuffers(1, &state->fbo);
            glDeleteTextures(1, &state->fbo_texture);
            PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 
                (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
            if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
            close(state->dmabuf_fd);
            state->dmabuf_fd = -1;
        }
    }
    
    loggy_wobbly( "[PLUGIN] ❌ All DMA-BUF FBO creation attempts failed\n");
    return false;
}
/**
 * Initializes the plugin's FBO, backed by a DMA-BUF for zero-copy sharing.
 */
//////////////////////////////////////////////////////////////////////////////breadcrumb
/*nvidia only
// In your plugin executable - create your own EGL context
static bool init_plugin_egl(struct plugin_state *state) {
    loggy_wobbly( "[PLUGIN-%d] Initializing isolated EGL context...\n", state->instance_id);
    
    // Initialize EGL display (shared but that's OK)
    state->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state->egl_display == EGL_NO_DISPLAY) {
        loggy_wobbly( "[PLUGIN-%d] Failed to get EGL display\n", state->instance_id);
        return false;
    }

    if (!eglInitialize(state->egl_display, NULL, NULL)) {
        loggy_wobbly( "[PLUGIN-%d] Failed to initialize EGL\n", state->instance_id);
        return false;
    }

    // Log EGL version for debugging
    const char* egl_version = eglQueryString(state->egl_display, EGL_VERSION);
    loggy_wobbly( "[PLUGIN-%d] EGL Version: %s\n", state->instance_id, egl_version);

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
        loggy_wobbly( "[PLUGIN-%d] Failed to choose EGL config\n", state->instance_id);
        return false;
    }

    if (config_count == 0) {
        loggy_wobbly( "[PLUGIN-%d] No suitable EGL config found\n", state->instance_id);
        return false;
    }

    loggy_wobbly( "[PLUGIN-%d] Found %d EGL config(s)\n", state->instance_id, config_count);

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
        loggy_wobbly( "[PLUGIN-%d] Failed to create EGL context: 0x%x\n", state->instance_id, error);
        
        // Fallback: try simpler context creation
        loggy_wobbly( "[PLUGIN-%d] Trying fallback context creation...\n", state->instance_id);
        EGLint simple_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        state->egl_context = eglCreateContext(state->egl_display, config, EGL_NO_CONTEXT, simple_attribs);
        if (state->egl_context == EGL_NO_CONTEXT) {
            loggy_wobbly( "[PLUGIN-%d] Fallback context creation also failed\n", state->instance_id);
            return false;
        }
    }

    loggy_wobbly( "[PLUGIN-%d] ✅ EGL context created: %p\n", state->instance_id, state->egl_context);

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
        loggy_wobbly( "[PLUGIN-%d] Failed to create PBuffer surface: 0x%x\n", state->instance_id, error);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }

    loggy_wobbly( "[PLUGIN-%d] ✅ PBuffer surface created: %p\n", state->instance_id, state->egl_surface);

    // Make context current with proper error checking
    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        EGLint error = eglGetError();
        loggy_wobbly( "[PLUGIN-%d] Failed to make EGL context current: 0x%x\n", state->instance_id, error);
        eglDestroySurface(state->egl_display, state->egl_surface);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }

    // Verify OpenGL context is working
    const char* gl_vendor = (const char*)glGetString(GL_VENDOR);
    const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
    const char* gl_version = (const char*)glGetString(GL_VERSION);
    
    loggy_wobbly( "[PLUGIN-%d] ✅ OpenGL Context Active:\n", state->instance_id);
    loggy_wobbly( "[PLUGIN-%d]   Vendor: %s\n", state->instance_id, gl_vendor ? gl_vendor : "Unknown");
    loggy_wobbly( "[PLUGIN-%d]   Renderer: %s\n", state->instance_id, gl_renderer ? gl_renderer : "Unknown");
    loggy_wobbly( "[PLUGIN-%d]   Version: %s\n", state->instance_id, gl_version ? gl_version : "Unknown");

    // Test basic OpenGL operation to ensure context works
    GLuint test_texture;
    glGenTextures(1, &test_texture);
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_wobbly( "[PLUGIN-%d] OpenGL context test failed: 0x%x\n", state->instance_id, gl_error);
        eglMakeCurrent(state->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(state->egl_display, state->egl_surface);
        eglDestroyContext(state->egl_display, state->egl_context);
        return false;
    }
    glDeleteTextures(1, &test_texture);

    loggy_wobbly( "[PLUGIN-%d] ✅ EGL context fully initialized and tested\n", state->instance_id);
    return true;
}
*/

// In your plugin executable - create your own EGL context
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

static GLuint texture_from_dmabuf(struct plugin_state *state, const ipc_window_info_t *info) {
    if (!state->dmabuf_ctx.initialized) {
        loggy_wobbly( "[PLUGIN] Shared DMA-BUF context not available, using fallback\n");
        return 0;  // Will fallback to stride fix method
    }
    
    loggy_wobbly( "[PLUGIN] Importing window DMA-BUF via shared library: %dx%d, fd=%d, format=0x%x, modifier=0x%lx\n",
            info->width, info->height, info->fd, info->format, info->modifier);
    
    // Convert to shared library format
    dmabuf_share_buffer_t dmabuf = {
        .fd = info->fd,
        .width = info->width,
        .height = info->height,
        .format = info->format,
        .stride = info->stride,
        .modifier = info->modifier
    };
    
    // Use shared library to import
    GLuint texture = dmabuf_share_import_texture(&state->dmabuf_ctx, &dmabuf);
    
    // ALWAYS close the FD - the shared library should have made its own references
   // loggy_wobbly( "[PLUGIN] 🔍 Shared library done, closing FD %d\n", info->fd);
  //  close(info->fd);
    
    if (texture > 0) {
        loggy_wobbly( "[PLUGIN] ✅ Imported window DMA-BUF via shared library: texture %u\n", texture);
    } else {
        loggy_wobbly( "[PLUGIN] Shared library DMA-BUF import failed for window\n");
    }
    
    return texture;
}

// In plugin main(), add FD monitoring:
static void check_fd_usage(void) {
    static int check_counter = 0;
    if (++check_counter % 60 == 0) {
        // Count open FDs
        int fd_count = 0;
        for (int i = 0; i < 1000; i++) {
            if (fcntl(i, F_GETFD) != -1) {
                fd_count++;
            }
        }
        loggy_wobbly( "[PLUGIN] Open file descriptors: %d/1024\n", fd_count);
        
        if (fd_count > 900) {
            loggy_wobbly( "[PLUGIN] WARNING: High FD usage! Possible leak!\n");
        }
    }
}


static void send_frame_notification_smart(struct plugin_state *state) {
    static int notification_counter = 0;
    static struct timeval last_notification = {0};
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Calculate time since last notification
    struct timeval diff;
    timersub(&now, &last_notification, &diff);
    
    // Only send notification if:
    // 1. Content actually changed, OR
    // 2. It's been more than 33ms (30fps max notification rate)
    bool should_notify = false;
    
    if (state->content_changed_this_frame) {
        should_notify = true;  // Always notify on content change
    } else if (diff.tv_sec > 0 || diff.tv_usec > 33333) {
        // Limit to 30fps when no content changes
        should_notify = true;
    }
    
    if (should_notify) {
        ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
        write(state->ipc_fd, &notification, sizeof(notification));
        last_notification = now;
        
        // Debug log occasionally
        if (++notification_counter <= 3) {
            loggy_wobbly("[PLUGIN] Sent notification #%d\n", notification_counter);
        }
    }
}

// Add these pool management functions
static int get_free_pool_buffer(struct plugin_state *state) {
    if (!state->dmabuf_pool.initialized) {
        return -1;
    }
    
    // Find first available buffer
    for (int i = 0; i < state->dmabuf_pool.pool_size; i++) {
        if (!state->dmabuf_pool.in_use[i]) {
            state->dmabuf_pool.in_use[i] = true;
            loggy_wobbly( "[PLUGIN] 📦 Using pool buffer %d (fd=%d)\n", i, state->dmabuf_pool.fds[i]);
            return i;
        }
    }
    
    loggy_wobbly( "[PLUGIN] ⚠️  No free pool buffers available\n");
    return -1;  // No free buffers
}

static void release_pool_buffer(struct plugin_state *state, int buffer_id) {
    if (buffer_id >= 0 && buffer_id < state->dmabuf_pool.pool_size) {
        state->dmabuf_pool.in_use[buffer_id] = false;
        loggy_wobbly( "[PLUGIN] 📤 Released pool buffer %d\n", buffer_id);
    }
}


// Add this function to test pool-based rendering
static void test_pool_rendering(struct plugin_state *state) {
    loggy_wobbly( "[PLUGIN] === STEP 3: Testing pool-based rendering ===\n");
    
    if (!state->dmabuf_pool.initialized) {
        loggy_wobbly( "[PLUGIN] ❌ Pool not initialized\n");
        return;
    }
    
    // Get a buffer from pool
    int buffer_id = get_free_pool_buffer(state);
    if (buffer_id < 0) {
        loggy_wobbly( "[PLUGIN] ❌ No free pool buffer for rendering\n");
        return;
    }
    
    // For now, just test that we can "use" the buffer
    int pool_fd = state->dmabuf_pool.fds[buffer_id];
    loggy_wobbly( "[PLUGIN] 🎨 Simulating render to pool buffer %d (fd=%d)\n", buffer_id, pool_fd);
    
    // Simulate some work
    usleep(1000); // 1ms "render time"
    
    // For now, just release it (later we'll send it to compositor)
    loggy_wobbly( "[PLUGIN] ✅ Render complete, releasing buffer %d\n", buffer_id);
    release_pool_buffer(state, buffer_id);
    
    loggy_wobbly( "[PLUGIN] ✅ Pool rendering test complete\n");
}

// Add this function to send pool FDs to compositor
static bool send_pool_to_compositor(struct plugin_state *state) {
    loggy_wobbly( "[PLUGIN] === STEP 4: Sending pool to compositor ===\n");
    
    if (!state->dmabuf_pool.initialized) {
        loggy_wobbly( "[PLUGIN] ❌ Pool not initialized\n");
        return false;
    }
    
    // Send simple notification + all FDs in one message
    ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
    
    struct msghdr msg = {0};
    struct iovec iov[1];
    char cmsg_buf[CMSG_SPACE(sizeof(int) * 4)]; // Space for 4 FDs

    // Message data
    iov[0].iov_base = &notification;
    iov[0].iov_len = sizeof(notification);
    
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    // Pack all 4 FDs into ancillary data
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * 4);
    
    int *fd_data = (int*)CMSG_DATA(cmsg);
    for (int i = 0; i < 4; i++) {
        fd_data[i] = dup(state->dmabuf_pool.fds[i]); // Dup for sending
        loggy_wobbly( "[PLUGIN] 📤 Packing pool buffer %d: fd=%d -> dup_fd=%d\n", 
                i, state->dmabuf_pool.fds[i], fd_data[i]);
    }

    // Send the message with all FDs at once
    if (sendmsg(state->ipc_fd, &msg, 0) < 0) {
        loggy_wobbly( "[PLUGIN] ❌ Failed to send pool FDs: %s\n", strerror(errno));
        
        // Clean up duped FDs on failure
        for (int i = 0; i < 4; i++) {
            close(fd_data[i]);
        }
        return false;
    }
    
    loggy_wobbly( "[PLUGIN] ✅ Sent pool (4 FDs) to compositor in single message!\n");
    return true;
}
static void test_pool_usage(struct plugin_state *state) {
    loggy_wobbly( "[PLUGIN] === STEP 2: Testing pool usage ===\n");
    
    // Test getting and releasing buffers
    int buf1 = get_free_pool_buffer(state);
    int buf2 = get_free_pool_buffer(state);
    int buf3 = get_free_pool_buffer(state);
    
    loggy_wobbly( "[PLUGIN] Got buffers: %d, %d, %d\n", buf1, buf2, buf3);
    
    // Release one and get another
    release_pool_buffer(state, buf1);
    int buf4 = get_free_pool_buffer(state);
    
    loggy_wobbly( "[PLUGIN] After release/reuse: buf4=%d\n", buf4);
    
    // Clean up
    release_pool_buffer(state, buf2);
    release_pool_buffer(state, buf3);
    release_pool_buffer(state, buf4);
    
    loggy_wobbly( "[PLUGIN] ✅ Pool usage test complete\n");
}


// In your plugin code:
void send_dmabuf_to_compositor(int ipc_fd, int dmabuf_fd, uint32_t width, uint32_t height, 
                               uint32_t format, uint32_t stride, uint64_t modifier) {
    ipc_notification_t notification = {0};
    notification.type = IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT;
    notification.data.dmabuf_frame_info.width = width;
    notification.data.dmabuf_frame_info.height = height;
    notification.data.dmabuf_frame_info.format = format;
    notification.data.dmabuf_frame_info.stride = stride;
    notification.data.dmabuf_frame_info.modifier = modifier;
    
    struct msghdr msg = {0};
    struct iovec iov = { .iov_base = &notification, .iov_len = sizeof(notification) };
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &dmabuf_fd, sizeof(int));
    
    sendmsg(ipc_fd, &msg, 0);
}



// Add these EGL extension function pointers (add after your includes)
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = NULL;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = NULL;
static PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA = NULL;
static PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA = NULL;

// Add this initialization function (call it once in your plugin init)
static bool init_egl_extensions(EGLDisplay display) {
    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    eglExportDMABUFImageQueryMESA = (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)eglGetProcAddress("eglExportDMABUFImageQueryMESA");
    eglExportDMABUFImageMESA = (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");
    
    if (!eglCreateImageKHR || !eglDestroyImageKHR) {
        loggy_wobbly( "[PLUGIN] Basic EGL image extensions not available\n");
        return false;
    }
    
    if (!eglExportDMABUFImageQueryMESA || !eglExportDMABUFImageMESA) {
        loggy_wobbly( "[PLUGIN] MESA DMA-BUF export extensions not available\n");
        return false;
    }
    
    return true;
}

// Fixed export function
static int export_texture_as_dmabuf(struct plugin_state *state) {
    if (!state->fbo_texture || state->fbo_texture == 0) {
        loggy_wobbly( "[PLUGIN] No FBO texture to export (texture=%u)\n", state->fbo_texture);
        return -1;
    }
    
    // Check if extensions are available
    if (!eglCreateImageKHR) {
        loggy_wobbly( "[PLUGIN] eglCreateImageKHR not available\n");
        return -1;
    }
    
    // Make sure context is current
    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        loggy_wobbly( "[PLUGIN] Failed to make EGL context current: 0x%x\n", eglGetError());
        return -1;
    }
    
    
    // Make sure we're using the right context
    eglMakeCurrent(state->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, state->egl_context);
    
    // Create EGL image from OpenGL texture
    EGLImageKHR egl_image = eglCreateImageKHR(
        state->egl_display,
        state->egl_context,
        EGL_GL_TEXTURE_2D_KHR,
        (EGLClientBuffer)(uintptr_t)state->fbo_texture,
        NULL
    );
    
    if (egl_image == EGL_NO_IMAGE_KHR) {
        loggy_wobbly( "[PLUGIN] Failed to create EGL image from FBO texture\n");
        return -1;
    }
    
    // Export EGL image as DMA-BUF
    int dmabuf_fd = -1;
    EGLint fourcc, num_planes;
    EGLuint64KHR modifier;
    
    // Get DMA-BUF attributes
    if (!eglExportDMABUFImageQueryMESA(state->egl_display, egl_image, 
                                       &fourcc, &num_planes, &modifier)) {
        loggy_wobbly( "[PLUGIN] Failed to query DMA-BUF attributes\n");
        eglDestroyImageKHR(state->egl_display, egl_image);
        return -1;
    }
    
    // Export the actual DMA-BUF
    EGLint stride, offset;
    if (!eglExportDMABUFImageMESA(state->egl_display, egl_image,
                                  &dmabuf_fd, &stride, &offset)) {
        loggy_wobbly( "[PLUGIN] Failed to export DMA-BUF\n");
        eglDestroyImageKHR(state->egl_display, egl_image);
        return -1;
    }
    
    eglDestroyImageKHR(state->egl_display, egl_image);
    
    loggy_wobbly( "[PLUGIN] ✅ Exported FBO texture as DMA-BUF: fd=%d, %dx%d, stride=%d\n",
            dmabuf_fd, state->width, state->height, stride);
    
    return dmabuf_fd;
}
static void render_frame_hybrid_zero_copy(struct plugin_state *state, int frame_count) {
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glViewport(0, 0, state->width, state->height);
    
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0); 
    
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid && state->windows[i].gl_texture != 0) {
            render_window(state, i);
        }
    }
    
    glDisable(GL_BLEND);
    glFlush();
    glFinish(); // Make sure rendering is complete
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    int dmabuf_fd_to_send = -1;
    if (state->dmabuf_fd >= 0) {
        // Duplicate the master FD. This is the FD we will send and then close.
        dmabuf_fd_to_send = dup(state->dmabuf_fd);
        if (dmabuf_fd_to_send >= 0) {
            send_dmabuf_to_compositor(
                state->ipc_fd, 
                dmabuf_fd_to_send,
                state->width,
                state->height,
                DRM_FORMAT_ARGB8888, // NOTE: Your init uses ABGR, ensure this is correct
                state->width * 4,
                DRM_FORMAT_MOD_LINEAR
            );

            // ===================================================================
            // THE FIX: CLOSE THE DUPLICATED FD IMMEDIATELY AFTER SENDING.
            close(dmabuf_fd_to_send);
            // ===================================================================
            
            loggy_wobbly( "[PLUGIN] ✅ Sent and closed dup'd DMA-BUF (orig fd=%d)\n", state->dmabuf_fd);

        } else {
             loggy_wobbly( "[PLUGIN] ❌ dup() failed for sending frame: %s\n", strerror(errno));
        }
    } else {
        loggy_wobbly( "[PLUGIN] No DMA-BUF available, falling back to SHM\n");
        // SHM fallback
    }
}
// REPLACE your render_with_pool_smart function with this version:
static void render_with_pool_smart(struct plugin_state *state, int frame_count) {
    static struct timeval last_render = {0};
    struct timeval now;
    gettimeofday(&now, NULL);
    
    struct timeval diff;
    timersub(&now, &last_render, &diff);
    
    // Only render if content changed OR 16ms elapsed (60fps)
    bool should_render = state->content_changed_this_frame || 
                        (diff.tv_sec > 0 || diff.tv_usec >= 16667);
    
    if (!should_render) {
        return; // Skip this frame
    }
    
    last_render = now;
    
    // CHOOSE THE RIGHT RENDER METHOD based on DMA-BUF availability
    if (state->dmabuf_fd >= 0) {
        // True zero-copy: render directly to DMA-BUF
        render_frame_hybrid_zero_copy(state, frame_count);
        
        static int zero_copy_log = 0;
        if (++zero_copy_log <= 5) {
            loggy_wobbly( "[PLUGIN] ✅ TRUE ZERO-COPY render to DMA-BUF fd=%d (no glReadPixels!)\n", 
                    state->dmabuf_fd);
        }
    } else {
        // Fallback: render to regular FBO + copy to SHM
        render_frame_hybrid(state, frame_count);
        
        static int shm_log = 0;
        if (++shm_log <= 5) {
            loggy_wobbly( "[PLUGIN] SHM fallback render (with glReadPixels)\n");
        }
    }
    
    // Send notifications when content changes
    if (state->content_changed_this_frame) {
        send_frame_notification_smart(state);
        state->content_changed_this_frame = false;
        
        if (frame_count <= 5) {
            loggy_wobbly( "[PLUGIN] 📢 Sent notification for content change\n");
        }
    }
    
    // Occasionally send heartbeat (every 2 seconds)
    static int heartbeat_counter = 0;
    if (++heartbeat_counter % 120 == 0) {  // Every 2 seconds at 60fps
        send_frame_notification_smart(state);
    }
}
/*
// Add this function after render_with_pool_smart and before render_with_wobbly_fixed
static void update_wobbly_windows(struct plugin_state *state, long elapsed_ms) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    for (int i = 0; i < state->window_count; i++) {
        struct plugin_window *win = &state->windows[i];
        if (!win->is_valid || !win->wobbly_surface) continue;
        
        struct surface *s = win->wobbly_surface;
        
        // Handle grab timeout
        struct timeval diff;
        timersub(&now, &win->last_move_time, &diff);
        if (s->grabbed && diff.tv_sec >= 1) {
            wobbly_ungrab_notify(s);
        }
        
        // Update wobbly animation
        wobbly_prepare_paint(s, elapsed_ms);
        wobbly_add_geometry(s);
        wobbly_done_paint(s);
        
        // Update window position from wobbly surface
        win->x = s->x;
        win->y = s->y;
    }
}*/





// FIXED: Handle window list updates during grid mode
static void handle_window_list_with_grid_support(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count) {
    // First, handle the window list update normally
    handle_window_list_hybrid(state, event, fds, fd_count);
    
    // FIXED: Handle grid layout for new windows
    for (int i = 0; i < event->data.window_list.count && i < 10; i++) {
        struct plugin_window *win = &state->windows[i];
        if (win->is_valid) {
            ipc_window_info_t *info = &event->data.window_list.windows[i];
            
            if (state->grid_mode_active) {
                // FIXED: If grid is active, new windows should animate to grid position
                // But don't interrupt ongoing animations
                if (!win->grid_animation_active) {
                    // This is a new window or position update - recalculate grid
                    calculate_and_start_grid_layout_fixed(state);
                    break; // Only recalculate once
                }
            } else {
                // FIXED: Normal mode - set targets to actual positions
                if (!win->grid_animation_active) {
                    win->target_x = info->x;
                    win->target_y = info->y;  
                    win->target_width = info->width;
                    win->target_height = info->height;
                }
            }
        }
    }
}
// Fixed Grid System - Replace your existing grid functions with these

#define GRID_ANIMATION_THRESHOLD 2.0f  // Pixels - when to consider animation complete

// Single, unified grid calculation function
// opengl_desktop_plugin.c

// ... (previous code) ...

// Single, unified grid calculation function -- WITH ASPECT RATIO CORRECTION
static void calculate_grid_layout(struct plugin_state *state) {
    loggy_wobbly("[GRID] Calculating aspect-ratio-correct grid layout\n");
    
    int valid_windows = 0;
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid) {
            valid_windows++;
        }
    }

    if (valid_windows == 0) return;

    // Calculate grid dimensions
    int cols = (int)ceil(sqrt((double)valid_windows));
    int rows = (int)ceil((double)valid_windows / (double)cols);
    
    float screen_margin = 50.0f;
    float available_width = state->width - (screen_margin * 2);
    float available_height = state->height - (screen_margin * 2);
    
    float cell_width = available_width / cols;
    float cell_height = available_height / rows;

    int current_window = 0;
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid) {
            struct plugin_window *win = &state->windows[i];

            // Store current position as original (for animating back)
            // This is also where we get the original size for our aspect ratio calculation
            win->original_x = win->x;
            win->original_y = win->y;
            win->original_width = win->width;
            win->original_height = win->height;

            // ======================= THE FIX IS HERE =======================

            // 1. Define the maximum available space inside the cell, accounting for padding.
            float padded_cell_w = cell_width - (2 * GRID_PADDING);
            float padded_cell_h = cell_height - (2 * GRID_PADDING);

            float target_w, target_h;

            // 2. Calculate the original aspect ratio. Avoid division by zero.
            if (win->original_height > 0) {
                float original_aspect_ratio = win->original_width / win->original_height;

                // 3. Determine if the window should be scaled based on its width or height
                // to fit inside the padded cell space.
                if ((padded_cell_w / original_aspect_ratio) <= padded_cell_h) {
                    // Limited by width: The window is proportionally wider than the cell.
                    // Set the width to the max available width and scale the height down.
                    target_w = padded_cell_w;
                    target_h = padded_cell_w / original_aspect_ratio;
                } else {
                    // Limited by height: The window is proportionally taller than the cell.
                    // Set the height to the max available height and scale the width down.
                    target_h = padded_cell_h;
                    target_w = padded_cell_h * original_aspect_ratio;
                }
            } else {
                // Fallback for windows with no height (unlikely, but safe)
                target_w = padded_cell_w;
                target_h = padded_cell_h;
            }

            // 4. Calculate the top-left position of the cell's padded area.
            float cell_content_x = screen_margin + (current_window % cols) * cell_width + GRID_PADDING;
            float cell_content_y = screen_margin + (current_window / cols) * cell_height + GRID_PADDING;

            // 5. Center the aspect-ratio-correct window inside the padded area.
            win->target_x = cell_content_x + (padded_cell_w - target_w) / 2.0f;
            win->target_y = cell_content_y + (padded_cell_h - target_h) / 2.0f;
            win->target_width = target_w;
            win->target_height = target_h;
            
            // ======================== END OF FIX =========================

            // Initialize animation state
            win->grid_animation_active = true;
            win->grid_animation_progress = 0.0f;

            loggy_wobbly("[GRID] Window %d: (%.0fx%.0f) -> grid (%.0fx%.0f)\n", 
                        i, win->original_width, win->original_height, win->target_width, win->target_height);

            current_window++;
        }
    }
}

// ... (rest of your code) ...

// Exit grid - animate back to original positions
static void exit_grid_layout(struct plugin_state *state) {
    loggy_wobbly("[GRID] Exiting grid layout\n");
    
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid) {
            struct plugin_window *win = &state->windows[i];
            
            // Swap current and original positions for return animation
            float temp_x = win->original_x;
            float temp_y = win->original_y;
            float temp_w = win->original_width;
            float temp_h = win->original_height;
            
            // Current position becomes animation start
            win->original_x = win->x;
            win->original_y = win->y;
            win->original_width = win->width;
            win->original_height = win->height;
            
            // Saved position becomes animation target
            win->target_x = temp_x;
            win->target_y = temp_y;
            win->target_width = temp_w;
            win->target_height = temp_h;
            
            // Reset animation state
            win->grid_animation_active = true;
            win->grid_animation_progress = 0.0f;
        }
    }
}

// Smooth easing function
static float ease_out_cubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

// Single animation update function
// Single animation update function -- CORRECTED LOGIC
static void update_grid_animations(struct plugin_state *state) {
    static struct timeval last_update = {0};
    struct timeval now;
    gettimeofday(&now, NULL);

    // Calculate delta time
    float delta_time = 0.016f; // Default 60fps
    if (last_update.tv_sec > 0) {
        delta_time = (now.tv_sec - last_update.tv_sec) + 
                     (now.tv_usec - last_update.tv_usec) / 1000000.0f;
        delta_time = fminf(delta_time, 0.033f); // Cap at 30fps minimum
    }
    last_update = now;
    
    bool any_animation_running = false;
    
    for (int i = 0; i < state->window_count; i++) {
        struct plugin_window *win = &state->windows[i];
        
        // Only process windows that are actively animating.
        if (win->is_valid && win->grid_animation_active) {
            // Update animation progress
            float animation_speed = 3.0f; // Complete in ~0.33 seconds
            win->grid_animation_progress += animation_speed * delta_time;

            // ======================= THE FIX IS HERE =======================
            // This structure cleanly separates the final frame from intermediate frames.

            if (win->grid_animation_progress >= 1.0f) {
                // --- Animation is FINISHED ---
                // Stop the animation permanently for this window.
                win->grid_animation_active = false;
                
                // Snap the window precisely to its final target coordinates.
                // This is the most critical part. It makes the state definitive.
                win->x = win->target_x;
                win->y = win->target_y;
                win->width = win->target_width;
                win->height = win->target_height;
                
                loggy_wobbly("[GRID] Window %d animation FINISHED and SNAPPED to final position.\n", i);

            } else {
                // --- Animation is IN PROGRESS ---
                // Interpolate position using the smooth easing function.
                float t = ease_out_cubic(win->grid_animation_progress);
                
                win->x = win->original_x + (win->target_x - win->original_x) * t;
                win->y = win->original_y + (win->target_y - win->original_y) * t;
                win->width = win->original_width + (win->target_width - win->original_width) * t;
                win->height = win->original_height + (win->target_height - win->original_height) * t;
                
                // Since this window is still moving, signal a repaint is needed.
                any_animation_running = true;
            }
            // ===================== END OF FIX ======================

            // Update the wobbly surface's base position to match the animation
            if (win->wobbly_surface) {
                win->wobbly_surface->x = (int)win->x;
                win->wobbly_surface->y = (int)win->y;
                win->wobbly_surface->width = (int)win->width;
                win->wobbly_surface->height = (int)win->height;
            }
        }
    }
    
    // Mark content as changed only if any animation is still running.
    if (any_animation_running) {
        state->content_changed_this_frame = true;
    }
}

// Fixed grid toggle function
static void handle_grid_toggle(struct plugin_state *state) {
    loggy_wobbly("[GRID] Toggle requested - current state: %s\n", 
                state->grid_mode_active ? "ACTIVE" : "INACTIVE");
    
    // Check if any animations are currently running
    bool animations_running = false;
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid && state->windows[i].grid_animation_active) {
            animations_running = true;
            break;
        }
    }
    
    if (animations_running) {
        loggy_wobbly("[GRID] Animations still running, ignoring toggle\n");
        return;
    }
    
    // Toggle state
    state->grid_mode_active = !state->grid_mode_active;
    
    if (state->grid_mode_active) {
        loggy_wobbly("[GRID] Activating grid mode\n");
        calculate_grid_layout(state);
    } else {
        loggy_wobbly("[GRID] Deactivating grid mode\n");
        exit_grid_layout(state);
    }
    
    loggy_wobbly("[GRID] Grid mode now: %s\n", 
                state->grid_mode_active ? "ACTIVE" : "INACTIVE");
}

// Fixed window list handler that works with grid
static void handle_window_list_with_grid_support_fixed(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count) {
    // First, handle the window list update normally
    handle_window_list_hybrid(state, event, fds, fd_count);
    /*
    // Handle new windows in grid mode
    if (state->grid_mode_active) {
        // Check if we have new windows that need to be added to the grid
        bool has_new_windows = false;
        for (int i = 0; i < event->data.window_list.count && i < 10; i++) {
            struct plugin_window *win = &state->windows[i];
            if (win->is_valid && !win->grid_animation_active) {
                has_new_windows = true;
                break;
            }
        }
        
        // If there are new windows, recalculate the entire grid
      if (has_new_windows) {
    // Check if any animations are currently running
    bool animations_running = false;
    for (int j = 0; j < 10; j++) {
        if (state->windows[j].is_valid && state->windows[j].grid_animation_active) {
            animations_running = true;
            break;
        }
    }
    
    if (!animations_running) {
        loggy_wobbly("[GRID] New windows detected, recalculating grid\n");
        calculate_grid_layout(state);
    } else {
        loggy_wobbly("[GRID] Skipping grid recalc - animations running\n");
    }
}
    } else*/ {
        // In normal mode, ensure targets match actual positions
        for (int i = 0; i < event->data.window_list.count && i < 10; i++) {
            struct plugin_window *win = &state->windows[i];
            if (win->is_valid && !win->grid_animation_active) {
                ipc_window_info_t *info = &event->data.window_list.windows[i];
                win->target_x = info->x;
                win->target_y = info->y;
                win->target_width = info->width;
                win->target_height = info->height;
            }
        }
    }
}



static void update_wobbly_windows(struct plugin_state *state, long elapsed_ms) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    for (int i = 0; i < state->window_count; i++) {
        struct plugin_window *win = &state->windows[i];
        if (!win->is_valid || !win->wobbly_surface) continue;
        
        struct surface *s = win->wobbly_surface;
        
        // Handle grab timeout - ungrab if window hasn't moved for 1 second
        struct timeval diff;
        timersub(&now, &win->last_move_time, &diff);
        if (s->grabbed && diff.tv_sec >= 1) {
            loggy_wobbly( "[WOBBLY] Auto-ungrabbing window %d after timeout\n", i);
            wobbly_ungrab_notify(s);
        }
        
        // Update wobbly animation if active
        if (s->ww && s->ww->wobbly) {
            int old_wobbly_state = s->ww->wobbly;
            
            wobbly_prepare_paint(s, elapsed_ms);
            wobbly_add_geometry(s);
            wobbly_done_paint(s);
            
            // Check if wobbly animation finished
            if (old_wobbly_state && !s->ww->wobbly) {
                loggy_wobbly( "[WOBBLY] Window %d animation finished, marking as synced\n", i);
                s->synced = 1;  // Animation finished, go back to normal rendering
            }
            
            // CRITICAL: Don't update plugin window position from wobbly surface
            // The wobbly deformation is applied during rendering only
            // Keep the base position as-is from the compositor
        }
    }
}

// SIMPLE FIX: Create decoration quads using the deformed edge vertices from the wobbly mesh
// This makes decorations follow the exact deformation of the window edges
// Corrected function to render wobbly decorations with the colorful shader effect.
static void render_wobbly_decorations_using_edges(struct plugin_state *state, int index, GLfloat *corrected_vertices) {
    struct plugin_window *win = &state->windows[index];
    struct surface *s = win->wobbly_surface;

    if (!win->ssd.enabled || !corrected_vertices || s->vertex_count == 0) {
        return;
    }

    // Get grid dimensions
    int grid_w = s->x_cells + 1;

    // Use the wobbly shader program, which now supports procedural colors
    glUseProgram(state->wobbly_program);

    // Set the Model-View-Projection matrix exactly as the main wobbly window does
    GLfloat mvp[16] = {0};
    float left = 0, right = state->width, bottom = 0, top = state->height;

    mvp[0] = 2.0f / (right - left);
    mvp[5] = 2.0f / (top - bottom);
    mvp[10] = -1.0f;
    mvp[12] = -(right + left) / (right - left);
    mvp[13] = -(top + bottom) / (top - bottom);
    mvp[15] = 1.0f;

    glUniformMatrix4fv(state->wobbly_mvp_uniform, 1, GL_FALSE, mvp);

    // *** THE FIX: Tell the shader to use the procedural color logic ***
    GLint procedural_flag_loc = glGetUniformLocation(state->wobbly_program, "u_use_procedural_color");
    glUniform1i(procedural_flag_loc, 1); // Set to true

    // Extract corner vertices from the corrected wobbly mesh
    float top_left_x = corrected_vertices[0];
    float top_left_y = corrected_vertices[1];

    float top_right_x = corrected_vertices[s->x_cells * 2];
    float top_right_y = corrected_vertices[s->x_cells * 2 + 1];

    int bottom_left_idx = s->y_cells * grid_w;
    float bottom_left_x = corrected_vertices[bottom_left_idx * 2];
    float bottom_left_y = corrected_vertices[bottom_left_idx * 2 + 1];

    int bottom_right_idx = s->y_cells * grid_w + s->x_cells;
    float bottom_right_x = corrected_vertices[bottom_right_idx * 2];
    float bottom_right_y = corrected_vertices[bottom_right_idx * 2 + 1];

    // Decoration dimensions
    float title_height = SSD_TITLE_BAR_HEIGHT;
    float border_width = SSD_BORDER_WIDTH;

    // --- DRAW DECORATION ELEMENTS ---
    // Note: We don't need to set colors via uniforms anymore,
    // the fragment shader generates them procedurally.

    // A dummy UV buffer is still needed to satisfy the vertex attribute layout.
    GLfloat dummy_uvs[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };
    glBindBuffer(GL_ARRAY_BUFFER, state->wobbly_uv_bo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy_uvs), dummy_uvs, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

    GLushort quad_indices[] = {0, 1, 2, 1, 3, 2};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->wobbly_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_DYNAMIC_DRAW);

    // TITLE BAR (approximated as a quad)
    GLfloat title_vertices[] = {
        top_left_x - border_width,  top_left_y - title_height,  // Top-left
        top_right_x + border_width, top_right_y - title_height, // Top-right
        top_left_x - border_width,  top_left_y,                 // Bottom-left
        top_right_x + border_width, top_right_y                 // Bottom-right
    };
    glBindBuffer(GL_ARRAY_BUFFER, state->wobbly_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(title_vertices), title_vertices, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    // LEFT BORDER
    GLfloat left_vertices[] = {
        top_left_x - border_width, top_left_y,     // Top-left
        top_left_x,                top_left_y,     // Top-right
        bottom_left_x - border_width, bottom_left_y, // Bottom-left
        bottom_left_x,             bottom_left_y   // Bottom-right
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(left_vertices), left_vertices, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    // RIGHT BORDER
    GLfloat right_vertices[] = {
        top_right_x,                top_right_y,     // Top-left
        top_right_x + border_width, top_right_y,     // Top-right
        bottom_right_x,             bottom_right_y,  // Bottom-left
        bottom_right_x + border_width, bottom_right_y // Bottom-right
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(right_vertices), right_vertices, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    // BOTTOM BORDER
    GLfloat bottom_vertices[] = {
        bottom_left_x - border_width,  bottom_left_y,              // Top-left
        bottom_right_x + border_width, bottom_right_y,             // Top-right
        bottom_left_x - border_width,  bottom_left_y + border_width, // Bottom-left
        bottom_right_x + border_width, bottom_right_y + border_width // Bottom-right
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(bottom_vertices), bottom_vertices, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    // CLOSE BUTTON & 'X' (rendered as colored quads)
    float button_size = SSD_BUTTON_SIZE;
    float button_margin = SSD_CLOSE_BUTTON_MARGIN;
    float button_x = top_right_x + border_width - button_size - button_margin;
    float button_y = top_right_y - title_height + (title_height - button_size) / 2;

    GLfloat button_vertices[] = {
        button_x,               button_y,              // Top-left
        button_x + button_size, button_y,              // Top-right
        button_x,               button_y + button_size, // Bottom-left
        button_x + button_size, button_y + button_size  // Bottom-right
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(button_vertices), button_vertices, GL_DYNAMIC_DRAW);
//    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    // --- CLEANUP ---

    // *** CRITICAL: Reset the flag to false ***
    // This ensures that when the main window content is drawn next (using the same shader program),
    // it will correctly sample from its texture instead of using the procedural color.
    glUniform1i(procedural_flag_loc, 0); // Set to false

    // Unbind buffers and attributes
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// REPLACE your render_wobbly() function with this version that calls the edge-based decorations:
static void render_wobbly(struct plugin_state *state, int index) {
    struct plugin_window *win = &state->windows[index];
    struct surface *s = win->wobbly_surface;
    
    if (!win->is_valid || win->gl_texture == 0 || !s) {
        return;
    }
    
    // For non-wobbly windows, render normally
    if (s->synced || state->wobbly_program == 0 || !s->v || s->vertex_count == 0) {
        render_window(state, index);
        return;
    }
    
    // STEP 1: Render wobbly window content (existing logic)
    glUseProgram(state->wobbly_program);
    
    GLfloat mvp[16] = {0};
    float left = 0, right = state->width, bottom = 0, top = state->height;
    
    mvp[0] = 2.0f / (right - left);
    mvp[5] = 2.0f / (top - bottom);
    mvp[10] = -1.0f;
    mvp[12] = -(right + left) / (right - left);
    mvp[13] = -(top + bottom) / (top - bottom);
    mvp[15] = 1.0f;
    
    glUniformMatrix4fv(state->wobbly_mvp_uniform, 1, GL_FALSE, mvp);
    
    float base_x = win->x;
    float base_y = win->y;
    float offset_x = base_x - s->x;
    float offset_y = base_y - s->y;
    
    GLfloat *corrected_vertices = malloc(sizeof(GLfloat) * 2 * s->vertex_count);
    GLfloat *corrected_uvs = malloc(sizeof(GLfloat) * 2 * s->vertex_count);
    
    for (int i = 0; i < s->vertex_count; i++) {
        corrected_vertices[i * 2] = s->v[i * 2] + offset_x;
        corrected_vertices[i * 2 + 1] = s->v[i * 2 + 1] + offset_y;
        corrected_uvs[i * 2] = s->uv[i * 2];
        corrected_uvs[i * 2 + 1] = 1.0f - s->uv[i * 2 + 1];
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, state->wobbly_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * s->vertex_count, corrected_vertices, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    
    glBindBuffer(GL_ARRAY_BUFFER, state->wobbly_uv_bo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * s->vertex_count, corrected_uvs, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    
    int grid_w = s->x_cells + 1;
    int index_count = s->x_cells * s->y_cells * 6;
    GLushort *indices = malloc(sizeof(GLushort) * index_count);
    int idx = 0;
    
    for (int y = 0; y < s->y_cells; y++) {
        for (int x = 0; x < s->x_cells; x++) {
            int i = y * grid_w + x;
            indices[idx++] = i;
            indices[idx++] = i + 1;
            indices[idx++] = i + grid_w;
            indices[idx++] = i + 1;
            indices[idx++] = i + grid_w + 1;
            indices[idx++] = i + grid_w;
        }
    }
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, win->gl_texture);
    glUniform1i(glGetUniformLocation(state->wobbly_program, "window_texture"), 0);

     // --- FIX: Set the new uniforms for the bevel/rounded corner effect ---
    glUniform1f(glGetUniformLocation(state->wobbly_program, "alpha"), win->alpha);
    glUniform2f(state->wobbly_iresolution_uniform, win->width, win->height);
    glUniform1f(state->wobbly_corner_radius_uniform, 12.0f);
    const float bevel_color[] = {1.0f, 1.0f, 1.0f, 0.4f};
    glUniform4fv(state->wobbly_bevel_color_uniform, 1, bevel_color);
    glUniform1f(state->wobbly_time_uniform, get_current_time());
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->wobbly_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * index_count, indices, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, 0);
    
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    // STEP 2: Render decorations that follow the deformed edges
    render_wobbly_decorations_using_edges(state, index, corrected_vertices);
    
    // Clean up
    free(corrected_vertices);
    free(corrected_uvs);
    free(indices);
}

// Then add the main render function that uses it
static void render_with_wobbly_fixed(struct plugin_state *state, int frame_count) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    long elapsed_ms = (now.tv_sec - state->last_frame_time.tv_sec) * 1000 + 
                     (now.tv_usec - state->last_frame_time.tv_usec) / 1000;
    state->last_frame_time = now;
    
    if (elapsed_ms <= 0) elapsed_ms = 1;
    if (elapsed_ms > 100) elapsed_ms = 16;

   // update_grid_animations(state);
    
    // Update wobbly animations
    update_wobbly_windows(state, elapsed_ms);
    
    // Use wobbly rendering with proper DMA-BUF handling
    render_frame_with_wobbly(state, frame_count);
    
    // CRITICAL: Add the DMA-BUF sending logic from your working version
    if (state->dmabuf_fd >= 0) {
        // Zero-copy: render directly to DMA-BUF then send notification
        int dmabuf_fd_to_send = dup(state->dmabuf_fd);
        if (dmabuf_fd_to_send >= 0) {
            send_dmabuf_to_compositor(
                state->ipc_fd, 
                dmabuf_fd_to_send,
                state->width,
                state->height,
                DRM_FORMAT_ARGB8888,
                state->width * 4,
                DRM_FORMAT_MOD_LINEAR
            );
            close(dmabuf_fd_to_send);
            loggy_wobbly( "[WOBBLY] ✅ Sent wobbly frame through DMA-BUF\n");
        }
    } else {
        // SHM fallback
        if (state->shm_ptr) {
            glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
            glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, state->shm_ptr);
            glFlush();
            msync(state->shm_ptr, SHM_BUFFER_SIZE, MS_ASYNC);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        
        ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
        write(state->ipc_fd, &notification, sizeof(notification));
        loggy_wobbly( "[WOBBLY] ✅ Sent wobbly frame through SHM fallback\n");
    }
    
    state->content_changed_this_frame = false;
}

// Add this function after render_wobbly
static void render_frame_with_wobbly(struct plugin_state *state, int frame_count) {
    loggy_wobbly("[WOBBLY] render_frame_with_wobbly called with %d windows\n", state->window_count);
    
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glViewport(0, 0, state->width, state->height);
    
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Set up blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    
    // Render all windows using wobbly renderer WITH decorations
    int rendered_count = 0;
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid && state->windows[i].gl_texture != 0) {
            render_wobbly(state, i);  // This now handles both window + decorations
            rendered_count++;
        }
    }
    
    glDisable(GL_BLEND);
    glFlush();
    glFinish();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    loggy_wobbly("[WOBBLY] Rendered %d windows with decorations\n", rendered_count);
}

// Add this test function before main()
static void force_wobbly_mesh(struct plugin_state *state) {
    static bool forced = false;
    static int delay = 0;
    
    if (!forced && state->window_count > 0 && delay++ > 60) {
        for (int i = 0; i < state->window_count; i++) {
            struct plugin_window *win = &state->windows[i];
            if (win->wobbly_surface && win->wobbly_surface->ww) {
                loggy_wobbly( "[WOBBLY] Forcing window %d into wobbly mesh mode\n", i);
                
                // Force unsynced mode
                win->wobbly_surface->synced = 0;
                win->wobbly_surface->ww->wobbly = WobblyInitial | WobblyVelocity | WobblyForce;
                
                // Force grab and movement
                wobbly_grab_notify(win->wobbly_surface, win->x + win->width/2, win->y + win->height/2);
                wobbly_move_notify(win->wobbly_surface, 50, 30);
            }
        }
        forced = true;
    }
}
/*
// Add this function after render_with_pool_smart
static void render_with_wobbly_fixed(struct plugin_state *state, int frame_count) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Calculate elapsed time since last frame
    long elapsed_ms = (now.tv_sec - state->last_frame_time.tv_sec) * 1000 + 
                     (now.tv_usec - state->last_frame_time.tv_usec) / 1000;
    state->last_frame_time = now;
    
    // Clamp elapsed time to reasonable values
    if (elapsed_ms <= 0) elapsed_ms = 1;
    if (elapsed_ms > 100) elapsed_ms = 16;
    
    // Update wobbly animations FIRST
    for (int i = 0; i < state->window_count; i++) {
        struct plugin_window *win = &state->windows[i];
        if (!win->is_valid || !win->wobbly_surface) continue;
        
        struct surface *s = win->wobbly_surface;
        
        // Handle grab timeout - ungrab if window hasn't moved for 1 second
        struct timeval diff;
        timersub(&now, &win->last_move_time, &diff);
        if (s->grabbed && diff.tv_sec >= 1) {
            wobbly_ungrab_notify(s);
        }
        
        // Update wobbly animation
        wobbly_prepare_paint(s, elapsed_ms);
        wobbly_add_geometry(s);
        wobbly_done_paint(s);
        
        // Update window position from wobbly surface
        win->x = s->x;
        win->y = s->y;
    }
    
    // THEN render using your existing render logic
    render_with_pool_smart(state, frame_count);
    
    // Send notification 
    if (state->content_changed_this_frame || frame_count <= 10) {
        send_frame_ready(state);
        state->content_changed_this_frame = false;
    }
}*/

// 2. REPLACE your create_texture_with_stride_fix with this FAST version:
static GLuint create_texture_with_stride_fix(struct plugin_state *state, const ipc_window_info_t *info) {
    // NO DEBUG LOGGING - this function was doing tons of printf
    uint32_t expected_stride = info->width * 4;
    
    size_t map_size = info->stride * info->height;
    void *mapped_data = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, info->fd, 0);
    if (mapped_data == MAP_FAILED) {
        return 0;
    }
    
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

        // SET TEXTURE PARAMETERS ONCE - NEVER AGAIN
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Fast path: if stride matches, use direct upload
    if (info->stride == expected_stride) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, mapped_data);
    } else {
        // Slow path: stride correction (but no debug logging)
        uint32_t corrected_stride = info->width * 4;
        size_t corrected_size = corrected_stride * info->height;
        uint8_t *corrected_data = malloc(corrected_size);
        if (!corrected_data) {
            munmap(mapped_data, map_size);
            glDeleteTextures(1, &texture_id);
            return 0;
        }
        
        uint8_t *src = (uint8_t *)mapped_data;
        uint8_t *dst = corrected_data;
        
        // Fast copy without format conversion debugging
        for (uint32_t y = 0; y < info->height; y++) {
            memcpy(dst, src, corrected_stride);
            src += info->stride;
            dst += corrected_stride;
        }
        
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, corrected_data);
        
        free(corrected_data);
    }
    
    munmap(mapped_data, map_size);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_id;
}



// Helper to create a Framebuffer Object (FBO) for off-screen rendering
static bool init_fbo(struct plugin_state *state, int width, int height) {
    state->width = width;
    state->height = height;

    glGenFramebuffers(1, &state->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);

    glGenTextures(1, &state->fbo_texture);
    glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state->fbo_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        loggy_wobbly( "Framebuffer is not complete!\n");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}


static bool init_opengl_resources(struct plugin_state *state) {
    loggy_wobbly( "[WM] Initializing OpenGL resources\n");
    
    // Initialize regular window shaders
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, window_vertex_shader);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, window_fragment_shader);
    
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return false;
    }
    
    state->window_program = glCreateProgram();
    glAttachShader(state->window_program, vertex);
    glAttachShader(state->window_program, fragment);
    
    glBindAttribLocation(state->window_program, 0, "position");
    glBindAttribLocation(state->window_program, 1, "texcoord");
    
    glLinkProgram(state->window_program);
    
    GLint linked;
    glGetProgramiv(state->window_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLchar log[512];
        glGetProgramInfoLog(state->window_program, sizeof(log), NULL, log);
        loggy_wobbly( "Program link error: %s\n", log);
        glDeleteProgram(state->window_program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    // Get uniform locations for regular shader
    state->window_texture_uniform = glGetUniformLocation(state->window_program, "window_texture");
    state->window_transform_uniform = glGetUniformLocation(state->window_program, "transform");
    state->window_size_uniform = glGetUniformLocation(state->window_program, "window_size");
    state->alpha_uniform = glGetUniformLocation(state->window_program, "alpha");
    state->iresolution_uniform = glGetUniformLocation(state->window_program, "iResolution");
    state->corner_radius_uniform = glGetUniformLocation(state->window_program, "cornerRadius");
    state->bevel_color_uniform = glGetUniformLocation(state->window_program, "bevelColor");
    state->time_uniform = glGetUniformLocation(state->window_program, "time");    
    
    loggy_wobbly( "[WM] Regular shaders initialized successfully\n");
    
    // Initialize wobbly shaders
    GLuint wobbly_vertex = compile_shader(GL_VERTEX_SHADER, wobbly_vertex_shader);
    GLuint wobbly_fragment = compile_shader(GL_FRAGMENT_SHADER, wobbly_fragment_shader);
    
    if (wobbly_vertex && wobbly_fragment) {
        state->wobbly_program = glCreateProgram();
        glAttachShader(state->wobbly_program, wobbly_vertex);
        glAttachShader(state->wobbly_program, wobbly_fragment);
        
        glBindAttribLocation(state->wobbly_program, 0, "pos");
        glBindAttribLocation(state->wobbly_program, 1, "texcoord");
        
        glLinkProgram(state->wobbly_program);
        
        GLint wobbly_linked;
        glGetProgramiv(state->wobbly_program, GL_LINK_STATUS, &wobbly_linked);
         state->wobbly_program = create_program(wobbly_vertex_shader, wobbly_fragment_shader);
    if (state->wobbly_program) {
        // Get uniform locations for wobbly shader
       state->wobbly_mvp_uniform = glGetUniformLocation(state->wobbly_program, "modelviewProjection");
        state->wobbly_iresolution_uniform = glGetUniformLocation(state->wobbly_program, "iResolution");
        state->wobbly_corner_radius_uniform = glGetUniformLocation(state->wobbly_program, "cornerRadius");
        state->wobbly_bevel_color_uniform = glGetUniformLocation(state->wobbly_program, "bevelColor");
        state->wobbly_time_uniform = glGetUniformLocation(state->wobbly_program, "time");
        
        loggy_wobbly( "[WM] ✅ Wobbly shaders initialized successfully\n");
    } else {
        loggy_wobbly( "[WM] ⚠️  Failed to create wobbly shaders\n");
    }
        
        glDeleteShader(wobbly_vertex);
        glDeleteShader(wobbly_fragment);
    } else {
        loggy_wobbly( "[WM] ⚠️  Failed to compile wobbly shaders\n");
        if (wobbly_vertex) glDeleteShader(wobbly_vertex);
        if (wobbly_fragment) glDeleteShader(wobbly_fragment);
        state->wobbly_program = 0;
    }
    
    // Create VAO and VBO for regular rendering
    glGenVertexArrays(1, &state->quad_vao);
    glBindVertexArray(state->quad_vao);
    
    glGenBuffers(1, &state->quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, state->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    
    glGenBuffers(1, &state->quad_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->quad_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    
    glBindVertexArray(0);
    
    // Create buffers for wobbly rendering (dynamic geometry)
    if (state->wobbly_program != 0) {
        glGenBuffers(1, &state->wobbly_vbo);
        glGenBuffers(1, &state->wobbly_uv_bo);
        glGenBuffers(1, &state->wobbly_ibo);
        
        loggy_wobbly( "[WM] ✅ Wobbly geometry buffers created\n");
    }
    
    loggy_wobbly( "[WM] ✅ All OpenGL resources initialized successfully\n");
    loggy_wobbly( "[WM]   - Regular shader program: %u\n", state->window_program);
    loggy_wobbly( "[WM]   - Wobbly shader program: %u\n", state->wobbly_program);
    
    return true;
}
static bool validate_window_texture(struct plugin_state *state, int window_index) {
    if (window_index < 0 || window_index >= state->window_count) {
        return false;
    }
    
    if (!state->windows[window_index].is_valid) {
        return false;
    }
    
    GLuint texture = state->windows[window_index].gl_texture;
    if (texture == 0) {
        return false;
    }
    
    // Check if texture object is still valid
    if (!glIsTexture(texture)) {
        loggy_wobbly( "[PLUGIN] Window %d texture %u is no longer valid\n", window_index, texture);
        state->windows[window_index].gl_texture = 0;
        state->windows[window_index].is_valid = false;
        return false;
    }
    
    // Check texture parameters
    glBindTexture(GL_TEXTURE_2D, texture);
    GLint width, height;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        loggy_wobbly( "[PLUGIN] Error validating texture %u: 0x%x\n", texture, error);
        state->windows[window_index].gl_texture = 0;
        state->windows[window_index].is_valid = false;
        return false;
    }
    
    if (width <= 0 || height <= 0) {
        loggy_wobbly( "[PLUGIN] Window %d texture has invalid dimensions: %dx%d\n", 
                window_index, width, height);
        state->windows[window_index].gl_texture = 0;
        state->windows[window_index].is_valid = false;
        return false;
    }
    
    return true;
}

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
        loggy_wobbly( "[PLUGIN] Error binding texture %u: 0x%x\n", 
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



    // Set all uniforms before drawing
    glUniformMatrix3fv(state->window_transform_uniform, 1, GL_FALSE, transform);
    glUniform1f(state->alpha_uniform, state->windows[index].alpha);
    glUniform2f(state->iresolution_uniform, width, height);
    glUniform1f(state->corner_radius_uniform, 12.0f);
    const float bevel_color[] = {1.0f, 1.0f, 1.0f, 0.4f};
    glUniform4fv(state->bevel_color_uniform, 1, bevel_color);
    glUniform1f(state->time_uniform, get_current_time());



    // Render
    glBindVertexArray(state->quad_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore state
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(current_program);

    render_window_decorations(state, index);
}


// Function to continuously check if framebuffer is complete
static bool check_framebuffer_status(struct plugin_state *state) {
    if (!state->fbo) {
        loggy_wobbly( "[WM] Warning: No FBO to check\n");
        return false;
    }
    
    // Bind the framebuffer to check its status
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    
    // Restore the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE:
            // Everything is good
            return true;
            
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            loggy_wobbly( "[WM] Error: Framebuffer incomplete - attachment issue\n");
            break;
            
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            loggy_wobbly( "[WM] Error: Framebuffer incomplete - missing attachment\n");
            break;
            
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            loggy_wobbly( "[WM] Error: Framebuffer incomplete - dimension mismatch\n");
            break;
            
        case GL_FRAMEBUFFER_UNSUPPORTED:
            loggy_wobbly( "[WM] Error: Framebuffer unsupported format\n");
            break;
            
        default:
            loggy_wobbly( "[WM] Error: Unknown framebuffer status: 0x%x\n", status);
            break;
    }
    
    return false;
}

// Add this function - just creates the pool, doesn't send anything yet
static bool init_dmabuf_pool(struct plugin_state *state) {
    loggy_wobbly( "[PLUGIN] === STEP 1: Creating DMA-BUF pool ===\n");
    
    state->dmabuf_pool.pool_size = 4;  // Start small - just 4 buffers
    state->dmabuf_pool.initialized = false;
    
    // Create 4 DMA-BUFs
    for (int i = 0; i < 4; i++) {
        int fd = create_dmabuf(SHM_WIDTH * SHM_HEIGHT * 4, state->instance_id);
        if (fd < 0) {
            loggy_wobbly( "[PLUGIN] ❌ Failed to create pool DMA-BUF %d\n", i);
            return false;
        }
        
        state->dmabuf_pool.fds[i] = fd;
        state->dmabuf_pool.in_use[i] = false;
        state->dmabuf_pool.textures[i] = 0;
        
        loggy_wobbly( "[PLUGIN] ✅ Created pool buffer %d: fd=%d\n", i, fd);
    }
    
    state->dmabuf_pool.initialized = true;
    loggy_wobbly( "[PLUGIN] ✅ Pool ready with %d buffers\n", 4);
    return true;
}

// In your plugin - replace the regular FBO with DMA-BUF backed FBO
// Enhanced init_dmabuf_fbo with detailed debugging
static bool init_dmabuf_fbo(struct plugin_state *state, int width, int height) {
    loggy_wobbly( "[PLUGIN] Attempting to create DMA-BUF backed FBO (%dx%d)\n", width, height);
    
    state->width = width;
    state->height = height;
    
    // Step 1: Create DMA-BUF
    state->dmabuf_fd = create_dmabuf(width * height * 4, state->instance_id);
    if (state->dmabuf_fd < 0) {
        loggy_wobbly( "[PLUGIN] Failed to create DMA-BUF\n");
        return false;
    }
    loggy_wobbly( "[PLUGIN] Created DMA-BUF fd: %d\n", state->dmabuf_fd);
    
    // Step 2: Check for required EGL extensions
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    if (!eglCreateImageKHR) {
        loggy_wobbly( "[PLUGIN] Missing eglCreateImageKHR extension\n");
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    if (!glEGLImageTargetTexture2DOES) {
        loggy_wobbly( "[PLUGIN] Missing glEGLImageTargetTexture2DOES extension\n");
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    loggy_wobbly( "[PLUGIN] EGL extensions available\n");

    // Step 3: Create EGL image from DMA-BUF
    EGLint attribs[] = {
        EGL_WIDTH, width, 
        EGL_HEIGHT, height,
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
        EGL_DMA_BUF_PLANE0_FD_EXT, state->dmabuf_fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, width * 4,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE
    };
    
    loggy_wobbly( "[PLUGIN] Creating EGL image with format ABGR8888, pitch %d\n", width * 4);
    
    state->egl_image = eglCreateImageKHR(state->egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
    if (state->egl_image == EGL_NO_IMAGE_KHR) {
        EGLint error = eglGetError();
        loggy_wobbly( "[PLUGIN] Failed to create EGL image: 0x%x\n", error);
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    loggy_wobbly( "[PLUGIN] Created EGL image successfully\n");
    
    // Step 4: Create GL texture from EGL image
    glGenTextures(1, &state->fbo_texture);
    glBindTexture(GL_TEXTURE_2D, state->fbo_texture);
    
    // CRITICAL: Set texture parameters BEFORE calling glEGLImageTargetTexture2DOES
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Now call the EGL image target function
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, state->egl_image);
    
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_wobbly( "[PLUGIN] Failed to create texture from EGL image: 0x%x\n", gl_error);
        
        // More detailed error checking
        switch (gl_error) {
            case GL_INVALID_VALUE:
                loggy_wobbly( "[PLUGIN] GL_INVALID_VALUE - Invalid texture target or EGL image\n");
                break;
            case GL_INVALID_OPERATION:
                loggy_wobbly( "[PLUGIN] GL_INVALID_OPERATION - Context/texture state issue\n");
                break;
            default:
                loggy_wobbly( "[PLUGIN] Unknown OpenGL error\n");
                break;
        }
        
        glDeleteTextures(1, &state->fbo_texture);
        // Clean up EGL image
        PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    loggy_wobbly( "[PLUGIN] ✅ Created GL texture %u from EGL image\n", state->fbo_texture);

    
    // Step 5: Create framebuffer
    glGenFramebuffers(1, &state->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state->fbo_texture, 0);
    
    GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
        loggy_wobbly( "[PLUGIN] DMA-BUF framebuffer not complete: 0x%x\n", fb_status);
        glDeleteFramebuffers(1, &state->fbo);
        glDeleteTextures(1, &state->fbo_texture);
        // Clean up EGL image
        PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if (eglDestroyImageKHR) eglDestroyImageKHR(state->egl_display, state->egl_image);
        close(state->dmabuf_fd);
        state->dmabuf_fd = -1;
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    loggy_wobbly( "[PLUGIN] ✅ Successfully created DMA-BUF backed FBO (%dx%d)\n", width, height);
    return true;
}



// BETTER APPROACH: Send DMA-BUF once, then just send notifications

// Send DMA-BUF info only once at startup
static bool send_dmabuf_info_once(struct plugin_state *state) {
   // CRITICAL FIX: Only send the DMA-BUF info TRULY ONCE
    if (state->dmabuf_sent_to_compositor || state->dmabuf_fd < 0) {
        return state->dmabuf_sent_to_compositor;
    }
    
    loggy_wobbly( "[PLUGIN] Sending DMA-BUF info for the FIRST AND ONLY TIME (fd: %d)\n", state->dmabuf_fd);
    
    ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_DMABUF_FRAME_SUBMIT;
    ipc_dmabuf_frame_info_t frame_info = {
        .width = state->width,
        .height = state->height,
        .format = DRM_FORMAT_ABGR8888,
        .stride = state->width * 4,
        .modifier = DRM_FORMAT_MOD_LINEAR,
        .offset = 0,
    };
    
    int fd_to_send = dup(state->dmabuf_fd);
    if (fd_to_send < 0) {
        loggy_wobbly( "[PLUGIN] Failed to dup DMA-BUF fd: %s\n", strerror(errno));
        return false;
    }

    struct msghdr msg = {0};
    struct iovec iov[2];
    char cmsg_buf[CMSG_SPACE(sizeof(int))];

    iov[0].iov_base = &notification;
    iov[0].iov_len = sizeof(notification);
    iov[1].iov_base = &frame_info;
    iov[1].iov_len = sizeof(frame_info);

    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *((int*)CMSG_DATA(cmsg)) = fd_to_send;

    if (sendmsg(state->ipc_fd, &msg, 0) < 0) {
        loggy_wobbly( "[PLUGIN] Failed to send DMA-BUF info: %s\n", strerror(errno));
        close(fd_to_send); // Also close on failure
        return false;
    }
    
    // FIX: Close the duplicated FD after it has been sent.
    close(fd_to_send);

    // CRITICAL: Mark as sent and NEVER send again
    state->dmabuf_sent_to_compositor = true;
    
    
    loggy_wobbly( "[PLUGIN] ✅ DMA-BUF info sent ONCE - will never send again\n");
    return true;
}

// Simple frame ready notification (no FD)
static void send_frame_ready(struct plugin_state *state) {
    // Only send simple notification - NO FDs, NO buffer info
    ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
    
    if (write(state->ipc_fd, &notification, sizeof(notification)) < 0) {
        loggy_wobbly( "[PLUGIN] Failed to send frame ready: %s\n", strerror(errno));
        return;
    }
    
    state->last_frame_sequence++;
    
    // Only log the first few times
    static int log_count = 0;
    if (++log_count <= 5) {
        loggy_wobbly( "[PLUGIN] ✅ Frame ready notification sent (seq=%u, NO FDs)\n", 
                state->last_frame_sequence);
    }
}


// ============================================================================
// FIXED RENDER FUNCTION - Ensure clear color is applied
// ============================================================================

// Add this debug version of render_desktop_to_fbo to see what's happening
static void render_desktop_to_fbo(struct plugin_state *state) {
   loggy_wobbly("[PLUGIN] Starting render_desktop_to_fbo\n");  // Only prints first 5 times
    loggy_wobbly("[PLUGIN] Window count: %d\n", state->window_count);
    
    if (!check_framebuffer_status(state)) {
        loggy_wobbly("[PLUGIN] Framebuffer not complete, skipping render\n");
        return;
    }
    
    // 1. Bind our off-screen framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glViewport(0, 0, state->width, state->height);
    
    // 2. Clear - keep your green background for debugging
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Keep this
    glClear(GL_COLOR_BUFFER_BIT);
    
    // 3. Setup OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // 4. Render windows - keep your original logic
    int rendered_count = 0;
    for (int i = 0; i < state->window_count; i++) {
        if (i != state->focused_window_index && state->windows[i].is_valid) {
            render_window(state, i);  // Use your ORIGINAL function
            rendered_count++;
        }
    }
    
    // 5. Render focused window on top
    if (state->focused_window_index >= 0 && 
        state->focused_window_index < state->window_count &&
        state->windows[state->focused_window_index].is_valid) {
        render_window(state, state->focused_window_index);  // Use your ORIGINAL function
        rendered_count++;
    }

    // 6. PERFORMANCE FIX: Use glFlush() instead of glFinish()
    glFlush();  // Non-blocking instead of glFinish()

    // 7. Unbind and restore
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindVertexArray(0);
    
    loggy_wobbly("[PLUGIN] render_desktop_to_fbo complete\n");
}



// 1. Optimized render function - no debug logging, minimal state changes
static void render_frame_optimized(struct plugin_state *state, int frame_count) {
   // Skip render if nothing changed and we're at 60fps
    static struct timeval last_render = {0};
    struct timeval now;
    gettimeofday(&now, NULL);
    
    struct timeval diff;
    timersub(&now, &last_render, &diff);
    
    // Only render if content changed OR 16ms passed (60fps)
    bool should_render = state->content_changed_this_frame || 
                        (diff.tv_sec > 0 || diff.tv_usec >= 8000);
    
    if (!should_render) {
        return;
    }
    
    last_render = now;
    
    // Bind FBO and set viewport once
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glViewport(0, 0, state->width, state->height);
    
    // Clear only if content changed
    if (state->content_changed_this_frame) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
    // Set OpenGL state once for all windows
    glUseProgram(state->window_program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(state->quad_vao);
    
    // Render all windows without state changes
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid && state->windows[i].gl_texture != 0) {
            render_window(state, i);
        }
    }
    
    // Restore state once
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_BLEND);
    
    // Only copy to SHM if content changed
    if (state->content_changed_this_frame) {
    //  glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, state->shm_ptr);
        glFlush(); // Use glFlush instead of glFinish
        msync(state->shm_ptr, SHM_BUFFER_SIZE, MS_ASYNC); // Async sync
        state->content_changed_this_frame = false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
            loggy_wobbly( "[PLUGIN] ❌ recvmsg FAILED: %s\n", strerror(errno));
        }
        return n;
    }

    *fd_count = 0;
    
    loggy_wobbly( "[PLUGIN] recvmsg received %zd bytes\n", n);
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg) {
        loggy_wobbly( "[PLUGIN] No control message (no FDs sent)\n");
    }
    
    while (cmsg != NULL) {
        loggy_wobbly( "[PLUGIN] Control message: level=%d, type=%d, len=%zu\n", 
                cmsg->cmsg_level, cmsg->cmsg_type, cmsg->cmsg_len);
                
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int num_fds_in_msg = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int *received_fds = (int *)CMSG_DATA(cmsg);
            
            loggy_wobbly( "[PLUGIN] ✅ Received %d FDs: ", num_fds_in_msg);
            for (int i = 0; i < num_fds_in_msg; i++) {
                if (*fd_count < 32) {
                    fds[*fd_count] = received_fds[i];
                    loggy_wobbly( "%d ", received_fds[i]);
                    (*fd_count)++;
                } else {
                    loggy_wobbly( "(overflow, closing %d) ", received_fds[i]);
                    close(received_fds[i]);
                }
            }
            loggy_wobbly( "\n");
        }
        cmsg = CMSG_NXTHDR(&msg, cmsg);
    }

    return n;
}

// ============================================================================
// NEW HELPER FUNCTION FOR SHM TEXTURES
// ============================================================================

/**
 * Creates a GL texture by mapping a shared memory (SHM) file descriptor.
 */
static GLuint texture_from_shm(struct plugin_state *state, const ipc_window_info_t *info) {
    if (info->fd < 0) {
        loggy_wobbly( "[WM] Invalid SHM file descriptor.\n");
        return 0;
    }

    // 1. Map the shared memory segment from the file descriptor
    void *shm_data = mmap(NULL, info->stride * info->height, PROT_READ, MAP_PRIVATE, info->fd, 0);
    if (shm_data == MAP_FAILED) {
        perror("[WM] Failed to mmap SHM buffer");
        return 0;
    }

    // 2. Create an OpenGL texture and upload the pixel data
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload the pixels from the mapped memory
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, shm_data);

    // 3. Unmap the memory as we are done with it
    munmap(shm_data, info->stride * info->height);

    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_wobbly( "[WM] OpenGL error creating texture from SHM: 0x%x\n", gl_error);
        glDeleteTextures(1, &texture_id);
        return 0;
    }

    loggy_wobbly( "[WM] Successfully created texture %u from SHM (fd:%d, %dx%d)\n", 
            texture_id, info->fd, (int)info->width, (int)info->height);

    return texture_id;
}




// Hybrid texture import: Try fast methods first, fallback to reliable methods
// REPLACE your create_texture_hybrid function with this fixed version:

static GLuint create_texture_hybrid(struct plugin_state *state, const ipc_window_info_t *info, int window_index) {
    if (info->fd < 0) {
        loggy_wobbly( "[PLUGIN] Invalid file descriptor for window %d\n", window_index);
        return 0;
    }
    
    // Log format for debugging weston apps
    static int format_log_count = 0;
    if (format_log_count < 10) {
        loggy_wobbly( "[PLUGIN] Window %d format: 0x%x (%dx%d, stride=%u, modifier=0x%lx)\n", 
                window_index, info->format, info->width, info->height, info->stride, info->modifier);
        format_log_count++;
    }
    
   GLuint texture = 0;
    
    if (info->buffer_type == IPC_BUFFER_TYPE_DMABUF && state->dmabuf_ctx.initialized) {
        texture = texture_from_dmabuf(state, info);
        // DO NOT close the FD here. Just return.
        if (texture > 0) return texture;
    }
    
    if (info->buffer_type == IPC_BUFFER_TYPE_DMABUF) {
        texture = create_texture_from_dmabuf_manual(state, info);
        // DO NOT close the FD here.
        if (texture > 0) return texture;
    }
    
    texture = create_texture_with_stride_fix_optimized(state, info);
    // DO NOT close the FD here.
    if (texture > 0) return texture;
    
    if (info->buffer_type == IPC_BUFFER_TYPE_SHM) {
        texture = texture_from_shm(state, info);
        // DO NOT close the FD here.
        if (texture > 0) return texture;
    }
    
    loggy_wobbly( "[PLUGIN] ❌ All texture creation methods failed for window %d (fd %d)\n", window_index, info->fd);
    
    // Even on failure, DO NOT close the FD. The caller must do it.
    return 0; 
}
// Manual EGL DMA-BUF import (bypass wlroots complexity)

// Enhanced manual DMA-BUF import with better format support
static GLuint create_texture_from_dmabuf_manual(struct plugin_state *state, const ipc_window_info_t *info) {
    // Validate FD first
    if (info->fd < 0) {
        return 0;
    }
    
    // Test FD validity
    struct stat fd_stat;
    if (fstat(info->fd, &fd_stat) != 0) {
        loggy_wobbly( "[PLUGIN] Invalid FD %d: %s\n", info->fd, strerror(errno));
        return 0;
    }
    
    // Get EGL extension functions
    static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = NULL;
    static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = NULL;
    static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = NULL;
    
    if (!eglCreateImageKHR) {
        eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    }
    
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES || !eglDestroyImageKHR) {
        return 0;
    }
    
    // FIXED: More robust format handling
    EGLint drm_format;
    switch (info->format) {
        case 0x34325241: // DRM_FORMAT_ARGB8888
            drm_format = info->format;
            break;
        case 0x34325258: // DRM_FORMAT_XRGB8888
            drm_format = info->format;
            break;
        case 0x34324142: // DRM_FORMAT_ABGR8888
            drm_format = info->format;
            break;
        case 0x34324258: // DRM_FORMAT_XBGR8888
            drm_format = info->format;
            break;
        default:
            loggy_wobbly( "[PLUGIN] Unsupported DMA-BUF format: 0x%x\n", info->format);
            return 0;
    }
    
    // FIXED: Ensure context is current before EGL operations
    if (!eglMakeCurrent(state->egl_display, state->egl_surface, state->egl_surface, state->egl_context)) {
        loggy_wobbly( "[PLUGIN] Failed to make EGL context current\n");
        return 0;
    }
    
    // Create EGL image with proper attributes
    EGLint attribs[] = {
        EGL_WIDTH, (EGLint)info->width,
        EGL_HEIGHT, (EGLint)info->height,
        EGL_LINUX_DRM_FOURCC_EXT, drm_format,
        EGL_DMA_BUF_PLANE0_FD_EXT, info->fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)info->stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE
    };
    
    EGLImageKHR egl_image = eglCreateImageKHR(
        state->egl_display, 
        EGL_NO_CONTEXT,  // FIXED: Use EGL_NO_CONTEXT for DMA-BUF
        EGL_LINUX_DMA_BUF_EXT, 
        NULL, 
        attribs
    );
    
    if (egl_image == EGL_NO_IMAGE_KHR) {
        EGLint egl_error = eglGetError();
        loggy_wobbly( "[PLUGIN] Failed to create EGL image: 0x%x\n", egl_error);
        return 0;
    }
    
    // Create GL texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // CRITICAL: Set texture parameters BEFORE EGL target call
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Bind EGL image to texture
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image);
    
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        loggy_wobbly( "[PLUGIN] OpenGL error creating texture from EGL image: 0x%x\n", gl_error);
        glDeleteTextures(1, &texture);
        eglDestroyImageKHR(state->egl_display, egl_image);
        return 0;
    }
    
    // CRITICAL: Clean up EGL image immediately - texture holds the reference
    eglDestroyImageKHR(state->egl_display, egl_image);
    
    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texture;
}
// Optimized stride correction (less debugging, faster processing)
static GLuint create_texture_with_stride_fix_optimized(struct plugin_state *state, const ipc_window_info_t *info) {
    uint32_t expected_stride = info->width * 4;
    
    // Enhanced format detection and handling
    uint32_t bytes_per_pixel = 4;
    bool needs_format_conversion = false;
    
    // Detect format and adjust accordingly
    switch (info->format) {
        case 0x34325241: // DRM_FORMAT_ARGB8888
        case 0x34325258: // DRM_FORMAT_XRGB8888  
        case 0x34324142: // DRM_FORMAT_ABGR8888
        case 0x34324258: // DRM_FORMAT_XBGR8888
            bytes_per_pixel = 4;
            needs_format_conversion = true;
            break;
        case 0x36314752: // DRM_FORMAT_RGB565
            bytes_per_pixel = 2;
            expected_stride = info->width * 2;
            break;
        default:
            // Assume RGBA8888 for unknown formats
            bytes_per_pixel = 4;
            break;
    }
    
    // Log format information for debugging
    if (info->stride != expected_stride) {
        loggy_wobbly( "[PLUGIN] Format 0x%x: stride %u vs expected %u (bpp=%u)\n", 
                info->format, info->stride, expected_stride, bytes_per_pixel);
    }
    
    size_t map_size = info->stride * info->height;
    void *mapped_data = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, info->fd, 0);
    if (mapped_data == MAP_FAILED) {
        loggy_wobbly( "[PLUGIN] mmap failed: %s\n", strerror(errno));
        return 0;
    }
    
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set proper pixel store parameters
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // Changed from 4 to 1 for better compatibility
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    
    // Fast path: if stride matches and no format conversion needed
    if (info->stride == expected_stride && !needs_format_conversion) {
        GLenum gl_format = GL_RGBA;
        GLenum gl_type = GL_UNSIGNED_BYTE;
        
        // Handle RGB565 format
        if (info->format == 0x36314752) {
            gl_format = GL_RGB;
            gl_type = GL_UNSIGNED_SHORT_5_6_5;
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 
                     0, gl_format, gl_type, mapped_data);
    } else {
        // Slow path: stride correction and/or format conversion needed
        size_t corrected_size = info->width * info->height * 4;
        uint8_t *corrected_data = malloc(corrected_size);
        if (!corrected_data) {
            munmap(mapped_data, map_size);
            glDeleteTextures(1, &texture);
            return 0;
        }
        
        uint8_t *src = (uint8_t *)mapped_data;
        uint8_t *dst = corrected_data;
        
        for (uint32_t y = 0; y < info->height; y++) {
            if (needs_format_conversion) {
                // Convert pixel formats
                for (uint32_t x = 0; x < info->width; x++) {
                    uint32_t *src_pixel = (uint32_t *)(src + x * 4);
                    uint32_t *dst_pixel = (uint32_t *)(dst + x * 4);
                    uint32_t pixel = *src_pixel;
                    
                    switch (info->format) {
                        case 0x34325241: // ARGB8888 -> RGBA8888
                        case 0x34325258: // XRGB8888 -> RGBA8888
                        {
                            uint32_t a = (info->format == 0x34325241) ? (pixel >> 24) & 0xFF : 0xFF;
                            uint32_t r = (pixel >> 16) & 0xFF;
                            uint32_t g = (pixel >> 8) & 0xFF;
                            uint32_t b = pixel & 0xFF;
                            *dst_pixel = (r << 0) | (g << 8) | (b << 16) | (a << 24);
                            break;
                        }
                        case 0x34324142: // ABGR8888 -> RGBA8888
                        case 0x34324258: // XBGR8888 -> RGBA8888
                        {
                            uint32_t a = (info->format == 0x34324142) ? (pixel >> 24) & 0xFF : 0xFF;
                            uint32_t b = (pixel >> 16) & 0xFF;
                            uint32_t g = (pixel >> 8) & 0xFF;
                            uint32_t r = pixel & 0xFF;
                            *dst_pixel = (r << 0) | (g << 8) | (b << 16) | (a << 24);
                            break;
                        }
                        default:
                            *dst_pixel = pixel; // No conversion
                            break;
                    }
                }
            } else {
                // Simple stride correction without format conversion
                memcpy(dst, src, info->width * bytes_per_pixel);
            }
            
            src += info->stride;
            dst += info->width * 4;
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info->width, info->height, 
                     0, GL_RGBA, GL_UNSIGNED_BYTE, corrected_data);
        
        free(corrected_data);
    }
    
    munmap(mapped_data, map_size);
 /*   
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        loggy_wobbly( "[PLUGIN] OpenGL error creating texture: 0x%x\n", error);
        glDeleteTextures(1, &texture);
        return 0;
    }
   */ 
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return texture;
}

// Add this field to your window struct (if not already there):
// uint32_t texture_creation_frame;

// In opengl_desktop_plugin.c
// In opengl_desktop_plugin.c

// opengl_desktop_plugin.c

// ... (previous code) ...

static void handle_window_list_hybrid(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count) {
    int new_window_count = event->data.window_list.count;
    bool existing_windows[10] = {false};

    for (int i = 0; i < new_window_count && i < 10; i++) {
        ipc_window_info_t *info = &event->data.window_list.windows[i];
        struct plugin_window *win = &state->windows[i];
        
        existing_windows[i] = true;

        // Initialize wobbly surface if it's the first time we see this window. This is always safe to do.
        if (!win->is_valid && !win->wobbly_surface) {
            win->wobbly_surface = malloc(sizeof(struct surface));
            if(win->wobbly_surface) {
                memset(win->wobbly_surface, 0, sizeof(struct surface));
                win->wobbly_surface->x = info->x;
                win->wobbly_surface->y = info->y;
                win->wobbly_surface->width = info->width;
                win->wobbly_surface->height = info->height;
                win->wobbly_surface->x_cells = 8;
                win->wobbly_surface->y_cells = 8;
                win->wobbly_surface->synced = 1;  // Start synced
                wobbly_init(win->wobbly_surface);
                gettimeofday(&win->last_move_time, NULL);
            }
            win->last_x = info->x;
            win->last_y = info->y;
        }

        // ======================= THE CORRECTED LOGIC =======================
        // This block decides whether to accept position/size updates from the compositor
        // or let the animation system control the window's geometry.
        if (!state->grid_mode_active) {
            // --- GRID MODE IS OFF ---
            // In normal mode, we sync our window state with the compositor.
            win->x = info->x;
            win->y = info->y;
            win->width = info->width;
            win->height = info->height;

            // Handle wobbly surface resizing based on compositor data
            if (win->wobbly_surface && (win->wobbly_surface->width != info->width || 
                                       win->wobbly_surface->height != info->height)) {
                win->wobbly_surface->width = info->width;
                win->wobbly_surface->height = info->height;
                wobbly_resize_notify(win->wobbly_surface);
            }

            // Handle wobbly movement detection based on compositor data
            if (win->wobbly_surface && win->wobbly_surface->ww) {
                int dx = (int)info->x - win->last_x;
                int dy = (int)info->y - win->last_y;
                
                if (abs(dx) > 2 || abs(dy) > 2) {
                    if (!win->wobbly_surface->ww->grabbed) {
                        wobbly_grab_notify(win->wobbly_surface, win->wobbly_surface->width / 2.0f, win->wobbly_surface->height / 2.0f);
                    }
                    wobbly_move_notify(win->wobbly_surface, dx, dy);
                    gettimeofday(&win->last_move_time, NULL);
                    win->wobbly_surface->synced = 0;
                }
                win->last_x = (int)info->x;
                win->last_y = (int)info->y;
            }
        }
        // --- If grid mode IS active, we simply do nothing here, preserving the animated positions. ---
        // ===================== END OF CORRECTED LOGIC ======================

        // These properties are always updated regardless of grid mode.
        win->is_valid = true;
        win->alpha = 1.0f;

        // Initialize or update Server-Side Decoration (SSD) info.
        if (!win->ssd.enabled) {
            win->ssd.enabled = true;
            win->ssd.needs_close_button = true;
            win->ssd.needs_title_bar = true;
            strncpy(win->ssd.title, info->title[0] ? info->title : "Window", sizeof(win->ssd.title) - 1);
            win->ssd.title[sizeof(win->ssd.title) - 1] = '\0';
        } else if (info->title[0] && strcmp(win->ssd.title, info->title) != 0) {
            strncpy(win->ssd.title, info->title, sizeof(win->ssd.title) - 1);
        }
        
        // Always update the texture, as the window content can change in any mode.
        if (i < fd_count && fds[i] >= 0) {
            if (win->gl_texture) glDeleteTextures(1, &win->gl_texture);
            
            info->fd = fds[i];
            win->gl_texture = create_texture_hybrid(state, info, i);
            close(fds[i]);
            state->content_changed_this_frame = true;
        }
    }
    
    // Clean up any windows that were removed from the list.
    for (int i = 0; i < 10; i++) {
        if (state->windows[i].is_valid && !existing_windows[i]) {
            if (state->windows[i].gl_texture) glDeleteTextures(1, &state->windows[i].gl_texture);
            if (state->windows[i].wobbly_surface) {
                wobbly_fini(state->windows[i].wobbly_surface);
                free(state->windows[i].wobbly_surface);
                state->windows[i].wobbly_surface = NULL;
            }
            memset(&state->windows[i].ssd, 0, sizeof(state->windows[i].ssd));
            state->windows[i].is_valid = false;
        }
    }

    state->window_count = new_window_count;
}

// ... (rest of the code) ...

/*
static void handle_window_list_hybrid(struct plugin_state *state, ipc_event_t *event, int *fds, int fd_count) {
     int new_window_count = event->data.window_list.count;
    bool existing_windows[10] = {false};

    for (int i = 0; i < new_window_count && i < 10; i++) {
        ipc_window_info_t *info = &event->data.window_list.windows[i];
        struct plugin_window *win = &state->windows[i];
        
        existing_windows[i] = true;

        // Initialize wobbly surface if first time
        if (!win->is_valid && !win->wobbly_surface) {
            win->wobbly_surface = malloc(sizeof(struct surface));
            if(win->wobbly_surface) {
                memset(win->wobbly_surface, 0, sizeof(struct surface));
                win->wobbly_surface->x = info->x;
                win->wobbly_surface->y = info->y;
                win->wobbly_surface->width = info->width;
                win->wobbly_surface->height = info->height;
                win->wobbly_surface->x_cells = 8;
                win->wobbly_surface->y_cells = 8;
                win->wobbly_surface->synced = 1;
                wobbly_init(win->wobbly_surface);
                gettimeofday(&win->last_move_time, NULL);
            }
            win->last_x = info->x;
            win->last_y = info->y;
        }

        // Update window properties (EXACT same as working version)
        win->x = info->x;
        win->y = info->y;
        win->is_valid = true;
        win->alpha = 1.0f;
        win->width = info->width;
        win->height = info->height;

        // Handle wobbly surface resizing
        if (win->wobbly_surface && (win->wobbly_surface->width != info->width || 
                                   win->wobbly_surface->height != info->height)) {
            win->wobbly_surface->width = info->width;
            win->wobbly_surface->height = info->height;
            wobbly_resize_notify(win->wobbly_surface);
        }
        
        // CRITICAL: Use EXACT texture creation logic from working version
        if (i < fd_count && fds[i] >= 0) {
            if (win->gl_texture) glDeleteTextures(1, &win->gl_texture);
            
            // Set FD in info (like working version)
            info->fd = fds[i];
            
            win->gl_texture = create_texture_hybrid(state, info, i);
            close(fds[i]); // Close FD like working version
            state->content_changed_this_frame = true;
        }

        // Handle wobbly movement
        if (win->wobbly_surface) {
            int dx = win->x - win->last_x;
            int dy = win->y - win->last_y;
            if (abs(dx) > 1 || abs(dy) > 1) {
                if (!win->wobbly_surface->grabbed) {
                    wobbly_grab_notify(win->wobbly_surface, win->x + win->width / 2, win->y + win->height / 2);
                }
                wobbly_move_notify(win->wobbly_surface, dx, dy);
                gettimeofday(&win->last_move_time, NULL);
            }
            win->last_x = win->x;
            win->last_y = win->y;
        }
    }
    
    // Clean up removed windows (same as working version)
    for (int i = 0; i < 10; i++) {
        if (state->windows[i].is_valid && !existing_windows[i]) {
            if (state->windows[i].gl_texture) glDeleteTextures(1, &state->windows[i].gl_texture);
            if (state->windows[i].wobbly_surface) {
                wobbly_fini(state->windows[i].wobbly_surface);
                free(state->windows[i].wobbly_surface);
                state->windows[i].wobbly_surface = NULL;
            }
            state->windows[i].is_valid = false;
        }
    }

    state->window_count = new_window_count;
}*/

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



// 5. REPLACE your render_frame_hybrid with NO-DEBUG version:
static void render_frame_hybrid(struct plugin_state *state, int frame_count) {
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glViewport(0, 0, state->width, state->height);
    
    // ONLY CHANGE: Only clear when content actually changed
    if (state->content_changed_this_frame) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
    // Rest is exactly the same as your original
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0); 
    
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid && state->windows[i].gl_texture != 0) {
            render_window(state, i);  // Keep your original function name
        }
    }
    
    glDisable(GL_BLEND);
    glFlush();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// Add this function to your plugin
static int export_dmabuf_fallback(struct plugin_state *state) {
    if (state->dmabuf_fd < 0) {
        loggy_wobbly( "[PLUGIN] No DMA-BUF FD available for fallback export\n");
        return -1;
    }
    
    // Make sure rendering is complete
    glFlush();
    glFinish();
    
    // Since the FBO is already DMA-BUF backed, just return a dup of the existing FD
    int export_fd = dup(state->dmabuf_fd);
    if (export_fd < 0) {
        loggy_wobbly( "[PLUGIN] Failed to dup DMA-BUF FD: %s\n", strerror(errno));
        return -1;
    }
    
    loggy_wobbly( "[PLUGIN] ✅ Exported DMA-BUF via fallback: fd=%d\n", export_fd);
    return export_fd;
}



// 2. Add this function before main():
static void drain_ipc_queue(struct plugin_state *state) {
    int drained = 0;
    while (drained < 10) { // Max 10 messages per frame
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(state->ipc_fd, &read_fds);
        
        struct timeval immediate = {0, 0}; // Non-blocking
        int ready = select(state->ipc_fd + 1, &read_fds, NULL, NULL, &immediate);
        
        if (ready <= 0) break; // No more messages
        
        ipc_event_t event;
        int fds[32];
        int fd_count = 0;
        
        ssize_t n = read_ipc_with_fds(state->ipc_fd, &event, fds, &fd_count);
        if (n <= 0) break;
        
        if (event.type == IPC_EVENT_TYPE_WINDOW_LIST_UPDATE) {
            handle_window_list_hybrid(state, &event, fds, fd_count);
        }
        
        // Close unclaimed FDs
        for (int i = state->window_count; i < fd_count; i++) {
            if (fds[i] >= 0) close(fds[i]);
        }
        
        drained++;
    }
    
    if (drained >= 10) {
        loggy_wobbly("[PLUGIN] Drained max messages, more may be pending\n");
    }
}

// 5. FIXED render function with proper sync
static void render_with_proper_sync_FIXED(struct plugin_state *state, int frame_count) {
    static struct timeval last_render = {0};
    struct timeval now;
    gettimeofday(&now, NULL);
    
    struct timeval diff;
    timersub(&now, &last_render, &diff);
    
    // Render at 60fps or when content changes
    bool should_render = state->content_changed_this_frame || 
                        (diff.tv_sec > 0 || diff.tv_usec >= 16667);
    
    if (!should_render) {
        return;
    }
    
    last_render = now;
    
    // Render to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, state->fbo);
    glViewport(0, 0, state->width, state->height);
    
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Set up blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    
    // Render all valid windows
    int rendered_count = 0;
    for (int i = 0; i < state->window_count; i++) {
        if (state->windows[i].is_valid && state->windows[i].gl_texture != 0) {
            render_window(state, i);
            rendered_count++;
        }
    }
    
    glDisable(GL_BLEND);
    
    // CRITICAL: Wait for rendering to complete before compositor accesses DMA-BUF
    glFlush();   // Submit commands
    glFinish();  // Wait for completion (required for DMA-BUF)
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Send notification (no FDs)
    if (state->content_changed_this_frame || rendered_count > 0) {
        send_frame_ready(state);
        state->content_changed_this_frame = false;
    }
    
    // Debug logging
    if (frame_count <= 10 || frame_count % 300 == 0) {
        loggy_wobbly( "[PLUGIN] Frame %d: rendered %d windows\n", frame_count, rendered_count);
    }
}


// Initialize SSD shaders (call this in your init function)
static bool init_ssd_shaders(struct plugin_state *state) {
    loggy_wobbly("[PLUGIN] Initializing SSD shaders...\n");
    
    state->rect_program = create_program(rect_vertex_shader, rect_fragment_shader);
    if (!state->rect_program) {
        loggy_wobbly("[PLUGIN] Failed to create rect shader program\n");
        return false;
    }
    
    // Get uniform locations
    state->rect_transform_uniform = glGetUniformLocation(state->rect_program, "transform");
    state->rect_color_uniform = glGetUniformLocation(state->rect_program, "color");

    state->rect_size_uniform = glGetUniformLocation(state->rect_program, "u_rect_size");
    state->rect_origin_uniform = glGetUniformLocation(state->rect_program, "u_rect_origin");

    state->rect_resolution_uniform = glGetUniformLocation(state->rect_program, "u_resolution");
    float current_time = get_current_time(); // You'll need to implement this
    state->rect_time_uniform = glGetUniformLocation(state->rect_program, "time");

// Set iResolution uniform (width and height of the rectangle being drawn)
glUniform2f(state->rect_iresolution_uniform, 100, 90);
    
    // Create VAO and buffers for rectangle rendering
    glGenVertexArrays(1, &state->rect_vao);
    glBindVertexArray(state->rect_vao);
    
    glGenBuffers(1, &state->rect_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, state->rect_vbo);
    
    // Simple quad vertices (position only)
    static const GLfloat rect_vertices[] = {
        0.0f, 0.0f,  // Bottom-left
        1.0f, 0.0f,  // Bottom-right  
        0.0f, 1.0f,  // Top-left
        1.0f, 1.0f,  // Top-right
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(rect_vertices), rect_vertices, GL_STATIC_DRAW);
    
    glGenBuffers(1, &state->rect_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->rect_ibo);
    
    static const GLuint rect_indices[] = {0, 1, 2, 1, 3, 2};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rect_indices), rect_indices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);
    
    glBindVertexArray(0);
    
    loggy_wobbly("[PLUGIN] SSD shaders initialized successfully\n");
    return true;
}

// OpenGL ES compatible rect rendering
// Fixed render_ssd_rect function
static void render_ssd_rect(struct plugin_state *state, float x, float y, float width, float height, const float color[4]) {
    glUseProgram(state->rect_program);

    // Calculate the transformation matrix
    float left = x;
    float top = y;
    float rect_width = width;
    float rect_height = height;
    float ndc_left = (left / state->width) * 2.0f - 1.0f;
    float ndc_right = ((left + rect_width) / state->width) * 2.0f - 1.0f;
    float ndc_top = ((top) / state->height) * 2.0f - 1.0f;
    float ndc_bottom = ((top + rect_height) / state->height) * 2.0f - 1.0f;
    float ndc_width = ndc_right - ndc_left;
    float ndc_height = ndc_top - ndc_bottom;
    float transform[9];
    transform[0] = ndc_width;  transform[3] = 0.0f;        transform[6] = ndc_left;
    transform[1] = 0.0f;       transform[4] = ndc_height;  transform[7] = ndc_bottom;
    transform[2] = 0.0f;       transform[5] = 0.0f;        transform[8] = 1.0f;

    // Pass the uniforms to the shader
    glUniformMatrix3fv(state->rect_transform_uniform, 1, GL_FALSE, transform);
    glUniform4f(state->rect_color_uniform, color[0], color[1], color[2], color[3]);
    glUniform2f(state->rect_resolution_uniform, (float)state->width, (float)state->height);

    // --- FIX START ---
    // 2. Get the current time and update the 'time' uniform before drawing.
    // This ensures the animation progresses with each new frame.
    float current_time = get_current_time();
    if (state->rect_time_uniform != -1) { // Check if the uniform location is valid
        glUniform1f(state->rect_time_uniform, current_time);
    }
    // --- FIX END ---

    // Draw the rectangle
    glBindVertexArray(state->rect_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// Render close button with X symbol
static void render_ssd_close_button(struct plugin_state *state, float x, float y) {
    // Draw close button background
    render_ssd_rect(state, x, y, SSD_BUTTON_SIZE, SSD_BUTTON_SIZE, SSD_CLOSE_BUTTON_COLOR);
    
    // Draw X symbol using two rectangles (diagonal lines approximation)
    float line_width = 2.0f;
    float margin = 4.0f;
    float line_length = SSD_BUTTON_SIZE - (margin * 2);
    
    static const float white_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    
    // Approximate X with rectangles (not perfect but works for OpenGL ES)
    // Top-left to bottom-right diagonal
    render_ssd_rect(state, x + margin, y + margin, line_length * 0.7f, line_width, white_color);
    render_ssd_rect(state, x + margin + line_length * 0.3f, y + margin + line_length * 0.3f, line_length * 0.7f, line_width, white_color);
    
    // Top-right to bottom-left diagonal  
    render_ssd_rect(state, x + SSD_BUTTON_SIZE - margin - line_length * 0.7f, y + margin, line_length * 0.7f, line_width, white_color);
    render_ssd_rect(state, x + margin, y + margin + line_length * 0.3f, line_length * 0.7f, line_width, white_color);
}

// Main decoration rendering function
void render_window_decorations(struct plugin_state *state, int window_index) {
    struct plugin_window *win = &state->windows[window_index];
    if (!win->ssd.enabled) {
        return;
    }
    
    loggy_wobbly("[SSD] Rendering decorations for window %d at (%.1f,%.1f)\n", window_index, win->x, win->y);
    
    // Calculate decoration positions using CURRENT window position
    float win_x = win->x;
    float win_y = win->y;
    float win_width = win->width;
    float win_height = win->height;
    
    // Calculate decoration positions
    float frame_x = win_x - SSD_BORDER_WIDTH;
    float frame_y = win_y - SSD_TITLE_BAR_HEIGHT;
    float total_width = win_width + (SSD_BORDER_WIDTH * 2);
    
    // Render title bar ABOVE the window
    if (win->ssd.needs_title_bar) {
        render_ssd_rect(state, frame_x, frame_y, total_width, SSD_TITLE_BAR_HEIGHT, SSD_TITLE_BAR_COLOR);
        
        // Render close button in top-right of title bar
        if (win->ssd.needs_close_button) {
            float button_x = frame_x + total_width - SSD_BUTTON_SIZE - SSD_CLOSE_BUTTON_MARGIN;
            float button_y = frame_y + (SSD_TITLE_BAR_HEIGHT - SSD_BUTTON_SIZE) / 2;
    //        render_ssd_close_button(state, button_x, button_y);
        }
    }
    
    // Render borders around the window
    // Left border
    render_ssd_rect(state, frame_x, win_y, SSD_BORDER_WIDTH, win_height, SSD_BORDER_COLOR);
    
    // Right border  
    render_ssd_rect(state, win_x + win_width, win_y, SSD_BORDER_WIDTH, win_height, SSD_BORDER_COLOR);
    
    // Bottom border
    render_ssd_rect(state, frame_x, win_y + win_height, total_width, SSD_BORDER_BOTTOM_WIDTH, SSD_BORDER_COLOR);
    
    loggy_wobbly("[SSD] Decorations rendered at frame_pos(%.1f,%.1f) title_size(%.1fx%d)\n", 
                 frame_x, frame_y, total_width, SSD_TITLE_BAR_HEIGHT);
}

// HYBRID APPROACH: Simple initialization + Fast DMA-BUF import when possible
// Modified main() function to use DMA-BUF output instead of SHM
int main(int argc, char *argv[]) {
    loggy_wobbly( "[PLUGIN] === HYBRID FAST VERSION WITH DMA-BUF OUTPUT ===\n");


    
    // Parse arguments (same as before)
    int ipc_fd = -1;
    const char* shm_name = NULL;
    
    if (argc >= 3) {
        if (strncmp(argv[1], "--", 2) != 0) {
            ipc_fd = atoi(argv[1]);
            shm_name = argv[2];
        } else {
            for (int i = 1; i < argc - 1; i++) {
                if (strcmp(argv[i], "--ipc-fd") == 0 && i + 1 < argc) {
                    ipc_fd = atoi(argv[i + 1]);
                    i++;
                } else if (strcmp(argv[i], "--shm-name") == 0 && i + 1 < argc) {
                    shm_name = argv[i + 1];
                    i++;
                }
            }
        }
    }
    
    if (ipc_fd < 0 || !shm_name) {
        loggy_wobbly( "[PLUGIN] ERROR: Invalid arguments\n");
        return 1;
    }

    loggy_wobbly( "[PLUGIN] Starting hybrid plugin with DMA-BUF output\n");

       struct plugin_state state = {0};
    gettimeofday(&state.last_frame_time, NULL);    
    state.instance_id = getpid() % 1000;  // Simple unique ID
    snprintf(state.instance_name, sizeof(state.instance_name), "plugin-%d", state.instance_id);
    
    loggy_wobbly( "[%s] Starting isolated plugin instance\n", state.instance_name);
    
    // Update all your existing calls:
    state.dmabuf_fd = create_dmabuf(SHM_WIDTH * SHM_HEIGHT * 4, state.instance_id);

    // Initialize state
 
    state.ipc_fd = ipc_fd;
    state.focused_window_index = -1;
    state.width = SHM_WIDTH;
    state.height = SHM_HEIGHT;

state.dmabuf_sent_to_compositor = false;
    state.last_frame_sequence = 0;
    

    // Set up SHM buffer as fallback
    int shm_fd = shm_open(shm_name, O_RDWR, 0600);
    if (shm_fd < 0) { 
        loggy_wobbly( "[PLUGIN] ERROR: shm_open failed: %s\n", strerror(errno));
        return 1; 
    }
    
    state.shm_ptr = mmap(NULL, SHM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (state.shm_ptr == MAP_FAILED) { 
        loggy_wobbly( "[PLUGIN] ERROR: mmap failed: %s\n", strerror(errno));
        close(shm_fd); 
        return 1; 
    }
    close(shm_fd);
    loggy_wobbly( "[PLUGIN] ✅ SHM buffer ready (fallback)\n");

    // Initialize EGL context
    loggy_wobbly( "[PLUGIN] Initializing EGL context...\n");
    if (!init_plugin_egl(&state)) {
        loggy_wobbly( "[PLUGIN] ERROR: EGL initialization failed\n");
        return 1;
    }
    loggy_wobbly( "[PLUGIN] ✅ EGL context ready\n");

// Right after: loggy_wobbly( "[PLUGIN] ✅ EGL context ready\n");

// CRITICAL: Initialize EGL extensions
loggy_wobbly( "[PLUGIN] Initializing EGL extensions...\n");
if (!init_egl_extensions(state.egl_display)) {
    loggy_wobbly( "[PLUGIN] ⚠️  EGL DMA-BUF export extensions not available\n");
    loggy_wobbly( "[PLUGIN] Will use fallback DMA-BUF sharing\n");
    // Set flag to use fallback method
    state.use_egl_export = false;
} else {
    loggy_wobbly( "[PLUGIN] ✅ EGL DMA-BUF export extensions ready\n");
    state.use_egl_export = true;
}

    // Initialize OpenGL resources
    loggy_wobbly( "[PLUGIN] Initializing OpenGL resources...\n");
    if (!init_opengl_resources(&state)) {
        loggy_wobbly( "[PLUGIN] ERROR: OpenGL initialization failed\n");
        return 1;
    }
    loggy_wobbly( "[PLUGIN] ✅ OpenGL resources ready\n");

    if (!init_ssd_shaders(&state)) {
    loggy_wobbly("[PLUGIN] WARNING: SSD shaders failed, decorations disabled\n");
    // Continue without SSD support
}

    // STEP 1: Test pool creation
loggy_wobbly( "[PLUGIN] Testing DMA-BUF pool creation...\n");
if (init_dmabuf_pool(&state)) {
    loggy_wobbly( "[PLUGIN] ✅ Pool creation successful!\n");
} else {
    loggy_wobbly( "[PLUGIN] ❌ Pool creation failed\n");
}


test_pool_usage(&state);

test_pool_rendering(&state);    


// Add this right after: loggy_wobbly( "[PLUGIN] ✅ Pool rendering test complete\n");

// STEP 4: Send pool to compositor
if (send_pool_to_compositor(&state)) {
    loggy_wobbly( "[PLUGIN] ✅ Pool transfer successful!\n");
} else {
    loggy_wobbly( "[PLUGIN] ❌ Pool transfer failed!\n");
}

    // Try to initialize DMA-BUF backed FBO (the key change!)
    loggy_wobbly( "[PLUGIN] Attempting DMA-BUF backed FBO...\n");
    //bool dmabuf_fbo_available = init_dmabuf_fbo(&state, SHM_WIDTH, SHM_HEIGHT);

bool dmabuf_fbo_available = init_dmabuf_fbo_robust(&state, SHM_WIDTH, SHM_HEIGHT);
    
    if (!dmabuf_fbo_available) {
        loggy_wobbly( "[PLUGIN] DMA-BUF FBO failed, falling back to regular FBO...\n");
        if (!init_fbo(&state, SHM_WIDTH, SHM_HEIGHT)) {
            loggy_wobbly( "[PLUGIN] ERROR: Fallback FBO initialization failed\n");
            return 1;
        }
    }

    // Try to initialize shared DMA-BUF context for window import
    loggy_wobbly( "[PLUGIN] Attempting shared DMA-BUF context...\n");
    bool dmabuf_lib_available = dmabuf_share_init_with_display(&state.dmabuf_ctx, state.egl_display);
    if (dmabuf_lib_available) {
        loggy_wobbly( "[PLUGIN] ✅ Shared DMA-BUF library available\n");
    } else {
        loggy_wobbly( "[PLUGIN] ⚠️  Shared DMA-BUF library not available\n");
    }

    loggy_wobbly( "[PLUGIN] 🚀 Hybrid plugin ready!\n");
    loggy_wobbly( "[PLUGIN] Capabilities:\n");
    loggy_wobbly( "[PLUGIN]   - DMA-BUF output FBO: %s\n", dmabuf_fbo_available ? "YES" : "NO");
    loggy_wobbly( "[PLUGIN]   - Shared DMA-BUF library: %s\n", dmabuf_lib_available ? "YES" : "NO");
    loggy_wobbly( "[PLUGIN]   - EGL DMA-BUF import: YES\n");
    loggy_wobbly( "[PLUGIN]   - SHM fallback: YES\n");

    // Test IPC connection
    ipc_notification_type_t startup_notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
    write(state.ipc_fd, &startup_notification, sizeof(startup_notification));
    loggy_wobbly( "[PLUGIN] ✅ IPC connection verified\n");

    // Send DMA-BUF info once if available
    if (dmabuf_fbo_available) {
        if (send_dmabuf_info_once(&state)) {
            loggy_wobbly( "[PLUGIN] ✅ DMA-BUF output enabled - zero-copy to compositor!\n");
          //  verify_dmabuf_zero_copy(&state);
        } else {
            loggy_wobbly( "[PLUGIN] ⚠️  DMA-BUF output failed, will use SHM fallback\n");
            dmabuf_fbo_available = false;
        }
    }

    loggy_wobbly( "[PLUGIN] Entering main event loop...\n");

    // Main loop with DMA-BUF or SHM output
    int frame_count = 0;
    while (true) {
        // AT THE VERY TOP of the loop, call the animation function
        update_grid_animations(&state);
        
        ipc_event_t event;
        int fds[32];
        int fd_count = 0;
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(state.ipc_fd, &read_fds);
        
        struct timeval timeout = {0, 1000};
        int select_result = select(state.ipc_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result == 0) {
            frame_count++;
            
            // ADD: Log more frequently to see frame rendering
            if (frame_count <= 10 || frame_count % 60 == 0) {  // Every second at 60fps
                loggy_wobbly( "[PLUGIN] Frame %d (%s) - Windows: %d\n", frame_count, 
                        (state.dmabuf_fd >= 0) ? "ZERO-COPY" : "SHM-COPY", state.window_count);
            }
            
            // ADD: Force a test render even with no windows
            if (frame_count <= 5) {
                loggy_wobbly( "[PLUGIN] Test render frame %d...\n", frame_count);
                
                // Test render with debug background
                glBindFramebuffer(GL_FRAMEBUFFER, state.fbo);
                glViewport(0, 0, state.width, state.height);
                
                // Different color each frame for debugging
                float colors[][4] = {
                    {1.0f, 0.0f, 0.0f, 1.0f}, // Red
                    {0.0f, 1.0f, 0.0f, 1.0f}, // Green  
                    {0.0f, 0.0f, 1.0f, 1.0f}, // Blue
                    {1.0f, 1.0f, 0.0f, 1.0f}, // Yellow
                    {1.0f, 0.0f, 1.0f, 1.0f}, // Magenta
                };
                
                int color_idx = (frame_count - 1) % 5;
              //  glClearColor(colors[color_idx][0], colors[color_idx][1], colors[color_idx][2], colors[color_idx][3]);
              //  glClear(GL_COLOR_BUFFER_BIT);
                glFlush();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                
                loggy_wobbly( "[PLUGIN] Test render complete (color %d)\n", color_idx);
            }
            
            // Use the smart rendering that chooses zero-copy or SHM automatically
          //  render_with_pool_smart(&state, frame_count);
          //  force_wobbly_mesh(&state);  
            render_with_wobbly_fixed(&state, frame_count);


            // Send frame notification (works for both DMA-BUF and SHM)
            if (dmabuf_fbo_available && state.dmabuf_fd >= 0) {
                // Zero-copy: just notify compositor that DMA-BUF has new content
                send_frame_ready(&state);

            } else {
                // SHM fallback: copy the FBO to SHM then notify
                if (state.shm_ptr) {
                    glBindFramebuffer(GL_FRAMEBUFFER, state.fbo);
            //        glReadPixels(0, 0, state.width, state.height, GL_RGBA, GL_UNSIGNED_BYTE, state.shm_ptr);
                    glFlush();
                    msync(state.shm_ptr, SHM_BUFFER_SIZE, MS_ASYNC);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
                
                ipc_notification_type_t notification = IPC_NOTIFICATION_TYPE_FRAME_SUBMIT;
                write(state.ipc_fd, &notification, sizeof(notification));
            }
            
            continue;
        }

        if (select_result > 0) { // An IPC event is ready
            // Handle IPC events with enhanced error checking
            ssize_t n = read_ipc_with_fds(state.ipc_fd, &event, fds, &fd_count);
            
            if (n <= 0) {
                if (n == 0) {
                    loggy_wobbly( "[PLUGIN] IPC connection closed by compositor\n");
                } else {
                    loggy_wobbly( "[PLUGIN] IPC read error: %s\n", strerror(errno));
                }
                break;
            }
            
            // MODIFY your event handling to include the new type
      switch (event.type) {
    case IPC_EVENT_TYPE_WINDOW_LIST_UPDATE:
        // Process window list update - this function already handles grid support properly
        handle_window_list_with_grid_support_fixed(&state, &event, fds, fd_count);
        break;
    
    case IPC_EVENT_TYPE_TOGGLE_GRID_LAYOUT:
        handle_grid_toggle(&state);
        break;
    
    default:
        loggy_wobbly( "[PLUGIN] Unknown IPC event type: %d\n", event.type);
        break;
}
            // Close any remaining unclaimed FDs to prevent leaks
            for (int i = state.window_count; i < fd_count; i++) {
                if (fds[i] >= 0) {
                    loggy_wobbly( "[PLUGIN] Closing unclaimed FD %d\n", fds[i]);
                    close(fds[i]);
                }
            }
        }
    }
    
    // Cleanup
    loggy_wobbly( "[PLUGIN] Cleaning up hybrid plugin...\n");
    
    for (int i = 0; i < state.window_count; i++) {
        if (state.windows[i].gl_texture != 0) {
            glDeleteTextures(1, &state.windows[i].gl_texture);
        }
    }
    
    if (dmabuf_lib_available) {
        dmabuf_share_cleanup(&state.dmabuf_ctx);
    }
    
    if (state.shm_ptr) munmap(state.shm_ptr, SHM_BUFFER_SIZE);
    if (state.fbo) glDeleteFramebuffers(1, &state.fbo);
    if (state.fbo_texture) glDeleteTextures(1, &state.fbo_texture);
    if (state.dmabuf_fd >= 0) close(state.dmabuf_fd);
    
    loggy_wobbly( "[PLUGIN] Hybrid plugin shutdown complete\n");
    return 0;
}
////////////////////////breadcrumbs///////////////////////////////////
//////////////////////////////////////////////////////////////////////

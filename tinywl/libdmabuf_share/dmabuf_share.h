#ifndef DMABUF_SHARE_H
#define DMABUF_SHARE_H

#include <stdint.h>
#include <stdbool.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <gbm.h>

typedef struct {
    int drm_fd;
    struct gbm_device *gbm;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLConfig egl_config;
    bool initialized;
    bool owns_context;  // true if we created the context, false if external
        struct gbm_bo *export_bo;          // Persistent export buffer
    GLuint export_texture;             // GL texture for export buffer
    GLuint export_fbo;                 // FBO for export buffer
} dmabuf_share_context_t;

typedef struct {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t stride;
    uint64_t modifier;
} dmabuf_share_buffer_t;

// Initialize shared DMA-BUF context (creates its own EGL context)
bool dmabuf_share_init(dmabuf_share_context_t *ctx);

// Initialize with external EGL display (use existing context)
bool dmabuf_share_init_with_display(dmabuf_share_context_t *ctx, EGLDisplay external_display);

// Cleanup shared context
void dmabuf_share_cleanup(dmabuf_share_context_t *ctx);

// Import DMA-BUF as OpenGL texture
GLuint dmabuf_share_import_texture(dmabuf_share_context_t *ctx, const dmabuf_share_buffer_t *dmabuf);

// Export OpenGL texture as DMA-BUF
bool dmabuf_share_export_texture(dmabuf_share_context_t *ctx, GLuint texture, 
                                uint32_t width, uint32_t height, dmabuf_share_buffer_t *out_dmabuf);

// Export FBO texture as DMA-BUF (new function)
bool dmabuf_share_export_fbo_texture(dmabuf_share_context_t *ctx, GLuint fbo_texture, 
                                     uint32_t width, uint32_t height, dmabuf_share_buffer_t *out_dmabuf);

#endif
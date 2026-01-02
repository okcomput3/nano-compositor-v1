// native_webgl.cc - Native Node.js addon for zero-copy WebGL to DMA-BUF
#include <napi.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <gbm.h>
#include <drm/drm_fourcc.h>

// EGL extension function pointers
static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;

class WebGLContext : public Napi::ObjectWrap<WebGLContext> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    WebGLContext(const Napi::CallbackInfo& info);
    ~WebGLContext();

private:
    // EGL state
    int drm_fd_ = -1;
    struct gbm_device* gbm_ = nullptr;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    
    // Double-buffered DMA-BUF state
    struct Buffer {
        int fd = -1;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t stride = 0;
        uint32_t format = 0;
        EGLImage egl_image = EGL_NO_IMAGE_KHR;
        GLuint texture = 0;
        GLuint fbo = 0;
    };
    Buffer buffers_[2];
    int current_buffer_ = 0;
    
    // Shader state
    GLuint shader_program_ = 0;
    GLuint vbo_ = 0;
    GLint u_time_ = -1;
    GLint u_resolution_ = -1;
    GLint u_mouse_ = -1;
    
    // Methods exposed to JavaScript
    Napi::Value Initialize(const Napi::CallbackInfo& info);
    Napi::Value ImportDMABuf(const Napi::CallbackInfo& info);
    Napi::Value BindFramebuffer(const Napi::CallbackInfo& info);
    Napi::Value Clear(const Napi::CallbackInfo& info);
    Napi::Value ClearColor(const Napi::CallbackInfo& info);
    Napi::Value UseShader(const Napi::CallbackInfo& info);
    Napi::Value SetUniform1f(const Napi::CallbackInfo& info);
    Napi::Value SetUniform2f(const Napi::CallbackInfo& info);
    Napi::Value DrawFullscreen(const Napi::CallbackInfo& info);
    Napi::Value SwapBuffers(const Napi::CallbackInfo& info);
    Napi::Value Finish(const Napi::CallbackInfo& info);
    Napi::Value GetCurrentBuffer(const Napi::CallbackInfo& info);
    Napi::Value CompileShader(const Napi::CallbackInfo& info);
    Napi::Value Viewport(const Napi::CallbackInfo& info);
    Napi::Value GetError(const Napi::CallbackInfo& info);
    Napi::Value Destroy(const Napi::CallbackInfo& info);
    
    // Internal helpers
    bool InitEGL();
    GLuint CompileShaderInternal(GLenum type, const char* source);
    bool CreateShaderProgram(const char* vert_src, const char* frag_src);
};

Napi::Object WebGLContext::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "WebGLContext", {
        InstanceMethod("initialize", &WebGLContext::Initialize),
        InstanceMethod("importDMABuf", &WebGLContext::ImportDMABuf),
        InstanceMethod("bindFramebuffer", &WebGLContext::BindFramebuffer),
        InstanceMethod("clear", &WebGLContext::Clear),
        InstanceMethod("clearColor", &WebGLContext::ClearColor),
        InstanceMethod("useShader", &WebGLContext::UseShader),
        InstanceMethod("setUniform1f", &WebGLContext::SetUniform1f),
        InstanceMethod("setUniform2f", &WebGLContext::SetUniform2f),
        InstanceMethod("drawFullscreen", &WebGLContext::DrawFullscreen),
        InstanceMethod("swapBuffers", &WebGLContext::SwapBuffers),
        InstanceMethod("finish", &WebGLContext::Finish),
        InstanceMethod("getCurrentBuffer", &WebGLContext::GetCurrentBuffer),
        InstanceMethod("compileShader", &WebGLContext::CompileShader),
        InstanceMethod("viewport", &WebGLContext::Viewport),
        InstanceMethod("getError", &WebGLContext::GetError),
        InstanceMethod("destroy", &WebGLContext::Destroy),
    });

    Napi::FunctionReference* constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("WebGLContext", func);
    return exports;
}

WebGLContext::WebGLContext(const Napi::CallbackInfo& info) 
    : Napi::ObjectWrap<WebGLContext>(info) {
    fprintf(stderr, "[NATIVE-WEBGL] WebGLContext created\n");
}

WebGLContext::~WebGLContext() {
    fprintf(stderr, "[NATIVE-WEBGL] WebGLContext destroying\n");
    
    // Cleanup GL resources
    if (context_ != EGL_NO_CONTEXT && display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_);
        
        if (shader_program_) glDeleteProgram(shader_program_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        
        for (int i = 0; i < 2; i++) {
            if (buffers_[i].fbo) glDeleteFramebuffers(1, &buffers_[i].fbo);
            if (buffers_[i].texture) glDeleteTextures(1, &buffers_[i].texture);
            if (buffers_[i].egl_image != EGL_NO_IMAGE_KHR && eglDestroyImageKHR) {
                eglDestroyImageKHR(display_, buffers_[i].egl_image);
            }
        }
        
        eglDestroyContext(display_, context_);
    }
    
    if (display_ != EGL_NO_DISPLAY) {
        eglTerminate(display_);
    }
    
    if (gbm_) gbm_device_destroy(gbm_);
    if (drm_fd_ >= 0) close(drm_fd_);
}

bool WebGLContext::InitEGL() {
    // Open DRM render node
    drm_fd_ = open("/dev/dri/renderD128", O_RDWR);
    if (drm_fd_ < 0) {
        fprintf(stderr, "[NATIVE-WEBGL] Failed to open render node\n");
        return false;
    }
    
    // Create GBM device
    gbm_ = gbm_create_device(drm_fd_);
    if (!gbm_) {
        fprintf(stderr, "[NATIVE-WEBGL] Failed to create GBM device\n");
        return false;
    }
    
    // Get EGL display
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = 
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    
    if (eglGetPlatformDisplayEXT) {
        display_ = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, gbm_, nullptr);
    } else {
        display_ = eglGetDisplay((EGLNativeDisplayType)gbm_);
    }
    
    if (display_ == EGL_NO_DISPLAY) {
        fprintf(stderr, "[NATIVE-WEBGL] Failed to get EGL display\n");
        return false;
    }
    
    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(display_, &major, &minor)) {
        fprintf(stderr, "[NATIVE-WEBGL] Failed to initialize EGL\n");
        return false;
    }
    
    fprintf(stderr, "[NATIVE-WEBGL] EGL %d.%d initialized\n", major, minor);
    
    // Choose config
    EGLConfig config;
    EGLint config_count;
    EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    if (!eglChooseConfig(display_, config_attribs, &config, 1, &config_count) || config_count == 0) {
        fprintf(stderr, "[NATIVE-WEBGL] Failed to choose EGL config\n");
        return false;
    }
    
    // Create context
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, context_attribs);
    if (context_ == EGL_NO_CONTEXT) {
        fprintf(stderr, "[NATIVE-WEBGL] Failed to create EGL context\n");
        return false;
    }
    
    // Make context current (surfaceless)
    if (!eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_)) {
        fprintf(stderr, "[NATIVE-WEBGL] Failed to make context current\n");
        return false;
    }
    
    // Load extension functions
    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    
    if (!eglCreateImageKHR || !glEGLImageTargetTexture2DOES) {
        fprintf(stderr, "[NATIVE-WEBGL] Required EGL extensions not available\n");
        return false;
    }
    
    fprintf(stderr, "[NATIVE-WEBGL] ✅ EGL initialized: %s\n", glGetString(GL_RENDERER));
    
    // Create fullscreen quad VBO
    float vertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    return true;
}

Napi::Value WebGLContext::Initialize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!InitEGL()) {
        Napi::Error::New(env, "Failed to initialize EGL").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    return Napi::Boolean::New(env, true);
}

Napi::Value WebGLContext::ImportDMABuf(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 5) {
        Napi::TypeError::New(env, "Expected: bufferIndex, fd, width, height, stride").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    int buffer_idx = info[0].As<Napi::Number>().Int32Value();
    int fd = info[1].As<Napi::Number>().Int32Value();
    uint32_t width = info[2].As<Napi::Number>().Uint32Value();
    uint32_t height = info[3].As<Napi::Number>().Uint32Value();
    uint32_t stride = info[4].As<Napi::Number>().Uint32Value();
    uint32_t format = DRM_FORMAT_ARGB8888;  // Default format
    
    if (info.Length() > 5) {
        format = info[5].As<Napi::Number>().Uint32Value();
    }
    
    if (buffer_idx < 0 || buffer_idx > 1) {
        Napi::Error::New(env, "Buffer index must be 0 or 1").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    Buffer& buf = buffers_[buffer_idx];
    buf.fd = fd;
    buf.width = width;
    buf.height = height;
    buf.stride = stride;
    buf.format = format;
    
    fprintf(stderr, "[NATIVE-WEBGL] Importing DMA-BUF %d: fd=%d %ux%u stride=%u format=0x%x\n",
            buffer_idx, fd, width, height, stride, format);
    
    // Create EGL image from DMA-BUF
    EGLint attribs[] = {
        EGL_WIDTH, (EGLint)width,
        EGL_HEIGHT, (EGLint)height,
        EGL_LINUX_DRM_FOURCC_EXT, (EGLint)format,
        EGL_DMA_BUF_PLANE0_FD_EXT, fd,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)stride,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_NONE
    };
    
    buf.egl_image = eglCreateImageKHR(display_, EGL_NO_CONTEXT, 
                                       EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);
    
    if (buf.egl_image == EGL_NO_IMAGE_KHR) {
        EGLint error = eglGetError();
        fprintf(stderr, "[NATIVE-WEBGL] ❌ Failed to create EGL image: 0x%x\n", error);
        Napi::Error::New(env, "Failed to create EGL image from DMA-BUF").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    // Create texture from EGL image
    glGenTextures(1, &buf.texture);
    glBindTexture(GL_TEXTURE_2D, buf.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, buf.egl_image);
    
    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        fprintf(stderr, "[NATIVE-WEBGL] ❌ GL error after EGLImageTargetTexture2D: 0x%x\n", gl_error);
        Napi::Error::New(env, "GL error creating texture from EGL image").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    // Create framebuffer
    glGenFramebuffers(1, &buf.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, buf.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buf.texture, 0);
    
    GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fbo_status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[NATIVE-WEBGL] ❌ Framebuffer incomplete: 0x%x\n", fbo_status);
        Napi::Error::New(env, "Framebuffer incomplete").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    fprintf(stderr, "[NATIVE-WEBGL] ✅ Imported buffer %d: texture=%u fbo=%u\n",
            buffer_idx, buf.texture, buf.fbo);
    
    return Napi::Boolean::New(env, true);
}

GLuint WebGLContext::CompileShaderInternal(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fprintf(stderr, "[NATIVE-WEBGL] Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool WebGLContext::CreateShaderProgram(const char* vert_src, const char* frag_src) {
    GLuint vs = CompileShaderInternal(GL_VERTEX_SHADER, vert_src);
    GLuint fs = CompileShaderInternal(GL_FRAGMENT_SHADER, frag_src);
    
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    
    if (shader_program_) {
        glDeleteProgram(shader_program_);
    }
    
    shader_program_ = glCreateProgram();
    glAttachShader(shader_program_, vs);
    glAttachShader(shader_program_, fs);
    glBindAttribLocation(shader_program_, 0, "a_position");
    glBindAttribLocation(shader_program_, 1, "a_texcoord");
    glLinkProgram(shader_program_);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    GLint linked;
    glGetProgramiv(shader_program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(shader_program_, sizeof(log), nullptr, log);
        fprintf(stderr, "[NATIVE-WEBGL] Program link error: %s\n", log);
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
        return false;
    }
    
    // Get uniform locations
    u_time_ = glGetUniformLocation(shader_program_, "u_time");
    u_resolution_ = glGetUniformLocation(shader_program_, "u_resolution");
    u_mouse_ = glGetUniformLocation(shader_program_, "u_mouse");
    
    return true;
}

Napi::Value WebGLContext::CompileShader(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 2) {
        Napi::TypeError::New(env, "Expected: vertexShaderSource, fragmentShaderSource").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    std::string vert_src = info[0].As<Napi::String>().Utf8Value();
    std::string frag_src = info[1].As<Napi::String>().Utf8Value();
    
    if (!CreateShaderProgram(vert_src.c_str(), frag_src.c_str())) {
        Napi::Error::New(env, "Failed to compile shader program").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    fprintf(stderr, "[NATIVE-WEBGL] ✅ Shader program compiled\n");
    return Napi::Boolean::New(env, true);
}

Napi::Value WebGLContext::BindFramebuffer(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    int buffer_idx = current_buffer_;
    if (info.Length() > 0) {
        buffer_idx = info[0].As<Napi::Number>().Int32Value();
    }
    
    if (buffer_idx < 0 || buffer_idx > 1 || buffers_[buffer_idx].fbo == 0) {
        Napi::Error::New(env, "Invalid buffer index or buffer not imported").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, buffers_[buffer_idx].fbo);
    return env.Undefined();
}

Napi::Value WebGLContext::Viewport(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    int x = 0, y = 0;
    int width = buffers_[current_buffer_].width;
    int height = buffers_[current_buffer_].height;
    
    if (info.Length() >= 4) {
        x = info[0].As<Napi::Number>().Int32Value();
        y = info[1].As<Napi::Number>().Int32Value();
        width = info[2].As<Napi::Number>().Int32Value();
        height = info[3].As<Napi::Number>().Int32Value();
    }
    
    glViewport(x, y, width, height);
    return env.Undefined();
}

Napi::Value WebGLContext::ClearColor(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    float r = 0, g = 0, b = 0, a = 1;
    if (info.Length() >= 4) {
        r = info[0].As<Napi::Number>().FloatValue();
        g = info[1].As<Napi::Number>().FloatValue();
        b = info[2].As<Napi::Number>().FloatValue();
        a = info[3].As<Napi::Number>().FloatValue();
    }
    
    glClearColor(r, g, b, a);
    return env.Undefined();
}

Napi::Value WebGLContext::Clear(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    GLbitfield mask = GL_COLOR_BUFFER_BIT;
    if (info.Length() > 0) {
        mask = info[0].As<Napi::Number>().Uint32Value();
    }
    
    glClear(mask);
    return env.Undefined();
}

Napi::Value WebGLContext::UseShader(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (shader_program_) {
        glUseProgram(shader_program_);
    }
    return env.Undefined();
}

Napi::Value WebGLContext::SetUniform1f(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 2) return env.Undefined();
    
    std::string name = info[0].As<Napi::String>().Utf8Value();
    float value = info[1].As<Napi::Number>().FloatValue();
    
    GLint location = glGetUniformLocation(shader_program_, name.c_str());
    if (location >= 0) {
        glUniform1f(location, value);
    }
    
    return env.Undefined();
}

Napi::Value WebGLContext::SetUniform2f(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 3) return env.Undefined();
    
    std::string name = info[0].As<Napi::String>().Utf8Value();
    float x = info[1].As<Napi::Number>().FloatValue();
    float y = info[2].As<Napi::Number>().FloatValue();
    
    GLint location = glGetUniformLocation(shader_program_, name.c_str());
    if (location >= 0) {
        glUniform2f(location, x, y);
    }
    
    return env.Undefined();
}

Napi::Value WebGLContext::DrawFullscreen(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    return env.Undefined();
}

Napi::Value WebGLContext::Finish(const Napi::CallbackInfo& info) {
    glFinish();
    return info.Env().Undefined();
}

Napi::Value WebGLContext::SwapBuffers(const Napi::CallbackInfo& info) {
    current_buffer_ = 1 - current_buffer_;
    return Napi::Number::New(info.Env(), current_buffer_);
}

Napi::Value WebGLContext::GetCurrentBuffer(const Napi::CallbackInfo& info) {
    return Napi::Number::New(info.Env(), current_buffer_);
}

Napi::Value WebGLContext::GetError(const Napi::CallbackInfo& info) {
    return Napi::Number::New(info.Env(), glGetError());
}

Napi::Value WebGLContext::Destroy(const Napi::CallbackInfo& info) {
    // Will be cleaned up in destructor
    return info.Env().Undefined();
}

// Module initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    WebGLContext::Init(env, exports);
    
    // Export GL constants
    exports.Set("COLOR_BUFFER_BIT", Napi::Number::New(env, GL_COLOR_BUFFER_BIT));
    exports.Set("DEPTH_BUFFER_BIT", Napi::Number::New(env, GL_DEPTH_BUFFER_BIT));
    
    return exports;
}

NODE_API_MODULE(native_webgl, Init)

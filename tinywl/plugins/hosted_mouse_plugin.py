import sys
import os
import time
import math
import struct
import socket
import mmap
import ctypes
import select
from typing import Optional, List
import logging

# OpenGL ES bindings
from OpenGL.GLES2 import *
from OpenGL.EGL import *

# Image loading
from PIL import Image
import numpy as np

# =========================
# IPC enums (match nano_ipc.h)
# =========================
IPC_EVENT_TYPE_POINTER_MOTION = 0
IPC_EVENT_TYPE_COMPOSITOR_SHUTDOWN = 31
IPC_NOTIFICATION_TYPE_FRAME_SUBMIT = 1

# =========================
# GLSL SHADERS for 3D Roll Effect
# =========================

ROLL_VERTEX_SHADER = """
#version 100
precision mediump float;

attribute vec4 a_position;
attribute vec2 a_texCoord;

uniform mat4 u_projection;
uniform mat4 u_view;

varying vec3 v_worldPos;
varying vec2 v_texCoord;

void main() {
    // The vertex position from Python (a_position) is already correct.
    // We just need to apply the camera and perspective.
    v_worldPos = a_position.xyz;
    v_texCoord = a_texCoord;
    gl_Position = u_projection * u_view * a_position;
}
"""

ROLL_FRAGMENT_SHADER = """
#version 100
#extension GL_OES_standard_derivatives : enable
precision mediump float;

varying vec3 v_worldPos;
varying vec2 v_texCoord;

uniform float time;
uniform sampler2D u_texture;

void main() {
    // Calculate surface normal for lighting
    vec3 dx = dFdx(v_worldPos);
    vec3 dy = dFdy(v_worldPos);
    vec3 normal = normalize(cross(dx, dy));
    
    // Sample the texture
    vec4 texColor = texture2D(u_texture, v_texCoord);
    
    // Basic lighting
    vec3 lightDir = normalize(vec3(0.5, 0.0, 1.0));
    float diffuse = max(dot(normal, lightDir), 0.3); // Minimum ambient
    
    // Specular highlight
    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfDir), 0.0), 32.0) * 0.3;
    
    // Apply lighting to texture
    vec3 finalColor = texColor.rgb * diffuse + vec3(1.0) * specular;
    
    gl_FragColor = vec4(finalColor, texColor.a);
}
"""

# =========================
# Math & Helpers for 3D
# =========================

def normalize(v):
    mag = math.sqrt(sum(x*x for x in v))
    if mag == 0: return [0.0, 0.0, 0.0]
    return [x / mag for x in v]

def cross(a, b):
    return [a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]]

def look_at(eye, center, up):
    f = normalize([center[i] - eye[i] for i in range(3)])
    s = normalize(cross(f, up))
    u = cross(s, f)
    return [
        s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0,
        -s[0]*eye[0] - s[1]*eye[1] - s[2]*eye[2],
        -u[0]*eye[0] - u[1]*eye[1] - u[2]*eye[2],
        f[0]*eye[0] + f[1]*eye[1] + f[2]*eye[2], 1
    ]

def perspective(fovy, aspect, near, far):
    f = 1.0 / math.tan(math.radians(fovy) / 2)
    return [
        f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + near) / (near - far), -1,
        0, 0, (2 * far * near) / (near - far), 0
    ]

def ease_in_out_quad(t):
    if t < 0.5: return 2 * t * t
    return 1 - pow(-2 * t + 2, 2) / 2

def create_translation_matrix(x, y, z):
    """Create a 4x4 translation matrix"""
    return [
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        x,   y,   z,   1.0
    ]

def multiply_matrices(a, b):
    """Multiply two 4x4 matrices"""
    result = [0.0] * 16
    for i in range(4):
        for j in range(4):
            for k in range(4):
                result[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j]
    return result

# =========================
# MagicRollEffect Class
# =========================

class MagicRollEffect:
    def __init__(self, width, height):
        self.subdivisions = 60
        self.animation_start_time = time.time()
        
        # Position variables - these just offset the view matrix
        self.pos_x = 0.0    # Horizontal position offset (-2.0 to 2.0)
        self.pos_y = 0.0    # Vertical position offset (-2.0 to 2.0)  
        self.pos_z = 0.0    # Depth position offset (-2.0 to 2.0)
        
        aspect = height / width
        scale = 1.2
        self.width = scale
        self.height = scale * aspect
        self.x = -self.width / 2
        self.y = -self.height / 2
        
        self.program, self.vbo, self.ibo = 0, 0, 0
        self.index_count = 0
        self.texture_id = 0  
        self.uniform_projection, self.uniform_view, self.uniform_time = -1, -1, -1
        
        self.original_vertices = []
        self.deformed_vertices = []

    def update_position(self, pos_x=None, pos_y=None, pos_z=None):
        """Update position variables - no vertex rebuilding needed"""
        if pos_x is not None:
            self.pos_x = pos_x
            print(f"Updated pos_x to {pos_x}")
        if pos_y is not None:
            self.pos_y = pos_y
            print(f"Updated pos_y to {pos_y}")
        if pos_z is not None:
            self.pos_z = pos_z
            print(f"Updated pos_z to {pos_z}")

# =========================
# Main Plugin State
# =========================
class PluginState:
    def __init__(self, ipc_fd):
        self.ipc_fd = ipc_fd
        self.ipc_socket = socket.fromfd(ipc_fd, socket.AF_UNIX, socket.SOCK_STREAM)
        self.width, self.height = 1920, 1080
        self.fbos = [0, 0]
        self.fbo_textures = [0, 0]
        self.current_buffer_idx = 0
        self.roll_effect: Optional[MagicRollEffect] = None

# =========================
# Core Logic
# =========================

def compile_shader(shader_type: int, source: str) -> int:
    shader = glCreateShader(shader_type)
    glShaderSource(shader, source)
    glCompileShader(shader)
    if glGetShaderiv(shader, GL_COMPILE_STATUS) != GL_TRUE:
        info_log = glGetShaderInfoLog(shader)
        logging.error(f"Shader compilation failed: {info_log.decode()}")
        glDeleteShader(shader)
        return 0
    return shader

def init_roll_effect(state: PluginState) -> bool:
    logging.info("Initializing 3D Roll Effect...")
    try:
        effect = MagicRollEffect(state.width, state.height)
        
        vs = compile_shader(GL_VERTEX_SHADER, ROLL_VERTEX_SHADER)
        fs = compile_shader(GL_FRAGMENT_SHADER, ROLL_FRAGMENT_SHADER)
        if not vs or not fs: return False

        program = glCreateProgram()
        glAttachShader(program, vs)
        glAttachShader(program, fs)
        glBindAttribLocation(program, 0, "a_position")
        glBindAttribLocation(program, 1, "a_texCoord")
        glLinkProgram(program)
        glDeleteShader(vs); glDeleteShader(fs)

        if glGetProgramiv(program, GL_LINK_STATUS) != GL_TRUE:
            logging.error(f"Program linking failed: {glGetProgramInfoLog(program).decode()}")
            return False
        
        effect.program = program
        effect.uniform_projection = glGetUniformLocation(program, "u_projection")
        effect.uniform_view = glGetUniformLocation(program, "u_view")
        effect.uniform_time = glGetUniformLocation(program, "time")
        effect.uniform_texture = glGetUniformLocation(program, "u_texture") # <--- ADD THIS LINE

        # --- ADD THE TEXTURE LOADING CALL ---
        texture_path = "roll_texture.png" # Make sure this file is in the same directory
        effect.texture_id = load_texture(texture_path)
        if effect.texture_id == 0:
            logging.error("Aborting initialization due to texture failure.")
            return False
        # ------------------------------------

        s = effect.subdivisions
        vertices = []
        for y in range(s + 1):
            for x in range(s + 1):
                u, v = x / s, y / s
                px, py, pz = effect.x + u * effect.width, effect.y + v * effect.height, 0.0
                vertices.extend([px, py, pz, u, v])
                effect.original_vertices.append([px, py, pz])
        
        effect.deformed_vertices = list(vertices)

        indices = []
        for y in range(s):
            for x in range(s):
                tl, tr = y * (s + 1) + x, y * (s + 1) + x + 1
                bl, br = (y + 1) * (s + 1) + x, (y + 1) * (s + 1) + x + 1
                indices.extend([tl, bl, tr, tr, bl, br])
        
        effect.index_count = len(indices)
        
        effect.vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
        glBufferData(GL_ARRAY_BUFFER, len(vertices) * 4, (ctypes.c_float * len(vertices))(*vertices), GL_DYNAMIC_DRAW)

        effect.ibo = glGenBuffers(1)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effect.ibo)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, len(indices) * 2, (ctypes.c_ushort * len(indices))(*indices), GL_STATIC_DRAW)
        
        state.roll_effect = effect
        logging.info("✅ 3D Roll Effect initialized")
        return True
    except Exception as e:
        logging.error(f"Roll effect initialization failed: {e}", exc_info=True)
        return False

def _rebuild_vertices(effect):
    """Rebuild vertex data based on current position"""
    current_x, current_y, current_z = effect.get_current_position()
    
    effect.original_vertices.clear()
    vertices = []
    s = effect.subdivisions
    
    for y in range(s + 1):
        for x in range(s + 1):
            u, v = x / s, y / s
            px = current_x + u * effect.width
            py = current_y + v * effect.height
            pz = current_z
            vertices.extend([px, py, pz, u, v])
            effect.original_vertices.append([px, py, pz])
    
    effect.deformed_vertices = list(vertices)
    print(f"Rebuilt {len(effect.original_vertices)} vertices at position ({current_x:.3f}, {current_y:.3f}, {current_z:.3f})")

def update_and_render_roll_effect(state: PluginState):
    effect = state.roll_effect
    if not effect: return

    # --- Update Mesh (Standard roll animation - vertices never change) ---
    PEEL_DEPTH = 0.4
    
    current_time = time.time()
    duration_sweep, duration_dwell = 10.0, 0.0
    total_cycle_time = 2 * duration_sweep + 2 * duration_dwell
    adjusted_time = current_time - effect.animation_start_time
    norm_time = adjusted_time % total_cycle_time
    
    prog = 0.0
    if norm_time < duration_sweep: prog = norm_time / duration_sweep
    elif norm_time < duration_sweep + duration_dwell: prog = 1.0
    elif norm_time < 2 * duration_sweep + duration_dwell: prog = 1.0 - ((norm_time - (duration_sweep + duration_dwell)) / duration_sweep)
    
    prog = ease_in_out_quad(prog)

    # Standard peel calculation using original positions
    peel_line_x = effect.x + prog * effect.width

    s = effect.subdivisions
    vertex_index = 0
    for y in range(s + 1):
        for x in range(s + 1):
            orig_x, orig_y, orig_z = effect.original_vertices[y * (s + 1) + x]
            
            if orig_x < peel_line_x:
                dist_from_crease = peel_line_x - orig_x
                angle = dist_from_crease / (PEEL_DEPTH / math.pi)
                
                new_x = peel_line_x - (PEEL_DEPTH / math.pi) * math.sin(angle)
                new_z = orig_z + (PEEL_DEPTH / math.pi) * (1 - math.cos(angle))
            else:
                new_x, new_z = orig_x, orig_z

            # Apply position offset to move the entire object in 3D space
            final_x = new_x + effect.pos_x
            final_y = orig_y + effect.pos_y  
            final_z = new_z + effect.pos_z

            start_idx = vertex_index * 5
            effect.deformed_vertices[start_idx] = final_x
            effect.deformed_vertices[start_idx+1] = final_y
            effect.deformed_vertices[start_idx+2] = final_z
            vertex_index += 1

    glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
    glBufferSubData(GL_ARRAY_BUFFER, 0, len(effect.deformed_vertices) * 4, (ctypes.c_float * len(effect.deformed_vertices))(*effect.deformed_vertices))

    # --- Render Mesh with Fixed Camera Position ---
    glEnable(GL_DEPTH_TEST)
    glUseProgram(effect.program)
    # --- ADD THESE THREE LINES TO BIND THE TEXTURE ---
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.texture_id)
    glUniform1i(effect.uniform_texture, 0) # Tell the shader to use texture unit 0
    # ----------------------------------------------------
    
    proj_matrix = perspective(45.0, state.width / state.height, 0.1, 100.0)
    
    # Fixed camera position - object moves, not camera
    view_matrix = look_at([0, 0, 3.0], [0, 0, 0], [0, 1, 0])
    
    glUniformMatrix4fv(effect.uniform_projection, 1, GL_FALSE, proj_matrix)
    glUniformMatrix4fv(effect.uniform_view, 1, GL_FALSE, view_matrix)
    glUniform1f(effect.uniform_time, adjusted_time)

    glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effect.ibo)
    
    stride = 5 * 4
    glEnableVertexAttribArray(0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(0))
    glEnableVertexAttribArray(1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(3 * 4))
    
    glDrawElements(GL_TRIANGLES, effect.index_count, GL_UNSIGNED_SHORT, None)

    glDisableVertexAttribArray(0)
    glDisableVertexAttribArray(1)
    glBindBuffer(GL_ARRAY_BUFFER, 0)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)
    glUseProgram(0)
    glDisable(GL_DEPTH_TEST)

def render_frame_into_buffer(state: PluginState, buffer_idx: int):
    glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[buffer_idx])
    glViewport(0, 0, state.width, state.height)
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    update_and_render_roll_effect(state)
    glFinish()
    glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
def init_fbo_for_buffer(state: PluginState, texture_id: int, index: int) -> bool:
    try:
        state.fbo_textures[index] = texture_id
        state.fbos[index] = glGenFramebuffers(1)
        glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[index])
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0)
        
        depth_renderbuffer = glGenRenderbuffers(1)
        glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer)
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, state.width, state.height)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_renderbuffer)

        if glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE:
            logging.error(f"FBO {index} is incomplete.")
            glDeleteFramebuffers(1, [state.fbos[index]])
            return False
            
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
        logging.info(f"✅ FBO {index} initialized with Texture ID {texture_id} and Depth Buffer")
        return True
    except Exception as e:
        logging.error(f"Failed to init FBO {index}: {e}")
        return False

# =========================
# Math & Helpers for 3D
# =========================

# ... (keep all your existing math functions) ...

def load_texture(path: str) -> int:
    """Loads an image, creates an OpenGL texture, and returns its ID."""
    try:
        logging.info(f"Loading texture from: {path}")
        img = Image.open(path).convert("RGBA")
        # OpenGL expects textures to be flipped vertically
        img = img.transpose(Image.FLIP_TOP_BOTTOM)
        img_data = np.array(list(img.getdata()), np.uint8)

        texture_id = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, texture_id)
        
        # Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
        
        # Upload the texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data)
        
        logging.info(f"✅ Texture loaded successfully (ID: {texture_id})")
        return texture_id
    except FileNotFoundError:
        logging.error(f"Texture file not found at: {path}")
        return 0
    except Exception as e:
        logging.error(f"Failed to load texture: {e}", exc_info=True)
        return 0
        
# =========================
# MAIN FUNCTION (Async Loop)
# =========================
def main(ipc_fd: int, shm_name: str, egl_display: int, egl_context: int,
         fbo_texture_0: int, dmabuf_fd_0: int, width: int, height: int, stride_0: int, format_0: int,
         fbo_texture_1: int, dmabuf_fd_1: int, _w: int, _h: int, stride_1: int, format_1: int) -> int:
    
    logging.basicConfig(level=logging.INFO, format='[HOSTED-PY 3D] %(message)s')
    log = logging.getLogger("py-main")
    log.info("=== Python 3D Roll Effect Plugin RUNNING ===")

    state = PluginState(ipc_fd)
    state.width, state.height = width, height
    texture_path = "roll_texture.png" 
    def send_frame_ready_notification(sock, buffer_idx: int):
        try:
            message = struct.pack('=Ii', IPC_NOTIFICATION_TYPE_FRAME_SUBMIT, buffer_idx)
            sock.sendall(message)
        except (BrokenPipeError, ConnectionResetError):
            log.warning("Compositor connection lost.")
            raise

    try:
        if not init_roll_effect(state): return 1
        if not init_fbo_for_buffer(state, fbo_texture_0, 0): return 1
        if not init_fbo_for_buffer(state, fbo_texture_1, 1): return 1
        log.info("✅ Double buffering initialized successfully.")

        # Test with position changes - NOW ACTUALLY MOVES THE OBJECT
        state.roll_effect.update_position(pos_x=1.6,pos_y=-0.80)  # Move right
        # state.roll_effect.update_position(pos_y=0.3)   # Move up  
        # state.roll_effect.update_position(pos_z=-0.5)  # Move closer
        # state.roll_effect.update_position(pos_x=0.0, pos_y=0.0, pos_z=0.0)  # Back to center

        while True:
            ready, _, _ = select.select([state.ipc_socket], [], [], 0)
            if ready:
                data = state.ipc_socket.recv(4096)
                if not data:
                    log.info("Compositor closed IPC socket. Exiting.")
                    break
            
            back_buffer_idx = state.current_buffer_idx
            render_frame_into_buffer(state, back_buffer_idx)
            send_frame_ready_notification(state.ipc_socket, back_buffer_idx)
            state.current_buffer_idx = (back_buffer_idx + 1) % 2

    except (KeyboardInterrupt, BrokenPipeError, ConnectionResetError):
        log.info("Shutting down gracefully.")
    except Exception as e:
        log.error(f"Unhandled exception in main loop: {e}", exc_info=True)
        return 1
    finally:
        log.info("Cleaning up Python resources...")
        try:
            if state.fbos[0]: glDeleteFramebuffers(1, [state.fbos[0]])
            if state.fbos[1]: glDeleteFramebuffers(1, [state.fbos[1]])
            if state.roll_effect and state.roll_effect.program: glDeleteProgram(state.roll_effect.program)
            if state.ipc_socket: state.ipc_socket.close()
        except Exception as e:
            log.error(f"Error during cleanup: {e}")
    
    return 0
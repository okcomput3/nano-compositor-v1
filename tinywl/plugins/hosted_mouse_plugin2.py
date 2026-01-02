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

# =========================
# GLSL SHADERS for Triple Screen Backdrop
# =========================

BACKDROP_VERTEX_SHADER = """
#version 100
precision mediump float;

attribute vec4 a_position;
attribute vec2 a_texCoord;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

varying vec2 v_texCoord;

void main() {
    gl_Position = u_projection * u_view * u_model * a_position;
    v_texCoord = a_texCoord;
}
"""

BACKDROP_FRAGMENT_SHADER = """
#version 100
precision mediump float;

varying vec2 v_texCoord;
uniform sampler2D u_texture;

void main() {
    gl_FragColor = texture2D(u_texture, v_texCoord);
}
"""

# =========================
# TripleBackdropEffect Class
# =========================

class TripleBackdropEffect:
    def __init__(self, width, height):
        self.program = 0
        self.vbo = 0
        self.texture_ids = [0, 0, 0]  # Three textures
        
        # Store screen dimensions
        self.screen_width = width
        self.screen_height = height
        
        # Uniform locations
        self.uniform_texture = -1
        self.uniform_projection = -1
        self.uniform_view = -1
        self.uniform_model = -1
        
        # Animation parameters
        self.start_time = time.time()
        self.pause_duration = 5.0  # Seconds to pause on each backdrop
        self.transition_duration = 2.0  # Seconds to slide between backdrops
        self.camera_x = 0.0  # Camera position that slides left/right
        
        # Screen positions will be calculated after creation
        self.screen_positions = [-6.0, 0.0, 6.0]  # Default values

    def setup_screen_positions(self):
        """Calculate screen positions based on camera view"""
        # Calculate the distance between screens based on what the camera can see
        fov_radians = math.radians(60.0)  # Field of view
        distance_to_camera = 3.0
        aspect_ratio = self.screen_width / self.screen_height
        screen_width_at_distance = 2.0 * distance_to_camera * math.tan(fov_radians / 2.0) * aspect_ratio
        
        self.screen_positions = [
            -screen_width_at_distance,  # Left screen (slide 1) - one full screen left
             0.0,                       # Center screen (slide 2) - at center
             screen_width_at_distance   # Right screen (slide 3) - one full screen right
        ]
        
        return screen_width_at_distance

    def get_current_camera_position(self):
        """Calculate camera X position - simple consistent movement"""
        current_time = time.time() - self.start_time
        
        # Each phase: pause + transition
        phase_duration = self.pause_duration + self.transition_duration
        cycle_progress = current_time % (3 * phase_duration)
        
        # Distance between adjacent screens (should be consistent)
        screen_distance = self.screen_positions[1] - self.screen_positions[0]
        
        if cycle_progress < phase_duration:
            # Phase 1: Screen 1 -> Screen 2
            phase_time = cycle_progress
            if phase_time < self.pause_duration:
                self.camera_x = self.screen_positions[0]  # Pause on screen 1
            else:
                # Slide to screen 2
                t = (phase_time - self.pause_duration) / self.transition_duration
                t = self.ease_in_out(t)
                self.camera_x = self.screen_positions[0] + t * screen_distance
                
        elif cycle_progress < 2 * phase_duration:
            # Phase 2: Screen 2 -> Screen 3  
            phase_time = cycle_progress - phase_duration
            if phase_time < self.pause_duration:
                self.camera_x = self.screen_positions[1]  # Pause on screen 2
            else:
                # Slide to screen 3
                t = (phase_time - self.pause_duration) / self.transition_duration
                t = self.ease_in_out(t)
                self.camera_x = self.screen_positions[1] + t * screen_distance
                
        else:
            # Phase 3: Screen 3 -> Screen 1 (via carousel wrap)
            phase_time = cycle_progress - 2 * phase_duration
            if phase_time < self.pause_duration:
                self.camera_x = self.screen_positions[2]  # Pause on screen 3
            else:
                # Slide to the "next" screen which is a wrapped-around copy of screen 1
                t = (phase_time - self.pause_duration) / self.transition_duration
                t = self.ease_in_out(t)
                self.camera_x = self.screen_positions[2] + t * screen_distance
        
        return self.camera_x

    def ease_in_out(self, t):
        """Smooth easing function for transitions"""
        return t * t * (3.0 - 2.0 * t)

    def update_timing(self, pause_duration=None, transition_duration=None):
        """Update slideshow timing parameters."""
        if pause_duration is not None:
            self.pause_duration = pause_duration
        if transition_duration is not None:
            self.transition_duration = transition_duration

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
        self.backdrop_effect: Optional[TripleBackdropEffect] = None

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

def create_default_texture(color_rgba=(255, 0, 0, 255), size=(512, 512)) -> int:
    """Create a solid color texture as fallback."""
    try:
        # Create a solid color image
        img_data = np.full((size[1], size[0], 4), color_rgba, dtype=np.uint8)
        
        texture_id = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, texture_id)
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size[0], size[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data)
        
        logging.info(f"✅ Created default texture (ID: {texture_id}, color: {color_rgba})")
        return texture_id
    except Exception as e:
        logging.error(f"Failed to create default texture: {e}")
        return 0

def init_backdrop_effect(state: PluginState) -> bool:
    logging.info("Initializing Triple Fullscreen Backdrop Effect...")
    try:
        effect = TripleBackdropEffect(state.width, state.height)
        
        # Set up screen positions and get dimensions for backdrop creation
        screen_width_at_distance = effect.setup_screen_positions()
        
        vs = compile_shader(GL_VERTEX_SHADER, BACKDROP_VERTEX_SHADER)
        fs = compile_shader(GL_FRAGMENT_SHADER, BACKDROP_FRAGMENT_SHADER)
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
        effect.uniform_texture = glGetUniformLocation(program, "u_texture")
        effect.uniform_projection = glGetUniformLocation(program, "u_projection")
        effect.uniform_view = glGetUniformLocation(program, "u_view")
        effect.uniform_model = glGetUniformLocation(program, "u_model")

        # Try to load three textures, fallback to colored defaults
        texture_paths = [
            "slide1.png",
            "slide2.png", 
            "slide3.png"
        ]
        
        fallback_colors = [
            (255, 100, 100, 255),  # Red
            (100, 255, 100, 255),  # Green
            (100, 100, 255, 255),  # Blue
        ]
        
        for i, path in enumerate(texture_paths):
            texture_id = load_texture(path)
            if texture_id == 0:
                logging.warning(f"Failed to load {path}, creating default texture instead")
                texture_id = create_default_texture(fallback_colors[i])
            
            effect.texture_ids[i] = texture_id
            if texture_id == 0:
                logging.error(f"Failed to create texture {i+1}")
                return False

        # Create fullscreen backdrops that match screen aspect ratio
        # Calculate proper dimensions to fill the screen completely
        aspect_ratio = state.width / state.height
        
        # The camera is at Z=3, so we need to calculate screen coverage at that distance
        fov_radians = math.radians(60.0)  # Field of view from perspective matrix
        distance_to_camera = 3.0
        
        # Calculate how much screen space we see at distance 3.0
        screen_height_at_distance = 2.0 * distance_to_camera * math.tan(fov_radians / 2.0)
        
        # Make backdrops exactly the right size to fill the screen
        backdrop_width = screen_width_at_distance * 0.5  # Scale to perfectly fill screen
        backdrop_height = screen_height_at_distance * 0.5
        
        # Each vertex: (x, y, z, w, u, v) where (u,v) are texture coordinates
        vertices = [
            # Triangle 1
            -backdrop_width, -backdrop_height, 0.0, 1.0,  0.0, 0.0,  # Bottom-left
             backdrop_width, -backdrop_height, 0.0, 1.0,  1.0, 0.0,  # Bottom-right
            -backdrop_width,  backdrop_height, 0.0, 1.0,  0.0, 1.0,  # Top-left
            # Triangle 2
             backdrop_width, -backdrop_height, 0.0, 1.0,  1.0, 0.0,  # Bottom-right
             backdrop_width,  backdrop_height, 0.0, 1.0,  1.0, 1.0,  # Top-right
            -backdrop_width,  backdrop_height, 0.0, 1.0,  0.0, 1.0,  # Top-left
        ]
        
        effect.vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
        glBufferData(GL_ARRAY_BUFFER, len(vertices) * 4, (ctypes.c_float * len(vertices))(*vertices), GL_STATIC_DRAW)
        
        state.backdrop_effect = effect
        logging.info("✅ Triple Fullscreen Backdrop Effect initialized")
        return True
    except Exception as e:
        logging.error(f"Backdrop effect initialization failed: {e}", exc_info=True)
        return False

def get_translation_matrix(x, y, z):
    """Create a 4x4 translation matrix"""
    return [
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        x,   y,   z,   1.0
    ]

def render_screen_at_position(effect, screen_x, texture_id):
    """Render one fullscreen backdrop at the given X position"""
    # Create model matrix for this screen's position
    model_matrix = get_translation_matrix(screen_x, 0.0, 0.0)
    glUniformMatrix4fv(effect.uniform_model, 1, GL_FALSE, model_matrix)
    
    # Bind texture for this screen
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, texture_id)
    glUniform1i(effect.uniform_texture, 0)
    
    # Draw the fullscreen quad
    glDrawArrays(GL_TRIANGLES, 0, 6)

def update_and_render_backdrop_effect(state: PluginState):
    effect = state.backdrop_effect
    if not effect: return

    # Update camera position to slide between screens
    camera_x = effect.get_current_camera_position()

    # Enable depth testing for proper 3D rendering
    glEnable(GL_DEPTH_TEST)
    glUseProgram(effect.program)
    
    # Set up 3D matrices - use wider FOV to see full backdrops
    proj_matrix = perspective(60.0, state.width / state.height, 0.1, 100.0)
    
    # Camera slides left/right to view different screens
    view_matrix = look_at([camera_x, 0, 3.0], [camera_x, 0, 0], [0, 1, 0])
    
    glUniformMatrix4fv(effect.uniform_projection, 1, GL_FALSE, proj_matrix)
    glUniformMatrix4fv(effect.uniform_view, 1, GL_FALSE, view_matrix)

    glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
    
    stride = 6 * 4  # 6 floats per vertex (x, y, z, w, u, v)
    glEnableVertexAttribArray(0) # a_position (4 components: x,y,z,w)
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(0))
    glEnableVertexAttribArray(1) # a_texCoord (2 components: u,v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(4 * 4))
    
    # Render all three screens side by side
    for i, screen_x in enumerate(effect.screen_positions):
        render_screen_at_position(effect, screen_x, effect.texture_ids[i])

    # Also draw the first screen again on the far right for the carousel effect,
    # allowing the camera to seamlessly slide from the last screen to the first.
    screen_distance = effect.screen_positions[2] - effect.screen_positions[1]
    wrapping_screen_x = effect.screen_positions[2] + screen_distance
    render_screen_at_position(effect, wrapping_screen_x, effect.texture_ids[0])
    
    # Log camera position and current phase occasionally
    current_time = time.time() - effect.start_time
    if int(current_time * 10) % 50 == 0:  # Every 5 seconds
        phase_duration = effect.pause_duration + effect.transition_duration
        cycle_progress = current_time % (3 * phase_duration)
        
        if cycle_progress < phase_duration:
            phase_time = cycle_progress
            if phase_time < effect.pause_duration:
                status = f"PAUSED on RED backdrop ({effect.pause_duration - phase_time:.1f}s remaining)"
            else:
                status = "SLIDING Red → Green"
        elif cycle_progress < 2 * phase_duration:
            phase_time = cycle_progress - phase_duration
            if phase_time < effect.pause_duration:
                status = f"PAUSED on GREEN backdrop ({effect.pause_duration - phase_time:.1f}s remaining)"
            else:
                status = "SLIDING Green → Blue"
        else:
            phase_time = cycle_progress - 2 * phase_duration
            if phase_time < effect.pause_duration:
                status = f"PAUSED on BLUE backdrop ({effect.pause_duration - phase_time:.1f}s remaining)"
            else:
                status = "SLIDING Blue → Red"
                
        logging.info(f"Time: {current_time:.1f}s, Camera X: {camera_x:.2f}, Status: {status}")

    glDisableVertexAttribArray(0)
    glDisableVertexAttribArray(1)
    glBindBuffer(GL_ARRAY_BUFFER, 0)
    glUseProgram(0)
    glDisable(GL_DEPTH_TEST)

def render_frame_into_buffer(state: PluginState, buffer_idx: int):
    glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[buffer_idx])
    glViewport(0, 0, state.width, state.height)
    glClearColor(0.0, 0.0, 0.0, 1.0) # Black background
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    update_and_render_backdrop_effect(state)
    glFinish()
    glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
def init_fbo_for_buffer(state: PluginState, texture_id: int, index: int) -> bool:
    try:
        state.fbo_textures[index] = texture_id
        state.fbos[index] = glGenFramebuffers(1)
        glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[index])
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0)
        
        # Add depth buffer for 3D
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

def load_texture(path: str) -> int:
    """Loads an image, creates an OpenGL texture, and returns its ID."""
    try:
        if not os.path.exists(path):
            logging.warning(f"Texture file not found: {path}")
            return 0
            
        logging.info(f"Loading texture from: {path}")
        img = Image.open(path).convert("RGBA")
        img = img.transpose(Image.FLIP_TOP_BOTTOM)
        img_data = np.array(list(img.getdata()), np.uint8)

        texture_id = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, texture_id)
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data)
        
        logging.info(f"✅ Texture loaded successfully (ID: {texture_id})")
        return texture_id
    except FileNotFoundError:
        logging.warning(f"Texture file not found: {path}")
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
    
    logging.basicConfig(level=logging.INFO, format='[HOSTED-PY TRIPLE-BACKDROP] %(message)s')
    log = logging.getLogger("py-main")
    log.info("=== Python Triple Fullscreen Backdrop Plugin RUNNING ===")

    state = PluginState(ipc_fd)
    state.width, state.height = width, height
    
    def send_frame_ready_notification(sock, buffer_idx: int):
        try:
            message = struct.pack('=Ii', IPC_NOTIFICATION_TYPE_FRAME_SUBMIT, buffer_idx)
            sock.sendall(message)
        except (BrokenPipeError, ConnectionResetError):
            log.warning("Compositor connection lost.")
            raise

    try:
        if not init_backdrop_effect(state): return 1
        if not init_fbo_for_buffer(state, fbo_texture_0, 0): return 1
        if not init_fbo_for_buffer(state, fbo_texture_1, 1): return 1
        log.info("✅ Double buffering initialized successfully.")

        # Configure timing: 5 seconds pause + 2 seconds transition
        state.backdrop_effect.update_timing(pause_duration=5.0, transition_duration=2.0)

        log.info("🎬 Camera will pause 5 seconds on each backdrop, then slide smoothly to the next!")
        log.info("📺 Cycle: RED (5s) → slide (2s) → GREEN (5s) → slide (2s) → BLUE (5s) → slide (2s) → repeat")
        log.info(f"⏱️  Total cycle time: {(5.0 + 2.0) * 3:.1f} seconds")

        frame_count = 0

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
            
            frame_count += 1
            
            # Add a small sleep to prevent 100% CPU usage
          #  time.sleep(0.016)  # ~60 FPS

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
            if state.backdrop_effect and state.backdrop_effect.program: 
                glDeleteProgram(state.backdrop_effect.program)
            if state.backdrop_effect:
                for texture_id in state.backdrop_effect.texture_ids:
                    if texture_id: glDeleteTextures(1, [texture_id])
            if state.ipc_socket: state.ipc_socket.close()
        except Exception as e:
            log.error(f"Error during cleanup: {e}")
    
    return 0

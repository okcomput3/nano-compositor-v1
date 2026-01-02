'''
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

# ===================================================================
# GLSL SHADERS for the Left Hand Dock
# ===================================================================

# --- Panel Shaders ---
PANEL_VERTEX_SHADER = """
#version 100
precision mediump float;
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 v_texcoord;
uniform mat3 mvp;
void main() {
    vec3 pos_transformed = mvp * vec3(position, 1.0);
    gl_Position = vec4(pos_transformed.xy, 0.0, 1.0);
    v_texcoord = texcoord;
}
"""

PANEL_FRAGMENT_SHADER = """
#version 100
precision mediump float;
varying vec2 v_texcoord;
uniform float time;
uniform vec2 iResolution;
uniform vec2 panelDimensions;

// Simplex noise for oil effect
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x*34.0)+1.0)*x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
float snoise(vec3 v) {
  const vec2 C = vec2(1.0/6.0, 1.0/3.0);
  const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
  vec3 i  = floor(v + dot(v, C.yyy));
  vec3 x0 = v - i + dot(i, C.xxx);
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min(g.xyz, l.zxy);
  vec3 i2 = max(g.xyz, l.zxy);
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy;
  vec3 x3 = x0 - D.yyy;
  i = mod289(i);
  vec4 p = permute(permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x + vec4(0.0, i1.x, i2.x, 1.0));
  float n_ = 0.142857142857;
  vec3 ns = n_ * D.wyz - D.xzx;
  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_);
  vec4 x = x_ *ns.x + ns.yyyy;
  vec4 y = y_ *ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);
  vec4 b0 = vec4(x.xy, y.xy);
  vec4 b1 = vec4(x.zw, y.zw);
  vec4 s0 = floor(b0)*2.0 + 1.0;
  vec4 s1 = floor(b1)*2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));
  vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy;
  vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww;
  vec3 p0 = vec3(a0.xy,h.x);
  vec3 p1 = vec3(a0.zw,h.y);
  vec3 p2 = vec3(a1.xy,h.z);
  vec3 p3 = vec3(a1.zw,h.w);
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x; p1 *= norm.y; p2 *= norm.z; p3 *= norm.w;
  vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
  m = m * m;
  return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

vec3 Rainbow(float t) {
    return clamp(1.0 - pow(abs(vec3(t) - vec3(0.65, 0.5, 0.2)) * vec3(3.0, 3.0, 5.0), vec3(1.5)), 0.0, 1.0);
}

vec3 AssignRGB(float lambda) {
    return Rainbow(clamp((lambda - 400.0) / (600.0 - 400.0), 0.0, 1.0));
}

vec4 getOilFuelColor() {
    vec2 uv = gl_FragCoord.xy / 1000.0;
    vec3 vMCposition = vec3(uv * 8.0, time * 0.05);
    float nv = snoise(vMCposition);
    float d = 1.0 * (0.5 + 0.5 * nv);
    vec3 color = vec3(0.0);
    for (int m = 1; m <= 8; m++) {
       color += AssignRGB(900.0 * d * 1.4 / (float(m) + 0.1));
    }
    return vec4(color / 8.0, 0.3);
}

void main() {
    float radiusPixels = 40.0;
    float borderWidthPixels = 2.0;
    float smoothingFactor = 1.0;
    vec2 pixelCoord = v_texcoord * panelDimensions;
    float borderGradient = 0.0;
    float corner_mask = 1.0;

    // Check if we're in the top curved area
    if (pixelCoord.y > panelDimensions.y - radiusPixels) { 
        // Top area with curves
        vec2 corner_dist;
        if (pixelCoord.x < radiusPixels) { 
            // Top-left corner
            corner_dist = vec2(radiusPixels - pixelCoord.x, (panelDimensions.y - radiusPixels) - pixelCoord.y);
        } else if (pixelCoord.x > (panelDimensions.x - radiusPixels)) { 
            // Top-right corner
            corner_dist = vec2(pixelCoord.x - (panelDimensions.x - radiusPixels), (panelDimensions.y - radiusPixels) - pixelCoord.y);
        } else { 
            // Top edge (straight)
            float distToTop = panelDimensions.y - pixelCoord.y;
            if (distToTop < borderWidthPixels) {
                borderGradient = smoothstep(borderWidthPixels + smoothingFactor, borderWidthPixels - smoothingFactor, distToTop);
            }
            corner_dist = vec2(0.0, 0.0);
        }
        
        if(length(corner_dist) > 0.0){
            float dist_to_center = length(corner_dist);
            corner_mask = smoothstep(radiusPixels + 1.0, radiusPixels - 1.0, dist_to_center);
            float cornerBorderWidth = borderWidthPixels * 0.5;
            float innerEdge = radiusPixels - cornerBorderWidth;
            float outerEdge = radiusPixels + cornerBorderWidth;
            if (dist_to_center <= outerEdge && dist_to_center >= innerEdge - smoothingFactor) {
                borderGradient = smoothstep(outerEdge + smoothingFactor, innerEdge - smoothingFactor, dist_to_center);
            }
        }
    } 
    // Check if we're in the bottom curved area
    else if (pixelCoord.y < radiusPixels) { 
        // Bottom area with curves
        vec2 corner_dist;
        if (pixelCoord.x < radiusPixels) { 
            // Bottom-left corner
            corner_dist = vec2(radiusPixels - pixelCoord.x, radiusPixels - pixelCoord.y);
        } else if (pixelCoord.x > (panelDimensions.x - radiusPixels)) { 
            // Bottom-right corner
            corner_dist = vec2(pixelCoord.x - (panelDimensions.x - radiusPixels), radiusPixels - pixelCoord.y);
        } else { 
            // Bottom edge (straight)
            if (pixelCoord.y < borderWidthPixels) {
                borderGradient = smoothstep(borderWidthPixels + smoothingFactor, borderWidthPixels - smoothingFactor, pixelCoord.y);
            }
            corner_dist = vec2(0.0, 0.0);
        }
        
        if(length(corner_dist) > 0.0){
            float dist_to_center = length(corner_dist);
            corner_mask = smoothstep(radiusPixels + 1.0, radiusPixels - 1.0, dist_to_center);
            float cornerBorderWidth = borderWidthPixels * 0.5;
            float innerEdge = radiusPixels - cornerBorderWidth;
            float outerEdge = radiusPixels + cornerBorderWidth;
            if (dist_to_center <= outerEdge && dist_to_center >= innerEdge - smoothingFactor) {
                borderGradient = smoothstep(outerEdge + smoothingFactor, innerEdge - smoothingFactor, dist_to_center);
            }
        }
    } 
    else { 
        // Main body (between top and bottom curves)
        float distToLeft = pixelCoord.x;
        float distToRight = panelDimensions.x - pixelCoord.x;
        float minDistToEdge = min(distToLeft, distToRight);
        if (minDistToEdge < borderWidthPixels) {
            borderGradient = smoothstep(borderWidthPixels + smoothingFactor, borderWidthPixels - smoothingFactor, minDistToEdge);
        }
    }
    
    vec4 background_color = getOilFuelColor();
    background_color.a *= corner_mask;
    
    vec3 borderColor = vec3(0.0, 0.0, 0.0);
    background_color.rgb = mix(background_color.rgb, borderColor, borderGradient * corner_mask);

    gl_FragColor = vec4(background_color.b, background_color.g, background_color.r, background_color.a);
}
"""

# --- Icon Shaders ---
ICON_VERTEX_SHADER = """
#version 100
precision mediump float;
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 v_texcoord;
uniform mat3 mvp;
void main() {
    vec3 pos_transformed = mvp * vec3(position, 1.0);
    gl_Position = vec4(pos_transformed.xy, 0.0, 1.0);
    v_texcoord = texcoord;
}
"""

ICON_FLAME_FRAGMENT_SHADER = """
#version 100
precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D u_texture;
uniform float time;
uniform vec2 iResolution;

float rand(vec2 n) { return fract(sin(cos(dot(n, vec2(12.9898, 12.1414)))) * 83758.5453); }
float noise(vec2 n) {
    const vec2 d = vec2(0.0, 1.0);
    vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
    return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}
float fbm(vec2 n) {
    float total = 0.0, amplitude = 1.0;
    for (int i = 0; i < 5; i++) {
        total += noise(n) * amplitude;
        n += n * 1.7;
        amplitude *= 0.67;
    }
    return total;
}

void main() {
    vec2 uv = v_texcoord;
    vec4 tex_color = texture2D(u_texture, uv);
    if(tex_color.a < 0.9) { discard; }

    float iTime = mod(time, 100.0);
    const vec3 c1 = vec3(0.5, 0.0, 0.1);
    const vec3 c2 = vec3(0.9, 0.1, 0.0);
    const vec3 c4 = vec3(1.0, 0.9, 0.1);
    const vec3 c7 = vec3(1.0, 1.0, 1.0);
    
    vec2 speed = vec2(0.1, 0.9);
    float dist = 3.5 - sin(iTime * 0.4) / 1.89;
    
    vec2 adjusted_uv = vec2(uv.x, 1.0 - uv.y * 1.2 + 0.3);
    vec2 p = adjusted_uv * dist;
    p.x -= iTime / 1.1;
    
    float q = fbm(p - iTime * 0.3);
    float flame_shape = smoothstep(0.0, 0.3, adjusted_uv.y);
    vec2 r = vec2(fbm(p + q / 2.0 + iTime * speed.x), fbm(p + q - iTime * speed.y));
    float flame_intensity = q * flame_shape;
    float flame_intensity_smooth = smoothstep(0.1, 0.95, flame_intensity);
    
    vec3 fire_color = mix(c1, c2, fbm(p + r));
    fire_color = mix(fire_color, c4, r.x);
    fire_color = mix(fire_color, c7, pow(flame_intensity, 2.0) * 0.1);
    fire_color /= (10.0 + max(vec3(0.0), fire_color));
    
    vec3 final_color = mix(vec3(0.0), fire_color, flame_intensity_smooth);
    final_color = mix(final_color, tex_color.xyz, 0.2);
    
    if(flame_intensity_smooth < 0.1) { discard; }
    
    vec4 final_rgba = vec4(final_color * 4.0, flame_intensity_smooth);
//gl_FragColor = vec4(final_rgba.b, final_rgba.g, final_rgba.r, final_rgba.a);
    gl_FragColor = vec4(final_rgba.r, final_rgba.g, final_rgba.b, final_rgba.a);
}
"""

ICON_BEVELED_FRAGMENT_SHADER = """
#version 100
precision highp float;
varying vec2 v_texcoord;
uniform sampler2D u_texture;
uniform vec2 iResolution;
uniform float cornerRadius;
uniform vec4 bevelColor;
uniform float time;

const float bevelWidth = 12.0;
const float aa = 1.5;
const float blurRadius = 2.0;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

vec4 gaussianBlur(sampler2D tex, vec2 uv, vec2 texelSize, float radius) {
    vec4 color = vec4(0.0);
    float total = 0.0;
    float weights[9];
    weights[0]=1.0; weights[1]=2.0; weights[2]=1.0;
    weights[3]=2.0; weights[4]=4.0; weights[5]=2.0;
    weights[6]=1.0; weights[7]=2.0; weights[8]=1.0;
    
    int index = 0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize * radius;
            color += texture2D(tex, uv + offset) * weights[index];
            total += weights[index];
            index++;
        }
    }
    return color / total;
}

void main() {
    float bounce = sin(time * 3.0) * 0.03 + 1.00;
    vec2 p = (v_texcoord - 0.5) * iResolution;
    float scaledCornerRadius = cornerRadius * bounce;
    float d = sdRoundedBox(p, iResolution * 0.5 * bounce, scaledCornerRadius);
    float shape_alpha = 1.0 - smoothstep(-aa, aa, d);
    
    float bevel_intensity = smoothstep(-bevelWidth, 0.0, d) - smoothstep(0.0, aa, d);
    
    float center_distance = length(p) / (min(iResolution.x, iResolution.y) * 0.5);
    float button_height = pow(1.0 - smoothstep(0.0, 0.8, center_distance), 2.0);
    
    vec2 light_dir = normalize(vec2(-1.0, -1.0));
    float button_lighting = 0.5 + dot(normalize(p), light_dir) * 0.3 * button_height;
    
    float combined_bevel = max(bevel_intensity, button_height * 0.4);
    float angle = atan(p.y, p.x);
    float gradient_wave = sin(angle * 2.0 - time * 2.5) * 0.5 + 0.5;
    float brightness_modulator = 0.7 + pow(gradient_wave, 8.0) * 0.6;
    brightness_modulator *= button_lighting;
    
    float shimmer_wave = sin((p.x + p.y) / (iResolution.x + iResolution.y) * 8.0 + time * 4.0);
    float shimmer_intensity = smoothstep(0.6, 1.0, shimmer_wave) * 0.3 + button_height * 0.1;
    
    vec2 scaled_uv = (v_texcoord - 0.5) / bounce + 0.5;
    
    vec2 texel_size = 1.0 / iResolution;
    float blur_intensity = combined_bevel * 0.8 + 0.2;
    vec4 tex_color = gaussianBlur(u_texture, scaled_uv, texel_size, blurRadius * blur_intensity);
    
    vec3 final_bevel_color = bevelColor.rgb * brightness_modulator;
    final_bevel_color = mix(final_bevel_color, vec3(1.0, 1.0, 0.9), shimmer_intensity);
    
    vec3 final_rgb = mix(tex_color.rgb, final_bevel_color, combined_bevel * bevelColor.a);
    float final_alpha = tex_color.a * shape_alpha;
    
    vec4 final_rgba = vec4(final_rgb, final_alpha);
  //  gl_FragColor = vec4(final_rgba.b, final_rgba.g, final_rgba.r, final_rgba.a);
  gl_FragColor = vec4(final_rgba.r, final_rgba.g, final_rgba.b, final_rgba.a);
}
"""

# =========================
# Math & 2D Helpers
# =========================

def create_ortho_projection_matrix(left, right, bottom, top):
    """Creates a 3x3 orthographic projection matrix for 2D rendering."""
    sx = 2.0 / (right - left)
    sy = 2.0 / (top - bottom)
    tx = -(right + left) / (right - left)
    ty = -(top + bottom) / (top - bottom)
    # Column-major order for glUniformMatrix3fv
    return [
        sx,   0.0,  0.0,
        0.0,  sy,   0.0,
        tx,   ty,   1.0
    ]

def create_model_matrix(tx, ty, sx, sy):
    """Creates a 3x3 model matrix for 2D translation and scaling."""
    # Column-major order
    return [
        sx,   0.0,  0.0,
        0.0,  sy,   0.0,
        tx,   ty,   1.0
    ]

def multiply_matrices_3x3(a, b):
    """Multiplies two 3x3 column-major matrices."""
    c = [0.0] * 9
    c[0] = a[0]*b[0] + a[3]*b[1] + a[6]*b[2]
    c[1] = a[1]*b[0] + a[4]*b[1] + a[7]*b[2]
    c[2] = a[2]*b[0] + a[5]*b[1] + a[8]*b[2]
    c[3] = a[0]*b[3] + a[3]*b[4] + a[6]*b[5]
    c[4] = a[1]*b[3] + a[4]*b[4] + a[7]*b[5]
    c[5] = a[2]*b[3] + a[5]*b[4] + a[8]*b[5]
    c[6] = a[0]*b[6] + a[3]*b[7] + a[6]*b[8]
    c[7] = a[1]*b[6] + a[4]*b[7] + a[7]*b[8]
    c[8] = a[2]*b[6] + a[5]*b[7] + a[8]*b[8]
    return c

# =========================
# Left Dock Class
# =========================

class LeftDock:
    def __init__(self):
        # Shared geometry
        self.vao, self.vbo, self.ebo = 0, 0, 0
        
        # Panel resources
        self.panel_shader = 0
        self.panel_uniforms = {}

        # Icon resources
        self.icon_flame_shader = 0
        self.icon_flame_uniforms = {}
        self.icon_beveled_shader = 0
        self.icon_beveled_uniforms = {}
        self.icon_texture_1 = 0
        self.icon_texture_2 = 0
        self.icon_texture_3 = 0
        self.icon_texture_4 = 0   
        self.icon_texture_5 = 0
        self.icon_texture_6 = 0
        self.icon_texture_7 = 0
        self.icon_texture_8 = 0   
        self.icon_texture_9 = 0
        self.icon_texture_10 = 0
    
        
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
        self.dock_effect: Optional[LeftDock] = None

# =========================
# Core Logic
# =========================

def compile_shader(shader_type: int, source: str) -> int:
    shader = glCreateShader(shader_type)
    glShaderSource(shader, source)
    glCompileShader(shader)
    if glGetShaderiv(shader, GL_COMPILE_STATUS) != GL_TRUE:
        info_log = glGetShaderInfoLog(shader)
        logging.error(f"Shader compilation failed for type {shader_type}: {info_log.decode()}")
        glDeleteShader(shader)
        return 0
    return shader
    
def create_program(vertex_source, fragment_source, attributes):
    vs = compile_shader(GL_VERTEX_SHADER, vertex_source)
    fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source)
    if not vs or not fs: return 0
    
    program = glCreateProgram()
    glAttachShader(program, vs)
    glAttachShader(program, fs)
    
    for loc, name in attributes.items():
        glBindAttribLocation(program, loc, name)
        
    glLinkProgram(program)
    glDeleteShader(vs); glDeleteShader(fs)

    if glGetProgramiv(program, GL_LINK_STATUS) != GL_TRUE:
        logging.error(f"Program linking failed: {glGetProgramInfoLog(program).decode()}")
        return 0
    return program

def load_texture(path: str) -> int:
    try:
        logging.info(f"Loading texture from: {path}")
        img = Image.open(path).convert("RGBA")
        img_data = np.array(list(img.getdata()), np.uint8)

        texture_id = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, texture_id)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data)
        glGenerateMipmap(GL_TEXTURE_2D)
        
        logging.info(f"✅ Texture loaded successfully (ID: {texture_id})")
        return texture_id
    except FileNotFoundError:
        logging.error(f"Texture file not found: {path}. Using placeholder.")
        texture_id = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, texture_id)
        placeholder = np.array([255, 0, 255, 255], dtype=np.uint8)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, placeholder)
        return texture_id
    except Exception as e:
        logging.error(f"Failed to load texture '{path}': {e}", exc_info=True)
        return 0

def init_dock_effect(state: PluginState) -> bool:
    logging.info("Initializing Left Dock Effect...")
    try:
        effect = LeftDock()
        
        # --- Compile Shaders ---
        attributes = {0: "position", 1: "texcoord"}
        effect.panel_shader = create_program(PANEL_VERTEX_SHADER, PANEL_FRAGMENT_SHADER, attributes)
        effect.icon_flame_shader = create_program(ICON_VERTEX_SHADER, ICON_FLAME_FRAGMENT_SHADER, attributes)
        effect.icon_beveled_shader = create_program(ICON_VERTEX_SHADER, ICON_BEVELED_FRAGMENT_SHADER, attributes)
        if not all([effect.panel_shader, effect.icon_flame_shader, effect.icon_beveled_shader]):
            return False

        # --- Get Uniform Locations (Unchanged) ---
        effect.panel_uniforms['mvp'] = glGetUniformLocation(effect.panel_shader, "mvp")
        effect.panel_uniforms['time'] = glGetUniformLocation(effect.panel_shader, "time")
        effect.panel_uniforms['iResolution'] = glGetUniformLocation(effect.panel_shader, "iResolution")
        effect.panel_uniforms['panelDimensions'] = glGetUniformLocation(effect.panel_shader, "panelDimensions")

        effect.icon_flame_uniforms['mvp'] = glGetUniformLocation(effect.icon_flame_shader, "mvp")
        effect.icon_flame_uniforms['u_texture'] = glGetUniformLocation(effect.icon_flame_shader, "u_texture")
        effect.icon_flame_uniforms['time'] = glGetUniformLocation(effect.icon_flame_shader, "time")
        effect.icon_flame_uniforms['iResolution'] = glGetUniformLocation(effect.icon_flame_shader, "iResolution")
        
        effect.icon_beveled_uniforms['mvp'] = glGetUniformLocation(effect.icon_beveled_shader, "mvp")
        effect.icon_beveled_uniforms['u_texture'] = glGetUniformLocation(effect.icon_beveled_shader, "u_texture")
        effect.icon_beveled_uniforms['time'] = glGetUniformLocation(effect.icon_beveled_shader, "time")
        effect.icon_beveled_uniforms['iResolution'] = glGetUniformLocation(effect.icon_beveled_shader, "iResolution")
        effect.icon_beveled_uniforms['cornerRadius'] = glGetUniformLocation(effect.icon_beveled_shader, "cornerRadius")
        effect.icon_beveled_uniforms['bevelColor'] = glGetUniformLocation(effect.icon_beveled_shader, "bevelColor")

        # --- Load Textures (Unchanged) ---
        effect.icon_texture_1 = load_texture("icon1.png")
        effect.icon_texture_2 = load_texture("icon2.png")
        effect.icon_texture_3 = load_texture("icon3.png")
        effect.icon_texture_4 = load_texture("icon4.png")
        effect.icon_texture_5 = load_texture("icon5.png")
        effect.icon_texture_6 = load_texture("icon6.png")
        effect.icon_texture_7 = load_texture("icon7.png")
        effect.icon_texture_8 = load_texture("icon8.png")
        effect.icon_texture_9 = load_texture("icon9.png")
        effect.icon_texture_10 = load_texture("icon10.png")
        if not effect.icon_texture_1 or not effect.icon_texture_2  or not effect.icon_texture_3  or not effect.icon_texture_4  or not effect.icon_texture_5  or not effect.icon_texture_6  or not effect.icon_texture_7  or not effect.icon_texture_8  or not effect.icon_texture_9  or not effect.icon_texture_10:
            return False

        # --- Create Shared Geometry (VBO and EBO only) ---
        vertices = np.array([0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0], dtype=np.float32)
        indices = np.array([0, 1, 2, 0, 2, 3], dtype=np.uint16)
        
        # VAO calls are removed
        effect.vbo = glGenBuffers(1)
        effect.ebo = glGenBuffers(1)

        glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
        glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL_STATIC_DRAW)
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effect.ebo)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices, GL_STATIC_DRAW)
        
        # Unbind buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)
        
        state.dock_effect = effect
        logging.info("✅ Left Dock Effect initialized (GLES 2.0 compatible)")
        return True
    except Exception as e:
        logging.error(f"Dock effect initialization failed: {e}", exc_info=True)
        return False


def update_and_render_dock_effect(state: PluginState):
    effect = state.dock_effect
    if not effect: return

    time_val = float(time.time())
    
    # --- Shared Setup for GLES 2.0 (replaces VAO binding) ---
    glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effect.ebo)
    
    # Set up vertex attributes for every draw
    stride = 4 * 4 # 4 floats per vertex
    glEnableVertexAttribArray(0) # position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(0))
    glEnableVertexAttribArray(1) # texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(2 * 4))
    
    # --- Create Projection Matrix ---
    ortho_proj = create_ortho_projection_matrix(0.0, float(state.width), 0.0, float(state.height))
    
    # --- 1. Render the Dock Panel ---
    glUseProgram(effect.panel_shader)
    panel_width, panel_height = 80.0, float(state.height)
    panel_model = create_model_matrix(0.0, 0.0, panel_width, panel_height)
    panel_mvp = multiply_matrices_3x3(ortho_proj, panel_model)

    glUniformMatrix3fv(effect.panel_uniforms['mvp'], 1, GL_FALSE, panel_mvp)
    glUniform1f(effect.panel_uniforms['time'], time_val)
    glUniform2f(effect.panel_uniforms['iResolution'], float(state.width), float(state.height))
    glUniform2f(effect.panel_uniforms['panelDimensions'], panel_width, panel_height)
#    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # --- 2. Render the Icons ---
    icon_size = 64.0
    icon_padding = (panel_width - icon_size) / 2.0

    # Icon 1: Beveled/Bouncing
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_1)
    
    icon1_model = create_model_matrix(icon_padding, panel_height - 125.0, icon_size, icon_size)
    icon1_mvp = multiply_matrices_3x3(ortho_proj, icon1_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon1_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
#    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # Icon 2: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_2)
    
    icon2_model = create_model_matrix(icon_padding, panel_height - 225.0, icon_size, icon_size)
    icon2_mvp = multiply_matrices_3x3(ortho_proj, icon2_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon2_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # Icon 3: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_3)
    
    icon3_model = create_model_matrix(icon_padding, panel_height - 325.0, icon_size, icon_size)
    icon3_mvp = multiply_matrices_3x3(ortho_proj, icon3_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon3_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # Icon 4: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_4)
    
    icon4_model = create_model_matrix(icon_padding, panel_height - 425.0, icon_size, icon_size)
    icon4_mvp = multiply_matrices_3x3(ortho_proj, icon4_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon4_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # Icon 5: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_5)
    
    icon5_model = create_model_matrix(icon_padding, panel_height - 525.0, icon_size, icon_size)
    icon5_mvp = multiply_matrices_3x3(ortho_proj, icon5_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon5_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # Icon 6: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_6)
    
    icon6_model = create_model_matrix(icon_padding, panel_height - 625.0, icon_size, icon_size)
    icon6_mvp = multiply_matrices_3x3(ortho_proj, icon6_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon6_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None) 

    # Icon 7: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_7)
    
    icon7_model = create_model_matrix(icon_padding, panel_height - 725.0, icon_size, icon_size)
    icon7_mvp = multiply_matrices_3x3(ortho_proj, icon7_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon7_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # Icon 8: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_8)
    
    icon8_model = create_model_matrix(icon_padding, panel_height - 825.0, icon_size, icon_size)
    icon8_mvp = multiply_matrices_3x3(ortho_proj, icon8_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon8_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # Icon 9: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_9)
    
    icon9_model = create_model_matrix(icon_padding, panel_height - 925.0, icon_size, icon_size)
    icon9_mvp = multiply_matrices_3x3(ortho_proj, icon9_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon9_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)

    # Icon 10: Flame
    glUseProgram(effect.icon_beveled_shader)
    glActiveTexture(GL_TEXTURE0)
    glBindTexture(GL_TEXTURE_2D, effect.icon_texture_10)
    
    icon10_model = create_model_matrix(icon_padding, panel_height - 1025.0, icon_size, icon_size)
    icon10_mvp = multiply_matrices_3x3(ortho_proj, icon10_model)
    
    glUniformMatrix3fv(effect.icon_beveled_uniforms['mvp'], 1, GL_FALSE, icon10_mvp)
    glUniform1i(effect.icon_beveled_uniforms['u_texture'], 0)
    glUniform1f(effect.icon_beveled_uniforms['time'], time_val)
    glUniform2f(effect.icon_beveled_uniforms['iResolution'], icon_size, icon_size)
    glUniform1f(effect.icon_beveled_uniforms['cornerRadius'], 12.0)
    glUniform4f(effect.icon_beveled_uniforms['bevelColor'], 0.8, 0.9, 1.0, 0.7)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)









    # --- Cleanup ---
    glDisableVertexAttribArray(0)
    glDisableVertexAttribArray(1)
    glBindBuffer(GL_ARRAY_BUFFER, 0)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)
    glUseProgram(0)

def render_frame_into_buffer(state: PluginState, buffer_idx: int):
    glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[buffer_idx])
    glViewport(0, 0, state.width, state.height)
    glClearColor(0.0, 0.0, 0.0, 0.0) # Clear to transparent black
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    update_and_render_dock_effect(state)
    glFinish()
    glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
def init_fbo_for_buffer(state: PluginState, texture_id: int, index: int) -> bool:
    try:
        state.fbo_textures[index] = texture_id
        state.fbos[index] = glGenFramebuffers(1)
        glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[index])
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0)
        
        # We don't need a depth buffer for this 2D effect, but it's good practice
        # depth_renderbuffer = glGenRenderbuffers(1)
        # glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer)
        # glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, state.width, state.height)
        # glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_renderbuffer)

        if glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE:
            logging.error(f"FBO {index} is incomplete.")
            glDeleteFramebuffers(1, [state.fbos[index]])
            return False
            
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
        logging.info(f"✅ FBO {index} initialized with Texture ID {texture_id}")
        return True
    except Exception as e:
        logging.error(f"Failed to init FBO {index}: {e}")
        return False
        
# =========================
# MAIN FUNCTION (Async Loop)
# =========================
def main(ipc_fd: int, shm_name: str, egl_display: int, egl_context: int,
         fbo_texture_0: int, dmabuf_fd_0: int, width: int, height: int, stride_0: int, format_0: int,
         fbo_texture_1: int, dmabuf_fd_1: int, _w: int, _h: int, stride_1: int, format_1: int) -> int:
    
    logging.basicConfig(level=logging.INFO, format='[HOSTED-PY DOCK] %(message)s')
    log = logging.getLogger("py-main")
    log.info("=== Python Left Dock Effect Plugin RUNNING ===")

    state = PluginState(ipc_fd)
    state.width, state.height = width, height
    
    def send_frame_ready_notification(sock, buffer_idx: int):
        try:
            message = struct.pack('=Ii', 1, buffer_idx) # IPC_NOTIFICATION_TYPE_FRAME_SUBMIT = 1
            sock.sendall(message)
        except (BrokenPipeError, ConnectionResetError):
            log.warning("Compositor connection lost.")
            raise

    try:
        if not init_dock_effect(state): return 1
        if not init_fbo_for_buffer(state, fbo_texture_0, 0): return 1
        if not init_fbo_for_buffer(state, fbo_texture_1, 1): return 1
        log.info("✅ Double buffering initialized successfully.")

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
            if state.dock_effect:
                glDeleteProgram(state.dock_effect.panel_shader)
                glDeleteProgram(state.dock_effect.icon_flame_shader)
                glDeleteProgram(state.dock_effect.icon_beveled_shader)
                # Remove the glDeleteVertexArrays call
                glDeleteBuffers(1, [state.dock_effect.vbo, state.dock_effect.ebo])
                glDeleteTextures([state.dock_effect.icon_texture_1, state.dock_effect.icon_texture_2, state.dock_effect.icon_texture_3, state.dock_effect.icon_texture_4, state.dock_effect.icon_texture_5, state.dock_effect.icon_texture_6, state.dock_effect.icon_texture_7, state.dock_effect.icon_texture_8, state.dock_effect.icon_texture_9, state.dock_effect.icon_texture_10])
            if state.ipc_socket: state.ipc_socket.close()
        except Exception as e:
            log.error(f"Error during cleanup: {e}")
'''
import sys
import os
import time
import math
import struct
import socket
import select
import logging
import ctypes
from typing import Optional

# OpenGL ES bindings
from OpenGL.GLES2 import *
from OpenGL.EGL import *

import numpy as np

# ===================================================================
# GLSL SHADERS (Sparse & Varied Shapes)
# ===================================================================

SNOW_VERTEX_SHADER = """
#version 100
precision mediump float;
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 v_uv;
uniform mat3 mvp;

void main() {
    vec3 pos_transformed = mvp * vec3(position, 1.0);
    gl_Position = vec4(pos_transformed.xy, 0.0, 1.0);
    v_uv = texcoord;
}
"""

SNOW_FRAGMENT_SHADER = """
#version 100
precision highp float;
varying vec2 v_uv;
uniform float time;
uniform vec2 iResolution;

// 3-Component Hash for more random properties (Density, Shape, Rotation)
vec3 hash3(vec2 p) {
    // Modulo to prevent precision loss on mobile/embedded
    p = mod(p, vec2(100.0, 100.0));
    
    vec3 q = vec3( dot(p,vec2(127.1,311.7)), 
                   dot(p,vec2(269.5,183.3)), 
                   dot(p,vec2(419.2,371.9)) );
    return fract(sin(q)*43758.5453);
}

void main() {
    vec2 uv = v_uv;
    uv.x *= iResolution.x / iResolution.y;
    
    // --- Movement ---
    vec2 pos = uv;
    pos.y -= time * 0.12; // Fall down
    pos.x += sin(time * 0.4 + pos.y * 3.0) * 0.02; // Sway
    
    // --- Grid ---
    // Zoom 12.0 = Small but visible flakes
    float zoom = 12.0;
    vec2 p = pos * zoom;
    vec2 n = floor(p);
    vec2 f = fract(p);

    float minDist = 100.0;
    vec2 vectorToCenter = vec2(0.0);
    vec2 cellId = vec2(0.0);

    // --- Voronoi Pass ---
    for(int j=-1; j<=1; j++) {
        for(int i=-1; i<=1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec3 h = hash3(n + g);
            
            // Animate center
            vec2 o = 0.5 + 0.35 * sin(time * 1.5 + 6.28 * h.xy);
            
            vec2 r = g + o - f;
            float d = dot(r, r);

            if(d < minDist) {
                minDist = d;
                vectorToCenter = r;
                cellId = n + g;
            }
        }
    }

    // --- Snowflake Logic ---
    
    // 1. DENSITY CHECK ("Twice as less")
    // Get unique random values for this specific snowflake
    vec3 rnd = hash3(cellId);
    
    // If rnd.x is > 0.4, we don't draw it. 
    // This makes the snow sparse.
    if(rnd.x > 0.4) {
        gl_FragColor = vec4(0.0); // Transparent
        return;
    }

    // 2. SHAPE GENERATION ("Other shapes")
    float dist = length(vectorToCenter);
    float angle = atan(vectorToCenter.y, vectorToCenter.x);
    
    // Rotation
    float spinSpeed = (rnd.y - 0.5) * 6.0;
    angle += time * spinSpeed;
    
    // Petal Math (Similar to original shader)
    // Randomize petal count between 5 and 9
    float numPetals = floor(mix(5.0, 9.0, rnd.z));
    float petalVal = 0.5 + 0.5 * sin(angle * numPetals);
    petalVal = pow(petalVal, 3.0); // Make petals thinner
    
    // Ring Math (Concentric circles)
    // Create ring pattern based on distance
    float numRings = floor(mix(2.0, 5.0, rnd.x));
    float ringVal = 0.5 + 0.5 * sin(dist * 20.0 * numRings);
    ringVal = pow(ringVal, 2.0);
    
    // Mix Petals and Rings based on random seed
    // Some flakes look like flowers, some like targets/gears
    float shape = mix(petalVal, ringVal, 0.3 + 0.4 * rnd.y);
    
    // 3. Masking (Size limit)
    // Randomize size slightly
    float size = 0.3 + 0.1 * rnd.z;
    float mask = smoothstep(size, size - 0.1, dist);
    
    // 4. Final Alpha
    float alpha = shape * mask;
    
    // Boost visibility
    alpha = smoothstep(0.05, 0.6, alpha);

    gl_FragColor = vec4(1.0, 1.0, 1.0, alpha);
}
"""

# =========================
# Helpers
# =========================

def create_ortho_projection_matrix(left, right, bottom, top):
    sx = 2.0 / (right - left)
    sy = 2.0 / (top - bottom)
    tx = -(right + left) / (right - left)
    ty = -(top + bottom) / (top - bottom)
    return [sx, 0.0, 0.0, 0.0, sy, 0.0, tx, ty, 1.0]

def create_model_matrix(tx, ty, sx, sy):
    return [sx, 0.0, 0.0, 0.0, sy, 0.0, tx, ty, 1.0]

def multiply_matrices_3x3(a, b):
    c = [0.0] * 9
    c[0] = a[0]*b[0] + a[3]*b[1] + a[6]*b[2]
    c[1] = a[1]*b[0] + a[4]*b[1] + a[7]*b[2]
    c[2] = a[2]*b[0] + a[5]*b[1] + a[8]*b[2]
    c[3] = a[0]*b[3] + a[3]*b[4] + a[6]*b[5]
    c[4] = a[1]*b[3] + a[4]*b[4] + a[7]*b[5]
    c[5] = a[2]*b[3] + a[5]*b[4] + a[8]*b[5]
    c[6] = a[0]*b[6] + a[3]*b[7] + a[6]*b[8]
    c[7] = a[1]*b[6] + a[4]*b[7] + a[7]*b[8]
    c[8] = a[2]*b[6] + a[5]*b[7] + a[8]*b[8]
    return c

# =========================
# Plugin Classes
# =========================

class SnowEffect:
    def __init__(self):
        self.program = 0
        self.vbo = 0
        self.ebo = 0
        self.uniforms = {}

class PluginState:
    def __init__(self, ipc_fd):
        self.ipc_fd = ipc_fd
        self.ipc_socket = socket.fromfd(ipc_fd, socket.AF_UNIX, socket.SOCK_STREAM)
        self.width, self.height = 1920, 1080
        self.fbos = [0, 0]
        self.fbo_textures = [0, 0]
        self.current_buffer_idx = 0
        self.effect: Optional[SnowEffect] = None

# =========================
# Core Functions
# =========================

def compile_shader(shader_type, source):
    shader = glCreateShader(shader_type)
    glShaderSource(shader, source)
    glCompileShader(shader)
    if glGetShaderiv(shader, GL_COMPILE_STATUS) != GL_TRUE:
        logging.error(f"Shader Error ({shader_type}): {glGetShaderInfoLog(shader).decode()}")
        glDeleteShader(shader)
        return 0
    return shader

def create_program(vs_src, fs_src):
    vs = compile_shader(GL_VERTEX_SHADER, vs_src)
    fs = compile_shader(GL_FRAGMENT_SHADER, fs_src)
    if not vs or not fs: return 0
    
    prog = glCreateProgram()
    glAttachShader(prog, vs)
    glAttachShader(prog, fs)
    glBindAttribLocation(prog, 0, "position")
    glBindAttribLocation(prog, 1, "texcoord")
    glLinkProgram(prog)
    glDeleteShader(vs)
    glDeleteShader(fs)
    
    if glGetProgramiv(prog, GL_LINK_STATUS) != GL_TRUE:
        logging.error(f"Link Error: {glGetProgramInfoLog(prog).decode()}")
        return 0
    return prog

def init_effect(state: PluginState) -> bool:
    logging.info("Initializing Sparse Varied Snow...")
    eff = SnowEffect()
    
    eff.program = create_program(SNOW_VERTEX_SHADER, SNOW_FRAGMENT_SHADER)
    if not eff.program: return False
    
    eff.uniforms['mvp'] = glGetUniformLocation(eff.program, "mvp")
    eff.uniforms['time'] = glGetUniformLocation(eff.program, "time")
    eff.uniforms['iResolution'] = glGetUniformLocation(eff.program, "iResolution")
    
    vertices = np.array([
        0.0, 1.0, 0.0, 1.0,
        1.0, 1.0, 1.0, 1.0,
        1.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    ], dtype=np.float32)
    
    indices = np.array([0, 1, 2, 0, 2, 3], dtype=np.uint16)
    
    eff.vbo = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, eff.vbo)
    glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL_STATIC_DRAW)
    
    eff.ebo = glGenBuffers(1)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eff.ebo)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices, GL_STATIC_DRAW)
    
    glBindBuffer(GL_ARRAY_BUFFER, 0)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)
    
    state.effect = eff
    return True

def render_frame(state: PluginState, buffer_idx: int):
    glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[buffer_idx])
    glViewport(0, 0, state.width, state.height)
    
    # Transparent Background
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glClear(GL_COLOR_BUFFER_BIT)
    
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    
    eff = state.effect
    if eff and eff.program:
        glUseProgram(eff.program)
        glBindBuffer(GL_ARRAY_BUFFER, eff.vbo)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eff.ebo)
        
        stride = 16
        glEnableVertexAttribArray(0)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(0))
        glEnableVertexAttribArray(1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(8))
        
        proj = create_ortho_projection_matrix(0.0, float(state.width), 0.0, float(state.height))
        model = create_model_matrix(0.0, 0.0, float(state.width), float(state.height))
        mvp = multiply_matrices_3x3(proj, model)
        
        glUniformMatrix3fv(eff.uniforms['mvp'], 1, GL_FALSE, mvp)
        glUniform1f(eff.uniforms['time'], time.time() % 3600.0)
        glUniform2f(eff.uniforms['iResolution'], float(state.width), float(state.height))
        
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, None)
        
        glDisableVertexAttribArray(0)
        glDisableVertexAttribArray(1)
        
    glFinish()
    glBindFramebuffer(GL_FRAMEBUFFER, 0)

def init_fbo(state: PluginState, tex_id: int, idx: int) -> bool:
    try:
        state.fbo_textures[idx] = tex_id
        state.fbos[idx] = glGenFramebuffers(1)
        glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[idx])
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0)
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER)
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
        return status == GL_FRAMEBUFFER_COMPLETE
    except:
        return False

# =========================
# Main
# =========================
def main(ipc_fd: int, shm_name: str, egl_display: int, egl_context: int,
         fbo_texture_0: int, dmabuf_fd_0: int, width: int, height: int, stride_0: int, format_0: int,
         fbo_texture_1: int, dmabuf_fd_1: int, _w: int, _h: int, stride_1: int, format_1: int) -> int:
    
    logging.basicConfig(level=logging.INFO, format='[SPARSE-SNOW] %(message)s')
    state = PluginState(ipc_fd)
    state.width, state.height = width, height

    if not init_effect(state): return 1
    if not init_fbo(state, fbo_texture_0, 0): return 1
    if not init_fbo(state, fbo_texture_1, 1): return 1

    logging.info("Snow loop running...")

    try:
        while True:
            ready, _, _ = select.select([state.ipc_socket], [], [], 0)
            if ready:
                if not state.ipc_socket.recv(4096): break
            
            idx = state.current_buffer_idx
            render_frame(state, idx)
            state.ipc_socket.sendall(struct.pack('=Ii', 1, idx))
            state.current_buffer_idx = (idx + 1) % 2
            time.sleep(0.01)

    except KeyboardInterrupt:
        pass
    except Exception as e:
        logging.error(f"Error: {e}")
        return 1
    finally:
        state.ipc_socket.close()
        
    return 0

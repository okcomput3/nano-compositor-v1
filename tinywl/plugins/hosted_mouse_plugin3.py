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
import random

# --- Audio Processing Imports ---
import pyaudio
import numpy as np
import collections

# OpenGL ES bindings
from OpenGL.GLES2 import *
from OpenGL.EGL import *

# Image loading (though not used in this effect, it's good practice to keep)
from PIL import Image

# =========================
# IPC enums (match nano_ipc.h)
# =========================
IPC_EVENT_TYPE_POINTER_MOTION = 0
IPC_EVENT_TYPE_COMPOSITOR_SHUTDOWN = 31
IPC_NOTIFICATION_TYPE_FRAME_SUBMIT = 1

# =========================
# GLSL SHADERS for 3D Spectrometer Effect
# =========================

SPECTROMETER_VERTEX_SHADER = """
#version 100
precision mediump float;

attribute vec4 a_position;
attribute vec3 a_color;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_model;

varying vec3 v_color;
varying vec3 v_worldPos;

void main() {
    v_worldPos = a_position.xyz;
    v_color = a_color;
    gl_Position = u_projection * u_view * u_model * a_position;
}
"""

SPECTROMETER_FRAGMENT_SHADER = """
#version 100
#extension GL_OES_standard_derivatives : enable
precision mediump float;

varying vec3 v_color;
varying vec3 v_worldPos;
uniform float time;

void main() {
    float pulse = sin(time * 3.0) * 0.2 + 0.8;
    
    // Increase brightness and contrast
    vec3 brightColor = v_color * 1.5; // Make brighter
    brightColor = pow(brightColor, vec3(0.7)); // Increase contrast
    
    // Apply pulse but don't dim based on height
    vec3 finalColor = brightColor * pulse;
    
    // Add glow effect for upper portions only (optional enhancement)
    // But don't use this to dim the base color
    if (v_worldPos.y > 0.0) {
        float glowIntensity = min(v_worldPos.y / 1.0, 1.0); // Normalize to max bar height
        finalColor += vec3(0.3, 0.2, 0.1) * glowIntensity * 0.5;
    }
    
    gl_FragColor = vec4(finalColor, 1.0);
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

def create_rotation_matrix_y(angle_rad):
    cos_a = math.cos(angle_rad)
    sin_a = math.sin(angle_rad)
    return [
        cos_a,  0.0, sin_a,  0.0, 0.0, 1.0, 0.0, 0.0,
        -sin_a, 0.0, cos_a,  0.0, 0.0, 0.0, 0.0, 1.0
    ]

def create_translation_matrix(x, y, z):
    return [
        1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0, x,   y,   z,   1.0
    ]

def multiply_matrices(a, b):
    result = [0.0] * 16
    for i in range(4):
        for j in range(4):
            for k in range(4):
                result[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j]
    return result

def hsv_to_rgb(h, s, v):
    h = h % 360
    c = v * s
    x = c * (1 - abs((h / 60) % 2 - 1))
    m = v - c
    if h < 60: r, g, b = c, x, 0
    elif h < 120: r, g, b = x, c, 0
    elif h < 180: r, g, b = 0, c, x
    elif h < 240: r, g, b = 0, x, c
    elif h < 300: r, g, b = x, 0, c
    else: r, g, b = c, 0, x
    return [r + m, g + m, b + m]

# =========================
# ENHANCED: AudioProcessor Class
# =========================

class AudioProcessor:
    """
    Enhanced audio processor with better frequency analysis and silence detection.
    """
    def __init__(self, num_bars):
        """
        Enhanced audio processor with better frequency analysis and silence detection.
        Modified to capture system audio (speakers) instead of microphone.
        """
        self.num_bars = num_bars
        self.sample_rate = 44100
        self.chunk_size = 4096  # Increased for better frequency resolution
        self.min_freq = 20      # Lower minimum for bass
        self.max_freq = 20000   # Higher maximum for treble

        # Audio data storage
        self.fft_data_deque = collections.deque(maxlen=1)  # More smoothing
        self.raw_audio_deque = collections.deque(maxlen=10)
        
        # Frequency band calculations
        self.bar_indices = self._calculate_log_indices()
        
        # Silence detection and thresholds
        self.silence_threshold = 0.01 # RMS threshold for silence detection
        self.silence_counter = 0
        self.max_silence_frames = 30
        
        # Response thresholds to prevent overreaction
        self.minimum_response_threshold = 0.05  # Must exceed this to trigger response
        self.maximum_sensitivity = 0.7  # Cap the maximum response
        self.attack_threshold = 0.1  # How much change needed to start growing
        self.release_threshold = 0.05  # How much change needed to start shrinking
        
        # Dynamic range adjustment
        self.noise_floor = np.ones(num_bars) * 0.02
        self.peak_values = np.ones(num_bars) * 1.0
        self.decay_factor = 0.98
        
        # Noise gate - ignore very small signals
        self.noise_gate_threshold = 0.1  # Increased to be less sensitive
        
        # Smoothing for threshold detection
        self.threshold_history = collections.deque(maxlen=5)
        
        # Initialize PyAudio
        self.p = pyaudio.PyAudio()
        
        # First, log all available devices for debugging
        logging.info("Available audio devices:")
        for i in range(self.p.get_device_count()):
            device_info = self.p.get_device_info_by_index(i)
            host_api_info = self.p.get_host_api_info_by_index(device_info['hostApi'])
            logging.info(f"  {i}: {device_info['name']} (inputs: {device_info['maxInputChannels']}, API: {host_api_info['name']})")
        
        # Check for PipeWire and try to find monitor sources
        loopback_device_index = None
        has_pipewire = False
        
        # First check if we have PipeWire
        for i in range(self.p.get_device_count()):
            device_info = self.p.get_device_info_by_index(i)
            if 'pipewire' in device_info['name'].lower():
                has_pipewire = True
                break
        
        if has_pipewire:
            logging.info("🔧 Detected PipeWire system - checking for monitor sources...")
            
            # For PipeWire, try to query available sources using pw-cli or wpctl
            try:
                import subprocess
                
                # Try wpctl (WirePlumber control) first - more modern
                result = subprocess.run(['wpctl', 'status'], capture_output=True, text=True)
                if result.returncode == 0:
                    logging.info("PipeWire sources:")
                    for line in result.stdout.split('\n'):
                        if 'Monitor' in line or 'monitor' in line:
                            logging.info(f"  Found monitor: {line.strip()}")
                
                # Try pw-cli as fallback
                result = subprocess.run(['pw-cli', 'list-objects'], capture_output=True, text=True)
                if result.returncode == 0:
                    sources = []
                    current_object = {}
                    
                    for line in result.stdout.split('\n'):
                        line = line.strip()
                        if line.startswith('id'):
                            if current_object and current_object.get('class') == 'PipeWire:Interface:Node':
                                if 'monitor' in current_object.get('name', '').lower():
                                    sources.append(current_object)
                            current_object = {'id': line.split()[1].replace(',', '')}
                        elif 'node.name' in line:
                            current_object['name'] = line.split('"')[1] if '"' in line else ''
                        elif 'media.class' in line and 'Source' in line:
                            current_object['class'] = 'PipeWire:Interface:Node'
                    
                    if sources:
                        logging.info("Found PipeWire monitor sources:")
                        for src in sources:
                            logging.info(f"  {src.get('name', 'Unknown')}")
                            
            except Exception as e:
                logging.warning(f"Could not query PipeWire sources: {e}")
            
            # Try the pipewire device directly - it might give us access to monitors
            for i in range(self.p.get_device_count()):
                device_info = self.p.get_device_info_by_index(i)
                if 'pipewire' in device_info['name'].lower() and device_info['maxInputChannels'] > 0:
                    logging.info(f"🔊 Trying PipeWire device: {device_info['name']}")
                    
                    try:
                        test_stream = self.p.open(
                            format=pyaudio.paFloat32,
                            channels=1,
                            rate=self.sample_rate,
                            input=True,
                            input_device_index=i,
                            frames_per_buffer=self.chunk_size
                        )
                        test_stream.close()
                        
                        loopback_device_index = i
                        logging.info(f"✅ Successfully opened PipeWire device")
                        break
                        
                    except Exception as e:
                        logging.warning(f"Could not open PipeWire device: {e}")
        
        # If PipeWire didn't work, try traditional monitor device detection
        if loopback_device_index is None:
            for i in range(self.p.get_device_count()):
                device_info = self.p.get_device_info_by_index(i)
                device_name = device_info['name'].lower()
                
                # Look for monitor devices, loopback devices, or other system audio sources
                if (device_info['maxInputChannels'] > 0 and 
                    ('monitor' in device_name or 
                     '.monitor' in device_name or
                     'loopback' in device_name or
                     'what u hear' in device_name or
                     'stereo mix' in device_name)):
                    
                    logging.info(f"🔊 Found potential monitor device: {device_info['name']}")
                    
                    try:
                        test_stream = self.p.open(
                            format=pyaudio.paFloat32,
                            channels=1,
                            rate=self.sample_rate,
                            input=True,
                            input_device_index=i,
                            frames_per_buffer=self.chunk_size
                        )
                        test_stream.close()
                        
                        loopback_device_index = i
                        logging.info(f"✅ Successfully tested monitor device: {device_info['name']}")
                        break
                        
                    except Exception as e:
                        logging.warning(f"Could not open device {device_info['name']}: {e}")
                        continue
        
        # If no monitor device found, provide PipeWire-specific guidance
        if loopback_device_index is None:
            logging.warning("⚠️ No existing monitor/loopback audio device found!")
            
            if has_pipewire:
                logging.warning("For PipeWire systems, you can:")
                logging.warning("1. Use 'pwvucontrol' or 'helvum' to visually route audio")
                logging.warning("2. Create a virtual sink: pw-cli create-object adapter '{ factory.name=support.null-audio-sink node.name=visualizer }'")
                logging.warning("3. Check available sources with: wpctl status")
            else:
                logging.warning("For ALSA/PulseAudio systems, you can:")
                logging.warning("1. Use pavucontrol to redirect audio")
                logging.warning("2. Enable ALSA loopback: sudo modprobe snd-aloop")
            
            logging.warning("Available input devices:")
            for i in range(self.p.get_device_count()):
                device_info = self.p.get_device_info_by_index(i)
                if device_info['maxInputChannels'] > 0:
                    logging.warning(f"    {i}: {device_info['name']}")
            
            logging.warning("⚠️ Falling back to default input device (likely microphone)")
            
            try:
                default_device = self.p.get_default_input_device_info()
                loopback_device_index = default_device['index']
            except:
                # Use the first available input device
                for i in range(self.p.get_device_count()):
                    device_info = self.p.get_device_info_by_index(i)
                    if device_info['maxInputChannels'] > 0:
                        loopback_device_index = i
                        break
        
        # Open the selected audio stream
        try:
            self.stream = self.p.open(
                format=pyaudio.paFloat32,
                channels=1,
                rate=self.sample_rate,
                input=True,
                input_device_index=loopback_device_index,
                frames_per_buffer=self.chunk_size,
                stream_callback=self._audio_callback
            )
            
            device_name = self.p.get_device_info_by_index(loopback_device_index)['name']
            if 'monitor' in device_name.lower() or 'loopback' in device_name.lower():
                logging.info(f"🎵 Successfully capturing system audio from: {device_name}")
            else:
                logging.warning(f"🎤 Using microphone input: {device_name}")
                
        except Exception as e:
            logging.error(f"Failed to initialize audio stream: {e}")
            raise

    def _calculate_log_indices(self):
        """Calculate logarithmically spaced frequency band indices with better distribution."""
        freqs = np.fft.rfftfreq(self.chunk_size, 1.0 / self.sample_rate)
        max_freq_idx = len(freqs) - 1
        
        # Ensure we have valid frequency range
        actual_max_freq = min(self.max_freq, freqs[-1])
        actual_min_freq = max(self.min_freq, freqs[1])  # Skip DC component
        
        logging.info(f"Frequency range: {actual_min_freq:.1f}Hz - {actual_max_freq:.1f}Hz")
        logging.info(f"FFT bins: {len(freqs)}, Sample rate: {self.sample_rate}")
        
        # Create logarithmic frequency bands with proper spacing
        min_log = np.log10(actual_min_freq)
        max_log = np.log10(actual_max_freq)
        
        # Generate logarithmically spaced frequencies
        log_freqs = np.logspace(min_log, max_log, self.num_bars + 1)
        
        # Find corresponding indices in FFT array with bounds checking
        indices = []
        for i, f in enumerate(log_freqs):
            # Find closest frequency bin
            idx = np.argmin(np.abs(freqs - f))
            # Ensure we don't exceed array bounds
            idx = min(idx, max_freq_idx)
            indices.append(idx)
            
            if i < 5 or i >= len(log_freqs) - 5:  # Log first and last few bands
                logging.info(f"Band {i}: {f:.1f}Hz -> FFT bin {idx} ({freqs[idx]:.1f}Hz)")
        
        # Ensure indices are monotonically increasing and have minimum width
        indices = np.array(indices, dtype=int)
        for i in range(1, len(indices)):
            if indices[i] <= indices[i-1]:
                indices[i] = indices[i-1] + 1
        
        # Ensure last index doesn't exceed array bounds
        indices[-1] = min(indices[-1], max_freq_idx)
        
        logging.info(f"Final frequency bands: {len(indices)-1} bands covering bins {indices[0]} to {indices[-1]}")
        return indices

    def _audio_callback(self, in_data, frame_count, time_info, status):
        """Enhanced audio callback with threshold-based response control."""
        try:
            # Convert audio data
            audio_data = np.frombuffer(in_data, dtype=np.float32)
            
            # Store raw audio for silence detection
            self.raw_audio_deque.append(audio_data)
            
            # Check for actual silence first
            audio_rms = np.sqrt(np.mean(audio_data ** 2))
            if audio_rms < self.silence_threshold:
                # Return baseline values during silence
                self.fft_data_deque.append(np.zeros(self.num_bars))
                return (None, pyaudio.paContinue)
            
            # Apply window function for better FFT
            window = np.hanning(len(audio_data))
            windowed_data = audio_data * window
            
            # Perform FFT
            fft_raw = np.fft.rfft(windowed_data)
            fft_magnitude = np.abs(fft_raw)
            
            # Calculate bar values for each frequency band
            bar_values = np.zeros(self.num_bars)
            for i in range(self.num_bars):
                start_idx = self.bar_indices[i]
                end_idx = self.bar_indices[i + 1] if i < self.num_bars - 1 else len(fft_magnitude)
                
                if start_idx < end_idx:
                    # Use max value for more responsive visualization
                    band_data = fft_magnitude[start_idx:end_idx]
                    bar_values[i] = np.max(band_data)
            
            # Apply stronger noise gate to prevent overreaction
            bar_values = np.where(bar_values < self.noise_gate_threshold, 0.0, bar_values)
            
            # Apply logarithmic scaling with reduced sensitivity
            bar_values = np.log1p(bar_values * 30)  # Reduced from 50
            
            # Normalize with threshold-based response
            max_expected = 6.0  # Reduced maximum for less sensitivity
            normalized_values = np.clip(bar_values / max_expected, 0.0, 1.0)
            
            # Apply minimum response threshold - values below this become zero
            normalized_values = np.where(
                normalized_values < self.minimum_response_threshold, 
                0.0, 
                normalized_values
            )
            
            # Scale remaining values and apply maximum sensitivity cap
            if np.any(normalized_values > 0):
                # Re-scale the remaining values to use full range
                normalized_values = np.where(
                    normalized_values > 0,
                    ((normalized_values - self.minimum_response_threshold) / 
                     (1.0 - self.minimum_response_threshold)) * self.maximum_sensitivity,
                    0.0
                )
            
            self.fft_data_deque.append(normalized_values)
            
        except Exception as e:
            logging.error(f"Audio callback error: {e}")
            # Return zero data on error
            self.fft_data_deque.append(np.zeros(self.num_bars))
        
        return (None, pyaudio.paContinue)

    def get_frequencies(self) -> Optional[np.ndarray]:
        """Get processed frequency data with advanced threshold control."""
        if not self.fft_data_deque:
            return np.zeros(self.num_bars)
        
        # Calculate smoothed average with more smoothing for stability
        smoothed_data = np.mean(list(self.fft_data_deque), axis=0)
        
        # Store in threshold history for trend analysis
        self.threshold_history.append(np.mean(smoothed_data))
        
        # Detect silence more accurately
        is_silent = self._is_silent()
        
        if is_silent:
            self.silence_counter += 1
            # During silence, return zeros quickly
            if self.silence_counter > 3:  # Very quick silence response
                return np.zeros(self.num_bars)
        else:
            self.silence_counter = 0
        
        # Apply attack/release thresholds for smoother response
        if len(self.threshold_history) >= 3:
            recent_avg = np.mean(list(self.threshold_history)[-3:])
            
            # If overall energy is below attack threshold, gradually reduce response
            if recent_avg < self.attack_threshold:
                smoothed_data *= 0.3  # Reduce sensitivity during quiet periods
            
            # Cap maximum response to prevent overwhelming visuals
            smoothed_data = np.minimum(smoothed_data, self.maximum_sensitivity)
        
        return smoothed_data

    def _is_silent(self) -> bool:
        """Improved silence detection with threshold hysteresis."""
        if not self.raw_audio_deque:
            return True
        
        # Check RMS level of recent audio with more samples for stability
        recent_audio = np.concatenate(list(self.raw_audio_deque)[-5:])  # More history
        rms = np.sqrt(np.mean(recent_audio ** 2))
        
        # Use hysteresis - different thresholds for entering/leaving silence
        if hasattr(self, '_was_silent'):
            if self._was_silent:
                # Higher threshold to exit silence (prevent flickering)
                threshold = self.silence_threshold * 1.5
            else:
                # Lower threshold to enter silence
                threshold = self.silence_threshold * 0.8
        else:
            threshold = self.silence_threshold
        
        is_silent = rms < threshold
        self._was_silent = is_silent
        return is_silent

    def stop(self):
        """Clean shutdown of audio processing."""
        try:
            if hasattr(self, 'stream') and self.stream.is_active():
                self.stream.stop_stream()
            if hasattr(self, 'stream'):
                self.stream.close()
            if hasattr(self, 'p'):
                self.p.terminate()
            logging.info("🎤 Enhanced PyAudio stream stopped and terminated.")
        except Exception as e:
            logging.error(f"Error stopping audio processor: {e}")

# =========================
# ENHANCED: SpectrometerEffect Class
# =========================

class SpectrometerEffect:
    def __init__(self, width, height):
        self.num_bars = 64  # More bars for better resolution
        self.animation_start_time = time.time()
        
        self.pos_x, self.pos_y, self.pos_z, self.rotation_y = 0.0, 0.0, 0.0, 0.0
        
        self.bar_width = 0.04   # Slightly thinner bars
        self.bar_spacing = 0.10 # Closer spacing
        self.max_height = 2.0   # Taller maximum height
        self.min_height = 0.08  # Consistent minimum height for all bars
        
        # Enhanced animation system with threshold-aware parameters
        self.frequencies = np.full(self.num_bars, self.min_height, dtype=np.float32)
        self.target_frequencies = np.full(self.num_bars, self.min_height, dtype=np.float32)
        self.frequency_velocities = np.zeros(self.num_bars, dtype=np.float32)
        
        # Animation parameters - more controlled response
        self.spring_strength = 1600.0  # Slightly reduced for less jumpy movement
        self.damping = 0.82         # More damping for smoother animation
        self.velocity_limit = 6.0   # Limit velocity to prevent sudden jumps
        
        # OpenGL resources
        self.program, self.vbo, self.ibo = 0, 0, 0
        self.index_count = 0
        
        self.uniform_projection, self.uniform_view, self.uniform_model, self.uniform_time = -1, -1, -1, -1

    def set_audio_data(self, audio_data: np.ndarray):
        """Set target heights from audio input."""
        if len(audio_data) == self.num_bars:
            # If all values are zero (silence), set all bars to minimum height
            if np.all(audio_data == 0):
                self.target_frequencies.fill(self.min_height)
            else:
                # Scale audio data to height range
                self.target_frequencies = self.min_height + audio_data * (self.max_height - self.min_height)

    def update_frequencies(self, dt):
        """Enhanced frequency animation with velocity limiting and threshold control."""
        # Spring physics for smooth, responsive animation
        for i in range(self.num_bars):
            # Calculate spring force towards target
            spring_force = (self.target_frequencies[i] - self.frequencies[i]) * self.spring_strength
            
            # Apply force to velocity with limiting
            self.frequency_velocities[i] += spring_force * dt
            
            # Limit velocity to prevent sudden jumps
            self.frequency_velocities[i] = np.clip(
                self.frequency_velocities[i], 
                -self.velocity_limit, 
                self.velocity_limit
            )
            
            # Apply damping
            self.frequency_velocities[i] *= self.damping
            
            # Update position
            self.frequencies[i] += self.frequency_velocities[i] * dt
            
            # Ensure bounds with smoother clamping
            self.frequencies[i] = max(self.min_height, min(self.max_height, self.frequencies[i]))
            
            # Additional smoothing near minimum height to prevent jitter
            if self.frequencies[i] < self.min_height * 1.2:
                self.frequency_velocities[i] *= 0.7  # Extra damping near bottom

    def update_position(self, pos_x=None, pos_y=None, pos_z=None, rotation_y=None):
        if pos_x is not None: self.pos_x = pos_x
        if pos_y is not None: self.pos_y = pos_y
        if pos_z is not None: self.pos_z = pos_z
        if rotation_y is not None: self.rotation_y = rotation_y

    def create_bar_geometry(self, bar_index, height_factor):
        """Create geometry for a single bar with consistent coloring."""
        x_pos = (bar_index - self.num_bars/2) * self.bar_spacing
        hw = self.bar_width / 2
        hh = height_factor / 2  # height_factor is already scaled
        hd = self.bar_width / 2
        
        # Calculate intensity based on height relative to minimum
        intensity = max(0.0, (height_factor - self.min_height) / (self.max_height - self.min_height))
        
        # Color mapping based on frequency band
        if bar_index < self.num_bars * 0.25:  # Bass - Red
            base_hue = 0
        elif bar_index < self.num_bars * 0.5:  # Low-mid - Orange/Yellow  
            base_hue = 30
        elif bar_index < self.num_bars * 0.75:  # High-mid - Green
            base_hue = 120
        else:  # Treble - Blue
            base_hue = 240
        
        # During silence or very low intensity, use a dim neutral color
        if intensity < 0.05:
            color = [0.1, 0.1, 0.15]  # Dark blue-gray for all bars when silent
        else:
            saturation = 0.7 + intensity * 0.3
            value = 0.4 + intensity * 0.6
            color = hsv_to_rgb(base_hue, saturation, value)
        
        # Generate cube vertices
        vertices = []
        positions = [
            [x_pos - hw, -hh, -hd], [x_pos + hw, -hh, -hd], [x_pos + hw, -hh, hd], [x_pos - hw, -hh, hd],
            [x_pos - hw, hh, -hd], [x_pos + hw, hh, -hd], [x_pos + hw, hh, hd], [x_pos - hw, hh, hd]
        ]
        
        for pos in positions:
            vertices.extend(pos + color)
        
        return vertices

    def generate_geometry(self):
        """Generate all bar geometry."""
        all_vertices, all_indices = [], []
        
        for i in range(self.num_bars):
            vertices = self.create_bar_geometry(i, self.frequencies[i])
            all_vertices.extend(vertices)
            
            # Cube indices for each bar
            base_idx = i * 8
            indices = [
                # Bottom face
                0,1,2, 0,2,3,
                # Top face
                4,6,5, 4,7,6,
                # Front face
                3,2,6, 3,6,7,
                # Back face
                1,0,4, 1,4,5,
                # Right face
                2,1,5, 2,5,6,
                # Left face
                0,3,7, 0,7,4
            ]
            
            all_indices.extend([idx + base_idx for idx in indices])
        
        return all_vertices, all_indices

# =========================
# Main Plugin State (unchanged)
# =========================
class PluginState:
    def __init__(self, ipc_fd):
        self.ipc_fd = ipc_fd
        self.ipc_socket = socket.fromfd(ipc_fd, socket.AF_UNIX, socket.SOCK_STREAM)
        self.width, self.height = 1920, 1080
        self.fbos = [0, 0]
        self.fbo_textures = [0, 0]
        self.current_buffer_idx = 0
        self.spectrometer_effect: Optional[SpectrometerEffect] = None
        self.audio_processor: Optional[AudioProcessor] = None
        self.last_frame_time = time.time()

# =========================
# Core Logic (with minor enhancements)
# =========================

def compile_shader(shader_type: int, source: str) -> int:
    shader = glCreateShader(shader_type)
    glShaderSource(shader, source)
    glCompileShader(shader)
    if glGetShaderiv(shader, GL_COMPILE_STATUS) != GL_TRUE:
        logging.error(f"Shader compilation failed: {glGetShaderInfoLog(shader).decode()}")
        glDeleteShader(shader)
        return 0
    return shader

def init_spectrometer_effect(state: PluginState) -> bool:
    logging.info("Initializing Enhanced 3D Spectrometer Effect...")
    try:
        effect = SpectrometerEffect(state.width, state.height)
        vs = compile_shader(GL_VERTEX_SHADER, SPECTROMETER_VERTEX_SHADER)
        fs = compile_shader(GL_FRAGMENT_SHADER, SPECTROMETER_FRAGMENT_SHADER)
        if not vs or not fs: return False

        program = glCreateProgram()
        glAttachShader(program, vs); glAttachShader(program, fs)
        glBindAttribLocation(program, 0, "a_position"); glBindAttribLocation(program, 1, "a_color")
        glLinkProgram(program)
        glDeleteShader(vs); glDeleteShader(fs)

        if glGetProgramiv(program, GL_LINK_STATUS) != GL_TRUE:
            logging.error(f"Program linking failed: {glGetProgramInfoLog(program).decode()}")
            return False
        
        effect.program = program
        effect.uniform_projection = glGetUniformLocation(program, "u_projection")
        effect.uniform_view = glGetUniformLocation(program, "u_view")
        effect.uniform_model = glGetUniformLocation(program, "u_model")
        effect.uniform_time = glGetUniformLocation(program, "u_time")

        vertices, indices = effect.generate_geometry()
        effect.index_count = len(indices)
        
        effect.vbo = glGenBuffers(1)
        glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
        glBufferData(GL_ARRAY_BUFFER, len(vertices) * 4, (ctypes.c_float * len(vertices))(*vertices), GL_DYNAMIC_DRAW)
        
        effect.ibo = glGenBuffers(1)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effect.ibo)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, len(indices) * 2, (ctypes.c_ushort * len(indices))(*indices), GL_DYNAMIC_DRAW)
        
        state.spectrometer_effect = effect
        logging.info("✅ Enhanced 3D Spectrometer Effect initialized")
        return True
    except Exception as e:
        logging.error(f"Spectrometer effect initialization failed: {e}", exc_info=True)
        return False

def update_and_render_spectrometer_effect(state: PluginState):
    effect = state.spectrometer_effect
    audio_processor = state.audio_processor
    if not effect or not audio_processor: return

    current_time = time.time()
    dt = current_time - state.last_frame_time
    state.last_frame_time = current_time
    
    # Get audio data and pass it to the effect
    audio_freqs = audio_processor.get_frequencies()
    if audio_freqs is not None:
        effect.set_audio_data(audio_freqs)
    
    # Update animation
    effect.update_frequencies(dt)
    
    # Regenerate geometry
    vertices, indices = effect.generate_geometry()
    
    # Update VBO
    glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
    glBufferSubData(GL_ARRAY_BUFFER, 0, len(vertices) * 4, (ctypes.c_float * len(vertices))(*vertices))
    
    # Setup rendering state
    glEnable(GL_DEPTH_TEST)
    glEnable(GL_CULL_FACE)
    glCullFace(GL_BACK)
    glUseProgram(effect.program)
    
    # Setup matrices
    proj_matrix = perspective(50.0, state.width / state.height, 0.1, 100.0)  # Wider FOV
    view_matrix = look_at([0, 0.0, 5.0], [0, 0, 0], [0, 1, 0])  # Better camera position
    
    # Add subtle rotation over time
    rotation = effect.rotation_y + (current_time - effect.animation_start_time) * 0.0
    model_matrix = multiply_matrices(
        create_translation_matrix(effect.pos_x, effect.pos_y+2.45, effect.pos_z),
        create_rotation_matrix_y(rotation)
    )
    
    glUniformMatrix4fv(effect.uniform_projection, 1, GL_FALSE, proj_matrix)
    glUniformMatrix4fv(effect.uniform_view, 1, GL_FALSE, view_matrix)  
    glUniformMatrix4fv(effect.uniform_model, 1, GL_FALSE, model_matrix)
    glUniform1f(effect.uniform_time, current_time - effect.animation_start_time)

    # Render
    glBindBuffer(GL_ARRAY_BUFFER, effect.vbo)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effect.ibo)
    
    stride = 6 * 4
    glEnableVertexAttribArray(0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(0))
    glEnableVertexAttribArray(1) 
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, ctypes.c_void_p(3 * 4))
    
    glDrawElements(GL_TRIANGLES, effect.index_count, GL_UNSIGNED_SHORT, None)

    # Cleanup
    glDisableVertexAttribArray(0)
    glDisableVertexAttribArray(1)
    glBindBuffer(GL_ARRAY_BUFFER, 0)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0)
    glUseProgram(0)
    glDisable(GL_CULL_FACE)
    glDisable(GL_DEPTH_TEST)

def render_frame_into_buffer(state: PluginState, buffer_idx: int):
    glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[buffer_idx])
    glViewport(0, 0, state.width, state.height)
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    update_and_render_spectrometer_effect(state)
    glFinish()
    glBindFramebuffer(GL_FRAMEBUFFER, 0)
    
def init_fbo_for_buffer(state: PluginState, texture_id: int, index: int) -> bool:
    try:
        state.fbo_textures[index] = texture_id
        state.fbos[index] = glGenFramebuffers(1)
        glBindFramebuffer(GL_FRAMEBUFFER, state.fbos[index])
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0)
        
        depth_rb = glGenRenderbuffers(1)
        glBindRenderbuffer(GL_RENDERBUFFER, depth_rb)
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, state.width, state.height)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb)

        if glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE:
            logging.error(f"FBO {index} is incomplete.")
            return False
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0)
        logging.info(f"✅ FBO {index} initialized with Texture ID {texture_id}")
        return True
    except Exception as e:
        logging.error(f"Failed to init FBO {index}: {e}")
        return False

# =========================
# MAIN FUNCTION
# =========================
def main(ipc_fd: int, shm_name: str, egl_display: int, egl_context: int,
         fbo_texture_0: int, dmabuf_fd_0: int, width: int, height: int, stride_0: int, format_0: int,
         fbo_texture_1: int, dmabuf_fd_1: int, _w: int, _h: int, stride_1: int, format_1: int) -> int:
    
    logging.basicConfig(level=logging.INFO, format='[ENHANCED SPECTROMETER] %(message)s')
    log = logging.getLogger("enhanced-spectrometer")
    log.info("=== Enhanced Python 3D Spectrometer Effect Plugin RUNNING ===")

    state = PluginState(ipc_fd)
    state.width, state.height = width, height
    
    def send_frame_ready_notification(sock, buffer_idx: int):
        try:
            sock.sendall(struct.pack('=Ii', IPC_NOTIFICATION_TYPE_FRAME_SUBMIT, buffer_idx))
        except (BrokenPipeError, ConnectionResetError):
            log.warning("Compositor connection lost.")
            raise

    try:
        if not init_spectrometer_effect(state): return 1
        
        # Initialize the Enhanced Audio Processor
        state.audio_processor = AudioProcessor(state.spectrometer_effect.num_bars)

        if not init_fbo_for_buffer(state, fbo_texture_0, 0): return 1
        if not init_fbo_for_buffer(state, fbo_texture_1, 1): return 1
        
        log.info("✅ Enhanced double buffering and audio processor initialized.")
        
        # Set initial position for better view
        state.spectrometer_effect.update_position(pos_y=-0.5, rotation_y=0.0)

        while True:
            ready, _, _ = select.select([state.ipc_socket], [], [], 0)
            if ready and not state.ipc_socket.recv(4096):
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
        if state.audio_processor:
            state.audio_processor.stop()
        try:
            if state.fbos[0]: glDeleteFramebuffers(1, [state.fbos[0]])
            if state.fbos[1]: glDeleteFramebuffers(1, [state.fbos[1]])
            if state.spectrometer_effect and state.spectrometer_effect.program: 
                glDeleteProgram(state.spectrometer_effect.program)
            if state.ipc_socket: state.ipc_socket.close()
        except Exception as e:
            log.error(f"Error during cleanup: {e}")
    
    return 0

# This part is for standalone testing if needed, but the main entry is the `main` function.
# if __name__ == '__main__':
#     print("This script is intended to be run as a plugin and requires file descriptors from a host process.")
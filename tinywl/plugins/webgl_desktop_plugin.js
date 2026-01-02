#!/usr/bin/env node
// webgl_desktop_plugin.js - Zero-copy WebGL rendering using native addon

const path = require('path');

// Load native addon
let gl;
try {
    gl = require('./native-webgl');
    console.error('[JS-WEBGL] ✅ Native WebGL addon loaded');
} catch (e) {
    console.error('[JS-WEBGL] ❌ Failed to load native addon:', e.message);
    console.error('[JS-WEBGL] Run: cd plugins/native-webgl && npm install');
    process.exit(1);
}

// Get config from environment (passed by C loader)
const config = {
    width: parseInt(process.env.DMABUF_WIDTH) || 1920,
    height: parseInt(process.env.DMABUF_HEIGHT) || 1080,
    fd0: parseInt(process.env.DMABUF_FD_0) || -1,
    fd1: parseInt(process.env.DMABUF_FD_1) || -1,
    stride0: parseInt(process.env.DMABUF_STRIDE_0) || 7680,
    stride1: parseInt(process.env.DMABUF_STRIDE_1) || 7680,
    format: parseInt(process.env.DMABUF_FORMAT) || 0x34325241,
    ipcFd: parseInt(process.env.IPC_FD) || -1,
};

console.error('[JS-WEBGL] Config:', JSON.stringify(config));

// Create WebGL context
const ctx = new gl.WebGLContext();

// Initialize EGL
if (!ctx.initialize()) {
    console.error('[JS-WEBGL] ❌ Failed to initialize');
    process.exit(1);
}

// Import DMA-BUFs from compositor (ZERO COPY!)
console.error('[JS-WEBGL] Importing DMA-BUFs...');
ctx.importDMABuf(0, config.fd0, config.width, config.height, config.stride0, config.format);
ctx.importDMABuf(1, config.fd1, config.width, config.height, config.stride1, config.format);

console.error('[JS-WEBGL] ✅ DMA-BUFs imported - zero-copy rendering enabled!');

// Shader sources - write your WebGL shaders here!
const vertexShader = `
    attribute vec2 a_position;
    attribute vec2 a_texcoord;
    varying vec2 v_uv;
    
    void main() {
        gl_Position = vec4(a_position, 0.0, 1.0);
        v_uv = a_texcoord;
    }
`;

const fragmentShader = `
    precision mediump float;
    varying vec2 v_uv;
    uniform float u_time;
    uniform vec2 u_resolution;
    uniform vec2 u_mouse;
    
    // Classic plasma effect
    void main() {
        vec2 p = v_uv * 2.0 - 1.0;
        p.x *= u_resolution.x / u_resolution.y;
        
        float t = u_time * 0.5;
        
        float v1 = sin(p.x * 5.0 + t);
        float v2 = sin(5.0 * (p.x * sin(t / 2.0) + p.y * cos(t / 3.0)) + t);
        float v3 = sin(sqrt(p.x * p.x + p.y * p.y) * 8.0 + t);
        float v = v1 + v2 + v3;
        
        vec3 color = vec3(
            sin(v * 3.14159 + 0.0) * 0.5 + 0.5,
            sin(v * 3.14159 + 2.094) * 0.5 + 0.5,
            sin(v * 3.14159 + 4.188) * 0.5 + 0.5
        );
        
        gl_FragColor = vec4(color, 1.0);
    }
`;

// Compile shaders
console.error('[JS-WEBGL] Compiling shaders...');
ctx.compileShader(vertexShader, fragmentShader);
console.error('[JS-WEBGL] ✅ Shaders compiled');

// Send frame notification to compositor via IPC
const net = require('net');
const fs = require('fs');

let ipcSocket = null;
if (config.ipcFd >= 0) {
    try {
        ipcSocket = new net.Socket({ fd: config.ipcFd, readable: true, writable: true });
        console.error('[JS-WEBGL] ✅ IPC socket connected');
    } catch (e) {
        console.error('[JS-WEBGL] ⚠️ Could not open IPC socket:', e.message);
    }
}

function sendFrameNotification(bufferIndex) {
    if (ipcSocket) {
        // Match your nano_ipc.h notification structure
        const buf = Buffer.alloc(8);
        buf.writeUInt32LE(1, 0);  // IPC_NOTIFICATION_TYPE_FRAME_SUBMIT
        buf.writeInt32LE(bufferIndex, 4);
        
        try {
            ipcSocket.write(buf);
        } catch (e) {
            // Ignore write errors
        }
    }
}

// Render loop
const startTime = Date.now();
let frameCount = 0;
let currentBuffer = 0;

function render() {
    const time = (Date.now() - startTime) / 1000.0;
    
    // Bind current buffer's framebuffer
    ctx.bindFramebuffer(currentBuffer);
    ctx.viewport(0, 0, config.width, config.height);
    
    // Clear
    ctx.clearColor(0.0, 0.0, 0.0, 1.0);
    ctx.clear(gl.COLOR_BUFFER_BIT);
    
    // Use shader and set uniforms
    ctx.useShader();
    ctx.setUniform1f('u_time', time);
    ctx.setUniform2f('u_resolution', config.width, config.height);
    ctx.setUniform2f('u_mouse', 0.5, 0.5);
    
    // Draw fullscreen quad
    ctx.drawFullscreen();
    
    // Ensure rendering is complete
    ctx.finish();
    
    // Notify compositor
    sendFrameNotification(currentBuffer);
    
    // Swap buffers
    currentBuffer = 1 - currentBuffer;
    frameCount++;
    
    // Log FPS every 60 frames
    if (frameCount % 60 === 0) {
        const elapsed = (Date.now() - startTime) / 1000;
        console.error(`[JS-WEBGL] Frame ${frameCount}, FPS: ${(frameCount / elapsed).toFixed(1)} (zero-copy GPU)`);
    }
}

// Handle graceful shutdown
process.on('SIGTERM', () => {
    console.error('[JS-WEBGL] Shutting down...');
    ctx.destroy();
    process.exit(0);
});

process.on('SIGINT', () => {
    console.error('[JS-WEBGL] Shutting down...');
    ctx.destroy();
    process.exit(0);
});

console.error('[JS-WEBGL] ✅ Starting render loop (zero-copy GPU rendering)');

// Run at 60fps
setInterval(render, 16);
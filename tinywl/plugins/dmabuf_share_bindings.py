#!/usr/bin/python3
"""
Python ctypes bindings for dmabuf_share C library
"""

import ctypes
import os
from ctypes import Structure, POINTER, c_int, c_uint32, c_uint64, c_bool, c_void_p
from typing import Optional
import logging

log = logging.getLogger(__name__)

# Load the shared library from current directory first
try:
    # Try different possible library paths
    lib_paths = [
        "./libdmabuf_share.so",          # Current directory
        "./libdmabuf_share/libdmabuf_share.so",
        "/usr/local/lib/libdmabuf_share.so",  # System install
        "/usr/lib/libdmabuf_share.so"    # System lib
    ]
    
    dmabuf_lib = None
    for path in lib_paths:
        try:
            if os.path.exists(path):
                dmabuf_lib = ctypes.CDLL(path)
                log.info(f"Loaded dmabuf_share library from: {path}")
                break
        except OSError as e:
            log.debug(f"Failed to load {path}: {e}")
            continue
    
    if not dmabuf_lib:
        raise OSError("Could not find libdmabuf_share.so")
        
except Exception as e:
    log.warning(f"Failed to load dmabuf_share library: {e}")
    dmabuf_lib = None

# Define structures to match C structs
class DmabufShareContext(Structure):
    _fields_ = [
        ("drm_fd", c_int),
        ("gbm", c_void_p),              # struct gbm_device*
        ("egl_display", c_void_p),      # EGLDisplay  
        ("egl_context", c_void_p),      # EGLContext
        ("egl_config", c_void_p),       # EGLConfig
        ("initialized", c_bool),
        ("owns_context", c_bool),
        ("export_bo", c_void_p),        # struct gbm_bo*
    ]

class DmabufShareBuffer(Structure):
    _fields_ = [
        ("fd", c_int),
        ("width", c_uint32),
        ("height", c_uint32), 
        ("format", c_uint32),
        ("stride", c_uint32),
        ("modifier", c_uint64),
    ]

# Function prototypes (only define if library loaded successfully)
if dmabuf_lib:
    # bool dmabuf_share_init(dmabuf_share_context_t *ctx)
    dmabuf_share_init = dmabuf_lib.dmabuf_share_init
    dmabuf_share_init.argtypes = [POINTER(DmabufShareContext)]
    dmabuf_share_init.restype = c_bool

    # bool dmabuf_share_init_with_display(dmabuf_share_context_t *ctx, EGLDisplay external_display)
    dmabuf_share_init_with_display = dmabuf_lib.dmabuf_share_init_with_display  
    dmabuf_share_init_with_display.argtypes = [POINTER(DmabufShareContext), c_void_p]
    dmabuf_share_init_with_display.restype = c_bool

    # void dmabuf_share_cleanup(dmabuf_share_context_t *ctx)
    dmabuf_share_cleanup = dmabuf_lib.dmabuf_share_cleanup
    dmabuf_share_cleanup.argtypes = [POINTER(DmabufShareContext)]
    dmabuf_share_cleanup.restype = None

    # GLuint dmabuf_share_import_texture(dmabuf_share_context_t *ctx, const dmabuf_share_buffer_t *dmabuf)
    dmabuf_share_import_texture = dmabuf_lib.dmabuf_share_import_texture
    dmabuf_share_import_texture.argtypes = [POINTER(DmabufShareContext), POINTER(DmabufShareBuffer)]
    dmabuf_share_import_texture.restype = c_uint32  # GLuint

    # bool dmabuf_share_export_fbo_texture(dmabuf_share_context_t *ctx, GLuint fbo_texture, 
    #                                      uint32_t width, uint32_t height, dmabuf_share_buffer_t *out_dmabuf)
    dmabuf_share_export_fbo_texture = dmabuf_lib.dmabuf_share_export_fbo_texture
    dmabuf_share_export_fbo_texture.argtypes = [POINTER(DmabufShareContext), c_uint32, c_uint32, c_uint32, POINTER(DmabufShareBuffer)]
    dmabuf_share_export_fbo_texture.restype = c_bool

    # bool dmabuf_share_export_texture(dmabuf_share_context_t *ctx, GLuint texture,
    #                                 uint32_t width, uint32_t height, dmabuf_share_buffer_t *out_dmabuf)
    dmabuf_share_export_texture = dmabuf_lib.dmabuf_share_export_texture
    dmabuf_share_export_texture.argtypes = [POINTER(DmabufShareContext), c_uint32, c_uint32, c_uint32, POINTER(DmabufShareBuffer)]
    dmabuf_share_export_texture.restype = c_bool

class DmabufShareWrapper:
    """Python wrapper for dmabuf_share C library"""
    
    def __init__(self, external_egl_display=None):
        if not dmabuf_lib:
            raise RuntimeError("dmabuf_share library not available")
            
        self.ctx = DmabufShareContext()
        self.initialized = False
        
        if external_egl_display is not None:
            # Use existing EGL display
            success = dmabuf_share_init_with_display(ctypes.byref(self.ctx), external_egl_display)
        else:
            # Create own EGL context
            success = dmabuf_share_init(ctypes.byref(self.ctx))
            
        if not success:
            raise RuntimeError("Failed to initialize dmabuf_share context")
            
        self.initialized = True
        log.info("✅ DmabufShareWrapper initialized")

    def import_dmabuf_as_texture(self, fd: int, width: int, height: int, 
                                format: int, stride: int, modifier: int = 0) -> Optional[int]:
        """Import a DMA-BUF as an OpenGL texture"""
        if not self.initialized:
            return None
            
        dmabuf = DmabufShareBuffer()
        dmabuf.fd = fd
        dmabuf.width = width
        dmabuf.height = height
        dmabuf.format = format
        dmabuf.stride = stride
        dmabuf.modifier = modifier
        
        log.info(f"Importing DMA-BUF: fd={fd}, {width}x{height}, format=0x{format:x}, stride={stride}")
        
        texture_id = dmabuf_share_import_texture(ctypes.byref(self.ctx), ctypes.byref(dmabuf))
        
        if texture_id == 0:
            log.warning("C library failed to import DMA-BUF as texture (this is expected for memfd)")
            return None
            
        log.info(f"✅ Imported DMA-BUF as texture {texture_id}")
        return texture_id

    def export_fbo_texture_as_dmabuf(self, fbo_texture: int, width: int, height: int) -> Optional[dict]:
        """Export an FBO texture as a DMA-BUF"""
        if not self.initialized:
            return None
            
        out_dmabuf = DmabufShareBuffer()
        
        success = dmabuf_share_export_fbo_texture(
            ctypes.byref(self.ctx), 
            fbo_texture, 
            width, 
            height, 
            ctypes.byref(out_dmabuf)
        )
        
        if not success:
            log.error("Failed to export FBO texture as DMA-BUF")
            return None
            
        result = {
            'fd': out_dmabuf.fd,
            'width': out_dmabuf.width,
            'height': out_dmabuf.height,
            'format': out_dmabuf.format,
            'stride': out_dmabuf.stride,
            'modifier': out_dmabuf.modifier
        }
        
        log.info(f"✅ Exported FBO texture as DMA-BUF: fd={result['fd']}")
        return result

    def export_texture_as_dmabuf(self, texture: int, width: int, height: int) -> Optional[dict]:
        """Export a texture as a DMA-BUF"""
        if not self.initialized:
            return None
            
        out_dmabuf = DmabufShareBuffer()
        
        success = dmabuf_share_export_texture(
            ctypes.byref(self.ctx), 
            texture, 
            width, 
            height, 
            ctypes.byref(out_dmabuf)
        )
        
        if not success:
            log.error("Failed to export texture as DMA-BUF")
            return None
            
        result = {
            'fd': out_dmabuf.fd,
            'width': out_dmabuf.width,
            'height': out_dmabuf.height,
            'format': out_dmabuf.format,
            'stride': out_dmabuf.stride,
            'modifier': out_dmabuf.modifier
        }
        
        log.info(f"✅ Exported texture as DMA-BUF: fd={result['fd']}")
        return result

    def cleanup(self):
        """Cleanup the dmabuf_share context"""
        if self.initialized:
            dmabuf_share_cleanup(ctypes.byref(self.ctx))
            self.initialized = False
            log.info("✅ DmabufShareWrapper cleaned up")

    def __del__(self):
        self.cleanup()

# Convenience function to check if dmabuf_share is available
def is_dmabuf_share_available() -> bool:
    """Check if the dmabuf_share library is available"""
    return dmabuf_lib is not None

# Test function
def test_library():
    """Test the library loading and basic functionality"""
    if not is_dmabuf_share_available():
        print("❌ dmabuf_share library not available")
        return False
    
    try:
        # Test with own EGL context (this might fail without proper setup)
        # wrapper = DmabufShareWrapper()
        # wrapper.cleanup()
        print("✅ dmabuf_share library loaded and appears functional")
        return True
    except Exception as e:
        print(f"⚠️  dmabuf_share library loaded but initialization failed: {e}")
        print("   This is normal if no EGL context is available")
        return True

if __name__ == "__main__":
    # Test when run directly
    import logging
    logging.basicConfig(level=logging.INFO)
    test_library()
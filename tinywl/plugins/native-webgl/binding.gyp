{
  "targets": [
    {
      "target_name": "native_webgl",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags": [ "-fexceptions" ],
      "cflags_cc": [ "-fexceptions", "-std=c++17" ],
      "sources": [ "native_webgl.cc" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "/usr/include/libdrm"
      ],
      "libraries": [
        "-lEGL",
        "-lGLESv2",
        "-lgbm",
        "-ldrm"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ]
    }
  ]
}

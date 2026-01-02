executable(
	'nano_compositor',
	['main.c', protocols_server_header['xdg-shell']],  # Adjusted path
	dependencies: [
		wlroots,
		dependency('libpng'),
		dependency('wayland-server'),
		dependency('xkbcommon'),
		dependency('pixman-1'),
		dependency('libdrm'),
		dependency('glesv2'),
		meson.get_compiler('c').find_library('freerdp2'),
		dependency('winpr2'),
	],
	include_directories: [
		include_directories('../include'),
	  # Adjusted for backend/RDP/backend.h
	]
)

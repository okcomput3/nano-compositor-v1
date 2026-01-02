# Nano Compositor Makefile
PKG_CONFIG?=pkg-config
WAYLAND_PROTOCOLS!=$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols
WAYLAND_SCANNER!=$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner

# Define PKG_CONFIG_PATH to include custom paths and system paths
PKG_CONFIG_PATH?=/usr/local/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:$$PKG_CONFIG_PATH

# Check if required pkg-config packages are available
CHECK_PKG_CONFIG=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --exists $(1) && echo yes || echo no)

# Ensure all required packages are found
REQUIRED_PKGS=wlroots wayland-server wayland-client xkbcommon pixman-1 libdrm glesv2 libpng
$(foreach pkg,$(REQUIRED_PKGS),$(if $(call CHECK_PKG_CONFIG,$(pkg)),,\
	$(error Package $(pkg) not found in PKG_CONFIG_PATH=$(PKG_CONFIG_PATH))))

PYTHON_CFLAGS   = $(shell python3-config --cflags)
PYTHON_LDFLAGS  = $(shell python3-config --ldflags)

# Define LIBS with all required libraries, manually adding -lEGL and -ldl for plugin loading
LIBS=\
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs wlroots-0.19) \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs wayland-server) \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs wayland-client) \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs xkbcommon) \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs pixman-1) \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs libdrm) \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs glesv2) \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs libpng) \
	-lEGL -lm -lgbm  -ldl -lpthread $(shell python3-config --ldflags --embed)

# Define CFLAGS with all required includes
CFLAGS_PKG_CONFIG=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --cflags wlroots-0.19 wayland-server wayland-client xkbcommon pixman-1 libdrm glesv2 libpng)
CFLAGS+=$(CFLAGS_PKG_CONFIG) -I. -Iinclude -I/usr/local/include/wlroots-0.19 -DWLR_USE_UNSTABLE -g -fPIC $(shell python3-config --cflags)

LDFLAGS+=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs-only-L wlroots-0.19 wayland-server wayland-client xkbcommon pixman-1 libdrm glesv2 libpng )

# Plugin-specific flags
PLUGIN_CFLAGS=$(CFLAGS) -shared
PLUGIN_LDFLAGS=-shared

# Directories
PLUGIN_DIR=plugins
BUILD_DIR=build
DMABUF_SHARE_DIR=libdmabuf_share
INSTALL_PREFIX?=/usr/local
PLUGIN_INSTALL_DIR=$(INSTALL_PREFIX)/lib/nano-compositor/plugins

# DMA-BUF shared library
DMABUF_SHARE_LIB=$(DMABUF_SHARE_DIR)/libdmabuf_share.so
DMABUF_SHARE_CFLAGS=-I$(DMABUF_SHARE_DIR)
DMABUF_SHARE_LIBS=-L$(DMABUF_SHARE_DIR) -ldmabuf_share

# Source files
NANO_SOURCES= concurrent_queue.c nano_server.c nano_plugin.c nano_event.c nano_api.c nano_layers.c main.c
NANO_OBJECTS=$(NANO_SOURCES:.c=.o)

# Plugin sources (only build if they exist)
PLUGIN_SOURCES=$(wildcard $(PLUGIN_DIR)/*_plugin.c)
PLUGIN_EXECS=$(PLUGIN_SOURCES:.c=)

# Main targets - build shared library first
all: $(DMABUF_SHARE_LIB) nano-compositor
ifneq ($(PLUGIN_EXECS),)
all: $(PLUGIN_EXECS)
endif

# Build shared DMA-BUF library
$(DMABUF_SHARE_LIB): $(DMABUF_SHARE_DIR)
	$(MAKE) -C $(DMABUF_SHARE_DIR)

# Create shared library directory if it doesn't exist
$(DMABUF_SHARE_DIR):
	mkdir -p $(DMABUF_SHARE_DIR)


# Generate xdg-shell protocol headers
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-client-protocol.h:
	$(WAYLAND_SCANNER) client-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

# Core compositor objects - now depend on shared library
%.o: %.c nano_compositor.h xdg-shell-protocol.h xdg-shell-client-protocol.h $(DMABUF_SHARE_LIB)
	$(CC) -c $< $(CFLAGS) $(DMABUF_SHARE_CFLAGS) -o $@

# Main compositor binary - link with shared library
nano-compositor: $(NANO_OBJECTS) $(DMABUF_SHARE_LIB)
	$(CC) $^ $(CFLAGS) $(LDFLAGS) $(LIBS) $(DMABUF_SHARE_LIBS) -rdynamic -o $@

# Plugin compilation - link with shared library
$(PLUGIN_DIR)/%: $(PLUGIN_DIR)/%.c nano_compositor.h $(DMABUF_SHARE_LIB)
	$(CC) $(CFLAGS) $(DMABUF_SHARE_CFLAGS) -o $@ $< $(LIBS) $(DMABUF_SHARE_LIBS)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Create plugins directory
$(PLUGIN_DIR):
	mkdir -p $(PLUGIN_DIR)

# Install targets - include shared library
install: nano-compositor $(PLUGIN_EXECS) $(DMABUF_SHARE_LIB)
	install -d $(INSTALL_PREFIX)/bin
	install -m 755 nano-compositor $(INSTALL_PREFIX)/bin/
	install -d $(INSTALL_PREFIX)/lib
	install -m 755 $(DMABUF_SHARE_LIB) $(INSTALL_PREFIX)/lib/
	install -d $(PLUGIN_INSTALL_DIR)
	install -m 755 $(PLUGIN_EXECS) $(PLUGIN_INSTALL_DIR)/
	install -d $(INSTALL_PREFIX)/share/nano-compositor
	install -m 644 config/nano-compositor.conf $(INSTALL_PREFIX)/share/nano-compositor/ 2>/dev/null || true
	install -m 644 config/plugins.conf $(INSTALL_PREFIX)/share/nano-compositor/ 2>/dev/null || true
	install -d $(INSTALL_PREFIX)/include/nano-compositor
	install -m 644 $(DMABUF_SHARE_DIR)/dmabuf_share.h $(INSTALL_PREFIX)/include/nano-compositor/

# Uninstall targets - remove shared library
uninstall:
	rm -f $(INSTALL_PREFIX)/bin/nano-compositor
	rm -f $(INSTALL_PREFIX)/lib/libdmabuf_share.so
	rm -rf $(PLUGIN_INSTALL_DIR)
	rm -rf $(INSTALL_PREFIX)/share/nano-compositor
	rm -rf $(INSTALL_PREFIX)/include/nano-compositor

# Development helpers
debug: CFLAGS += -DDEBUG -O0
debug: $(DMABUF_SHARE_LIB) nano-compositor $(PLUGIN_EXECS)

release: CFLAGS += -O2 -DNDEBUG
release: $(DMABUF_SHARE_LIB) nano-compositor $(PLUGIN_EXECS)

# Plugin development helpers
plugin-reload: $(PLUGIN_EXECS)
	killall -USR1 nano-compositor 2>/dev/null || true

# Test a single plugin
test-plugin-%: $(PLUGIN_DIR)/%
	LD_LIBRARY_PATH=$(DMABUF_SHARE_DIR):$$LD_LIBRARY_PATH ./nano-compositor --plugin-test $<

# Development server with auto-reload
dev-server: nano-compositor $(PLUGIN_EXECS)
	LD_LIBRARY_PATH=$(DMABUF_SHARE_DIR):$$LD_LIBRARY_PATH ./scripts/dev-server.sh

# Static analysis
check:
	cppcheck --enable=all --std=c11 --platform=unix64 \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		*.c $(PLUGIN_DIR)/*.c $(DMABUF_SHARE_DIR)/*.c

# Format code
format:
	clang-format -i *.c *.h $(PLUGIN_DIR)/*.c $(DMABUF_SHARE_DIR)/*.c $(DMABUF_SHARE_DIR)/*.h

# Generate documentation
docs:
	doxygen Doxyfile

# Create distribution tarball
VERSION?=0.1.0
DIST_NAME=nano-compositor-$(VERSION)
dist: clean
	mkdir -p $(DIST_NAME)
	cp -r *.c *.h $(PLUGIN_DIR) $(DMABUF_SHARE_DIR) config scripts Makefile README.md LICENSE $(DIST_NAME)/ 2>/dev/null || true
	tar -czf $(DIST_NAME).tar.gz $(DIST_NAME)
	rm -rf $(DIST_NAME)

# Clean targets - include shared library
clean:
	rm -f nano-compositor *.o xdg-shell-protocol.h xdg-shell-client-protocol.h xdg-shell-protocol.c
	rm -f $(PLUGIN_EXECS)
	rm -rf $(BUILD_DIR)
	$(MAKE) -C $(DMABUF_SHARE_DIR) clean 2>/dev/null || true

clean-all: clean
	rm -f *.tar.gz
	rm -rf docs

# Configuration file creation
config:
	mkdir -p ~/.config/nano-compositor
	cp config/nano-compositor.conf ~/.config/nano-compositor/ 2>/dev/null || true
	cp config/plugins.conf ~/.config/nano-compositor/ 2>/dev/null || true

# Development dependencies check
deps-check:
	@echo "Checking development dependencies..."
	@which clang-format > /dev/null || echo "Warning: clang-format not found"
	@which cppcheck > /dev/null || echo "Warning: cppcheck not found"
	@which doxygen > /dev/null || echo "Warning: doxygen not found"

# Plugin template generation
new-plugin:
	@read -p "Plugin name: " name; \
	mkdir -p $(PLUGIN_DIR); \
	if [ -f $(PLUGIN_DIR)/template_plugin.c ]; then \
		cp $(PLUGIN_DIR)/template_plugin.c $(PLUGIN_DIR)/${name}_plugin.c; \
		sed -i "s/PLUGIN_NAME/${name}/g" $(PLUGIN_DIR)/${name}_plugin.c; \
	else \
		echo "Creating simple plugin template..."; \
		echo '// '${name}'_plugin.c' > $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '#include "../nano_compositor.h"' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '#include "../libdmabuf_share/dmabuf_share.h"' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo 'static int '${name}'_init(struct nano_plugin *plugin, const struct nano_plugin_api *api) {' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '    return 0;' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '}' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo 'static void '${name}'_destroy(struct nano_plugin *plugin) {' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '}' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo 'static const struct nano_plugin_interface '${name}'_interface = {' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '    .name = "'${name}'",' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '    .version = "1.0.0",' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '    .description = "'${name}' plugin",' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '    .required_caps = NANO_CAP_NONE,' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '    .init = '${name}'_init,' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '    .destroy = '${name}'_destroy,' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '};' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo 'const struct nano_plugin_interface *nano_plugin_get_interface(void) {' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '    return &'${name}'_interface;' >> $(PLUGIN_DIR)/${name}_plugin.c; \
		echo '}' >> $(PLUGIN_DIR)/${name}_plugin.c; \
	fi; \
	echo "Created $(PLUGIN_DIR)/${name}_plugin.c"

# Shared library specific targets
dmabuf-lib: $(DMABUF_SHARE_LIB)

dmabuf-lib-clean:
	$(MAKE) -C $(DMABUF_SHARE_DIR) clean

dmabuf-lib-install: $(DMABUF_SHARE_LIB)
	install -d $(INSTALL_PREFIX)/lib
	install -m 755 $(DMABUF_SHARE_LIB) $(INSTALL_PREFIX)/lib/
	install -d $(INSTALL_PREFIX)/include/nano-compositor
	install -m 644 $(DMABUF_SHARE_DIR)/dmabuf_share.h $(INSTALL_PREFIX)/include/nano-compositor/

# Runtime plugin information
plugin-info:
	@echo "Available plugins:"
	@for plugin in $(PLUGIN_EXECS); do \
		echo "  $$(basename $$plugin _plugin)"; \
	done

# Runtime with proper library path
run: nano-compositor
	LD_LIBRARY_PATH=$(DMABUF_SHARE_DIR):$$LD_LIBRARY_PATH ./nano-compositor

# Help target
help:
	@echo "Nano Compositor Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build compositor, shared library and all plugins"
	@echo "  nano-compositor - Build main compositor only"
	@echo "  plugins       - Build all plugins"
	@echo "  dmabuf-lib    - Build shared DMA-BUF library only"
	@echo "  install       - Install to $(INSTALL_PREFIX)"
	@echo "  uninstall     - Remove installation"
	@echo "  debug         - Build with debug symbols"
	@echo "  release       - Build optimized release"
	@echo "  clean         - Remove build artifacts"
	@echo "  clean-all     - Remove all generated files"
	@echo "  config        - Install default config files"
	@echo "  check         - Run static analysis"
	@echo "  format        - Format source code"
	@echo "  docs          - Generate documentation"
	@echo "  dist          - Create distribution tarball"
	@echo "  new-plugin    - Create new plugin from template"
	@echo "  plugin-info   - Show available plugins"
	@echo "  run           - Run compositor with proper library path"
	@echo "  help          - Show this help"
	@echo ""
	@echo "Plugin Development:"
	@echo "  test-plugin-<name> - Test specific plugin"
	@echo "  plugin-reload      - Reload plugins in running compositor"
	@echo "  dev-server         - Start development server with auto-reload"
	@echo ""
	@echo "Shared Library:"
	@echo "  dmabuf-lib         - Build shared DMA-BUF library"
	@echo "  dmabuf-lib-clean   - Clean shared library"
	@echo "  dmabuf-lib-install - Install shared library only"
	@echo ""
	@echo "Variables:"
	@echo "  INSTALL_PREFIX  - Installation prefix (default: /usr/local)"
	@echo "  PKG_CONFIG_PATH - Custom pkg-config search paths"
	@echo "  VERSION         - Version for distribution (default: 0.1.0)"

# Phony targets
.PHONY: all install uninstall debug release clean clean-all config \
        deps-check new-plugin plugin-info plugin-reload dev-server check \
        format docs dist help test-plugin-% dmabuf-lib dmabuf-lib-clean \
        dmabuf-lib-install run

# Default target
.DEFAULT_GOAL := all

# Include dependency files if they exist
-include *.d $(PLUGIN_DIR)/*.d

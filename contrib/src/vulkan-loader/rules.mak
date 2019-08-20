VULKAN_LOADER_VERSION := 1.1.127
VULKAN_LOADER_URL := https://github.com/KhronosGroup/Vulkan-Loader/archive/v$(VULKAN_LOADER_VERSION).tar.gz
VULKAN_LOADER_ARCHIVE := Vulkan-Loader-$(VULKAN_LOADER_VERSION).tar.gz

DEPS_vulkan-loader = vulkan-headers $(DEPS_vulkan-headers)

# On WIN32 platform, we don't know where to find the loader
# so always build it for the Vulkan module.
ifdef HAVE_WIN32
PKGS += vulkan-loader
ifeq ($(call need_pkg,"vulkan"),)
PKGS_FOUND += vulkan-loader
endif
endif

VULKAN_LOADER_CONF := \
	-DENABLE_STATIC_LOADER=ON \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_TESTS=OFF \
	-DBUILD_LOADER=ON

$(TARBALLS)/$(VULKAN_LOADER_ARCHIVE):
	$(call download_pkg,$(VULKAN_LOADER_URL),vulkan-loader)

.sum-vulkan-loader: $(VULKAN_LOADER_ARCHIVE)

vulkan-loader: $(VULKAN_LOADER_ARCHIVE) .sum-vulkan-loader
	$(UNPACK)
	# Patches are from msys2 package system
	# https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-vulkan-loader
	cd $(UNPACK_DIR) && patch -p1 -i ../$(SRC)/vulkan-loader/001-build-fix.patch
	cd $(UNPACK_DIR) && patch -p1 -i ../$(SRC)/vulkan-loader/002-proper-def-files-for-32bit.patch
	cd $(UNPACK_DIR) && patch -p1 -i ../$(SRC)/vulkan-loader/003-generate-pkgconfig-files.patch
	$(MOVE)

# Needed for the loader's cmake script to find the registry files
VULKAN_LOADER_ENV_CONF = \
	VULKAN_HEADERS_INSTALL_DIR="$(PREFIX)"

.vulkan-loader: vulkan-loader toolchain.cmake
ifndef HAVE_WIN32
	$(error vulkan-loader contrib can only be used on WIN32 targets)
endif
	cd $< && rm -rf ./build && mkdir -p build
	cd $</build && $(VULKAN_LOADER_ENV_CONF) $(HOSTVARS) \
		$(CMAKE) $(VULKAN_LOADER_CONF) ..
	cd $</build && $(MAKE)
	# CMake will generate a .pc file with -lvulkan even if the static library
	# generated is libVKstatic.1.a. It also forget to link with libcfgmgr32.
	cd $< && sed -i.orig -e "s,-lvulkan,-lVKstatic.1 -lcfgmgr32," build/loader/vulkan.pc
	$(call pkg_static,"build/loader/vulkan.pc")
	cd $</build && $(MAKE) install
	touch $@

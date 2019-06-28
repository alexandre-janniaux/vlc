# libplacebo

VULKANLOADER_HASH :=
VULKANLOADER_VERSION := 1.1.107
VULKANLOADER_URL := https://github.com/KhronosGroup/Vulkan-Loader/archive/v$(VULKANLOADER_VERSION).tar.gz
VULKANLOADER_ARCHIVE := Vulkan-Loader-$(VULKANLOADER_VERSION).tar.gz

DEPS_vulkanloader = vulkan-headers $(DEPS_vulkan-headers)

ifeq ($(call need_pkg,"libplacebo"),)
PKGS_FOUND += vulkanloader
endif

VULKANLOADER_CONF := \
	-DENABLE_STATIC_LOADER=ON \
	-DBUILD_TESTS=OFF \
	-DBUILD_LOADER=ON

$(TARBALLS)/$(VULKANLOADER_ARCHIVE):
	$(call download_pkg,$(VULKANLOADER_URL),vulkanloader)

.sum-vulkanloader: $(VULKANLOADER_ARCHIVE)

vulkanloader: $(VULKANLOADER_ARCHIVE) .sum-vulkanloader
	$(UNPACK)
	$(APPLY) $(SRC)/vulkanloader/0001-WIP.patch
	#rm -rf $@ Vulkan-LoaderAndValidationLayers-sdk-1.1.73.0
	#tar xvzfo $<
	#mv Vulkan-LoaderAndValidationLayers-sdk-1.1.73.0 vulkan-loader-$(VULKANLOADER_VERSION)
	$(MOVE)

.vulkanloader: vulkanloader toolchain.cmake
	cd $< && rm -rf ./build && mkdir -p build
	cd $</build && $(HOSTVARS) $(CMAKE) $(VULKANLOADER_CONF) ..
	cd $< && cd build && make install
	touch $@

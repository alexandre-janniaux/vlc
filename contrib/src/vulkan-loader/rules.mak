# libplacebo

VULKAN_LOADER_HASH :=
VULKAN_LOADER_VERSION := 1.1.119
VULKAN_LOADER_URL := https://github.com/KhronosGroup/Vulkan-Loader/archive/v$(VULKAN_LOADER_VERSION).tar.gz
VULKAN_LOADER_ARCHIVE := Vulkan-Loader-$(VULKAN_LOADER_VERSION).tar.gz

DEPS_vulkan-loader = vulkan-headers $(DEPS_vulkan-headers)

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
	$(APPLY) $(SRC)/vulkan-loader/0001-cmake-generate-static-libraries.patch
	#$(APPLY) $(SRC)/vulkan-loader/0001-WIP.patch
	#rm -rf $@ Vulkan-LoaderAndValidationLayers-sdk-1.1.73.0
	#tar xvzfo $<
	#mv Vulkan-LoaderAndValidationLayers-sdk-1.1.73.0 vulkan-loader-$(VULKANLOADER_VERSION)
	$(MOVE)

.vulkan-loader: vulkan-loader toolchain.cmake
	cd $< && rm -rf ./build && mkdir -p build
	cd $</build && $(HOSTVARS) $(CMAKE) $(VULKAN_LOADER_CONF) ..
	cd $< && cd build && make install
	touch $@

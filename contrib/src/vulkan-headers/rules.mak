# libplacebo

VULKAN_HEADER_VERSION := 1.1.127
VULKAN_HEADER_URL := https://github.com/KhronosGroup/Vulkan-Headers/archive/v$(VULKAN_HEADER_VERSION).tar.gz
VULKAN_HEADER_ARCHIVE := Vulkan-Headers-$(VULKAN_HEADER_VERSION).tar.gz

DEPS_vulkan-headers =

$(TARBALLS)/$(VULKAN_HEADER_ARCHIVE):
	$(call download_pkg,$(VULKAN_HEADER_URL),vulkan-headers)

.sum-vulkan-headers: $(VULKAN_HEADER_ARCHIVE)

vulkan-headers: $(VULKAN_HEADER_ARCHIVE) .sum-vulkan-headers
	$(UNPACK)
	$(MOVE)

.vulkan-headers: vulkan-headers toolchain.cmake
	cd $< && rm -rf ./build && mkdir -p build
	cd $</build && $(HOSTVARS) $(CMAKE) .. -G Ninja
	cd $< && cd build && ninja install
	touch $@

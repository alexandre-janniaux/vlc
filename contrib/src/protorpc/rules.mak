PROTORPC_HASH := 06f434f6352ab1d72db082417d4d0a17df4c4447
PROTORPC_VERSION := dev
PROTORPC_GITURL := https://gitlab.com/Sideway/protorpc.git

PKGS += protorpc

PROTORPC_CONF := -Dsidl_vlc_contrib=true

$(TARBALLS)/protorpc-$(PROTORPC_VERSION).tar.xz:
	$(call download_git,$(PROTORPC_GITURL),,$(PROTORPC_HASH))

.sum-protorpc: protorpc-$(PROTORPC_VERSION).tar.xz
	$(call check_githash,$(PROTORPC_HASH))

protorpc: protorpc-$(PROTORPC_VERSION).tar.xz .sum-protorpc
	$(UNPACK)
	$(MOVE)

.protorpc: protorpc crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(PROTORPC_CONF) build
	cd $< && ninja -C build install
	touch $@


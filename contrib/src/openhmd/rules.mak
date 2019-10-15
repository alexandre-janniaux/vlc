# OpenHMD

OPENHMD_VERSION := d337a154ad1d156722499234c205a9c2e5d3f4f9
OPENHMD_GITURL = https://github.com/OpenHMD/OpenHMD.git
OPENHMD_BRANCH = master
OPENHMD_TARBALL = $(TARBALLS)/openhmd-git-$(OPENHMD_VERSION).tar.xz

OPENHMD_DRIVERS = rift,vive,deepoon,psvr,nolo,external

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
DEPS_openhmd = hidapi
endif
endif

ifdef HAVE_WIN32
DEPS_openhmd = hidapi $(DEPS_hidapi) pthreads $(DEPS_pthreads)
endif

ifdef HAVE_ANDROID
#OPENHMD_DRIVER_CONFIG = --disable-driver-oculus-rift --disable-driver-htc-vive --disable-driver-deepoon --disable-driver-psvr --disable-driver-nolo --disable-driver-external --disable-driver-wmr --enable-driver-android
OPENHMD_DRIVERS = android
endif

PKGS += openhmd

ifeq ($(call need_pkg,"openhmd"),)
PKGS_FOUND += openhmd
endif

$(OPENHMD_TARBALL):
	$(call download_git,$(OPENHMD_GITURL),,$(OPENHMD_VERSION))

.sum-openhmd: $(OPENHMD_TARBALL)
	$(call check_githash,$(OPENHMD_VERSION))
	touch $@

openhmd: $(OPENHMD_TARBALL) .sum-openhmd
	$(UNPACK)
	$(APPLY) $(SRC)/openhmd/0001-disable-test.patch
	$(APPLY) $(SRC)/openhmd/0001-meson-define-OHMD_STATIC.patch
	$(MOVE)

OPENHMD_CONFIG = \
	-Ddrivers="$(OPENHMD_DRIVERS)" \
	-Dexamples=

.openhmd: openhmd crossfile.meson
	cd $< && rm -rf build
	cd $< && $(HOSTVARS_MESON) $(MESON) build $(OPENHMD_CONFIG)
	cd $< && ninja -C build install
	touch $@

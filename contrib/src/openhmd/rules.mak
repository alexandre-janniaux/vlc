# OpenHMD

OPENHMD_VERSION := 7e702cc62fa4835585f4b32cd53e9f6ba6b641d2
OPENHMD_GITURL = https://github.com/magwyz/OpenHMD.git
OPENHMD_BRANCH = testVive

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
DEPS_openhmd = hidapi
endif
endif

ifdef HAVE_WIN32
DEPS_openhmd = hidapi
endif

ifdef HAVE_ANDROID
OPENHMD_DRIVER_CONFIG = --disable-driver-oculus-rift --disable-driver-htc-vive --disable-driver-deepoon --disable-driver-psvr --disable-driver-nolo --disable-driver-external --disable-driver-wmr --enable-driver-android
endif

PKGS += openhmd

ifeq ($(call need_pkg,"openhmd"),)
PKGS_FOUND += openhmd
endif

$(TARBALLS)/openhmd-git.tar.xz:
	$(call download_git,$(OPENHMD_GITURL),$(OPENHMD_BRANCH),$(OPENHMD_VERSION))

.sum-openhmd: openhmd-git.tar.xz
	$(call check_githash,$(OPENHMD_VERSION))
	touch $@

openhmd: openhmd-git.tar.xz .sum-openhmd
	$(UNPACK)
	$(MOVE)

.openhmd: openhmd
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-static --disable-shared $(OPENHMD_DRIVER_CONFIG)
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@

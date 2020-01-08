# GPGERROR
GPGERROR_VERSION := 1.27
GPGERROR_URL := ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-$(GPGERROR_VERSION).tar.bz2

$(TARBALLS)/libgpg-error-$(GPGERROR_VERSION).tar.bz2:
	$(call download_pkg,$(GPGERROR_URL),gpg-error)

ifeq ($(call need_pkg,"gpg-error >= 1.27"),)
PKGS_FOUND += gpg-error
endif

.sum-gpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2

libgpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2 .sum-gpg-error
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/gpg-error/windres-make.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/gpg-error/winrt.patch
endif
endif
	$(APPLY) $(SRC)/gpg-error/missing-unistd-include.patch
	$(APPLY) $(SRC)/gpg-error/no-executable.patch
	$(APPLY) $(SRC)/gpg-error/win32-unicode.patch
	$(APPLY) $(SRC)/gpg-error/version-bump-gawk-5.patch
	$(MOVE)
ifdef HAVE_ANDROID
ifeq ($(ARCH),aarch64)
	# x86_64-linux-gnu matches exactly what gets generated by gen-posix-lock-obj on arm64
	cp $@/src/syscfg/lock-obj-pub.x86_64-pc-linux-gnu.h $@/src/syscfg/lock-obj-pub.linux-android.h
else
	cp $@/src/syscfg/lock-obj-pub.arm-unknown-linux-androideabi.h $@/src/syscfg/lock-obj-pub.linux-android.h
endif
endif
ifdef HAVE_DARWIN_OS
ifdef HAVE_ARMV7A
	cp $@/src/syscfg/lock-obj-pub.arm-apple-darwin.h $@/src/syscfg/lock-obj-pub.$(HOST).h
else
ifeq ($(ARCH),aarch64)
ifneq ($(HOST),aarch64-apple-darwin)
	cp $@/src/syscfg/lock-obj-pub.aarch64-apple-darwin.h $@/src/syscfg/lock-obj-pub.$(HOST).h
endif
else
	cp $@/src/syscfg/lock-obj-pub.x86_64-apple-darwin.h $@/src/syscfg/lock-obj-pub.$(HOST).h
endif
endif
endif
ifdef HAVE_NACL
ifeq ($(ARCH),i386) # 32bits intel
	cp $@/src/syscfg/lock-obj-pub.i686-pc-linux-gnu.h $@/src/syscfg/lock-obj-pub.nacl.h
else
ifeq ($(ARCH),x86_64)
	cp $@/src/syscfg/lock-obj-pub.x86_64-pc-linux-gnu.h $@/src/syscfg/lock-obj-pub.nacl.h
endif
endif
endif

GPGERROR_CONF := $(HOSTCONF) \
	--disable-nls \
	--disable-shared \
	--disable-languages \
	--disable-tests

.gpg-error: libgpg-error
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(GPGERROR_CONF)
	cd $< && $(MAKE) install
	touch $@

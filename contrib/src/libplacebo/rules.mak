# libplacebo

PLACEBO_VERSION := 1.8.0
PLACEBO_ARCHIVE = libplacebo-v$(PLACEBO_VERSION).tar.xz
#PLACEBO_URL := https://code.videolan.org/videolan/libplacebo/-/archive/v$(PLACEBO_VERSION)/$(PLACEBO_ARCHIVE)

PLACEBO_HASH := 9ad38ee5cf391fb9a57a66f897e9fd86f96b002d
PLACEBO_BRANCH := hack_branch-bench
PLACEBO_GITURL := https://github.com/Psilokos/libplacebo.git

DEPS_libplacebo = glslang $(DEPS_glslang)

ifndef HAVE_WINSTORE
PKGS += libplacebo
endif
ifeq ($(call need_pkg,"libplacebo"),)
PKGS_FOUND += libplacebo
endif

ifdef HAVE_WIN32
DEPS_libplacebo += pthreads $(DEPS_pthreads)
endif

PLACEBOCONF := -Dglslang=enabled \
	-Dshaderc=disabled

$(TARBALLS)/$(PLACEBO_ARCHIVE):
	$(call download_git,$(PLACEBO_GITURL),$(PLACEBO_BRANCH),$(PLACEBO_HASH))

.sum-libplacebo: $(PLACEBO_ARCHIVE)
	$(call check_githash,$(PLACEBO_HASH))
	touch $@

libplacebo: $(PLACEBO_ARCHIVE) .sum-libplacebo
	$(UNPACK)
	$(APPLY) $(SRC)/libplacebo/0001-meson-fix-glslang-search-path.patch
	$(MOVE)

.libplacebo: libplacebo crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(PLACEBOCONF) build
	cd $< && cd build && ninja install
# Work-around messon issue https://github.com/mesonbuild/meson/issues/4091
	sed -i.orig -e 's/Libs: \(.*\) -L$${libdir} -lplacebo/Libs: -L$${libdir} -lplacebo \1/g' $(PREFIX)/lib/pkgconfig/libplacebo.pc
	touch $@

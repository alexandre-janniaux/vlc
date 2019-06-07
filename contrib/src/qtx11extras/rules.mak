# qtx11extras

QTX11_VERSION_MAJOR := 5.12
QTX11_VERSION := $(QTX11_VERSION_MAJOR).2
QTX11_URL := https://download.qt.io/official_releases/qt/$(QTX11_VERSION_MAJOR)/$(QTX11_VERSION)/submodules/qtx11extras-everywhere-src-$(QTX11_VERSION).tar.xz

DEPS_qtx11extras += qt $(DEPS_qt)

ifeq ($(call need_pkg,"Qt5X11Extras"),)
PKGS_FOUND += qtx11extras
endif

QTX11_PC = Qt5X11Extra.pc

$(TARBALLS)/qtx11extras-$(QTX11_VERSION).tar.xz:
	$(call download,$(QTX11_URL))

.sum-qtx11extras: qtx11extras-$(QTX11_VERSION).tar.xz

qtx11extras: qtx11extras-$(QTX11_VERSION).tar.xz .sum-qtx11extras
	$(UNPACK)
	mv qtx11extras-everywhere-src-$(QTX11_VERSION) qtx11extras-$(QTX11_VERSION)
	$(MOVE)

.qtx11extras: qtx11extras
	cd $< && $(PREFIX)/bin/qmake
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-x11extras-install_subtargets

	# Patch all pkgconfig files
	for pc_file in $(QTX11_PC); do \
		$(call pkg_static,"$(PREFIX)/lib/pkgconfig/$${pc_file}"); \
	done

	touch $@

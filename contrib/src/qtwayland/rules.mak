# qtwayland

QTWAYLAND_VERSION_MAJOR := 5.12
QTWAYLAND_VERSION := $(QTWAYLAND_VERSION_MAJOR).2
QTWAYLAND_URL := http://download.qt.io/official_releases/qt/$(QTWAYLAND_VERSION_MAJOR)/$(QTWAYLAND_VERSION)/submodules/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

DEPS_qtwayland += qt $(DEPS_qt)

ifeq ($(call need_pkg,"Qt5WaylandClient >= 5.11")))
PKGS_FOUND += qtwayland
endif

$(TARBALLS)/qtwayland-$(QTWAYLAND_VERSION).tar.xz:
	$(call download,$(QTWAYLAND_URL))

.sum-qtwayland: qtwayland-$(QTWAYLAND_VERSION).tar.xz

qtwayland: qtwayland-$(QTWAYLAND_VERSION).tar.xz .sum-qtwayland
	$(UNPACK)
	mv qtwayland-everywhere-src-$(QTWAYLAND_VERSION) qtwayland-$(QTWAYLAND_VERSION)
	$(MOVE)

.qtwayland: qtwayland
	cd $< && $(PREFIX)/bin/qmake "QT_CONFIG+=wayland-client wayland-server wayland-egl"
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src
	cd $< && $(MAKE) -C src sub-client-install_subtargets
	cd $</src/plugins && $(PREFIX)/bin/qmake
	cd $< && $(MAKE) -C src/plugins sub-platforms-install_subtargets sub-decorations-install_subtargets sub-shellintegration-install_subtargets
	cp $(PREFIX)/plugins/platforms/libqwayland-egl.a $(PREFIX)/lib/
	cp $(PREFIX)/plugins/wayland-shell-integration/libwl-shell.a $(PREFIX)/lib/
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5WaylandClient/ -lqwayland-egl -lwl-shell -lQt5WaylandClient -lQt5EglSupport -lEGL -lwayland-egl/' Qt5WaylandClient.pc
	$(SRC)/qt/FixPkgConfig.sh "$(PREFIX)/lib/pkgconfig/Qt5WaylandClient.pc"
	$(call pkg_static,"$(PREFIX)/lib/pkgconfig/Qt5WaylandClient.pc")
	touch $@

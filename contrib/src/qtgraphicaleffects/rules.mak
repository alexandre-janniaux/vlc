# QtGraphicalEffects

QTGE_VERSION_MAJOR := 5.12
QTGE_VERSION := $(QTGE_VERSION_MAJOR).2
QTGE_URL := http://download.qt.io/official_releases/qt/$(QTGE_VERSION_MAJOR)/$(QTGE_VERSION)/submodules/qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz

DEPS_qtgraphicaleffects += qtdeclarative $(DEPS_qtdeclarative)

ifdef HAVE_WIN32
PKGS += qtgraphicaleffects
endif

ifeq ($(call need_pkg,"Qt5QuickControls2"),)
PKGS_FOUND += qtgraphicaleffects
endif

QTGE_PC = Qt5QuickWidgets.pc

$(TARBALLS)/qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz:
	$(call download_pkg,$(QTGE_URL),qt)

.sum-qtgraphicaleffects: qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz

qtgraphicaleffects: qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz .sum-qtgraphicaleffects
	$(UNPACK)
	$(MOVE)

.qtgraphicaleffects: qtgraphicaleffects
	cd $< && $(PREFIX)/bin/qmake
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-effects-install_subtargets
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5QuickWidgets qml/QtGraphicalEffects qtgraphicaleffectsplugin
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5QuickWidgets qml/QtGraphicalEffects/private qtgraphicaleffectsprivate

	# Patch all pkgconfig files
	for pc_file in $(QTGE_PC); do \
		$(call pkg_static,"$(PREFIX)/lib/pkgconfig/$${pc_file}"); \
		$(SRC)/qt/FixPkgConfig.sh "$(PREFIX)/lib/pkgconfig/$${pc_file}"; \
	done

	touch $@

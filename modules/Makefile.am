noinst_LTLIBRARIES =
check_LTLIBRARIES =
pkglib_LTLIBRARIES =
noinst_HEADERS =
check_PROGRAMS =
pkglibexec_PROGRAMS =
EXTRA_DIST =

EXTRA_SUBDIRS = \
	hw/mmal

SUBDIRS = .
DIST_SUBDIRS = . $(EXTRA_SUBDIRS)
if HAVE_MMAL
SUBDIRS += hw/mmal
endif

TESTS =

dist_noinst_SCRIPTS = module.rc.in
EXTRA_LTLIBRARIES =

include common.am
include access/Makefile.am
include access/http/Makefile.am
include access/rtp/Makefile.am
include arm_neon/Makefile.am
include audio_filter/Makefile.am
include audio_mixer/Makefile.am
include audio_output/Makefile.am
include codec/Makefile.am
include control/Makefile.am
include demux/Makefile.am
include gui/Makefile.am
include hw/d3d9/Makefile.am
include hw/d3d11/Makefile.am
include hw/vaapi/Makefile.am
include hw/vdpau/Makefile.am
include keystore/Makefile.am
include logger/Makefile.am
include lua/Makefile.am
include meta_engine/Makefile.am
include misc/Makefile.am
include notify/Makefile.am
include packetizer/Makefile.am
include services_discovery/Makefile.am
include spu/Makefile.am
include stream_filter/Makefile.am
include stream_extractor/Makefile.am
include text_renderer/Makefile.am
include video_chroma/Makefile.am
include video_filter/Makefile.am
include video_splitter/Makefile.am
include video_output/Makefile.am
include visualization/Makefile.am
if ENABLE_SOUT
include access_output/Makefile.am
include mux/Makefile.am
include stream_out/Makefile.am
endif

BUILT_SOURCES += dummy.cpp

dummy.cpp:
	touch dummy.cpp

if HAVE_WIN32
BUILT_SOURCES += module.rc.lo
CLEANFILES += module.rc
#noinst_DATA = module.rc.lo

module.rc: module.rc.in $(top_builddir)/config.status
	$(AM_V_GEN) cd "$(top_builddir)" && $(SHELL) ./config.status --file="modules/$@"

module.rc.lo: module.rc
	$(AM_V_GEN) $(LIBTOOL) --tag=RC --mode=compile $(WINDRES) \
		--include-dir $(top_srcdir)/share \
		--include-dir $(top_srcdir)/extras/package/win32 \
		-i $< -o $@
endif

# Wayland
SUFFIXES += -client-protocol.h -protocol.c .xml

.xml-client-protocol.h:
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < "$^" > "$@.tmp"
	$(AM_V_at)mv -f -- "$@.tmp" "$@"

.xml-protocol.c:
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code < "$^" > "$@.tmp"
	$(AM_V_at)mv -f -- "$@.tmp" "$@"

ALL_LIBS_VAR = \
	$(access_LTLIBRARIES) \
	$(neon_LTLIBRARIES) \
	$(audio_filter_LTLIBRARIES) \
	$(audio_mixer_LTLIBRARIES) \
	$(aout_LTLIBRARIES) \
	$(codec_LTLIBRARIES) \
	$(control_LTLIBRARIES) \
	$(demux_LTLIBRARIES) \
	$(gui_LTLIBRARIES) \
	$(hw_LTLIBRARIES) \
	$(keystore_LTLIBRARIES) \
	$(logger_LTLIBRARIES) \
	$(lua_LTLIBRARIES) \
	$(meta_engine_LTLIBRARIES) \
	$(misc_LTLIBRARIES) \
	$(mux_LTLIBRARIES) \
	$(notify_LTLIBRARIES) \
	$(packetizer_LTLIBRARIES) \
	$(sd_LTLIBRARIES) \
	$(spu_LTLIBRARIES) \
	$(stream_extractor_LTLIBRARIES) \
	$(stream_filter_LTLIBRARIES) \
	$(sout_LTLIBRARIES) \
	$(text_LTLIBRARIES) \
	$(chroma_LTLIBRARIES) \
	$(video_filter_LTLIBRARIES) \
	$(vout_LTLIBRARIES) \
	$(splitter_LTLIBRARIES) \
	$(visu_LTLIBRARIES)


libvlc_static_modules_la_SOURCES = libvlc_static_modules.c
libvlc_static_modules_la_LIBADD = $(ALL_LIBS_VAR)

MODULES_LIST := $(patsubst lib%_plugin.la,%,$(ALL_LIBS_VAR))

libvlc_static_modules.c:
	echo $(MODULES_LIST)
	exit 1

EXTRA_LTLIBRARIES += libvlc_static_modules.la

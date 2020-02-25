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

all_plugins =

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
include hw/nvdec/Makefile.am
include hw/d3d9/Makefile.am
include hw/d3d11/Makefile.am
include hw/vaapi/Makefile.am
include hw/vdpau/Makefile.am
include hw/gbm/Makefile.am
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

all_plugins += \
	$(access_plugins) \
	$(neon_plugins) \
	$(audio_filter_plugins) \
	$(audio_mixer_plugins) \
	$(aout_plugins) \
	$(codec_plugins) \
	$(control_plugins) \
	$(demux_plugins) \
	$(gui_plugins) \
	$(nvdec_plugins) \
	$(d3d9_plugins) \
	$(d3d11_plugins) \
	$(vaapi_plugins) \
	$(vdpau_plugins) \
	$(gbm_plugins) \
	$(keystore_plugins) \
	$(logger_plugins) \
	$(lua_plugins) \
	$(meta_engine_plugins) \
	$(misc_plugins) \
	$(notify_plugins) \
	$(packetizer_plugins) \
	$(sd_plugins) \
	$(spu_plugins) \
	$(stream_filter_plugins) \
	$(stream_extractor_plugins) \
	$(text_plugins) \
	$(chroma_plugins) \
	$(video_filter_plugins) \
	$(splitter_plugins) \
	$(vout_plugins) \
	$(visualization_plugins) \
	$(access_out_plugins) \
	$(mux_plugins) \
	$(sout_plugins)

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

###
### Handling of plugin merging
###

plugin_manifests = $(patsubst %_plugin.la,%_plugin.manifest.h,$(all_plugins))
plugin_objects = $(patsubst %_plugin.la,%_plugin-partial.o,$(all_plugins))

%.manifest.h: %.la
	$(AM_V_GEN) libfile=`grep old_library $< | cut -d"'" -f2`; \
	symbols=`nm -g .libs/$$libfile`; \
	entry=`echo "$${symbols}" | grep vlc_entry__ | cut -d" " -f3 | head -n1`; \
	echo "VLC_PLUGIN($${entry})" > $@

plugins.manifest.h: $(plugin_manifests)
	$(AM_V_GEN) rm -f $@; \
	for manifest in $^; do \
		echo "#include \"$${manifest}\"" >> $@; \
	done

#if HAVE_ANDROID
#INCR_LD := $(LD) -r
#else

OBJCOPY ?= objcopy
LD ?= ld

INCR_LD := $(LD) -Ur
#endif

%-partial.o: %.la
	mkdir -p "$<-objects-objcopy/"
	libfile=`grep old_library $< | cut -d"'" -f2`; \
	cd "$<-objects-objcopy/" && ar x "../.libs/$${libfile}"
	objects=$$(find "$<-objects-objcopy/" -name "*.o"); \
	echo $${objects}; \
	$(INCR_LD) -o "$@" $${objects}
	$(OBJCOPY) --wildcard \
		--localize-hidden \
		--localize-symbol="!vlc_entry" \
		--localize-symbol="*" \
		--strip-symbol="vlc_entry_api_version*" \
		--strip-symbol="vlc_entry_copyright*" \
		--strip-symbol="vlc_entry_license*" \
		--localize-symbol=vlc_module_name \
		--strip-unneeded $@

AR_WRAPPER = "$(top_srcdir)/extras/package/android/ar.sh" $@
# Assignment is needed to evaluate the foreach command
# Enfore link variable on plugin targets

dependencies = $(patsubst %.la,%_la,$(all_plugins))
dep_dependencies = $(foreach plugin,$(dependencies),$($(plugin)_LIBADD))

libvlc_libs = libvlc_http.la libvlc_srtp.la libchroma_copy.la libavcodec_common.la libvlc_vaapi.la libxiph_metadata

BUILT_SOURCES += $(plugin_manifests) plugins.manifest.h
nodist_libvlcmodules_plugin_la_SOURCES = plugin.manifest.h
libvlcmodules_plugin_la_SOURCES = ../extras/package/android/plugin.c dummy.cpp
libvlcmodules_plugin_la_LIBADD = $(plugin_objects)
libvlcmodules_plugin_la_CFLAGS = -I$(builddir) -frtti -fexceptions
libvlcmodules_plugin_la_LDFLAGS = \
	-llua \
	$(filter-out $(libvlc_libs),$(dep_dependencies)) \
	-avoid-version -module -export-symbols-regex "^vlc_entry" \
	-shrext $(LIBEXT) -no-undefined \
	-rpath $(pluginsdir) \
	$(top_builddir)/compat/libcompat.la $(LTLIBVLCCORE)

plugins_LTLIBRARIES = libvlcmodules_plugin.la
noinst_LTLIBRARIES += $(all_plugins)

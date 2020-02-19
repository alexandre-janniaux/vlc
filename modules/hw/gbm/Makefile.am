gbmdir = $(pluginsdir)/gbm

libvlc_gbm_plugin_la_SOURCES = hw/gbm/gbm.c
libvlc_gbm_plugin_la_CFLAGS = $(GBM_CFLAGS)
libvlc_gbm_plugin_la_LDFLAGS = $(AM_LDFLAGS) -lEGL -lGLESv2 -lgbm

# TODO: find gbm
if HAVE_LINUX
if HAVE_ANDROID
#pkglib_LTLIBRARIES += libvlc_gbm_plugin.la
endif
endif

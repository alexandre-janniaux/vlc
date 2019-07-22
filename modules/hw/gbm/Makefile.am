gbmdir = $(pluginsdir)/gbm

libvlc_gbm_plugin_la_SOURCES = hw/gbm/gbm.c
libvlc_gbm_plugin_la_CFLAGS = $(GBM_CFLAGS)
libvlc_gbm_plugin_la_LDFLAGS = $(AM_LDFLAGS) -lEGL -lGLESv2 -lgbm

pkglib_LTLIBRARIES += libvlc_gbm_plugin.la

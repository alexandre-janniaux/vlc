visudir = $(pluginsdir)/visualization
visualization_plugins =

libglspectrum_plugin_la_SOURCES = \
	visualization/glspectrum.c \
	visualization/visual/fft.c visualization/visual/fft.h \
	visualization/visual/window.c visualization/visual/window.h \
	visualization/visual/window_presets.h
libglspectrum_plugin_la_LIBADD = $(GL_LIBS) $(LIBM)
if HAVE_GL
visualization_plugins += libglspectrum_plugin.la
endif

libgoom_plugin_la_SOURCES = visualization/goom.c
libgoom_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(GOOM_CFLAGS)
libgoom_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(visudir)'
libgoom_plugin_la_LIBADD = $(GOOM_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libgoom_plugin.la
visualization_plugins += $(LTLIBgoom)

libprojectm_plugin_la_SOURCES = visualization/projectm.cpp
libprojectm_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(PROJECTM_CFLAGS)
libprojectm_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(visudir)'
libprojectm_plugin_la_LIBADD = $(PROJECTM_LIBS)
EXTRA_LTLIBRARIES += libprojectm_plugin.la
visualization_plugins += $(LTLIBprojectm)

libvisual_plugin_la_SOURCES = \
	visualization/visual/visual.c visualization/visual/visual.h \
	visualization/visual/effects.c \
	visualization/visual/fft.c visualization/visual/fft.h \
	visualization/visual/window.c visualization/visual/window.h \
	visualization/visual/window_presets.h
libvisual_plugin_la_LIBADD = $(LIBM)
visualization_plugins += libvisual_plugin.la

libvsxu_plugin_la_SOURCES = visualization/vsxu.cpp visualization/cyclic_buffer.h
libvsxu_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(VSXU_CFLAGS)
libvsxu_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(visudir)'
libvsxu_plugin_la_LIBADD = $(VSXU_LIBS) $(LIBM)
EXTRA_LTLIBRARIES += libvsxu_plugin.la
visualization_plugins += $(LTLIBvsxu)

if HAVE_RPC
noinst_LTLIBRARIES = libvlcrpc_messages.la
endif

IDL_SOURCES = \
	stream.sidl.hh \
	stream.sidl.cpp \
	streamfactory.sidl.hh \
	streamfactory.sidl.cpp \
	demux.sidl.hh \
	demux.sidl.cpp \
	demuxfactory.sidl.hh \
	demuxfactory.sidl.cpp \
	esout.sidl.hh \
	esout.sidl.cpp

BUILT_SOURCES = $(IDL_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

libvlcrpc_messages_la_SOURCES = $(IDL_SOURCES)
libvlcrpc_messages_la_LIBADD = $(PROTORPC_LIBS)
libvlcrpc_messages_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++17
libvlcrpc_messages_la_CPPFLAGS = $(PROTORPC_CFLAGS)

%.sidl.cpp: $(srcdir)/%.sidl
	$(SIDLCC) --source-only $<

%.sidl.hh: $(srcdir)/%.sidl
	$(SIDLCC) --header-only $<

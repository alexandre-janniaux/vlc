if HAVE_RPC
noinst_LTLIBRARIES = libvlcrpc_messages.la
endif

IDL_SOURCES = \
	stream.sidl.hh \
	stream.sidl.cpp \
	streamcontrol.sidl.hh \
	streamcontrol.sidl.cpp \
	streamfactory.sidl.hh \
	streamfactory.sidl.cpp \
	demuxfactory.sidl.hh \
	demuxfactory.sidl.cpp \
	demux.sidl.hh \
	demux.sidl.cpp \
	esout.sidl.hh \
	esout.sidl.cpp

BUILT_SOURCES = $(IDL_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

libvlcrpc_messages_la_SOURCES = $(IDL_SOURCES)
libvlcrpc_messages_la_LIBADD = -lprotorpc
libvlcrpc_messages_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++17

%.sidl.cpp: $(srcdir)/%.sidl
	$(CONTRIB_DIR)/bin/sidlcc.py --source-only $<

%.sidl.hh: $(srcdir)/%.sidl
	$(CONTRIB_DIR)/bin/sidlcc.py --header-only $<

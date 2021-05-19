if HAVE_RPC
noinst_LTLIBRARIES = libvlcrpc.la
endif

libvlcrpc_la_SOURCES = broker.cpp esout.cpp
libvlcrpc_la_LIBADD = $(PROTORPC_LIBS) $(top_builddir)/rpc/libvlcrpc_messages.la
libvlcrpc_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++17 -I$(top_builddir)/rpc
libvlcrpc_la_CPPFLAGS = $(PROTORPC_CFLAGS)

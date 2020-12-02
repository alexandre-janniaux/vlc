if HAVE_RPC
noinst_LTLIBRARIES = libvlcrpc.la
endif

libvlcrpc_la_SOURCES = broker.cpp
libvlcrpc_la_LIBADD = -lprotorpc $(top_builddir)/rpc/libvlcrpc_messages.la
libvlcrpc_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++17 -I$(top_builddir)/rpc/include

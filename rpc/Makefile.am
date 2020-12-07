if HAVE_RPC
noinst_LTLIBRARIES = libvlcrpc_messages.la
endif

RPC_HEADER_FOLDER = $(builddir)/include/vlc_rpc

IDL_SOURCES = \
		$(RPC_HEADER_FOLDER)/stream.sidl.hh \
		stream.sidl.cpp \
		$(RPC_HEADER_FOLDER)/objectfactory.sidl.hh \
		objectfactory.sidl.cpp \
		$(RPC_HEADER_FOLDER)/streamcontrol.sidl.hh \
		streamcontrol.sidl.cpp

BUILT_SOURCES = $(IDL_SOURCES)

CLEANFILES = $(IDL_SOURCES)

libvlcrpc_messages_la_SOURCES = $(IDL_SOURCES)
libvlcrpc_messages_la_LIBADD = -lprotorpc
libvlcrpc_messages_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++17 -I$(RPC_HEADER_FOLDER)

$(RPC_HEADER_FOLDER):
	mkdir -p $(RPC_HEADER_FOLDER)

stream.sidl.cpp: $(srcdir)/stream.sidl
	$(CONTRIB_DIR)/bin/sidlcc.py --source-only -o $(builddir) $<

$(RPC_HEADER_FOLDER)/stream.sidl.hh: $(RPC_HEADER_FOLDER) $(srcdir)/stream.sidl
	$(CONTRIB_DIR)/bin/sidlcc.py --header-only -o $(RPC_HEADER_FOLDER) $(srcdir)/stream.sidl

objectfactory.sidl.cpp: $(srcdir)/objectfactory.sidl
	$(CONTRIB_DIR)/bin/sidlcc.py --source-only -o $(builddir) $<

$(RPC_HEADER_FOLDER)/objectfactory.sidl.hh: $(RPC_HEADER_FOLDER) $(srcdir)/objectfactory.sidl
	$(CONTRIB_DIR)/bin/sidlcc.py --header-only -o $(RPC_HEADER_FOLDER) $(srcdir)/objectfactory.sidl

streamcontrol.sidl.cpp: $(srcdir)/streamcontrol.sidl
	$(CONTRIB_DIR)/bin/sidlcc.py --source-only -o $(builddir) $<

$(RPC_HEADER_FOLDER)/streamcontrol.sidl.hh: $(RPC_HEADER_FOLDER) $(srcdir)/streamcontrol.sidl
	$(CONTRIB_DIR)/bin/sidlcc.py --header-only -o $(RPC_HEADER_FOLDER) $(srcdir)/streamcontrol.sidl

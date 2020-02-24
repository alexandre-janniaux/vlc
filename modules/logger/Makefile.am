loggerdir = $(pluginsdir)/logger
logger_plugins =

libconsole_logger_plugin_la_SOURCES = logger/console.c
libfile_logger_plugin_la_SOURCES = logger/file.c
logger_plugins += libconsole_logger_plugin.la libfile_logger_plugin.la

libsyslog_plugin_la_SOURCES = logger/syslog.c
if HAVE_SYSLOG
logger_plugins += libsyslog_plugin.la
endif

libsd_journal_plugin_la_SOURCES = logger/journal.c
libsd_journal_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SYSTEMD_CFLAGS)
libsd_journal_plugin_la_LIBADD = $(SYSTEMD_LIBS)
if HAVE_SYSTEMD
logger_plugins += libsd_journal_plugin.la
endif

libandroid_logger_plugin_la_SOURCES = logger/android.c
libandroid_logger_plugin_la_CFLAGS = $(AM_CFLAGS)
libandroid_logger_plugin_la_LIBADD = -llog
if HAVE_ANDROID
logger_plugins += libandroid_logger_plugin.la
endif

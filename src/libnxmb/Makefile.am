SOURCES = dispatcher.cpp filter.cpp main.cpp message.cpp subscriber.cpp

lib_LTLIBRARIES = libnxmb.la

libnxmb_la_SOURCES = $(SOURCES)
libnxmb_la_CPPFLAGS=-I@top_srcdir@/include -DLIBNXMB_EXPORTS
libnxmb_la_LDFLAGS = -release $(NETXMS_LIBRARY_VERSION)
libnxmb_la_LIBADD = @top_srcdir@/src/libnetxms/libnetxms.la

EXTRA_DIST = \
	Makefile.w32 \
	libnxmb.vcxproj libnxmb.vcxproj.filters \
	libnxmb.h

ACLOCAL_AMFLAGS = -I m4

include_HEADERS = include/libdevq.h

lib_LTLIBRARIES = libdevq.la

libdevq_la_SOURCES = src/device.c					\
		     src/device_drm.c \
		     src/event_monitor_@opsys@.c
libdevq_la_CPPFLAGS = -I$(top_srcdir)/include

if ENABLE_PROGRAMS
bin_PROGRAMS = devq_lsdri devq_evwatch
endif

devq_evwatch_SOURCES = tools/devq_evwatch/devq_evwatch.c
devq_evwatch_CPPFLAGS = -I$(top_srcdir)/include
devq_evwatch_LDADD = libdevq.la

devq_lsdri_SOURCES = tools/lsdri/lsdri.c
devq_lsdri_CPPFLAGS = -I$(top_srcdir)/include
devq_lsdri_LDADD = libdevq.la

pkgconfigdir = $(prefix)/libdata/pkgconfig
nodist_pkgconfig_DATA = src/libdevq-1.0.pc

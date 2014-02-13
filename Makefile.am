ACLOCAL_AMFLAGS = -I m4

include_HEADERS = include/libdevq.h

lib_LTLIBRARIES = libdevq.la

libdevq_la_SOURCES = src/device.c					\
		     src/device_drm.c
libdevq_la_CPPFLAGS = -I$(top_srcdir)/include
libdevq_la_LIBADD = -lprocstat

bin_PROGRAMS = lsdri

lsdri_SOURCES = tools/lsdri/lsdri.c
lsdri_CPPFLAGS = -I$(top_srcdir)/include
lsdri_LDADD = libdevq.la

pkgconfigdir = $(libdir)/pkgconfig
nodist_pkgconfig_DATA = src/libdevq-1.0.pc

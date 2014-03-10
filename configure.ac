AC_INIT([libdevq],[0.1.0],[https://github.com/freebsd/libdevq],[libdevq])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR([ac-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign subdir-objects no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])

LT_INIT()

AC_PROG_CC_STDC

AC_CHECK_HEADERS([libprocstat.h], [
	AC_SEARCH_LIBS([procstat_open_sysctl], [procstat])
	], [], [
	#include <sys/param.h>
	#include <sys/queue.h>
	#include <sys/socket.h>
	])

AC_CONFIG_FILES([
	Makefile
	src/libdevq-1.0.pc
	])

AC_OUTPUT

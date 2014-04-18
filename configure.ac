AC_INIT([libdevq],[0.0.1],[https://github.com/freebsd/libdevq],[libdevq])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR([ac-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11 foreign subdir-objects no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])

LT_INIT()

AC_PROG_CC_STDC

case $target_os in
freebsd*) opsys=freebsd ;;
*) opsys=$target_os ;;
esac

AC_SUBST([opsys])

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

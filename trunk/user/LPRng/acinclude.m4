define([ENABLE_BOOLEAN],[dnl
AC_ARG_ENABLE([$1],AS_HELP_STRING([$2],[$3]),[v="$enableval"],[v=$4])
AC_MSG_NOTICE([$5])
AS_IF([test "x$v" = "xyes"], [$6], [$7])
])
define([WITH_DIR],[dnl
AC_ARG_WITH([$1],AS_HELP_STRING([$2],[$3]),[$4="$withval"],[$4=$5])
AC_MSG_NOTICE([$6])
AC_SUBST($4)
])
dnl The following code is taken from "po.m4 serial 7 (gettext-0.14.3)"
dnl "gettext.m4 serial 37 (gettext-0.14.4)" and "nls.m4 serial 2 (gettext-0.14.3)"
dnl and mangled heavily to do a bare minimum.
dnl The original files state:
dnl # Copyright (C) 1995-2005 Free Software Foundation, Inc.
dnl # This file is free software; the Free Software Foundation
dnl # gives unlimited permission to copy and/or distribute it,
dnl # with or without modifications, as long as this notice is preserved.
dnl # Authors:
dnl #  Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl #  Bruno Haible <haible@clisp.cons.org>, 2000-2003.
AC_DEFUN([MY_GETTEXT],
[
AC_MSG_CHECKING([whether NLS is requested])
dnl Default is disabled NLS
AC_ARG_ENABLE(nls,AS_HELP_STRING([--enable-nls],[use Native Language Support]),
	USE_NLS=$enableval, USE_NLS=no)
AC_MSG_RESULT($USE_NLS)
AC_SUBST(USE_NLS)
dnl If we use NLS, test it
if test "$USE_NLS" = "yes"; then
        dnl If GNU gettext is available we use this. Fallback to external
	dnl library is not yet supported, but should be easy to request by just
	dnl adding the correct CFLAGS and LDFLAGS to ./configure
	dnl (note that gettext and ngettext must exist)

        AC_CACHE_CHECK([for GNU gettext in libc], gt_cv_func_gnugettext1_libc,
         [AC_TRY_LINK([#include <libintl.h>
extern int _nl_msg_cat_cntr;
extern int *_nl_domain_bindings;],
            [bindtextdomain ("", "");
return * gettext ("") + _nl_msg_cat_cntr + *_nl_domain_bindings],
            gt_cv_func_gnugettext1_libc=yes,
            gt_cv_func_gnugettext1_libc=no)])
	if test "$gt_cv_func_gnugettext1_libc" = "yes" ; then
		AC_CHECK_FUNC(ngettext,[
		AC_DEFINE(ENABLE_NLS, 1, [Define to 1 if translation of program messages to the user's native language is requested.])
		], [USE_NLS=no])
	else
		USE_NLS=no
	fi
fi
AC_MSG_CHECKING([whether to use NLS])
AC_MSG_RESULT([$USE_NLS])
dnl Perform the following tests also without --enable-nls, as
dnl they might be needed to generate the files (for make dist and so on)

dnl Search for GNU msgfmt in the PATH.
dnl The first test excludes Solaris msgfmt and early GNU msgfmt versions.
dnl The second test excludes FreeBSD msgfmt.
AM_PATH_PROG_WITH_TEST(MSGFMT, msgfmt,
  [$ac_dir/$ac_word --statistics /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1 &&
   (if $ac_dir/$ac_word --statistics /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi)],
  :)
AC_PATH_PROG(GMSGFMT, gmsgfmt, $MSGFMT)

dnl Search for GNU xgettext 0.12 or newer in the PATH.
dnl The first test excludes Solaris xgettext and early GNU xgettext versions.
dnl The second test excludes FreeBSD xgettext.
AM_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
  [$ac_dir/$ac_word --omit-header --copyright-holder= --msgid-bugs-address= /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1 &&
   (if $ac_dir/$ac_word --omit-header --copyright-holder= --msgid-bugs-address= /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi)],
  :)
dnl Remove leftover from FreeBSD xgettext call.
rm -f messages.po

dnl Search for GNU msgmerge 0.11 or newer in the PATH.
AM_PATH_PROG_WITH_TEST(MSGMERGE, msgmerge,
  [$ac_dir/$ac_word --update -q /dev/null /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1], :)
])
define([CHECK_PLUGINS],[dnl
AC_ARG_VAR(PLUGIN_LDFLAGS, [Additional LDFLAGS for plugins (default --shared)])
AC_ARG_VAR(PLUGIN_CFLAGS, [Additional CFLAGS for plugins (default -fPIC)])
AC_ARG_VAR(PLUGINUSER_LDFLAGS, [Additional LDFLAGS for a program loading plugins (default -export-dynamic)])
AC_ARG_ENABLE([plugins],AS_HELP_STRING([--enable-plugins],[build authentication modules as plugins]),[enable_plugins=$enableval],[enable_plugins=no])
DL_LIBS=""
if test $enable_plugins != no ; then
mysaved_LIBS="$LIBS"
AC_SEARCH_LIBS(dlopen, dl, [dnl
	if test "x$LIBS" != "x$mysaved_LIBS" ; then
		DL_LIBS="-ldl"
	fi
], [dnl
	if test $enable_plugins = default ; then
		enable_plugins=no
	else
		AC_MSG_ERROR([Plugins requested with --enable-plugins but did not find dlopen])
		enable_plugins=no
	fi
])
LIBS="$mysaved_LIBS"
fi
if test $enable_plugins != no ; then
if test "${PLUGIN_CFLAGS+set}" != set ; then
	PLUGIN_CFLAGS="-fPIC"
fi
if test "${PLUGIN_LDFLAGS+set}" != set ; then
	PLUGIN_LDFLAGS="--shared"
fi
if test "${PLUGINUSER_LDFLAGS+set}" != set ; then
	PLUGINUSER_LDFLAGS="-export-dynamic"
fi
AC_CACHE_CHECK([whether dynamic plugins can be generated and work],[my_cv_sys_shared_with_callback_works],[dnl
mysaved_LDFLAGS="$LDFLAGS"
mysaved_CFLAGS="$CFLAGS"
LDFLAGS="$LDFLAGS $PLUGIN_LDFLAGS"
CFLAGS="$CFLAGS $PLUGIN_CFLAGS"
AC_LINK_IFELSE([AC_LANG_SOURCE([[int test(void);int test(void) {return callback();}]])],
[cp conftest$ac_exeext libmyXYZtest.so || AC_MSG_ERROR([Internal error, perhaps autoconf changed soem internals])
 my_cv_sys_shared_with_callback_works=yes],
[my_cv_sys_shared_with_callback_works=no])
if test $my_cv_sys_shared_with_callback_works = yes ; then
	LDFLAGS="$mysaved_LDFLAGS $PLUGINUSER_LDFLAGS -L. -lmyXYZtest"
	CFLAGS="$mysaved_CFLAGS"
	AC_TRY_LINK([int callback(void) { return 17; } return test();], [my_cv_sys_shared_with_callback_works=yes], [my_cv_sys_shared_with_callback_works=no])
fi
LDFLAGS="$mysaved_LDFLAGS"
CFLAGS="$mysaved_CFLAGS"
rm -f libmyXYZtest.so]
)
if test $my_cv_sys_shared_with_callback_works = no ; then
	if test $enable_plugins = default ; then
		enable_plugins=no
	else
		AC_MSG_ERROR([Plugins requested with --enable-plugins but shared libraries cannot be created (might only work with gcc on linux yet, also make sure you have no -Wl,-z,defs or similar set)])
		enable_plugins=no
	fi
fi
fi
AM_CONDITIONAL(WITHPLUGINS, [test $enable_plugins != no])
if test $enable_plugins != no ; then
	AC_DEFINE([WITHPLUGINS], 1, [Build dynamic loadable plugins])
fi
AC_SUBST(DL_LIBS)
AC_SUBST(PLUGINUSER_LDFLAGS)
AC_SUBST(PLUGIN_LDFLAGS)
AC_SUBST(PLUGIN_CFLAGS)
])

dnl Process this file with autoconf to produce a configure script.
dnl Copyright 1998-2013 Glyph & Cog, LLC

AC_INIT([xpdf],[3.0.4],)

AC_CONFIG_HEADERS([_config.hh:_config.hh.ac])
AC_CONFIG_MACRO_DIRS([etc/m4])

AM_INIT_AUTOMAKE([-Wno-portability subdir-objects dist-bzip2 nostdinc foreign])
AM_SILENT_RULES([yes])

AC_PROG_CXX
AC_CONFIG_CXX_WARNINGS

AC_ENABLE_CXX_DIALECT([c++2a])

AC_PROG_LIBTOOL
AC_PROG_MAKE_SET(gmake)
AM_PROG_AS

AC_ARG_ENABLE([a4-paper],
  AS_HELP_STRING([--enable-a4-paper], [Use A4 size for PS output]))

AS_IF([test "x$enable_a4_paper" = "xyes"],[
  AC_DEFINE([A4_PAPER], [1], [Size of PS output])
])

AC_ARG_ENABLE([no-text-select],
  AS_HELP_STRING([--enable-no-text-select],[disable text selection]))

AS_IF([test "x$enable_no_text_select" = "xyes"],[
  AC_DEFINE([NO_TEXT_SELECT], [1], [Disable text selection])
])

AC_ARG_ENABLE([opi],
  AS_HELP_STRING([--enable-opi],[enable support for OPI comments]))

AS_IF([test "x$enable_opi" = "xyes"],[
  AC_DEFINE([OPI_SUPPORT], [1], [Enable support for OPI comments])
])

AC_ARG_ENABLE([multithreaded],
  AS_HELP_STRING([--enable-multithreaded],[enable multithreading support]))

AS_IF([test "x$enable_multithreaded" = "xyes"],[
  AC_DEFINE([MULTITHREADED], [1], [Enable multithreading support])])

AC_ARG_ENABLE([exceptions],
  AS_HELP_STRING([--enable-exceptions],[use C++ exceptions]))

AS_IF([test "x$enable_exceptions" = "xyes"],[
  AC_DEFINE([USE_EXCEPTIONS], [1], [Use C++ exceptions])
])

AC_ARG_ENABLE([fixedpoint],
  AS_HELP_STRING([--enable-fixedpoint],[use fixed point arithmetic]))

AS_IF([test "x$enable_fixedpoint" = "xyes"],[
  AC_DEFINE([USE_FIXEDPOINT], [1], [Use fixed-point arithmetic])
])

AC_ARG_ENABLE([cmyk],
  AS_HELP_STRING([--enable-cmyk],[enable support for CMYK rasterization]))

AS_IF([test "x$enable_cmyk" = "xyes"],[
  AC_DEFINE([SPLASH_CMYK],[1], [Enable support for CMYK rasterization])
])

AC_ARG_WITH([appdef-dir],
  AS_HELP_STRING([--with-appdef-dir],[set app-defaults directory]))

AS_IF([test "x$with_appdef_dir" != "xno"],[
  AC_DEFINE_UNQUOTED([APPDEFDIR], ["$with_appdef_dir"], [App defaults dir])
])

AC_PKG_CONFIG_WITH([libpng])
AC_PKG_CONFIG_WITH([freetype2])
AC_PKG_CONFIG_WITH([x11])
AC_PKG_CONFIG_WITH([xt])

AC_CONFIG_WITH_RANGE3
AC_CONFIG_WITH_PAPER
AC_CONFIG_WITH_MOTIF

BOOST_REQUIRE([1.69.0])

BOOST_FILESYSTEM
BOOST_IOSTREAMS
BOOST_PROGRAM_OPTIONS
BOOST_SYSTEM
BOOST_TEST

AM_CONDITIONAL([DARWIN],[test `uname` == Darwin])
AM_CONDITIONAL([LINUX], [test `uname` == Linux])

AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(goo/Makefile)
AC_CONFIG_FILES(fofi/Makefile)
AC_CONFIG_FILES(splash/Makefile)
AC_CONFIG_FILES(xpdf/Makefile)

AC_OUTPUT()

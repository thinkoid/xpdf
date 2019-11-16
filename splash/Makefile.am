# -*- mode: makefile; -*-

ACLOCAL_AMFLAGS = -I etc/m4

include $(top_srcdir)/Makefile.common

noinst_LIBRARIES = libsplash.a

libsplash_a_SOURCES =                           \
	Splash.cc                                   \
	SplashBitmap.cc                             \
	SplashClip.cc                               \
	SplashFTFont.cc                             \
	SplashFTFontEngine.cc                       \
	SplashFTFontFile.cc                         \
	SplashFont.cc                               \
	SplashFontEngine.cc                         \
	SplashFontFile.cc                           \
	SplashFontFileID.cc                         \
	SplashPath.cc                               \
	SplashPattern.cc                            \
	SplashScreen.cc                             \
	SplashState.cc                              \
	SplashXPath.cc                              \
	SplashXPathScanner.cc

libsplash_a_CPPFLAGS = $(AM_CPPFLAGS) $(FREETYPE2_CPPFLAGS)

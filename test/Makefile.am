# -*- mode: makefile -*-

EXTRA_DIST =

include $(top_srcdir)/Makefile.common
LIBS += $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

check_PROGRAMS = box_rotate
TESTS = $(check_PROGRAMS)

box_rotate_SOURCES = box_rotate.cc

LDADD =                                         \
	$(top_srcdir)/xpdf/libxpdf.a                \
	$(top_srcdir)/fofi/libfofi.a                \
	$(top_srcdir)/utils/libutils.a              \
	$(top_srcdir)/splash/libsplash.a            \
	$(LIBS)

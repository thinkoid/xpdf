# -*- mode: makefile -*-

EXTRA_DIST =

include $(top_srcdir)/Makefile.common
LIBS += $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

check_PROGRAMS = box_rotate box_segment asciihex_filter
TESTS = $(check_PROGRAMS)

box_rotate_SOURCES = box_rotate.cc
box_segment_SOURCES = box_segment.cc

asciihex_filter_SOURCES = asciihex_filter.cc
asciihex_filter_LDADD = $(LIBS)

LDADD =                                         \
	$(top_srcdir)/xpdf/libxpdf.a                \
	$(top_srcdir)/fofi/libfofi.a                \
	$(top_srcdir)/utils/libutils.a              \
	$(top_srcdir)/splash/libsplash.a            \
	$(LIBS)

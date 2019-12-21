# -*- mode: makefile -*-

EXTRA_DIST =

include $(top_srcdir)/Makefile.common
LIBS += $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

check_PROGRAMS = array_iterator
TESTS = $(check_PROGRAMS)

array_iterator_SOURCES = array_iterator.cc

LDADD =                                         \
	$(top_srcdir)/xpdf/libxpdf.a                \
	$(top_srcdir)/fofi/libfofi.a                \
	$(top_srcdir)/goo/libgoo.a                  \
	$(top_srcdir)/splash/libsplash.a            \
	$(LIBS)

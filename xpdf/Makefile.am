# -*- mode: makefile; -*-

ACLOCAL_AMFLAGS = -I etc/m4

include $(top_srcdir)/Makefile.common

noinst_LIBRARIES = libxpdf.a
libxpdf_a_SOURCES =                             \
	AcroForm.cc                                 \
	Annot.cc                                    \
	BuiltinFont.cc                              \
	BuiltinFontTables.cc                        \
	CMap.cc                                     \
	Catalog.cc                                  \
	CharCodeToUnicode.cc                        \
	CoreOutputDev.cc                            \
	Decrypt.cc                                  \
	Error.cc                                    \
	FontEncodingTables.cc                       \
	Form.cc                                     \
	Gfx.cc                                      \
	GfxFont.cc                                  \
	GfxState.cc                                 \
	GlobalParams.cc                             \
	JArithmeticDecoder.cc                       \
	JBIG2Stream.cc                              \
	JPXStream.cc                                \
	Lexer.cc                                    \
	Link.cc                                     \
	NameToCharCode.cc                           \
	OptionalContent.cc                          \
	Outline.cc                                  \
	OutputDev.cc                                \
	PDFCore.cc                                  \
	PDFDoc.cc                                   \
	PDFDocEncoding.cc                           \
	PSOutputDev.cc                              \
	PSTokenizer.cc                              \
	Page.cc                                     \
	Parser.cc                                   \
	PreScanOutputDev.cc                         \
	SecurityHandler.cc                          \
	SplashOutputDev.cc                          \
	Stream.cc                                   \
	TextBlock.cc                                \
	TextFontInfo.cc                             \
	TextLine.cc                                 \
	TextOutputDev.cc                            \
	TextOutputDev.cc                            \
	TextPage.cc                                 \
	TextPageSegment.cc                          \
	TextString.cc                               \
	TextString.cc                               \
	TextWord.cc                                 \
	UnicodeMap.cc                               \
	UnicodeTypeTable.cc                         \
	XFAForm.cc                                  \
	XRef.cc                                     \
	Zoox.cc                                     \
	bitpack.cc                                  \
	dict.cc                                     \
	function.cc                                 \
	obj.cc                                      \
	parser/ctype.cc                             \
	xpdf.cc

bin_PROGRAMS = xpdf3 xpdf4

xpdf3_SOURCES = xpdf3.cc                        \
   XPDFApp.cc                                   \
   XPDFAppRes.cc                                \
   XPDFCore.cc                                  \
   XPDFTree.cc                                  \
   XPDFUI.cc

xpdf4_SOURCES = xpdf4.cc

AM_LDFLAGS += $(FREETYPE2_CPPFLAGS)

LDADD = libxpdf.a                                       \
	$(top_srcdir)/fofi/libfofi.a                        \
	$(top_srcdir)/utils/libutils.a                      \
	$(top_srcdir)/splash/libsplash.a                    \
	$(LIBS) $(FREETYPE2_LIBS)                           \
	$(MOTIF_LIBS) $(XPM_LIBS) $(XT_LIBS) $(X11_LIBS)    \
	$(LIBPNG_LIBS) $(PAPER_LIBS)                        \
	$(FMT_LIBS)

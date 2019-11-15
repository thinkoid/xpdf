# -*- mode: makefile; -*-

ACLOCAL_AMFLAGS = -I etc/m4

include $(top_srcdir)/Makefile.common

bin_PROGRAMS = xpdf                             \
	pdfdetach                                   \
	pdffonts                                    \
	pdfimages                                   \
	pdfinfo                                     \
	pdftohtml                                   \
	pdftopng                                    \
	pdftoppm                                    \
	pdftops                                     \
	pdftotext

SRCS =                                          \
	AcroForm.cc                                 \
	Annot.cc                                    \
	Array.cc                                    \
	BuiltinFont.cc                              \
	BuiltinFontTables.cc                        \
	CMap.cc                                     \
	Catalog.cc                                  \
	CharCodeToUnicode.cc                        \
	Decrypt.cc                                  \
	Dict.cc                                     \
	Error.cc                                    \
	FontEncodingTables.cc                       \
	Form.cc                                     \
	Function.cc                                 \
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
	Object.cc                                   \
	OptionalContent.cc                          \
	Outline.cc                                  \
	OutputDev.cc                                \
	PDFDoc.cc                                   \
	PDFDocEncoding.cc                           \
	PSTokenizer.cc                              \
	Page.cc                                     \
	Parser.cc                                   \
	SecurityHandler.cc                          \
	Stream.cc                                   \
	TextString.cc                               \
	UnicodeMap.cc                               \
	XFAForm.cc                                  \
	XRef.cc                                     \
	XpdfPluginAPI.cc                            \
	Zoox.cc

xpdf_SOURCES = $(SRCS)                          \
	CoreOutputDev.cc                            \
	PDFCore.cc                                  \
	PSOutputDev.cc                              \
	PreScanOutputDev.cc                         \
	SplashOutputDev.cc                          \
	TextOutputDev.cc                            \
	UnicodeTypeTable.cc                         \
	XPDFApp.cc                                  \
	XPDFCore.cc                                 \
	XPDFTree.cc                                 \
	XPDFViewer.cc                               \
	xpdf.cc

pdftops_SOURCES = $(SRCS)                       \
	PreScanOutputDev.cc                         \
	PSOutputDev.cc                              \
	SplashOutputDev.cc                          \
	pdftops.cc

pdftohtml_SOURCES = $(SRCS)                     \
	HTMLGen.cc                                  \
	SplashOutputDev.cc                          \
	TextOutputDev.cc                            \
	UnicodeTypeTable.cc                         \
	pdftohtml.cc

pdfinfo_SOURCES = $(SRCS) pdfinfo.cc
pdffonts_SOURCES = $(SRCS) pdffonts.cc
pdfdetach_SOURCES = $(SRCS) pdfdetach.cc

pdftoppm_SOURCES = $(SRCS)                      \
	SplashOutputDev.cc                          \
	TextOutputDev.cc                            \
	UnicodeTypeTable.cc                         \
	pdftoppm.cc

pdftopng_SOURCES = $(SRCS)                      \
	SplashOutputDev.cc                          \
	TextOutputDev.cc                            \
	UnicodeTypeTable.cc                         \
	pdftopng.cc

pdfimages_SOURCES = $(SRCS)                     \
	pdfimages.cc                                \
	ImageOutputDev.cc

pdftotext_SOURCES = $(SRCS)						\
	pdftotext.cc								\
	TextOutputDev.cc							\
	UnicodeTypeTable.cc

LDADD =                                         \
	$(top_srcdir)/fofi/libfofi.a                \
	$(top_srcdir)/goo/libgoo.a                  \
	$(top_srcdir)/splash/libsplash.a            \
	$(LIBS) $(MOTIF_LIBS) $(XT_LIBS) $(X11_LIBS) $(LIBPNG_LIBS)


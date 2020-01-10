# -*- mode: makefile; -*-

ACLOCAL_AMFLAGS = -I etc/m4

include $(top_srcdir)/Makefile.common

noinst_LIBRARIES = libxpdf.a
libxpdf_a_SOURCES =                             \
   AcroForm.cc                                  \
   Annot.cc                                     \
   Array.cc                                     \
   BuiltinFont.cc                               \
   BuiltinFontTables.cc                         \
   CMap.cc                                      \
   Catalog.cc                                   \
   CharCodeToUnicode.cc                         \
   CoreOutputDev.cc                             \
   Decrypt.cc                                   \
   Dict.cc                                      \
   Error.cc                                     \
   FontEncodingTables.cc                        \
   Form.cc                                      \
   Gfx.cc                                       \
   GfxFont.cc                                   \
   GfxState.cc                                  \
   GlobalParams.cc                              \
   HTMLGen.cc                                   \
   ImageOutputDev.cc                            \
   JArithmeticDecoder.cc                        \
   JBIG2Stream.cc                               \
   JPXStream.cc                                 \
   Lexer.cc                                     \
   Link.cc                                      \
   NameToCharCode.cc                            \
   Object.cc                                    \
   OptionalContent.cc                           \
   Outline.cc                                   \
   OutputDev.cc                                 \
   PDFCore.cc                                   \
   PDFDoc.cc                                    \
   PDFDocEncoding.cc                            \
   PSOutputDev.cc                               \
   PSTokenizer.cc                               \
   Page.cc                                      \
   Parser.cc                                    \
   PreScanOutputDev.cc                          \
   SecurityHandler.cc                           \
   SplashOutputDev.cc                           \
   Stream.cc                                    \
   TextOutputDev.cc                             \
   TextString.cc                                \
   UnicodeMap.cc                                \
   UnicodeTypeTable.cc                          \
   XFAForm.cc                                   \
   XPDFApp.cc                                   \
   XPDFAppRes.c                                 \
   XPDFCore.cc                                  \
   XPDFTree.cc                                  \
   XPDFViewer.cc                                \
   XRef.cc                                      \
   XpdfPluginAPI.cc                             \
   Zoox.cc                                      \
   bitpack.cc                                   \
   function.cc

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

xpdf_SOURCES = xpdf.cc
pdfdetach_SOURCES = pdfdetach.cc
pdffonts_SOURCES = pdffonts.cc
pdfimages_SOURCES = pdfimages.cc
pdfinfo_SOURCES = pdfinfo.cc
pdftohtml_SOURCES = pdftohtml.cc
pdftopng_SOURCES = pdftopng.cc
pdftoppm_SOURCES = pdftoppm.cc
pdftops_SOURCES = pdftops.cc
pdftotext_SOURCES = pdftotext.cc

AM_LDFLAGS += $(FREETYPE2_CPPFLAGS)

LDADD = libxpdf.a                                       \
	$(top_srcdir)/fofi/libfofi.a                        \
	$(top_srcdir)/goo/libgoo.a                          \
	$(top_srcdir)/splash/libsplash.a                    \
	$(LIBS) $(FREETYPE2_LIBS)                           \
	$(MOTIF_LIBS) $(XPM_LIBS) $(XT_LIBS) $(X11_LIBS)    \
	$(LIBPNG_LIBS) $(PAPER_LIBS)


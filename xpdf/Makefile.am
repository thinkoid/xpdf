# -*- mode: makefile; -*-

ACLOCAL_AMFLAGS = -I etc/m4

include $(top_srcdir)/Makefile.common

noinst_LIBRARIES = libxpdf.a
libxpdf_a_SOURCES =                             \
   AcroForm.cc                                  \
   Annot.cc                                     \
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
   Link.cc                                      \
   NameToCharCode.cc                            \
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
   XRef.cc                                      \
   Zoox.cc                                      \
   array.cc                                     \
   bitpack.cc                                   \
   function.cc                                  \
   lexer.cc                                     \
   obj.cc                                       \
   xpdf.cc

bin_PROGRAMS = xpdfview
#   pdfdetach
#   pdffonts
#   pdfimages
#   pdfinfo
#   pdftohtml
#   pdftopng
#   pdftoppm
#   pdftops
#   pdftotext

xpdfview_SOURCES = xpdfview.cc                  \
   XPDFApp.cc                                   \
   XPDFAppRes.cc                                \
   XPDFCore.cc                                  \
   XPDFTree.cc                                  \
   XPDFUI.cc

#pdfdetach_SOURCES = pdfdetach.cc
#pdffonts_SOURCES = pdffonts.cc
#pdfimages_SOURCES = pdfimages.cc
#pdfinfo_SOURCES = pdfinfo.cc
#pdftohtml_SOURCES = pdftohtml.cc
#pdftopng_SOURCES = pdftopng.cc
#pdftoppm_SOURCES = pdftoppm.cc
#pdftops_SOURCES = pdftops.cc
#pdftotext_SOURCES = pdftotext.cc

AM_LDFLAGS += $(FREETYPE2_CPPFLAGS)

LDADD = libxpdf.a                                       \
	$(top_srcdir)/fofi/libfofi.a                        \
	$(top_srcdir)/goo/libgoo.a                          \
	$(top_srcdir)/splash/libsplash.a                    \
	$(LIBS) $(FREETYPE2_LIBS)                           \
	$(MOTIF_LIBS) $(XPM_LIBS) $(XT_LIBS) $(X11_LIBS)    \
	$(LIBPNG_LIBS) $(PAPER_LIBS)                        \
	$(FMT_LIBS)

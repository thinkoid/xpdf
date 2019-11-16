// -*- mode: c++; -*-

#ifndef XPDF_CONFIG_HH
#define XPDF_CONFIG_HH

#include <_config.hh>

#define XPDF_PDF_VERSION 1.7

#define XPDF_PDF_MAJOR_VERSION 1
#define XPDF_PDF_MINOR_VERSION 7

#define XPDF_COPYRIGHT "Copyright 1996-2014 Glyph & Cog, LLC"

//------------------------------------------------------------------------
// paper size
//------------------------------------------------------------------------

// default paper size (in points) for PostScript output
#ifdef A4_PAPER
#  define XPDF_PAPER_WIDTH  595    // ISO A4 (210x297 mm)
#  define XPDF_PAPER_HEIGHT 842
#else
#  define XPDF_PAPER_WIDTH  612    // American letter (8.5x11")
#  define XPDF_PAPER_HEIGHT 792
#endif

//------------------------------------------------------------------------
// config file (xpdfrc) path
//------------------------------------------------------------------------

// user config file name, relative to the user's home directory
#define XPDF_XPDFRC ".xpdfrc"

//------------------------------------------------------------------------
// X-related constants
//------------------------------------------------------------------------

// default maximum size of color cube to allocate
#define XPDF_RGBCUBE_MAX 5

#endif // XPDF_CONFIG_HH

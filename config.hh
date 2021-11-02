// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_CONFIG_HH
#define XPDF_CONFIG_HH

// Autoconf-like macros
#define PACKAGE "xpdf"
#define PACKAGE_NAME "xpdf"
#define PACKAGE_STRING "xpdf 3.0.4"
#define PACKAGE_TARNAME "xpdf"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "3.0.4"
#define VERSION "3.0.4"

#define XPDF_PDF_VERSION 1.7

#define XPDF_PDF_MAJOR_VERSION 1
#define XPDF_PDF_MINOR_VERSION 7

#define XPDF_COPYRIGHT "Copyright 1996-2014 Glyph & Cog, LLC"

//------------------------------------------------------------------------
// paper size
//------------------------------------------------------------------------

// default paper size (in points) for PostScript output
#ifdef A4_PAPER
#define XPDF_PAPER_WIDTH 595 // ISO A4 (210x297 mm)
#define XPDF_PAPER_HEIGHT 842
#else
#define XPDF_PAPER_WIDTH 612 // American letter (8.5x11")
#define XPDF_PAPER_HEIGHT 792
#endif

//------------------------------------------------------------------------
// X-related constants
//------------------------------------------------------------------------

// default maximum size of color cube to allocate
#define XPDF_RGBCUBE_MAX 5

#endif // XPDF_CONFIG_HH

// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHGLYPHBITMAP_HH
#define XPDF_SPLASH_SPLASHGLYPHBITMAP_HH

#include <defs.hh>

//------------------------------------------------------------------------
// SplashGlyphBitmap
//------------------------------------------------------------------------

struct SplashGlyphBitmap
{
    int  x, y, w, h; // offset and size of glyph
    bool aa; // anti-aliased: true means 8-bit alpha
        //   bitmap; false means 1-bit
    unsigned char *data; // bitmap data
    bool           freeData; // true if data memory should be freed
};

#endif // XPDF_SPLASH_SPLASHGLYPHBITMAP_HH

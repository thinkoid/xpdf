//========================================================================
//
// SplashFTFont.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#ifndef SPLASHFTFONT_H
#define SPLASHFTFONT_H

#include <defs.hh>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <splash/SplashFont.hh>

class SplashFTFontFile;

//------------------------------------------------------------------------
// SplashFTFont
//------------------------------------------------------------------------

class SplashFTFont : public SplashFont {
public:
    SplashFTFont (
        SplashFTFontFile* fontFileA, SplashCoord* matA, SplashCoord* textMatA);

    virtual ~SplashFTFont ();

    // Munge xFrac and yFrac before calling SplashFont::getGlyph.
    virtual GBool
    getGlyph (int c, int xFrac, int yFrac, SplashGlyphBitmap* bitmap);

    // Rasterize a glyph.  The <xFrac> and <yFrac> values are the same
    // as described for getGlyph.
    virtual GBool
    makeGlyph (int c, int xFrac, int yFrac, SplashGlyphBitmap* bitmap);

    // Return the path for a glyph.
    virtual SplashPath* getGlyphPath (int c);

private:
    FT_Size sizeObj;
    FT_Matrix matrix;
    FT_Matrix textMatrix;
    SplashCoord textScale;
};

#endif

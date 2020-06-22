// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHFTFONT_HH
#define XPDF_SPLASH_SPLASHFTFONT_HH

#include <defs.hh>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <splash/SplashFont.hh>

class SplashFTFontFile;

//------------------------------------------------------------------------
// SplashFTFont
//------------------------------------------------------------------------

class SplashFTFont : public SplashFont
{
public:
    SplashFTFont(SplashFTFontFile *fontFileA, SplashCoord *matA,
                 SplashCoord *textMatA);

    virtual ~SplashFTFont();

    // Munge xFrac and yFrac before calling SplashFont::getGlyph.
    virtual bool getGlyph(int c, int xFrac, int yFrac, SplashGlyphBitmap *bitmap);

    // Rasterize a glyph.  The <xFrac> and <yFrac> values are the same
    // as described for getGlyph.
    virtual bool makeGlyph(int c, int xFrac, int yFrac,
                           SplashGlyphBitmap *bitmap);

    // Return the path for a glyph.
    virtual SplashPath *getGlyphPath(int c);

private:
    FT_Size     sizeObj;
    FT_Matrix   matrix;
    FT_Matrix   textMatrix;
    SplashCoord textScale;
};

#endif // XPDF_SPLASH_SPLASHFTFONT_HH

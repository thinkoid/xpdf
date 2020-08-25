// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHFTFONTENGINE_HH
#define XPDF_SPLASH_SPLASHFTFONTENGINE_HH

#include <defs.hh>

#include <ft2build.h>
#include FT_FREETYPE_H

class SplashFontFile;
class SplashFontFileID;

//------------------------------------------------------------------------
// SplashFTFontEngine
//------------------------------------------------------------------------

class SplashFTFontEngine
{
public:
    static SplashFTFontEngine *init(bool aaA, unsigned flagsA);

    ~SplashFTFontEngine();

    // Load fonts.
    SplashFontFile *loadType1Font(SplashFontFileID *idA, GString *fontBuf,
                                  const char **enc);
    SplashFontFile *loadType1CFont(SplashFontFileID *idA, GString *fontBuf,
                                   const char **enc);
    SplashFontFile *loadOpenTypeT1CFont(SplashFontFileID *idA, GString *fontBuf,
                                        const char **enc);
    SplashFontFile *loadCIDFont(SplashFontFileID *idA, GString *fontBuf);
    SplashFontFile *loadOpenTypeCFFFont(SplashFontFileID *idA, GString *fontBuf,
                                        int *codeToGID, int codeToGIDLen);
    SplashFontFile *loadTrueTypeFont(SplashFontFileID *idA, GString *fontBuf,
                                     int fontNum, int *codeToGID,
                                     int codeToGIDLen);

private:
    SplashFTFontEngine(bool aaA, unsigned flagsA, FT_Library libA);

    bool       aa;
    unsigned   flags;
    FT_Library lib;
    bool       useCIDs;

    friend class SplashFTFontFile;
    friend class SplashFTFont;
};

#endif // XPDF_SPLASH_SPLASHFTFONTENGINE_HH

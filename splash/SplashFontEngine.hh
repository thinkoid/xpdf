// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHFONTENGINE_HH
#define XPDF_SPLASH_SPLASHFONTENGINE_HH

#include <defs.hh>

class GString;

class SplashFTFontEngine;
class SplashDTFontEngine;
class SplashDT4FontEngine;
class SplashFontFile;
class SplashFontFileID;
class SplashFont;

//------------------------------------------------------------------------

#define splashFontCacheSize 16
#define splashFTNoHinting (1 << 0)

//------------------------------------------------------------------------
// SplashFontEngine
//------------------------------------------------------------------------

class SplashFontEngine {
public:
    // Create a font engine.
    SplashFontEngine (bool enableFreeType, unsigned freeTypeFlags, bool aa);

    ~SplashFontEngine ();

    // Get a font file from the cache.  Returns NULL if there is no
    // matching entry in the cache.
    SplashFontFile* getFontFile (SplashFontFileID* id);

    // Load fonts - these create new SplashFontFile objects.
    SplashFontFile* loadType1Font (
        SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBuf,
#else
        const char* fileName, bool deleteFile,
#endif
        const char** enc);
    SplashFontFile* loadType1CFont (
        SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBuf,
#else
        const char* fileName, bool deleteFile,
#endif
        const char** enc);
    SplashFontFile* loadOpenTypeT1CFont (
        SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBuf,
#else
        const char* fileName, bool deleteFile,
#endif
        const char** enc);
    SplashFontFile* loadCIDFont (
        SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBuf
#else
        const char* fileName, bool deleteFile
#endif
    );
    SplashFontFile* loadOpenTypeCFFFont (
        SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBuf,
#else
        const char* fileName, bool deleteFile,
#endif
        int* codeToGID, int codeToGIDLen);
    SplashFontFile* loadTrueTypeFont (
        SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBuf,
#else
        const char* fileName, bool deleteFile,
#endif
        int fontNum, int* codeToGID, int codeToGIDLen, const char* fontName);

    // Get a font - this does a cache lookup first, and if not found,
    // creates a new SplashFont object and adds it to the cache.  The
    // matrix, mat = textMat * ctm:
    //    [ mat[0] mat[1] ]
    //    [ mat[2] mat[3] ]
    // specifies the font transform in PostScript style:
    //    [x' y'] = [x y] * mat
    // Note that the Splash y axis points downward.
    SplashFont*
    getFont (SplashFontFile* fontFile, SplashCoord* textMat, SplashCoord* ctm);

private:
    SplashFont* fontCache[splashFontCacheSize];
    SplashFTFontEngine* ftEngine;
};

#endif // XPDF_SPLASH_SPLASHFONTENGINE_HH

//========================================================================
//
// SplashFTFontEngine.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#ifndef SPLASHFTFONTENGINE_H
#define SPLASHFTFONTENGINE_H

#include <defs.hh>

#include <ft2build.h>
#include FT_FREETYPE_H
class GString;

class SplashFontFile;
class SplashFontFileID;

//------------------------------------------------------------------------
// SplashFTFontEngine
//------------------------------------------------------------------------

class SplashFTFontEngine {
public:
    static SplashFTFontEngine* init (bool aaA, unsigned flagsA);

    ~SplashFTFontEngine ();

    // Load fonts.
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
        int fontNum, int* codeToGID, int codeToGIDLen);

private:
    SplashFTFontEngine (bool aaA, unsigned flagsA, FT_Library libA);

    bool aa;
    unsigned flags;
    FT_Library lib;
    bool useCIDs;

    friend class SplashFTFontFile;
    friend class SplashFTFont;
};

#endif

// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef SPLASHFTFONTFILE_H
#define SPLASHFTFONTFILE_H

#include <defs.hh>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <splash/SplashFontFile.hh>

class SplashFontFileID;
class SplashFTFontEngine;

//------------------------------------------------------------------------
// SplashFTFontFile
//------------------------------------------------------------------------

class SplashFTFontFile : public SplashFontFile {
public:
    static SplashFontFile* loadType1Font (
        SplashFTFontEngine* engineA, SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBufA,
#else
        const char* fileNameA, bool deleteFileA,
#endif
        const char** encA, bool useLightHintingA);
    static SplashFontFile* loadCIDFont (
        SplashFTFontEngine* engineA, SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBufA,
#else
        const char* fileNameA, bool deleteFileA,
#endif
        int* codeToGIDA, int codeToGIDLenA);
    static SplashFontFile* loadTrueTypeFont (
        SplashFTFontEngine* engineA, SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBufA,
#else
        const char* fileNameA, bool deleteFileA,
#endif
        int fontNum, int* codeToGIDA, int codeToGIDLenA);

    virtual ~SplashFTFontFile ();

    // Create a new SplashFTFont, i.e., a scaled instance of this font
    // file.
    virtual SplashFont* makeFont (SplashCoord* mat, SplashCoord* textMat);

private:
    SplashFTFontFile (
        SplashFTFontEngine* engineA, SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBufA,
#else
        const char* fileNameA, bool deleteFileA,
#endif
        FT_Face faceA, int* codeToGIDA, int codeToGIDLenA, bool trueTypeA,
        bool useLightHintingA);

    SplashFTFontEngine* engine;
    FT_Face face;
    int* codeToGID;
    int codeToGIDLen;
    bool trueType;
    bool useLightHinting;

    friend class SplashFTFont;
};

#endif

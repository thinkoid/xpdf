// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHFONTFILE_HH
#define XPDF_SPLASH_SPLASHFONTFILE_HH

#include <defs.hh>

#include <splash/SplashTypes.hh>

class GString;

class SplashFontEngine;
class SplashFont;
class SplashFontFileID;

//------------------------------------------------------------------------
// SplashFontFile
//------------------------------------------------------------------------

class SplashFontFile {
public:
    virtual ~SplashFontFile ();

    // Create a new SplashFont, i.e., a scaled instance of this font
    // file.
    virtual SplashFont* makeFont (SplashCoord* mat, SplashCoord* textMat) = 0;

    // Get the font file ID.
    SplashFontFileID* getID () { return id; }

    // Increment the reference count.
    void incRefCnt ();

    // Decrement the reference count.  If the new value is zero, delete
    // the SplashFontFile object.
    void decRefCnt ();

protected:
    SplashFontFile (
        SplashFontFileID* idA,
#if LOAD_FONTS_FROM_MEM
        GString* fontBufA
#else
        const char* fileNameA, bool deleteFileA
#endif
    );

    SplashFontFileID* id;
#if LOAD_FONTS_FROM_MEM
    GString* fontBuf;
#else
    GString* fileName;
    bool deleteFile;
#endif
    int refCnt;

    friend class SplashFontEngine;
};

#endif // XPDF_SPLASH_SPLASHFONTFILE_HH

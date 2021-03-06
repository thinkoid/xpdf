// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <unistd.h>

#include <utils/string.hh>

#include <splash/SplashFontFile.hh>
#include <splash/SplashFontFileID.hh>

//------------------------------------------------------------------------
// SplashFontFile
//------------------------------------------------------------------------

SplashFontFile::SplashFontFile(SplashFontFileID *idA,
#if LOAD_FONTS_FROM_MEM
                               GString *fontBufA
#else
                               const char *fileNameA, bool deleteFileA
#endif
)
{
    id = idA;
#if LOAD_FONTS_FROM_MEM
    fontBuf = fontBufA;
#else
    fileName = new GString(fileNameA);
    deleteFile = deleteFileA;
#endif
    refCnt = 0;
}

SplashFontFile::~SplashFontFile()
{
#if LOAD_FONTS_FROM_MEM
    delete fontBuf;
#else
    if (deleteFile) {
        unlink(fileName->c_str());
    }
    delete fileName;
#endif
    delete id;
}

void SplashFontFile::incRefCnt()
{
    ++refCnt;
}

void SplashFontFile::decRefCnt()
{
    if (!--refCnt) {
        delete this;
    }
}

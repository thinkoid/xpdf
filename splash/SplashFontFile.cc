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

SplashFontFile::SplashFontFile(SplashFontFileID *idA, GString *fontBufA)
{
    id = idA;
    fontBuf = fontBufA;
    refCnt = 0;
}

SplashFontFile::~SplashFontFile()
{
    delete fontBuf;
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

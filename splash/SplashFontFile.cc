//========================================================================
//
// SplashFontFile.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stdio.h>
#include <unistd.h>

#include <goo/GString.hh>

#include <splash/SplashFontFile.hh>
#include <splash/SplashFontFileID.hh>

//------------------------------------------------------------------------
// SplashFontFile
//------------------------------------------------------------------------

SplashFontFile::SplashFontFile(SplashFontFileID *idA,
#if LOAD_FONTS_FROM_MEM
			       GString *fontBufA
#else
			       char *fileNameA, GBool deleteFileA
#endif
			       ) {
  id = idA;
#if LOAD_FONTS_FROM_MEM
  fontBuf = fontBufA;
#else
  fileName = new GString(fileNameA);
  deleteFile = deleteFileA;
#endif
  refCnt = 0;
}

SplashFontFile::~SplashFontFile() {
#if LOAD_FONTS_FROM_MEM
  delete fontBuf;
#else
  if (deleteFile) {
    unlink(fileName->getCString());
  }
  delete fileName;
#endif
  delete id;
}

void SplashFontFile::incRefCnt() {
  ++refCnt;
}

void SplashFontFile::decRefCnt() {
  if (!--refCnt) {
    delete this;
  }
}

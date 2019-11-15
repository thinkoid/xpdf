//========================================================================
//
// SplashFontFileID.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#ifndef SPLASHFONTFILEID_H
#define SPLASHFONTFILEID_H

#include <config.hh>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <goo/gtypes.hh>

//------------------------------------------------------------------------
// SplashFontFileID
//------------------------------------------------------------------------

class SplashFontFileID {
public:

  SplashFontFileID();
  virtual ~SplashFontFileID();
  virtual GBool matches(SplashFontFileID *id) = 0;
};

#endif

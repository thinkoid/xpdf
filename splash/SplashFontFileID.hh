// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHFONTFILEID_HH
#define XPDF_SPLASH_SPLASHFONTFILEID_HH

#include <defs.hh>

//------------------------------------------------------------------------
// SplashFontFileID
//------------------------------------------------------------------------

class SplashFontFileID {
public:
    SplashFontFileID ();
    virtual ~SplashFontFileID ();
    virtual bool matches (SplashFontFileID* id) = 0;
};

#endif // XPDF_SPLASH_SPLASHFONTFILEID_HH

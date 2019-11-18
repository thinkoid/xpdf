//========================================================================
//
// SplashFontFileID.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#ifndef SPLASHFONTFILEID_H
#define SPLASHFONTFILEID_H

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

#endif

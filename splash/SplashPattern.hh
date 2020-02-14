// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHPATTERN_HH
#define XPDF_SPLASH_SPLASHPATTERN_HH

#include <defs.hh>

#include <splash/SplashTypes.hh>

class SplashScreen;

//------------------------------------------------------------------------
// SplashPattern
//------------------------------------------------------------------------

class SplashPattern {
public:
    SplashPattern ();

    virtual SplashPattern* copy () = 0;

    virtual ~SplashPattern ();

    // Return the color value for a specific pixel.
    virtual void getColor (int x, int y, SplashColorPtr c) = 0;

    // Returns true if this pattern object will return the same color
    // value for all pixels.
    virtual bool isStatic () = 0;

private:
};

//------------------------------------------------------------------------
// SplashSolidColor
//------------------------------------------------------------------------

class SplashSolidColor : public SplashPattern {
public:
    SplashSolidColor (SplashColorPtr colorA);

    SplashSolidColor (unsigned char r, unsigned char g, unsigned char b)
        : color{ r, g, b }
        { }

    virtual SplashPattern* copy () { return new SplashSolidColor (color); }

    virtual ~SplashSolidColor ();

    virtual void getColor (int x, int y, SplashColorPtr c);

    virtual bool isStatic () { return true; }

private:
    SplashColor color;
};

#endif // XPDF_SPLASH_SPLASHPATTERN_HH

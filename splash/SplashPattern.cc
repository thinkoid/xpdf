//========================================================================
//
// SplashPattern.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <config.hh>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <splash/SplashMath.hh>
#include <splash/SplashScreen.hh>
#include <splash/SplashPattern.hh>

//------------------------------------------------------------------------
// SplashPattern
//------------------------------------------------------------------------

SplashPattern::SplashPattern() {
}

SplashPattern::~SplashPattern() {
}

//------------------------------------------------------------------------
// SplashSolidColor
//------------------------------------------------------------------------

SplashSolidColor::SplashSolidColor(SplashColorPtr colorA) {
  splashColorCopy(color, colorA);
}

SplashSolidColor::~SplashSolidColor() {
}

void SplashSolidColor::getColor(int x, int y, SplashColorPtr c) {
  splashColorCopy(c, color);
}

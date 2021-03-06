// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHERRORCODES_HH
#define XPDF_SPLASH_SPLASHERRORCODES_HH

#include <defs.hh>

//------------------------------------------------------------------------

#define splashOk 0 // no error

#define splashErrNoCurPt 1 // no current point

#define splashErrEmptyPath 2 // zero points in path

#define splashErrBogusPath 3 // only one point in subpath

#define splashErrNoSave 4 // state stack is empty

#define splashErrOpenFile 5 // couldn't open file

#define splashErrNoGlyph 6 // couldn't get the requested glyph

#define splashErrModeMismatch 7 // invalid combination of color modes

#define splashErrSingularMatrix 8 // matrix is singular

#endif // XPDF_SPLASH_SPLASHERRORCODES_HH

// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_BUILTINFONTTABLES_HH
#define XPDF_XPDF_BUILTINFONTTABLES_HH

#include <xpdf/BuiltinFont.hh>

#define nBuiltinFonts 14
#define nBuiltinFontSubsts 12

extern BuiltinFont builtinFonts[nBuiltinFonts];
extern BuiltinFont* builtinFontSubst[nBuiltinFontSubsts];

extern void initBuiltinFontTables ();
extern void freeBuiltinFontTables ();

#endif // XPDF_XPDF_BUILTINFONTTABLES_HH

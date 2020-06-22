// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTFONTINFO_HH
#define XPDF_XPDF_TEXTFONTINFO_HH

#include <defs.hh>

#include <xpdf/GfxFont.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/TextOutput.hh>

struct TextFontInfo
{
    explicit TextFontInfo(GfxState *);

    bool matches(GfxState *) const;

    //
    // Get the font name (which may be NULL):
    //
    GString *getFontName() const { return name; }

    //
    // Get font descriptor flags:
    //
    bool isFixedWidth() const { return flags & fontFixedWidth; }
    bool isSerif() const { return flags & fontSerif; }
    bool isSymbolic() const { return flags & fontSymbolic; }
    bool isItalic() const { return flags & fontItalic; }
    bool isBold() const { return flags & fontBold; }

    double getWidth() const { return width; }

    Ref      id;
    GString *name;

    double   width, ascent, descent;
    unsigned flags;
};

#endif // XPDF_XPDF_TEXTFONTINFO_HH

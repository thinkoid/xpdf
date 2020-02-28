// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTCHAR_HH
#define XPDF_XPDF_TEXTCHAR_HH

#include <defs.hh>

#include <xpdf/CharTypes.hh>
#include <xpdf/TextOutput.hh>

struct TextChar {
    TextFontInfoPtr font;
    double size;

    double xmin, ymin, xmax, ymax;
    double r, g, b;

    Unicode c;
    int charPos;

    unsigned char charLen : 4, rot : 2, clipped : 1, invisible : 1;
};

#endif // XPDF_XPDF_TEXTCHAR_HH

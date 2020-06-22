// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC

#ifndef XPDF_XPDF_TEXTOUTPUTCONTROL_HH
#define XPDF_XPDF_TEXTOUTPUTCONTROL_HH

#include <defs.hh>

#include <cstdio>

enum TextOutputMode {
    textOutReadingOrder, // format into reading order
    textOutPhysLayout, // maintain original physical layout
    textOutTableLayout, // similar to PhysLayout, but optimized for tables
    textOutLinePrinter, // strict fixed-pitch/height layout
    textOutRawOrder // keep text in content stream order
};

struct TextOutputControl
{
    //
    // Fixed-pitch characters with this width (if non-zero, only relevant for
    // PhysLayout, Table, and LinePrinter modes):
    //
    double fixedPitch = 0.;

    //
    // Fixed line spacing (only relevant for LinePrinter mode):
    //
    double fixedLineSpacing = 0.;

    //
    // Formatting mode:
    //
    TextOutputMode mode = textOutReadingOrder;

    //
    // - html : enable extra processing for HTML
    //
    unsigned char html : 1 = 0;

    //
    // - clipText : separate clipped text and add it back in after forming
    //              columns:
    //
    unsigned char clipText : 1 = 0;
};

#endif // XPDF_XPDF_TEXTOUTPUTCONTROL_HH

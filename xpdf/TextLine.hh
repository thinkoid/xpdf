// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTLINE_HH
#define XPDF_XPDF_TEXTLINE_HH

#include <defs.hh>

#include <xpdf/CharTypes.hh>
#include <xpdf/TextOutputDevFwd.hh>

struct TextLine {
    TextLine (TextWords, double, double, double, double, double);

    double getBaseline () const;

    TextWords words;

    // rotation, multiple of 90 degrees (0, 1, 2, or 3)
    int rot;

    // bounding box coordinates
    double xmin, xmax, ymin, ymax;

    // main (max) font size for this line
    double fontSize;

    //
    // Unicode text of the line, including spaces between words:
    //
    std::vector< Unicode > text;

    //
    // "Near" edge x or y coord of each char (plus one extra entry for the last
    // char):
    //
    std::vector< double > edge;

    int len;           // number of Unicode chars
    bool hyphenated;   // set if last char is a hyphen
    int px;            // x offset (in characters, relative to
                       //   containing column) in physical layout mode
    int pw;            // line width (in characters) in physical
                       //   layout mode
};

#endif // XPDF_XPDF_TEXTLINE_HH

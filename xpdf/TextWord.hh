// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTWORD_HH
#define XPDF_XPDF_TEXTWORD_HH

#include <defs.hh>

#include <utils/GString.hh>

#include <xpdf/CharTypes.hh>
#include <xpdf/TextOutputDevFwd.hh>

struct TextWord {
    TextWord (TextChars& chars, int start, int lenA, int rotA, bool spaceAfterA);

    // Get the TextFontInfo object associated with this word.
    TextFontInfoPtr getFontInfo () const { return font; }

    size_t size () const { return text.size (); }

    Unicode get (int idx) { return text[idx]; }

    GString* getFontName () const;

    void getColor (double* r, double* g, double* b) const {
        *r = colorR;
        *g = colorG;
        *b = colorB;
    }

    void getBBox (double* xminA, double* yminA,
                  double* xmaxA, double* ymaxA) const {
        *xminA = xmin;
        *yminA = ymin;
        *xmaxA = xmax;
        *ymaxA = ymax;
    }

    void getCharBBox (
        int charIdx, double* xminA, double* yminA, double* xmaxA,
        double* ymaxA);

    double getFontSize () const { return fontSize; }
    int getRotation () const { return rot; }

    int getCharPos () const { return charPos.front (); }
    int getCharLen () const { return charPos.back () - charPos.front (); }

    bool isInvisible () const { return invisible; }
    bool isUnderlined () const { return underlined; }
    bool getSpaceAfter () const { return spaceAfter; }

    double getBaseline ();

    //
    // The text:
    //
    std::vector< Unicode > text;

    //
    // Character position (within content stream) of each char (plus one extra
    // entry for the last char):
    //
    std::vector< off_t > charPos;

    //
    // "Near" edge x or y coord of each char (plus one extra entry for the last
    // char):
    //
    std::vector< double > edge;

    TextFontInfoPtr font;
    double fontSize;

    //
    // Bounding box, colors:
    //
    double xmin, xmax, ymin, ymax, colorR, colorG, colorB;

    unsigned char
        rot        : 2, // multiple of 90°: 0, 1, 2, or 3
        spaceAfter : 1, // set if ∃ separating space before next character
        underlined : 1, // underlined ...?
        invisible  : 1; // invisible, render mode 3
};

#endif // XPDF_XPDF_TEXTWORD_HH

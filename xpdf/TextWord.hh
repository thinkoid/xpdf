// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTWORD_HH
#define XPDF_XPDF_TEXTWORD_HH

#include <defs.hh>

#include <optional>

#include <utils/GString.hh>

#include <xpdf/bbox.hh>
#include <xpdf/CharTypes.hh>
#include <xpdf/TextLink.hh>
#include <xpdf/TextOutput.hh>

struct TextWord
{
    TextWord(TextChars &chars, int start, int lenA, int rotA, bool spaceAfterA);

    // Get the TextFontInfo object associated with this word.
    TextFontInfoPtr getFontInfo() const { return font; }

    size_t size() const { return text.size(); }

    Unicode get(int idx) { return text[idx]; }

    GString *getFontName() const;

    void getBBox(double *xminA, double *yminA, double *xmaxA, double *ymaxA) const
    {
        *xminA = box.xmin;
        *yminA = box.ymin;
        *xmaxA = box.xmax;
        *ymaxA = box.ymax;
    }

    double getFontSize() const { return fontSize; }
    int    getRotation() const { return rot; }

    int getCharPos() const { return charPos.front(); }
    int getCharLen() const { return charPos.back() - charPos.front(); }

    bool isInvisible() const { return invisible; }
    bool isUnderlined() const { return underlined; }
    bool getSpaceAfter() const { return spaceAfter; }

    double getBaseline();

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
    double          fontSize;

    xpdf::bbox_t box;

    //
    // Set once, unused otherwise:
    //
    std::optional< TextLink > link;

    unsigned char rot : 2, // multiple of 90°: 0, 1, 2, or 3
        spaceAfter : 1, // set if ∃ separating space before next character
        underlined : 1, // underlined ...?
        invisible : 1; // invisible, render mode 3
};

#endif // XPDF_XPDF_TEXTWORD_HH

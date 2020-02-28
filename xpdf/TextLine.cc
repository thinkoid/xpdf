// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>

#include <xpdf/TextFontInfo.hh>
#include <xpdf/TextLine.hh>
#include <xpdf/TextWord.hh>

TextLine::TextLine (
    TextWords wordsA,
    double xMinA, double yMinA, double xMaxA, double yMaxA, double fontSizeA)
    : words (std::move (wordsA)) {

    rot = 0;

    xmin = xMinA;
    ymin = yMinA;

    xmax = xMaxA;
    ymax = yMaxA;

    fontSize = fontSizeA;

    px = 0;
    pw = 0;

    //
    // Build the text:
    //
    len = 0;

    for (auto& word : words) {
        len += word->size ();

        if (word->spaceAfter) {
            ++len;
        }
    }

    text.resize (len);
    edge.resize (len + 1);

    if (!words.empty ()) {
        rot = words.front ()->rot;
    }

    size_t j = 0;

    for (auto& word : words) {
        for (size_t k = 0; k < word->size (); ++k) {
            text [j] = word->text [k];
            edge [j] = word->edge [k];
            ++j;
        }

        edge[j] = word->edge[word->size ()];

        if (word->spaceAfter) {
            text [j++] = (Unicode)0x0020;
            edge [j] = edge [j - 1];
        }
    }

    //
    // TODO: need to check for other Unicode chars used as hyphens:
    //
    hyphenated = text [len - 1] == (Unicode)'-';
}

double TextLine::getBaseline () const {
    auto& word = words.front ();

    switch (rot) {
    case 1:  return xmin - fontSize * word->font->descent;
    case 2:  return ymin - fontSize * word->font->descent;
    case 3:  return xmax + fontSize * word->font->descent;
    case 0:
    default: return ymax + fontSize * word->font->descent;
    }
}


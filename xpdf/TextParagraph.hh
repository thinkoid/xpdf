// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTPARAGRAPH_HH
#define XPDF_XPDF_TEXTPARAGRAPH_HH

#include <defs.hh>

#include <xpdf/TextOutput.hh>
#include <xpdf/TextLine.hh>

struct TextParagraph {
    TextParagraph (TextLines arg) : lines (std::move (arg)), box{ } {
        for (auto& line : lines) {
            box = coalesce (box, line->box);
        }
    }

    TextLines lines;
    xpdf::bbox_t box;
};

using TextParagraphPtr = std::shared_ptr< TextParagraph >;
using TextParagraphs = std::vector< TextParagraphPtr >;

#endif // XPDF_XPDF_TEXTPARAGRAPH_HH

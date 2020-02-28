// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTPARAGRAPH_HH
#define XPDF_XPDF_TEXTPARAGRAPH_HH

#include <defs.hh>

#include <xpdf/TextOutput.hh>
#include <xpdf/TextLine.hh>

struct TextParagraph {
    TextParagraph (TextLines arg)
        : lines (std::move (arg)) {
        bool first = true;

        for (auto& line : lines) {
            if (first || line->xmin < xmin) { xmin = line->xmin; }
            if (first || line->ymin < ymin) { ymin = line->ymin; }
            if (first || line->xmax > xmax) { xmax = line->xmax; }
            if (first || line->ymax > ymax) { ymax = line->ymax; }

            first = false;
        }
    }

    TextLines lines;

    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
};

using TextParagraphPtr = std::shared_ptr< TextParagraph >;
using TextParagraphs = std::vector< TextParagraphPtr >;

#endif // XPDF_XPDF_TEXTPARAGRAPH_HH

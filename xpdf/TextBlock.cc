// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>

#include <xpdf/TextBlock.hh>
#include <xpdf/TextChar.hh>

TextColumn::TextColumn (
    TextParagraphs paragraphsA, double xMinA, double yMinA, double xMaxA,
    double yMaxA) {
    paragraphs = paragraphsA;
    xmin = xMinA;
    ymin = yMinA;
    xmax = xMaxA;
    ymax = yMaxA;
    px = py = 0;
    pw = ph = 0;
}

TextBlock::TextBlock (TextBlockType typeA, int rotA)
    :  xs (make_variant (typeA)),
       xMin (0), yMin (0), xMax (0), yMax (0),
       type (typeA), tag (blkTagMulticolumn),
       rot (rotA), smallSplit ()
{ }

void TextBlock::addChild (TextBlockPtr p) {
    auto& blocks = as_blocks ();

    if (blocks.empty ()) {
        xMin = p->xMin;
        yMin = p->yMin;
        xMax = p->xMax;
        yMax = p->yMax;
    }
    else {
        if (p->xMin < xMin) { xMin = p->xMin; }
        if (p->yMin < yMin) { yMin = p->yMin; }
        if (p->xMax > xMax) { xMax = p->xMax; }
        if (p->yMax > yMax) { yMax = p->yMax; }
    }

    blocks.push_back (p);
}

void TextBlock::addChild (TextCharPtr p) {
    auto& chars = as_chars ();

    if (chars.empty ()) {
        xMin = p->xmin;
        yMin = p->ymin;
        xMax = p->xmax;
        yMax = p->ymax;
    }
    else {
        if (p->xmin < xMin) { xMin = p->xmin; }
        if (p->ymin < yMin) { yMin = p->ymin; }
        if (p->xmax > xMax) { xMax = p->xmax; }
        if (p->ymax > yMax) { yMax = p->ymax; }
    }

    chars.push_back (p);
}

void TextBlock::prependChild (TextCharPtr p) {
    auto& chars = as_chars ();

    if (chars.empty ()) {
        xMin = p->xmin;
        yMin = p->ymin;
        xMax = p->xmax;
        yMax = p->ymax;
    }
    else {
        if (p->xmin < xMin) { xMin = p->xmin; }
        if (p->ymin < yMin) { yMin = p->ymin; }
        if (p->xmax > xMax) { xMax = p->xmax; }
        if (p->ymax > yMax) { yMax = p->ymax; }
    }

    chars.insert (chars.begin (), p);
}

void TextBlock::updateBounds (int n) {
    auto& block = as_blocks ()[n];

    if (block->xMin < xMin) { xMin = block->xMin; }
    if (block->yMin < yMin) { yMin = block->yMin; }
    if (block->xMax > xMax) { xMax = block->xMax; }
    if (block->yMax > yMax) { yMax = block->yMax; }
}


// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>

#include <xpdf/TextBlock.hh>
#include <xpdf/TextChar.hh>

TextColumn::TextColumn (
    TextParagraphs paragraphsA,
    double xMinA, double yMinA, double xMaxA, double yMaxA)
    : paragraphs (paragraphsA), box{ xMinA, yMinA, xMaxA, yMaxA },
      px{ }, py{ }, pw{ }, ph{ }
{ }

TextBlock::TextBlock (TextBlockType typeA, int rotA)
    :  xs (make_variant (typeA)), box{ },
       type (typeA), tag (blkTagMulticolumn),
       rot (rotA), smallSplit ()
{ }

void TextBlock::addChild (TextBlockPtr p) {
    auto& blocks = as_blocks ();

    if (blocks.empty ()) {
        box = p->box;
    }
    else {
        box = coalesce (box, p->box);
    }

    blocks.push_back (p);
}

void TextBlock::addChild (TextCharPtr ch) {
    auto& chars = as_chars ();

    if (chars.empty ()) {
        box = ch->box;
    }
    else {
        box = coalesce (box, ch->box);
    }

    chars.push_back (ch);
}

void TextBlock::prependChild (TextCharPtr ch) {
    auto& chars = as_chars ();

    if (chars.empty ()) {
        box = ch->box;
    }
    else {
        box = coalesce (box, ch->box);
    }

    chars.insert (chars.begin (), ch);
}

void TextBlock::updateBounds (int n) {
    auto& block = as_blocks ()[n];
    box = coalesce (box, block->box);
}


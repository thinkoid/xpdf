// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTBLOCK_HH
#define XPDF_XPDF_TEXTBLOCK_HH

#include <defs.hh>

#include <variant>

#include <xpdf/bbox.hh>
#include <xpdf/TextOutput.hh>

struct TextColumn {
    TextColumn (TextParagraphs, double, double, double, double);

    TextParagraphs paragraphs;

    xpdf::bbox_t box;

    //
    // x, y position (in characters) in physical layout mode, and
    // column width, height (in characters) in physical layout mode:
    //
    int px, py, pw, ph;
};

enum TextBlockType {
    blkVertSplit, blkHorizSplit, blkLeaf
};

enum TextBlockTag {
    blkTagMulticolumn, blkTagColumn, blkTagLine
};

struct TextBlock {
    TextBlock (TextBlockType typeA, int rotA);

    void addChild (TextBlockPtr);
    void addChild (TextCharPtr);

    void prependChild (TextCharPtr);

    template< typename Iterator >
    void prependChildren (Iterator iter, Iterator last) {
        auto& chars = as_chars ();

        TextChars xs (iter, last);
        xs.insert (xs.end (), chars.begin (), chars.end ());

        chars = std::move (xs);
    }

    void updateBounds (int childIdx);

    TextChars&       as_chars ()       { return std::get< TextChars > (xs); }
    TextChars const& as_chars () const { return std::get< TextChars > (xs); }

    TextBlocks&       as_blocks ()       { return std::get< TextBlocks > (xs); }
    TextBlocks const& as_blocks () const { return std::get< TextBlocks > (xs); }

    std::variant< TextChars, TextBlocks > xs;

    //
    // Cached bounding box coordinates for the block:
    //
    xpdf::bbox_t box;

    //
    // For leaf blocks children are sequences of TextChar; for others,
    // children are sequences of TextBlock:
    //
    TextBlockType type;
    TextBlockTag tag;

    unsigned char rot : 2;
    unsigned char smallSplit : 1; // true for small gaps blk{Horiz,Vert}Split

private:
    static decltype(xs) make_variant (TextBlockType type) {
        switch (type) {
        case blkLeaf:
            return decltype(xs){ TextChars{ } };

        default:
            return decltype(xs){ TextBlocks{ } };
        }
    }
};

#endif // XPDF_XPDF_TEXTBLOCK_HH

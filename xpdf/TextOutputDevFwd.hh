// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTOUTPUTDEV_FWD_HH
#define XPDF_XPDF_TEXTOUTPUTDEV_FWD_HH

#include <defs.hh>

#include <memory>
#include <vector>

////////////////////////////////////////////////////////////////////////

// Size of bins used for horizontal and vertical profiles is
// splitPrecisionMul * minFontSize.
#define splitPrecisionMul 0.05

// Minimum allowed split precision.
#define minSplitPrecision 0.01

// yMin and yMax (or xMin and xMax for rotations ∈ { 1, 3 }) are adjusted by
// this fraction of the text height, to allow for slightly overlapping lines (or
// large ascent/descent values):
#define  ascentAdjustFactor 0.
#define descentAdjustFactor 0.35

// Gaps larger than max{ gap } - splitGapSlack * avgFontSize are
// considered to be equivalent.
#define splitGapSlack 0.2

// The vertical gap threshold (minimum gap required to split
// vertically) depends on the (approximate) number of lines in the
// block:
//   threshold (max + slope * nLines) * avgFontSize
// with a min value of vertGapThresholdMin * avgFontSize.
#define vertGapThresholdMin 0.8
#define vertGapThresholdMax 3.
#define vertGapThresholdSlope -0.5

// Vertical gap threshold for table mode.
#define vertGapThresholdTableMin 0.2
#define vertGapThresholdTableMax 0.5
#define vertGapThresholdTableSlope -0.02

// A large character has a font size larger than
// largeCharThreshold * avgFontSize.
#define largeCharThreshold 1.5

// A block will be split vertically only if the resulting chunk
// widths are greater than vertSplitChunkThreshold * avgFontSize.
#define vertSplitChunkThreshold 2.

// Max difference in primary,secondary coordinates (as a fraction of
// the font size) allowed for duplicated text (fake boldface, drop
// shadows) which is to be discarded.
#define dupMaxPriDelta 0.1
#define dupMaxSecDelta 0.2

// Inter-character spacing that varies by less than this multiple of
// font size is assumed to be equivalent.
#define uniformSpacing 0.07

// Typical word spacing, as a fraction of font size.  This will be
// added to the minimum inter-character spacing, to account for wide
// character spacing.
#define wordSpacing 0.1

// Minimum paragraph indent from left margin, as a fraction of font
// size.
#define minParagraphIndent 0.5

// If the space between two lines is greater than
// paragraphSpacingThreshold * avgLineSpacing, start a new paragraph.
#define paragraphSpacingThreshold 1.2

// If font size changes by at least this much (measured in points)
// between lines, start a new paragraph.
#define paragraphFontSizeDelta 1.

// Spaces at the start of a line in physical layout mode are this wide
// (as a multiple of font size).
#define physLayoutSpaceWidth 0.33

// Table cells (TextColumns) are allowed to overlap by this much
// in table layout mode (as a fraction of cell width or height).
#define tableCellOverlapSlack 0.05

// Primary axis delta which will cause a line break in raw mode
// (as a fraction of font size).
#define rawModeLineDelta 0.5

// Secondary axis delta which will cause a word break in raw mode
// (as a fraction of font size).
#define rawModeWordSpacing 0.15

// Secondary axis overlap which will cause a line break in raw mode
// (as a fraction of font size).
#define rawModeCharOverlap 0.2

// Max spacing (as a multiple of font size) allowed between the end of
// a line and a clipped character to be included in that line.
#define clippedTextMaxWordSpace 0.5

// Max width of underlines (in points).
#define maxUnderlineWidth 3.

// Max horizontal distance between edge of word and start of underline
// (as a fraction of font size).
#define underlineSlack 0.2

// Max vertical distance between baseline of word and start of
// underline (as a fraction of font size).
#define underlineBaselineSlack 0.2

// Max distance between edge of text and edge of link border (as a
// fraction of font size).
#define hyperlinkSlack 0.2

#define XPDF_TYPEDEF(x)                                     \
    struct x;                                               \
    using XPDF_CAT(x, Ptr) = std::shared_ptr< x >;          \
    using XPDF_CAT(x, s) = std::vector< XPDF_CAT(x, Ptr) >

XPDF_TYPEDEF (TextFontInfo);
XPDF_TYPEDEF (TextChar);
XPDF_TYPEDEF (TextWord);
XPDF_TYPEDEF (TextLine);
XPDF_TYPEDEF (TextUnderline);
XPDF_TYPEDEF (TextLink);
XPDF_TYPEDEF (TextParagraph);
XPDF_TYPEDEF (TextColumn);
XPDF_TYPEDEF (TextBlock);
XPDF_TYPEDEF (TextPage);

#undef XPDF_TYPEDEF

#endif // XPDF_XPDF_TEXTOUTPUTDEV_FWD_HH

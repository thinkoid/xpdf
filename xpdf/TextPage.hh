// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC

#ifndef XPDF_XPDF_TEXTPAGE_HH
#define XPDF_XPDF_TEXTPAGE_HH

#include <defs.hh>

#include <xpdf/bbox.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/Link.hh>
#include <xpdf/TextFontInfo.hh>
#include <xpdf/TextLink.hh>
#include <xpdf/TextOutput.hh>
#include <xpdf/TextOutputFunc.hh>
#include <xpdf/TextOutputControl.hh>
#include <xpdf/TextUnderline.hh>
#include <xpdf/unicode_map.hh>

struct TextPage
{
    TextPage(TextOutputControl *controlA);

    //
    // Find a matching string:
    // - startAtTop  : if true, search starts at the top of the page,
    // - startAtLast : if true, search starts after the last match,
    //     otherwise, starts looking at { xMin, yMin }.
    //
    // - stopAtBottom : if true, stops looking at the bottom of page,
    // - stopAtLast   : if true, stops looking before the last match,
    //     otherwise, stops looking at { xMax, yMax }
    //
    bool findText(Unicode *s, int len, bool startAtTop, bool stopAtBottom,
                  bool startAtLast, bool stopAtLast, bool caseSensitive,
                  bool backward, bool wholeWord, xpdf::bbox_t &);

    // Get the text which is inside the specified rectangle.
    GString *getText(const xpdf::bbox_t &);

    std::vector< xpdf::bbox_t > segment() const;

private:
    void clear();

    void startPage(GfxState *state);
    void updateFont(GfxState *state);

    void addChar(GfxState *, double, double, double, double, CharCode, int,
                 Unicode *, int);

    void doAddActualTextChar(double, double, double, double, int);

    void doAddChar(GfxState *, double, double, double, double, CharCode, int,
                   Unicode *, int);

    void incCharCount(int);

    void beginActualText(GfxState *state, Unicode *u, int uLen);
    void endActualText(GfxState *state);

    void addUnderline(double x0, double y0, double x1, double y1);

    void addLink(double, double, double, double, Link *);

    // output
    void encodeFragment(Unicode *, int, xpdf::unicode_map_t &, bool, GString *);

    // analysis
    int  rotateChars(TextChars &charsA);
    void rotateUnderlinesAndLinks(int rot);

    void unrotateChars(TextChars &, int);
    void unrotateColumns(TextColumns &, int);
    void unrotateWords(TextWords &, int);

    bool isPrevalentLeftToRight(TextChars &);
    void removeDuplicates(TextChars &, int);

    TextBlockPtr splitChars(TextChars &charsA);

    TextBlockPtr split(TextChars &charsA, int rot);

    TextChars charsIn(TextChars &, const xpdf::bbox_t &) const;

    void tagBlock(TextBlockPtr blk);

    void doInsertLargeChars(TextChars &, TextBlockPtr);
    void insertLargeChars(TextChars &, TextBlockPtr);

    void insertLargeCharsInFirstLeaf(TextChars &largeChars, TextBlockPtr blk);

    void insertLargeCharInLeaf(TextCharPtr ch, TextBlockPtr blk);

    void insertIntoTree(TextBlockPtr subtree, TextBlockPtr primaryTree);

    void insertColumnIntoTree(TextBlockPtr column, TextBlockPtr tree);

    void insertClippedChars(TextChars &, TextBlockPtr);

    TextBlockPtr findClippedCharLeaf(TextCharPtr ch, TextBlockPtr tree);

    TextColumns buildColumns(TextBlockPtr tree);

    TextColumnPtr buildColumn(TextBlockPtr tree);

    double getLineIndent(const TextLine &, TextBlockPtr) const;

    double getAverageLineSpacing(const TextLines &) const;

    double getLineSpacing(const TextLine &, const TextLine &) const;

    void makeLines(TextBlockPtr, TextLines &);

    TextLinePtr makeLine(TextBlockPtr);

    TextChars getLineOfChars(TextBlockPtr);

    double computeWordSpacingThreshold(TextChars &charsA, int rot);
    int    assignPhysLayoutPositions(TextColumns &);
    void   assignLinePhysPositions(TextColumns &);
    void   computeLinePhysWidth(TextLine &line, xpdf::unicode_map_t &uMap);
    int    assignColumnPhysPositions(TextColumns &);
    void   generateUnderlinesAndLinks(TextColumns &);

    TextOutputControl control; // formatting parameters

    double pageWidth, pageHeight; // width and height of current page
    int    charPos; // next character position (within content
        //   stream)

    TextFontInfos fonts; // all font info objects used on this page
        //   [TextFontInfo]

    TextFontInfoPtr curFont; // current font
    double          curFontSize; // current font size

    int curRot; // current rotation
    int nTinyChars; // number of "tiny" chars seen so far

    //
    // `ActualText' characters and length in characters:
    //
    std::optional< std::vector< Unicode > > actualText;

    //
    // Not the length of actualText!
    //
    int actualTextNBytes;

    //
    // The bounding box for the `ActualText':
    //
    double actualTextX0, actualTextY0, actualTextX1, actualTextY1;

    TextChars chars; // [TextChar]

    TextUnderlines underlines; // [TextUnderline]
    TextLinks      links; // [TextLink]

    TextColumns findCols; // text used by the findText function
        //   [TextColumn]

    //
    // Primary text direction, used by the findText function:
    //
    bool findLR;

    //
    // Coordinates of the last "find" result:
    //
    double lastFindXMin, lastFindYMin;

    bool haveLastFind;

    friend struct TextOutputDev;
};

#endif // XPDF_XPDF_TEXTPAGE_HH

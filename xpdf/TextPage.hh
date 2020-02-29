// -*- mode: c++; -*-
// Copyright 1997-2012 Glyph & Cog, LLC

#ifndef XPDF_XPDF_TEXTPAGE_HH
#define XPDF_XPDF_TEXTPAGE_HH

#include <defs.hh>

#include <xpdf/bbox.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/Link.hh>
#include <xpdf/TextFontInfo.hh>
#include <xpdf/TextOutput.hh>
#include <xpdf/TextOutputFunc.hh>
#include <xpdf/TextOutputControl.hh>
#include <xpdf/UnicodeMap.hh>

struct TextPage {
    TextPage (TextOutputControl* controlA);
    ~TextPage ();

    // Write contents of page to a stream.
    void write (void* outputStream, TextOutputFunc outputFunc);

    std::vector< xpdf::bbox_t > segment () const;

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
    bool findText (
        Unicode* s, int len,
        bool startAtTop, bool stopAtBottom, bool startAtLast, bool stopAtLast,
        bool caseSensitive, bool backward, bool wholeWord,
        xpdf::bbox_t&);

    // Get the text which is inside the specified rectangle.
    GString*
    getText (const xpdf::bbox_t&);

    // Build a flat word list, in the specified ordering.
    TextWords makeWordList ();

private:
    void startPage (GfxState* state);
    void clear ();
    void updateFont (GfxState* state);
    void addChar (
        GfxState* state, double x, double y, double dx, double dy, CharCode c,
        int nBytes, Unicode* u, int uLen);

    void incCharCount (int nChars);

    void beginActualText (GfxState* state, Unicode* u, int uLen);
    void endActualText (GfxState* state);

    void addUnderline (double x0, double y0, double x1, double y1);

    void
    addLink (double xMin, double yMin, double xMax, double yMax, Link* link);

    // output
    void writeReadingOrder (
        void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
        char* space, int spaceLen, char* eol, int eolLen);
    void writePhysLayout (
        void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
        char* space, int spaceLen, char* eol, int eolLen);
    void writeLinePrinter (
        void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
        char* space, int spaceLen, char* eol, int eolLen);
    void writeRaw (
        void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
        char* space, int spaceLen, char* eol, int eolLen);
    void encodeFragment (
        Unicode* text, int len, UnicodeMap* uMap, bool primaryLR, GString* s);

    // analysis
    int rotateChars (TextChars& charsA);
    void rotateUnderlinesAndLinks (int rot);

    void unrotateChars   (TextChars&, int rot);
    void unrotateColumns (TextColumns&, int);
    void unrotateWords   (TextWords&, int);

    bool isPrevalentLeftToRight (TextChars&);
    void removeDuplicates (TextChars&, int);

    TextBlockPtr
    splitChars (TextChars& charsA);

    TextBlockPtr
    split (TextChars& charsA, int rot);

    TextChars
    charsIn (TextChars&, const xpdf::bbox_t&) const;

    void tagBlock (TextBlockPtr blk);

    void
    insertLargeChars (TextChars& largeChars, TextBlockPtr blk);

    void
    insertLargeCharsInFirstLeaf (TextChars& largeChars, TextBlockPtr blk);

    void
    insertLargeCharInLeaf (TextCharPtr ch, TextBlockPtr blk);

    void
    insertIntoTree (TextBlockPtr subtree, TextBlockPtr primaryTree);

    void
    insertColumnIntoTree (TextBlockPtr column, TextBlockPtr tree);

    void
    insertClippedChars (TextChars&, TextBlockPtr);

    TextBlockPtr
    findClippedCharLeaf (TextCharPtr ch, TextBlockPtr tree);

    TextColumns
    buildColumns (TextBlockPtr tree);

    TextColumnPtr
    buildColumn (TextBlockPtr tree);

    double
    getLineIndent (const TextLine&, TextBlockPtr) const;

    double
    getAverageLineSpacing (const TextLines&) const;

    double
    getLineSpacing (const TextLine&, const TextLine&) const;

    void
    makeLines (TextBlockPtr, TextLines&);

    TextLinePtr
    makeLine (TextBlockPtr);

    TextChars
    getLineOfChars (TextBlockPtr);

    double computeWordSpacingThreshold (TextChars& charsA, int rot);
    int assignPhysLayoutPositions (TextColumns&);
    void assignLinePhysPositions (TextColumns&);
    void computeLinePhysWidth (TextLine& line, UnicodeMap* uMap);
    int assignColumnPhysPositions (TextColumns&);
    void generateUnderlinesAndLinks (TextColumns&);

    TextOutputControl control; // formatting parameters

    double pageWidth, pageHeight; // width and height of current page
    int charPos;                  // next character position (within content
                                  //   stream)

    TextFontInfos fonts;          // all font info objects used on this page
                                  //   [TextFontInfo]

    TextFontInfoPtr curFont;      // current font
    double curFontSize;           // current font size

    int curRot;                   // current rotation
    int nTinyChars;               // number of "tiny" chars seen so far

    //
    // `ActualText' characters and length in characters:
    //
    Unicode* actualText;          // current "ActualText" span
    int actualTextLen;

    //
    // The bounding box for the `ActualText':
    //
    double actualTextX0, actualTextY0, actualTextX1, actualTextY1;

    //
    // Number of bytes:
    //
    int actualTextNBytes;

    TextChars chars; // [TextChar]

    TextUnderlines underlines;   // [TextUnderline]
    TextLinks links;             // [TextLink]

    TextColumns findCols;  // text used by the findText function
                           //   [TextColumn]

    bool findLR;         // primary text direction, used by the
                         //   findText function
    double lastFindXMin, // coordinates of the last "find" result
        lastFindYMin;
    bool haveLastFind;

    friend class TextOutputDev;
};

#endif // XPDF_XPDF_TEXTPAGE_HH

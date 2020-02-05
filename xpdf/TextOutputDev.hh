// -*- mode: c++; -*-
// Copyright 1997-2012 Glyph & Cog, LLC

#ifndef XPDF_XPDF_TEXTOUTPUTDEV_HH
#define XPDF_XPDF_TEXTOUTPUTDEV_HH

#include <defs.hh>

#include <cstdio>

#include <memory>
#include <vector>

#include <xpdf/GfxFont.hh>
#include <xpdf/OutputDev.hh>

class GList;
class TextBlock;
class TextChar;
class TextLink;
class TextPage;
class UnicodeMap;

//------------------------------------------------------------------------

typedef void (*TextOutputFunc) (void* stream, const char* text, int len);

//
// TextOutputControl
//
enum TextOutputMode {
    textOutReadingOrder, // format into reading order
    textOutPhysLayout,   // maintain original physical layout
    textOutTableLayout,  // similar to PhysLayout, but optimized
                         //   for tables
    textOutLinePrinter,  // strict fixed-pitch/height layout
    textOutRawOrder      // keep text in content stream order
};

struct TextOutputControl {
    //
    // Fixed-pitch characters with this width (if non-zero, only relevant for
    // PhysLayout, Table, and LinePrinter modes):
    //
    double fixedPitch = 0.;

    //
    // Fixed line spacing (only relevant for LinePrinter mode):
    //
    double fixedLineSpacing = 0.;

    //
    // Formatting mode:
    //
    TextOutputMode mode = textOutReadingOrder;

    //
    // - html : enable extra processing for HTML
    //
    unsigned char html: 1 = 0;

    //
    // - clipText : separate clipped text and add it back in after forming
    //              columns:
    //
    unsigned char clipText: 1 = 0;
};

struct TextFontInfo;
struct TextWord;
struct TextLine;
struct TextColumn;

using TextWordPtr = std::shared_ptr< TextWord >;
using TextWords = std::vector< TextWordPtr >;

using TextLinePtr = std::shared_ptr< TextLine >;
using TextLines = std::vector< TextLinePtr >;

//
// TextPage
//
class TextPage {
public:
    TextPage (TextOutputControl* controlA);
    ~TextPage ();

    // Write contents of page to a stream.
    void write (void* outputStream, TextOutputFunc outputFunc);

    // Find a string.  If <startAtTop> is true, starts looking at the
    // top of the page; else if <startAtLast> is true, starts looking
    // immediately after the last find result; else starts looking at
    // <xMin>,<yMin>.  If <stopAtBottom> is true, stops looking at the
    // bottom of the page; else if <stopAtLast> is true, stops looking
    // just before the last find result; else stops looking at
    // <xMax>,<yMax>.
    bool findText (
        Unicode* s, int len, bool startAtTop, bool stopAtBottom,
        bool startAtLast, bool stopAtLast, bool caseSensitive,
        bool backward, bool wholeWord, double* xMin, double* yMin,
        double* xMax, double* yMax);

    // Get the text which is inside the specified rectangle.
    GString* getText (double xMin, double yMin, double xMax, double yMax);

    // Find a string by character position and length.  If found, sets
    // the text bounding rectangle and returns true; otherwise returns
    // false.
    bool findCharRange (
        int pos, int length, double* xMin, double* yMin, double* xMax,
        double* yMax);

    // Create and return a list of TextColumn objects.
    GList* makeColumns ();

    // Get the list of all TextFontInfo objects used on this page.
    GList* getFonts () { return fonts; }

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
    int rotateChars (GList* charsA);
    void rotateUnderlinesAndLinks (int rot);
    void unrotateChars (GList* charsA, int rot);
    void unrotateColumns (GList* columns, int rot);
    void unrotateWords (TextWords&, int);
    bool checkPrimaryLR (GList* charsA);
    void removeDuplicates (GList* charsA, int rot);
    TextBlock* splitChars (GList* charsA);
    TextBlock* split (GList* charsA, int rot);
    GList* getChars (
        GList* charsA, double xMin, double yMin, double xMax, double yMax);
    void tagBlock (TextBlock* blk);
    void insertLargeChars (GList* largeChars, TextBlock* blk);
    void insertLargeCharsInFirstLeaf (GList* largeChars, TextBlock* blk);
    void insertLargeCharInLeaf (TextChar* ch, TextBlock* blk);
    void insertIntoTree (TextBlock* subtree, TextBlock* primaryTree);
    void insertColumnIntoTree (TextBlock* column, TextBlock* tree);
    void insertClippedChars (GList* clippedChars, TextBlock* tree);
    TextBlock* findClippedCharLeaf (TextChar* ch, TextBlock* tree);
    GList* buildColumns (TextBlock* tree);
    void buildColumns2 (TextBlock* blk, GList* columns);
    TextColumn* buildColumn (TextBlock* tree);

    double
    getLineIndent (const TextLine&, TextBlock*) const;

    double
    getAverageLineSpacing (
        const TextLines&) const;

    double
    getLineSpacing (const TextLine&, const TextLine&) const;

    void
    buildLines (TextBlock*, TextLines&);

    TextLinePtr
    buildLine (TextBlock*);

    void getLineChars (TextBlock* blk, GList* charsA);
    double computeWordSpacingThreshold (GList* charsA, int rot);
    int assignPhysLayoutPositions (GList* columns);
    void assignLinePhysPositions (GList* columns);
    void computeLinePhysWidth (TextLine& line, UnicodeMap* uMap);
    int assignColumnPhysPositions (GList* columns);
    void generateUnderlinesAndLinks (GList* columns);

    // debug
#if 0 //~debug
  void dumpChars(GList *charsA);
  void dumpTree(TextBlock *tree, int indent = 0);
  void dumpColumns(GList *columns);
#endif

    TextOutputControl control; // formatting parameters

    double pageWidth, pageHeight; // width and height of current page
    int charPos;                  // next character position (within content
                                  //   stream)
    TextFontInfo* curFont;        // current font
    double curFontSize;           // current font size
    int curRot;                   // current rotation
    int nTinyChars;               // number of "tiny" chars seen so far
    Unicode* actualText;          // current "ActualText" span
    int actualTextLen;
    double actualTextX0, actualTextY0, actualTextX1, actualTextY1;
    int actualTextNBytes;

    GList* chars; // [TextChar]
    GList* fonts; // all font info objects used on this
                  //   page [TextFontInfo]

    GList* underlines; // [TextUnderline]
    GList* links;      // [TextLink]

    GList* findCols;     // text used by the findText function
                         //   [TextColumn]
    bool findLR;        // primary text direction, used by the
                         //   findText function
    double lastFindXMin, // coordinates of the last "find" result
        lastFindYMin;
    bool haveLastFind;

    friend class TextOutputDev;
};

//
// TextOutputDev
//
class TextOutputDev : public OutputDev {
public:
    // Open a text output file.  If <fileName> is NULL, no file is
    // written (this is useful, e.g., for searching text).  If
    // <physLayoutA> is true, the original physical layout of the text
    // is maintained.  If <rawOrder> is true, the text is kept in
    // content stream order.
    TextOutputDev (const char*, TextOutputControl*, bool);

    // Create a TextOutputDev which will write to a generic stream.  If
    // <physLayoutA> is true, the original physical layout of the text
    // is maintained.  If <rawOrder> is true, the text is kept in
    // content stream order.
    TextOutputDev (TextOutputFunc, void*, TextOutputControl*);

    // Destructor.
    virtual ~TextOutputDev ();

    // Check if file was successfully created.
    virtual bool isOk () { return ok; }

    //---- get info about output device

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    virtual bool upsideDown () { return true; }

    // Does this device use drawChar() or drawString()?
    virtual bool useDrawChar () { return true; }

    // Does this device use beginType3Char/endType3Char?  Otherwise,
    // text in Type 3 fonts will be drawn with drawChar/drawString.
    virtual bool interpretType3Chars () { return false; }

    // Does this device need non-text content?
    virtual bool needNonText () { return false; }

    // Does this device require incCharCount to be called for text on
    // non-shown layers?
    virtual bool needCharCount () { return true; }

    //----- initialization and control

    // Start a page.
    virtual void startPage (int pageNum, GfxState* state);

    // End a page.
    virtual void endPage ();

    //----- save/restore graphics state
    virtual void restoreState (GfxState* state);

    //----- update text state
    virtual void updateFont (GfxState* state);

    //----- text drawing
    virtual void beginString (GfxState* state, GString* s);
    virtual void endString (GfxState* state);
    virtual void drawChar (
        GfxState* state, double x, double y, double dx, double dy,
        double originX, double originY, CharCode c, int nBytes, Unicode* u,
        int uLen);
    virtual void incCharCount (int nChars);
    virtual void beginActualText (GfxState* state, Unicode* u, int uLen);
    virtual void endActualText (GfxState* state);

    //----- special access

    // Find a string.  If <startAtTop> is true, starts looking at the
    // top of the page; else if <startAtLast> is true, starts looking
    // immediately after the last find result; else starts looking at
    // <xMin>,<yMin>.  If <stopAtBottom> is true, stops looking at the
    // bottom of the page; else if <stopAtLast> is true, stops looking
    // just before the last find result; else stops looking at
    // <xMax>,<yMax>.
    bool findText (
        Unicode* s, int len, bool startAtTop, bool stopAtBottom,
        bool startAtLast, bool stopAtLast, bool caseSensitive,
        bool backward, bool wholeWord, double* xMin, double* yMin,
        double* xMax, double* yMax);

    // Get the text which is inside the specified rectangle.
    GString* getText (double xMin, double yMin, double xMax, double yMax);

    // Find a string by character position and length.  If found, sets
    // the text bounding rectangle and returns true; otherwise returns
    // false.
    bool findCharRange (
        int pos, int length, double* xMin, double* yMin, double* xMax,
        double* yMax);

    // Build a flat word list, in content stream order (if
    // this->rawOrder is true), physical layout order (if
    // this->physLayout is true and this->rawOrder is false), or reading
    // order (if both flags are false).
    TextWords makeWordList ();

    // Returns the TextPage object for the last rasterized page,
    // transferring ownership to the caller.
    TextPage* takeText ();

private:
    TextOutputFunc outputFunc; // output function
    void* outputStream;        // output stream
    bool needClose;           // need to close the output file?
                               //   (only if outputStream is a FILE*)
    TextPage* text;            // text for the current page
    TextOutputControl control; // formatting parameters
    bool ok;                  // set up ok?
};

#endif // XPDF_XPDF_TEXTOUTPUTDEV_HH

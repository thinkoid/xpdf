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
class UnicodeMap;

class TextBlock;
class TextChar;
class TextLink;
class TextPage;

//------------------------------------------------------------------------

typedef void (*TextOutputFunc) (void* stream, const char* text, int len);

//------------------------------------------------------------------------
// TextOutputControl
//------------------------------------------------------------------------

enum TextOutputMode {
    textOutReadingOrder, // format into reading order
    textOutPhysLayout,   // maintain original physical layout
    textOutTableLayout,  // similar to PhysLayout, but optimized
                         //   for tables
    textOutLinePrinter,  // strict fixed-pitch/height layout
    textOutRawOrder      // keep text in content stream order
};

class TextOutputControl {
public:
    TextOutputControl ();
    ~TextOutputControl () {}

    TextOutputMode mode;     // formatting mode
    double fixedPitch;       // if this is non-zero, assume fixed-pitch
                             //   characters with this width
                             //   (only relevant for PhysLayout, Table,
                             //   and LinePrinter modes)
    double fixedLineSpacing; // fixed line spacing (only relevant for
                             //   LinePrinter mode)
    bool html;              // enable extra processing for HTML
    bool clipText;          // separate clipped text and add it back
        //   in after forming columns
};

//------------------------------------------------------------------------
// TextFontInfo
//------------------------------------------------------------------------

struct TextFontInfo {
    TextFontInfo (GfxState* state);
    ~TextFontInfo ();

    bool matches (GfxState* state);

    // Get the font name (which may be NULL).
    GString* getFontName () { return fontName; }

    // Get font descriptor flags.
    bool isFixedWidth () { return flags & fontFixedWidth; }
    bool isSerif () { return flags & fontSerif; }
    bool isSymbolic () { return flags & fontSymbolic; }
    bool isItalic () { return flags & fontItalic; }
    bool isBold () { return flags & fontBold; }

    // Get the width of the 'm' character, if available.
    double getMWidth () { return mWidth; }

private:
    Ref fontID;
    GString* fontName;
    int flags;
    double mWidth;
    double ascent, descent;

    friend class TextLine;
    friend class TextPage;
    friend struct TextWord;
};

//------------------------------------------------------------------------
// TextWord
//------------------------------------------------------------------------

struct TextWord {
    TextWord (GList* chars, int start, int lenA, int rotA, bool spaceAfterA);

    // Get the TextFontInfo object associated with this word.
    TextFontInfo* getFontInfo () const { return font; }

    size_t size () const { return text.size (); }

    Unicode getChar (int idx) { return text[idx]; }

    GString* getFontName () const { return font->fontName; }

    void getColor (double* r, double* g, double* b) const {
        *r = colorR;
        *g = colorG;
        *b = colorB;
    }

    void getBBox (double* xMinA, double* yMinA,
                  double* xMaxA, double* yMaxA) const {
        *xMinA = xMin;
        *yMinA = yMin;
        *xMaxA = xMax;
        *yMaxA = yMax;
    }

    void getCharBBox (
        int charIdx, double* xMinA, double* yMinA, double* xMaxA,
        double* yMaxA);

    double getFontSize () const { return fontSize; }
    int getRotation () const { return rot; }

    int getCharPos () const { return charPos.front (); }
    int getCharLen () const { return charPos.back () - charPos.front (); }

    bool isInvisible () const { return invisible; }
    bool isUnderlined () const { return underlined; }
    bool getSpaceAfter () const { return spaceAfter; }

    double getBaseline ();

    GString* getLinkURI ();

private:
    static int cmpYX (const void* p1, const void* p2);
    static int cmpCharPos (const void* p1, const void* p2);

private:
    //
    // Rotation in multiple of 90°: 0, 1, 2, or 3.
    //
    int rot;

    //
    // Bounding box coordinates:
    //
    double xMin, xMax, yMin, yMax;

    //
    // The text:
    //
    std::vector< Unicode > text;

    //
    // Character position (within content stream) of each char (plus one extra
    // entry for the last char):
    //
    std::vector< off_t > charPos;

    //
    // "near" edge x or y coord of each char (plus one extra entry for the last
    // char):
    //
    std::vector< double > edge;

    TextFontInfo* font; // font information

    double fontSize;    // font size
    bool spaceAfter;   // set if there is a space between this
                        //   word and the next word on the line

    bool underlined;
    TextLink* link;

    // word color
    double colorR, colorG, colorB;
    bool invisible; // set for invisible text (render mode 3)

    friend class TextBlock;
    friend class TextLine;
    friend class TextPage;
};

//------------------------------------------------------------------------
// TextLine
//------------------------------------------------------------------------

class TextLine {
public:
    TextLine (
        GList* wordsA, double xMinA, double yMinA, double xMaxA, double yMaxA,
        double fontSizeA);
    ~TextLine ();

    double getXMin () { return xMin; }
    double getYMin () { return yMin; }
    double getBaseline ();
    int getRotation () { return rot; }
    GList* getWords () { return words; }

private:
    GList* words;      // [TextWord]
    int rot;           // rotation, multiple of 90 degrees
                       //   (0, 1, 2, or 3)
    double xMin, xMax; // bounding box x coordinates
    double yMin, yMax; // bounding box y coordinates
    double fontSize;   // main (max) font size for this line
    Unicode* text;     // Unicode text of the line, including
                       //   spaces between words
    double* edge;      // "near" edge x or y coord of each char
                       //   (plus one extra entry for the last char)
    int len;           // number of Unicode chars
    bool hyphenated;  // set if last char is a hyphen
    int px;            // x offset (in characters, relative to
                       //   containing column) in physical layout mode
    int pw;            // line width (in characters) in physical
                       //   layout mode

    friend class TextPage;
    friend class TextParagraph;
};

//------------------------------------------------------------------------
// TextParagraph
//------------------------------------------------------------------------

class TextParagraph {
public:
    TextParagraph (GList* linesA);
    ~TextParagraph ();

    // Get the list of TextLine objects.
    GList* getLines () { return lines; }

private:
    GList* lines;      // [TextLine]
    double xMin, xMax; // bounding box x coordinates
    double yMin, yMax; // bounding box y coordinates

    friend class TextPage;
};

//------------------------------------------------------------------------
// TextColumn
//------------------------------------------------------------------------

class TextColumn {
public:
    TextColumn (
        GList* paragraphsA, double xMinA, double yMinA, double xMaxA,
        double yMaxA);
    ~TextColumn ();

    // Get the list of TextParagraph objects.
    GList* getParagraphs () { return paragraphs; }

private:
    static int cmpX (const void* p1, const void* p2);
    static int cmpY (const void* p1, const void* p2);
    static int cmpPX (const void* p1, const void* p2);

    GList* paragraphs; // [TextParagraph]
    double xMin, xMax; // bounding box x coordinates
    double yMin, yMax; // bounding box y coordinates
    int px, py;        // x, y position (in characters) in physical
                       //   layout mode
    int pw, ph;        // column width, height (in characters) in
                       //   physical layout mode

    friend class TextPage;
};

//------------------------------------------------------------------------
// TextWordList
//------------------------------------------------------------------------

class TextWordList {
public:
    TextWordList (GList* wordsA);

    ~TextWordList ();

    // Return the number of words on the list.
    int getLength ();

    // Return the <idx>th word from the list.
    TextWord* get (int idx);

private:
    GList* words; // [TextWord]
};

//------------------------------------------------------------------------
// TextPage
//------------------------------------------------------------------------

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
    TextWordList* makeWordList ();

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
    void unrotateWords (GList* words, int rot);
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
    double getLineIndent (TextLine* line, TextBlock* blk);
    double getAverageLineSpacing (GList* lines);
    double getLineSpacing (TextLine* line0, TextLine* line1);
    void buildLines (TextBlock* blk, GList* lines);
    TextLine* buildLine (TextBlock* blk);
    void getLineChars (TextBlock* blk, GList* charsA);
    double computeWordSpacingThreshold (GList* charsA, int rot);
    int assignPhysLayoutPositions (GList* columns);
    void assignLinePhysPositions (GList* columns);
    void computeLinePhysWidth (TextLine* line, UnicodeMap* uMap);
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

//------------------------------------------------------------------------
// TextOutputDev
//------------------------------------------------------------------------

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

    //----- path painting
    virtual void stroke (GfxState* state);
    virtual void fill (GfxState* state);
    virtual void eoFill (GfxState* state);

    //----- link borders
    virtual void processLink (Link* link);

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
    TextWordList* makeWordList ();

    // Returns the TextPage object for the last rasterized page,
    // transferring ownership to the caller.
    TextPage* takeText ();

    // Turn extra processing for HTML conversion on or off.
    void enableHTMLExtras (bool html) { control.html = html; }

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

// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cmath>
#include <cctype>

#include <utils/memory.hh>
#include <utils/string.hh>
#include <utils/GList.hh>

#include <xpdf/Error.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/UnicodeMap.hh>
#include <xpdf/UnicodeTypeTable.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/Link.hh>
#include <xpdf/TextOutputDev.hh>

#include <range/v3/all.hpp>
using namespace ranges;

////////////////////////////////////////////////////////////////////////

// Size of bins used for horizontal and vertical profiles is
// splitPrecisionMul * minFontSize.
const double splitPrecisionMul = 0.05;

// Minimum allowed split precision.
const double minSplitPrecision = 0.01;

// yMin and yMax (or xMin and xMax for rot=1,3) are adjusted by this
// fraction of the text height, to allow for slightly overlapping
// lines (or large ascent/descent values).
const double ascentAdjustFactor = 0;
const double descentAdjustFactor = 0.35;

// Gaps larger than max{gap} - splitGapSlack * avgFontSize are
// considered to be equivalent.
const double splitGapSlack = 0.2;

// The vertical gap threshold (minimum gap required to split
// vertically) depends on the (approximate) number of lines in the
// block:
//   threshold = (max + slope * nLines) * avgFontSize
// with a min value of vertGapThresholdMin * avgFontSize.
const double vertGapThresholdMin = 0.8;
const double vertGapThresholdMax = 3;
const double vertGapThresholdSlope = -0.5;

// Vertical gap threshold for table mode.
const double vertGapThresholdTableMin = 0.2;
const double vertGapThresholdTableMax = 0.5;
const double vertGapThresholdTableSlope = -0.02;

// A large character has a font size larger than
// largeCharThreshold * avgFontSize.
const double largeCharThreshold = 1.5;

// A block will be split vertically only if the resulting chunk
// widths are greater than vertSplitChunkThreshold * avgFontSize.
const double vertSplitChunkThreshold = 2;

// Max difference in primary,secondary coordinates (as a fraction of
// the font size) allowed for duplicated text (fake boldface, drop
// shadows) which is to be discarded.
const double dupMaxPriDelta = 0.1;
const double dupMaxSecDelta = 0.2;

// Inter-character spacing that varies by less than this multiple of
// font size is assumed to be equivalent.
const double uniformSpacing = 0.07;

// Typical word spacing, as a fraction of font size.  This will be
// added to the minimum inter-character spacing, to account for wide
// character spacing.
const double wordSpacing = 0.1;

// Minimum paragraph indent from left margin, as a fraction of font
// size.
const double minParagraphIndent = 0.5;

// If the space between two lines is greater than
// paragraphSpacingThreshold * avgLineSpacing, start a new paragraph.
const double paragraphSpacingThreshold = 1.2;

// If font size changes by at least this much (measured in points)
// between lines, start a new paragraph.
const double paragraphFontSizeDelta = 1;

// Spaces at the start of a line in physical layout mode are this wide
// (as a multiple of font size).
const double physLayoutSpaceWidth = 0.33;

// Table cells (TextColumns) are allowed to overlap by this much
// in table layout mode (as a fraction of cell width or height).
const double tableCellOverlapSlack = 0.05;

// Primary axis delta which will cause a line break in raw mode
// (as a fraction of font size).
const double rawModeLineDelta = 0.5;

// Secondary axis delta which will cause a word break in raw mode
// (as a fraction of font size).
const double rawModeWordSpacing = 0.15;

// Secondary axis overlap which will cause a line break in raw mode
// (as a fraction of font size).
const double rawModeCharOverlap = 0.2;

// Max spacing (as a multiple of font size) allowed between the end of
// a line and a clipped character to be included in that line.
const double clippedTextMaxWordSpace = 0.5;

// Max width of underlines (in points).
const double maxUnderlineWidth = 3;

// Max horizontal distance between edge of word and start of underline
// (as a fraction of font size).
const double underlineSlack = 0.2;

// Max vertical distance between baseline of word and start of
// underline (as a fraction of font size).
const double underlineBaselineSlack = 0.2;

// Max distance between edge of text and edge of link border (as a
// fraction of font size).
const double hyperlinkSlack = 0.2;

////////////////////////////////////////////////////////////////////////

namespace {

//
// Character/column comparison objects:
//
const auto lessX = [](auto& lhs, auto& rhs) {
    return lhs->xMin < rhs->xMin;
};

const auto lessY = [](auto& lhs, auto& rhs) {
    return lhs->yMin < rhs->yMin;
};

//
// Pixel-based column comparison object:
//
const auto lessPixels = [](auto& lhs, auto& rhs) {
    return lhs->px < rhs->px;
};

//
// Word comparison objects:
//
const auto lessYX = [](auto& lhs, auto& rhs) {
    return
        lhs->yMin  < rhs->yMin || (
        lhs->yMin == rhs->yMin && lhs->xMin < rhs->xMin);
};

const auto lessCharPos = [](auto& lhs, auto& rhs) {
    return lhs->charPos [0] < rhs->charPos [0];
};

} // anonymous namespace

////////////////////////////////////////////////////////////////////////

//
// TextFontInfo
//
struct TextFontInfo {
    explicit TextFontInfo (GfxState*);

    bool matches (GfxState*) const;

    //
    // Get the font name (which may be NULL):
    //
    GString* getFontName () const { return name; }

    //
    // Get font descriptor flags:
    //
    bool isFixedWidth () const { return flags & fontFixedWidth; }
    bool isSerif      () const { return flags & fontSerif;      }
    bool isSymbolic   () const { return flags & fontSymbolic;   }
    bool isItalic     () const { return flags & fontItalic;     }
    bool isBold       () const { return flags & fontBold;       }

    double getWidth () const { return width; }

    Ref id;
    GString* name;

    double width, ascent, descent;
    unsigned flags;
};

//
// TextChar
//
struct TextChar {
    static int cmpX (const void* p1, const void* p2) {
        const TextChar* ch1 = *(const TextChar**)p1;
        const TextChar* ch2 = *(const TextChar**)p2;

        if (ch1->xmin < ch2->xmin) {
            return -1;
        }
        else if (ch1->xmin > ch2->xmin) {
            return 1;
        }
        else {
            return 0;
        }
    }

    static int cmpY (const void* p1, const void* p2) {
        const TextChar* ch1 = *(const TextChar**)p1;
        const TextChar* ch2 = *(const TextChar**)p2;

        if (ch1->ymin < ch2->ymin) {
            return -1;
        }
        else if (ch1->ymin > ch2->ymin) {
            return 1;
        }
        else {
            return 0;
        }
    }

    TextFontInfo* font;
    double fontSize;

    double xmin, ymin, xmax, ymax;
    double r, g, b;

    Unicode c;
    int charPos;

    unsigned char charLen : 4, rot : 2, clipped : 1, invisible : 1;
};

//
// TextWord
//
struct TextWord {
    TextWord (GList* chars, int start, int lenA, int rotA, bool spaceAfterA);

    // Get the TextFontInfo object associated with this word.
    TextFontInfo* getFontInfo () const { return font; }

    size_t size () const { return text.size (); }

    Unicode get (int idx) { return text[idx]; }

    GString* getFontName () const;

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
    // "Near" edge x or y coord of each char (plus one extra entry for the last
    // char):
    //
    std::vector< double > edge;

    //
    // Bounding box, colors:
    //
    double xMin, xMax, yMin, yMax, colorR, colorG, colorB;

    double fontSize;
    TextFontInfo* font;

    unsigned char
        rot        : 2, // multiple of 90°: 0, 1, 2, or 3
        spaceAfter : 1, // set if ∃ separating space before next character
        underlined : 1, // underlined ...?
        invisible  : 1; // invisible, render mode 3
};

//
// TextLine
//
class TextLine {
public:
    TextLine (TextWords, double, double, double, double, double);

    double getXMin () { return xMin; }
    double getYMin () { return yMin; }

    double getBaseline ();

    int getRotation () { return rot; }

    TextWords&       getWords ()       { return words; }
    TextWords const& getWords () const { return words; }

private:
    TextWords words;

    int rot;           // rotation, multiple of 90 degrees
                       //   (0, 1, 2, or 3)
    double xMin, xMax; // bounding box x coordinates
    double yMin, yMax; // bounding box y coordinates
    double fontSize;   // main (max) font size for this line

    //
    // Unicode text of the line, including spaces between words:
    //
    std::vector< Unicode > text;

    //
    // "Near" edge x or y coord of each char (plus one extra entry for the last
    // char):
    //
    std::vector< double > edge;

    int len;           // number of Unicode chars
    bool hyphenated;   // set if last char is a hyphen
    int px;            // x offset (in characters, relative to
                       //   containing column) in physical layout mode
    int pw;            // line width (in characters) in physical
                       //   layout mode

    friend class TextPage;
    friend class TextParagraph;
};

//
// TextParagraph
//
struct TextParagraph {
    TextParagraph (TextLines);

    TextLines lines;

    double xMin, xMax; // bounding box x coordinates
    double yMin, yMax; // bounding box y coordinates
};

//
// TextColumn
//
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

//
// TextBlock
//
enum TextBlockType { blkVertSplit, blkHorizSplit, blkLeaf };

enum TextBlockTag { blkTagMulticolumn, blkTagColumn, blkTagLine };

class TextBlock {
public:
    TextBlock (TextBlockType typeA, int rotA);
    ~TextBlock ();
    void addChild (TextBlock* child);
    void addChild (TextChar* child);
    void prependChild (TextChar* child);
    void updateBounds (int childIdx);

    TextBlockType type;
    TextBlockTag tag;
    int rot;
    double xMin, yMin, xMax, yMax;
    bool smallSplit; // true for blkVertSplit/blkHorizSplit
                      //   where the gap size is small
    GList* children;  // for blkLeaf, children are TextWord;
        //   for others, children are TextBlock
};

TextBlock::TextBlock (TextBlockType typeA, int rotA) {
    type = typeA;
    tag = blkTagMulticolumn;
    rot = rotA;
    xMin = yMin = xMax = yMax = 0;
    smallSplit = false;
    children = new GList ();
}

TextBlock::~TextBlock () {
    if (type == blkLeaf) { delete children; }
    else {
        deleteGList (children, TextBlock);
    }
}

void TextBlock::addChild (TextBlock* child) {
    if (children->getLength () == 0) {
        xMin = child->xMin;
        yMin = child->yMin;
        xMax = child->xMax;
        yMax = child->yMax;
    }
    else {
        if (child->xMin < xMin) { xMin = child->xMin; }
        if (child->yMin < yMin) { yMin = child->yMin; }
        if (child->xMax > xMax) { xMax = child->xMax; }
        if (child->yMax > yMax) { yMax = child->yMax; }
    }
    children->append (child);
}

void TextBlock::addChild (TextChar* child) {
    if (children->getLength () == 0) {
        xMin = child->xmin;
        yMin = child->ymin;
        xMax = child->xmax;
        yMax = child->ymax;
    }
    else {
        if (child->xmin < xMin) { xMin = child->xmin; }
        if (child->ymin < yMin) { yMin = child->ymin; }
        if (child->xmax > xMax) { xMax = child->xmax; }
        if (child->ymax > yMax) { yMax = child->ymax; }
    }
    children->append (child);
}

void TextBlock::prependChild (TextChar* child) {
    if (children->getLength () == 0) {
        xMin = child->xmin;
        yMin = child->ymin;
        xMax = child->xmax;
        yMax = child->ymax;
    }
    else {
        if (child->xmin < xMin) { xMin = child->xmin; }
        if (child->ymin < yMin) { yMin = child->ymin; }
        if (child->xmax > xMax) { xMax = child->xmax; }
        if (child->ymax > yMax) { yMax = child->ymax; }
    }
    children->insert (0, child);
}

void TextBlock::updateBounds (int childIdx) {
    TextBlock* child;

    child = (TextBlock*)children->get (childIdx);
    if (child->xMin < xMin) { xMin = child->xMin; }
    if (child->yMin < yMin) { yMin = child->yMin; }
    if (child->xMax > xMax) { xMax = child->xMax; }
    if (child->yMax > yMax) { yMax = child->yMax; }
}

//
// TextUnderline
//
class TextUnderline {
public:
    TextUnderline (double x0A, double y0A, double x1A, double y1A) {
        x0 = x0A;
        y0 = y0A;
        x1 = x1A;
        y1 = y1A;
        horiz = y0 == y1;
    }
    ~TextUnderline () {}

    double x0, y0, x1, y1;
    bool horiz;
};

//
// TextLink
//
class TextLink {
public:
    TextLink (
        double xMinA, double yMinA, double xMaxA, double yMaxA, GString* uriA) {
        xMin = xMinA;
        yMin = yMinA;
        xMax = xMaxA;
        yMax = yMaxA;
        uri = uriA;
    }
    ~TextLink ();

    double xMin, yMin, xMax, yMax;
    GString* uri;
};

TextLink::~TextLink () {
    if (uri) { delete uri; }
}

//
// TextFontInfo
//
TextFontInfo::TextFontInfo (GfxState* state)
    : id{ 0, -1, -1 }, name (), width (), ascent (0.75), descent (-0.25),
      flags () {

    GfxFont* gfxFont = state->getFont ();

    if (gfxFont) {
        id = *gfxFont->getID ();

        ascent  = gfxFont->getAscent ();
        descent = gfxFont->getDescent ();

        // "odd" ascent/descent values cause trouble more often than not
        // (in theory these could be legitimate values for oddly designed
        // fonts -- but they are more often due to buggy PDF generators)
        // (values that are too small are a different issue -- those seem
        // to be more commonly legitimate)
        if (ascent  >    1) { ascent  =  0.75; }
        if (descent < -0.5) { descent = -0.25; }

        flags = gfxFont->getFlags ();

        if (gfxFont->as_name ()) {
            name = gfxFont->as_name ();
        }
    }
    else {
        ascent = 0.75;
        descent = -0.25;
    }

    if (gfxFont && !gfxFont->isCIDFont ()) {
        Gfx8BitFont* cidFont = reinterpret_cast< Gfx8BitFont* > (gfxFont);

        for (int code = 0; code < 256; ++code) {
            const char* name = cidFont->getCharName (code);

            if (name && name [0] == 'm' && name [1] == '\0') {
                width = cidFont->getWidth (code);
                break;
            }
        }
    }
}

bool TextFontInfo::matches (GfxState* state) const {
    return state->getFont () && *state->getFont ()->getID () == id;
}

//
// TextWord
//
//
// Build a TextWord object, using chars[start .. start+len-1].
// (If rot >= 2, the chars list is in reverse order.)
//
TextWord::TextWord (
    GList* chars, int start, int lenA, int rotA, bool spaceAfterA) {
    TextChar* ch;
    int i;

    rot = rotA;

    const auto len = lenA;

    text.resize (len);
    edge.resize (len + 1);
    charPos.resize (len + 1);

    switch (rot) {
    case 0:
    default:
        ch = (TextChar*)chars->get (start);
        xMin = ch->xmin;
        yMin = ch->ymin;
        yMax = ch->ymax;
        ch = (TextChar*)chars->get (start + len - 1);
        xMax = ch->xmax;
        break;
    case 1:
        ch = (TextChar*)chars->get (start);
        xMin = ch->xmin;
        xMax = ch->xmax;
        yMin = ch->ymin;
        ch = (TextChar*)chars->get (start + len - 1);
        yMax = ch->ymax;
        break;
    case 2:
        ch = (TextChar*)chars->get (start);
        xMax = ch->xmax;
        yMin = ch->ymin;
        yMax = ch->ymax;
        ch = (TextChar*)chars->get (start + len - 1);
        xMin = ch->xmin;
        break;
    case 3:
        ch = (TextChar*)chars->get (start);
        xMin = ch->xmin;
        xMax = ch->xmax;
        yMax = ch->ymax;
        ch = (TextChar*)chars->get (start + len - 1);
        yMin = ch->ymin;
        break;
    }

    for (i = 0; i < len; ++i) {
        ch = (TextChar*)chars->get (rot >= 2 ? start + len - 1 - i : start + i);
        text[i] = ch->c;
        charPos[i] = ch->charPos;
        if (i == len - 1) { charPos[len] = ch->charPos + ch->charLen; }
        switch (rot) {
        case 0:
        default:
            edge[i] = ch->xmin;
            if (i == len - 1) { edge[len] = ch->xmax; }
            break;
        case 1:
            edge[i] = ch->ymin;
            if (i == len - 1) { edge[len] = ch->ymax; }
            break;
        case 2:
            edge[i] = ch->xmax;
            if (i == len - 1) { edge[len] = ch->xmin; }
            break;
        case 3:
            edge[i] = ch->ymax;
            if (i == len - 1) { edge[len] = ch->ymin; }
            break;
        }
    }

    ch = (TextChar*)chars->get (start);

    font = ch->font;
    fontSize = ch->fontSize;

    spaceAfter = spaceAfterA;
    underlined = false;

    colorR = ch->r;
    colorG = ch->g;
    colorB = ch->b;

    invisible = ch->invisible;
}

GString* TextWord::getFontName () const {
    return font->name;
}

void TextWord::getCharBBox (
    int charIdx, double* xMinA, double* yMinA, double* xMaxA, double* yMaxA) {
    if (charIdx < 0 || charIdx >= text.size ()) { return; }
    switch (rot) {
    case 0:
        *xMinA = edge[charIdx];
        *xMaxA = edge[charIdx + 1];
        *yMinA = yMin;
        *yMaxA = yMax;
        break;
    case 1:
        *xMinA = xMin;
        *xMaxA = xMax;
        *yMinA = edge[charIdx];
        *yMaxA = edge[charIdx + 1];
        break;
    case 2:
        *xMinA = edge[charIdx + 1];
        *xMaxA = edge[charIdx];
        *yMinA = yMin;
        *yMaxA = yMax;
        break;
    case 3:
        *xMinA = xMin;
        *xMaxA = xMax;
        *yMinA = edge[charIdx + 1];
        *yMaxA = edge[charIdx];
        break;
    }
}

double TextWord::getBaseline () {
    switch (rot) {
    case 0:
    default: return yMax + fontSize * font->descent;
    case 1: return xMin - fontSize * font->descent;
    case 2: return yMin - fontSize * font->descent;
    case 3: return xMax + fontSize * font->descent;
    }
}

//
// TextLine
//
TextLine::TextLine (
    TextWords wordsA,
    double xMinA, double yMinA,
    double xMaxA, double yMaxA,
    double fontSizeA)
    : words (std::move (wordsA)) {

    rot = 0;

    xMin = xMinA;
    yMin = yMinA;

    xMax = xMaxA;
    yMax = yMaxA;

    fontSize = fontSizeA;

    px = 0;
    pw = 0;

    //
    // Build the text:
    //
    len = 0;

    for (auto& word : words) {
        len += word->size ();

        if (word->spaceAfter) {
            ++len;
        }
    }

    text.resize (len);
    edge.resize (len + 1);

    if (!words.empty ()) {
        rot = words.front ()->rot;
    }

    size_t j = 0;

    for (auto& word : words) {
        for (size_t k = 0; k < word->size (); ++k) {
            text [j] = word->text [k];
            edge [j] = word->edge [k];
            ++j;
        }

        edge[j] = word->edge[word->size ()];

        if (word->spaceAfter) {
            text [j++] = (Unicode)0x0020;
            edge [j] = edge [j - 1];
        }
    }

    //
    // TODO: need to check for other Unicode chars used as hyphens:
    //
    hyphenated = text [len - 1] == (Unicode)'-';
}

double TextLine::getBaseline () {
    auto& word = words.front ();

    switch (rot) {
    case 1:  return xMin - fontSize * word->font->descent;
    case 2:  return yMin - fontSize * word->font->descent;
    case 3:  return xMax + fontSize * word->font->descent;
    case 0:
    default: return yMax + fontSize * word->font->descent;
    }
}

//
// TextParagraph
//
TextParagraph::TextParagraph (TextLines arg)
    : lines (std::move (arg)), xMin{ }, xMax{ }, yMin{ }, yMax{ } {

    bool first = true;

    for (auto& line : lines) {
        if (first || line->xMin < xMin) { xMin = line->xMin; }
        if (first || line->yMin < yMin) { yMin = line->yMin; }
        if (first || line->xMax > xMax) { xMax = line->xMax; }
        if (first || line->yMax > yMax) { yMax = line->yMax; }

        first = false;
    }
}

//
// TextColumn
//
TextColumn::TextColumn (
    GList* paragraphsA, double xMinA, double yMinA, double xMaxA,
    double yMaxA) {
    paragraphs = paragraphsA;
    xMin = xMinA;
    yMin = yMinA;
    xMax = xMaxA;
    yMax = yMaxA;
    px = py = 0;
    pw = ph = 0;
}

TextColumn::~TextColumn () { deleteGList (paragraphs, TextParagraph); }

int TextColumn::cmpX (const void* p1, const void* p2) {
    const TextColumn* col1 = *(const TextColumn**)p1;
    const TextColumn* col2 = *(const TextColumn**)p2;

    if (col1->xMin < col2->xMin) { return -1; }
    else if (col1->xMin > col2->xMin) {
        return 1;
    }
    else {
        return 0;
    }
}

int TextColumn::cmpY (const void* p1, const void* p2) {
    const TextColumn* col1 = *(const TextColumn**)p1;
    const TextColumn* col2 = *(const TextColumn**)p2;

    if (col1->yMin < col2->yMin) { return -1; }
    else if (col1->yMin > col2->yMin) {
        return 1;
    }
    else {
        return 0;
    }
}

int TextColumn::cmpPX (const void* p1, const void* p2) {
    const TextColumn* col1 = *(const TextColumn**)p1;
    const TextColumn* col2 = *(const TextColumn**)p2;

    if (col1->px < col2->px) { return -1; }
    else if (col1->px > col2->px) {
        return 1;
    }
    else {
        return 0;
    }
}

//
// TextPage
//
TextPage::TextPage (TextOutputControl* controlA) {
    control = *controlA;
    pageWidth = pageHeight = 0;
    charPos = 0;
    curFont = NULL;
    curFontSize = 0;
    curRot = 0;
    nTinyChars = 0;
    actualText = NULL;
    actualTextLen = 0;
    actualTextX0 = 0;
    actualTextY0 = 0;
    actualTextX1 = 0;
    actualTextY1 = 0;
    actualTextNBytes = 0;

    chars = new GList ();
    fonts = new GList ();

    underlines = new GList ();
    links = new GList ();

    findCols = NULL;
    findLR = true;
    lastFindXMin = lastFindYMin = 0;
    haveLastFind = false;
}

TextPage::~TextPage () {
    clear ();
    deleteGList (chars, TextChar);
    deleteGList (fonts, TextFontInfo);
    deleteGList (underlines, TextUnderline);
    deleteGList (links, TextLink);
    if (findCols) { deleteGList (findCols, TextColumn); }
}

void TextPage::startPage (GfxState* state) {
    clear ();
    if (state) {
        pageWidth = state->getPageWidth ();
        pageHeight = state->getPageHeight ();
    }
    else {
        pageWidth = pageHeight = 0;
    }
}

void TextPage::clear () {
    pageWidth = pageHeight = 0;
    charPos = 0;
    curFont = NULL;
    curFontSize = 0;
    curRot = 0;
    nTinyChars = 0;
    free (actualText);
    actualText = NULL;
    actualTextLen = 0;
    actualTextNBytes = 0;
    deleteGList (chars, TextChar);
    chars = new GList ();
    deleteGList (fonts, TextFontInfo);
    fonts = new GList ();
    deleteGList (underlines, TextUnderline);
    underlines = new GList ();
    deleteGList (links, TextLink);
    links = new GList ();

    if (findCols) {
        deleteGList (findCols, TextColumn);
        findCols = NULL;
    }
    findLR = true;
    lastFindXMin = lastFindYMin = 0;
    haveLastFind = false;
}

void TextPage::updateFont (GfxState* state) {
    GfxFont* gfxFont;
    double* fm;
    char* name;
    int code, mCode, letterCode, anyCode;
    double w;
    double m[4], m2[4];
    int i;

    // get the font info object
    curFont = NULL;

    for (i = 0; i < fonts->getLength (); ++i) {
        curFont = (TextFontInfo*)fonts->get (i);

        if (curFont->matches (state)) {
            break;
        }

        curFont = NULL;
    }

    if (!curFont) {
        curFont = new TextFontInfo (state);
        fonts->append (curFont);
    }

    // adjust the font size
    gfxFont = state->getFont ();
    curFontSize = state->getTransformedFontSize ();
    if (gfxFont && gfxFont->getType () == fontType3) {
        // This is a hack which makes it possible to deal with some Type 3
        // fonts.  The problem is that it's impossible to know what the
        // base coordinate system used in the font is without actually
        // rendering the font.  This code tries to guess by looking at the
        // width of the character 'm' (which breaks if the font is a
        // subset that doesn't contain 'm').
        mCode = letterCode = anyCode = -1;
        for (code = 0; code < 256; ++code) {
            name = ((Gfx8BitFont*)gfxFont)->getCharName (code);
            if (name && name[0] == 'm' && name[1] == '\0') { mCode = code; }
            if (letterCode < 0 && name && name[1] == '\0' &&
                ((name[0] >= 'A' && name[0] <= 'Z') ||
                 (name[0] >= 'a' && name[0] <= 'z'))) {
                letterCode = code;
            }
            if (anyCode < 0 && name &&
                ((Gfx8BitFont*)gfxFont)->getWidth (code) > 0) {
                anyCode = code;
            }
        }
        if (mCode >= 0 && (w = ((Gfx8BitFont*)gfxFont)->getWidth (mCode)) > 0) {
            // 0.6 is a generic average 'm' width -- yes, this is a hack
            curFontSize *= w / 0.6;
        }
        else if (
            letterCode >= 0 &&
            (w = ((Gfx8BitFont*)gfxFont)->getWidth (letterCode)) > 0) {
            // even more of a hack: 0.5 is a generic letter width
            curFontSize *= w / 0.5;
        }
        else if (
            anyCode >= 0 &&
            (w = ((Gfx8BitFont*)gfxFont)->getWidth (anyCode)) > 0) {
            // better than nothing: 0.5 is a generic character width
            curFontSize *= w / 0.5;
        }
        fm = gfxFont->getFontMatrix ();
        if (fm[0] != 0) { curFontSize *= fabs (fm[3] / fm[0]); }
    }

    // compute the rotation
    state->getFontTransMat (&m[0], &m[1], &m[2], &m[3]);
    if (gfxFont && gfxFont->getType () == fontType3) {
        fm = gfxFont->getFontMatrix ();
        m2[0] = fm[0] * m[0] + fm[1] * m[2];
        m2[1] = fm[0] * m[1] + fm[1] * m[3];
        m2[2] = fm[2] * m[0] + fm[3] * m[2];
        m2[3] = fm[2] * m[1] + fm[3] * m[3];
        m[0] = m2[0];
        m[1] = m2[1];
        m[2] = m2[2];
        m[3] = m2[3];
    }
    if (fabs (m[0] * m[3]) > fabs (m[1] * m[2])) {
        curRot = (m[0] > 0 || m[3] < 0) ? 0 : 2;
    }
    else {
        curRot = (m[2] > 0) ? 1 : 3;
    }
}

void TextPage::addChar (
    GfxState* state, double x, double y, double dx, double dy, CharCode c,
    int nBytes, Unicode* u, int uLen) {
    double x1, y1, x2, y2, w1, h1, dx2, dy2, ascent, descent, sp;
    double xMin, yMin, xMax, yMax;
    double clipXMin, clipYMin, clipXMax, clipYMax;
    GfxRGB rgb;
    bool clipped, rtl;
    int i, j;

    // if we're in an ActualText span, save the position info (the
    // ActualText chars will be added by TextPage::endActualText()).
    if (actualText) {
        if (!actualTextNBytes) {
            actualTextX0 = x;
            actualTextY0 = y;
        }
        actualTextX1 = x + dx;
        actualTextY1 = y + dy;
        actualTextNBytes += nBytes;
        return;
    }

    // subtract char and word spacing from the dx,dy values
    sp = state->getCharSpace ();
    if (c == (CharCode)0x20) { sp += state->getWordSpace (); }
    state->textTransformDelta (sp * state->getHorizScaling (), 0, &dx2, &dy2);
    dx -= dx2;
    dy -= dy2;
    state->transformDelta (dx, dy, &w1, &h1);

    // throw away chars that aren't inside the page bounds
    // (and also do a sanity check on the character size)
    state->transform (x, y, &x1, &y1);
    if (x1 + w1 < 0 || x1 > pageWidth || y1 + h1 < 0 || y1 > pageHeight ||
        w1 > pageWidth || h1 > pageHeight) {
        charPos += nBytes;
        return;
    }

    // check the tiny chars limit
    if (!globalParams->getTextKeepTinyChars () && fabs (w1) < 3 &&
        fabs (h1) < 3) {
        if (++nTinyChars > 50000) {
            charPos += nBytes;
            return;
        }
    }

    // skip space characters
    if (uLen == 1 && u[0] == (Unicode)0x20) {
        charPos += nBytes;
        return;
    }

    // check for clipping
    clipped = false;
    if (control.clipText) {
        state->getClipBBox (&clipXMin, &clipYMin, &clipXMax, &clipYMax);
        if (x1 + 0.1 * w1 < clipXMin || x1 + 0.9 * w1 > clipXMax ||
            y1 + 0.1 * h1 < clipYMin || y1 + 0.9 * h1 > clipYMax) {
            clipped = true;
        }
    }

    // add the characters
    if (uLen > 0) {
        // handle right-to-left ligatures: if there are multiple Unicode
        // characters, and they're all right-to-left, insert them in
        // right-to-left order
        if (uLen > 1) {
            rtl = true;
            for (i = 0; i < uLen; ++i) {
                if (!unicodeTypeR (u[i])) {
                    rtl = false;
                    break;
                }
            }
        }
        else {
            rtl = false;
        }

        w1 /= uLen;
        h1 /= uLen;
        ascent = curFont->ascent * curFontSize;
        descent = curFont->descent * curFontSize;
        for (i = 0; i < uLen; ++i) {
            x2 = x1 + i * w1;
            y2 = y1 + i * h1;
            switch (curRot) {
            case 0:
            default:
                xMin = x2;
                xMax = x2 + w1;
                yMin = y2 - ascent;
                yMax = y2 - descent;
                break;
            case 1:
                xMin = x2 + descent;
                xMax = x2 + ascent;
                yMin = y2;
                yMax = y2 + h1;
                break;
            case 2:
                xMin = x2 + w1;
                xMax = x2;
                yMin = y2 + descent;
                yMax = y2 + ascent;
                break;
            case 3:
                xMin = x2 - ascent;
                xMax = x2 - descent;
                yMin = y2 + h1;
                yMax = y2;
                break;
            }
            if ((state->getRender () & 3) == 1) {
                state->getStrokeRGB (&rgb);
            }
            else {
                state->getFillRGB (&rgb);
            }

            if (rtl) {
                j = uLen - 1 - i;
            }
            else {
                j = i;
            }

            if (xMin > xMax) { std::swap (xMin, xMax); }
            if (yMin > yMax) { std::swap (yMin, yMax); }

            chars->append (
                new TextChar{
                    curFont, curFontSize,
                    xMin, yMin, xMax, yMax,
                    xpdf::to_double (rgb.r),
                    xpdf::to_double (rgb.g),
                    xpdf::to_double (rgb.b),
                    u [j], charPos,
                    uint8_t (nBytes), uint8_t (curRot), clipped,
                    state->getRender () == 3 });
        }
    }

    charPos += nBytes;
}

void TextPage::incCharCount (int nChars) { charPos += nChars; }

void TextPage::beginActualText (GfxState* state, Unicode* u, int uLen) {
    if (actualText) { free (actualText); }
    actualText = (Unicode*)calloc (uLen, sizeof (Unicode));
    memcpy (actualText, u, uLen * sizeof (Unicode));
    actualTextLen = uLen;
    actualTextNBytes = 0;
}

void TextPage::endActualText (GfxState* state) {
    Unicode* u;

    u = actualText;
    actualText = NULL; // so we can call TextPage::addChar()
    if (actualTextNBytes) {
        // now that we have the position info for all of the text inside
        // the marked content span, we feed the "ActualText" back through
        // addChar()
        addChar (
            state, actualTextX0, actualTextY0, actualTextX1 - actualTextX0,
            actualTextY1 - actualTextY0, 0, actualTextNBytes, u, actualTextLen);
    }
    free (u);
    actualText = NULL;
    actualTextLen = 0;
    actualTextNBytes = false;
}

void TextPage::addUnderline (double x0, double y0, double x1, double y1) {
    underlines->append (new TextUnderline (x0, y0, x1, y1));
}

void TextPage::addLink (
    double xMin, double yMin, double xMax, double yMax, Link* link) {
    GString* uri;

    if (link && link->getAction () &&
        link->getAction ()->getKind () == actionURI) {
        uri = ((LinkURI*)link->getAction ())->getURI ()->copy ();
        links->append (new TextLink (xMin, yMin, xMax, yMax, uri));
    }
}

//
// TextPage: output
//
void TextPage::write (void* outputStream, TextOutputFunc outputFunc) {
    UnicodeMap* uMap;
    char space[8], eol[16], eop[8];
    int spaceLen, eolLen, eopLen;
    bool pageBreaks;

    // get the output encoding
    if (!(uMap = globalParams->getTextEncoding ())) { return; }
    spaceLen = uMap->mapUnicode (0x20, space, sizeof (space));
    eolLen = 0; // make gcc happy
    switch (globalParams->getTextEOL ()) {
    case eolUnix: eolLen = uMap->mapUnicode (0x0a, eol, sizeof (eol)); break;
    case eolDOS:
        eolLen = uMap->mapUnicode (0x0d, eol, sizeof (eol));
        eolLen += uMap->mapUnicode (0x0a, eol + eolLen, sizeof (eol) - eolLen);
        break;
    case eolMac: eolLen = uMap->mapUnicode (0x0d, eol, sizeof (eol)); break;
    }
    eopLen = uMap->mapUnicode (0x0c, eop, sizeof (eop));
    pageBreaks = globalParams->getTextPageBreaks ();

    switch (control.mode) {
    case textOutReadingOrder:
        writeReadingOrder (
            outputStream, outputFunc, uMap, space, spaceLen, eol, eolLen);
        break;
    case textOutPhysLayout:
    case textOutTableLayout:
        writePhysLayout (
            outputStream, outputFunc, uMap, space, spaceLen, eol, eolLen);
        break;
    case textOutLinePrinter:
        writeLinePrinter (
            outputStream, outputFunc, uMap, space, spaceLen, eol, eolLen);
        break;
    case textOutRawOrder:
        writeRaw (outputStream, outputFunc, uMap, space, spaceLen, eol, eolLen);
        break;
    }

    // end of page
    if (pageBreaks) { (*outputFunc) (outputStream, eop, eopLen); }

    uMap->decRefCnt ();
}

void TextPage::writeReadingOrder (
    void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
    char* space, int spaceLen, char* eol, int eolLen) {
    TextBlock* tree;
    TextColumn* col;
    TextParagraph* par;
    GList* columns;
    bool primaryLR;
    GString* s;
    int colIdx, parIdx, lineIdx, rot, n;

    rot = rotateChars (chars);
    primaryLR = checkPrimaryLR (chars);
    tree = splitChars (chars);
#if 0 //~debug
  dumpTree(tree);
#endif
    if (!tree) {
        // no text
        unrotateChars (chars, rot);
        return;
    }
    columns = buildColumns (tree);
    delete tree;
    unrotateChars (chars, rot);
#if 0 //~debug
  dumpColumns(columns);
#endif

    for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
        col = (TextColumn*)columns->get (colIdx);
        for (parIdx = 0; parIdx < col->paragraphs->getLength (); ++parIdx) {
            par = (TextParagraph*)col->paragraphs->get (parIdx);
            for (lineIdx = 0; lineIdx < par->lines.size (); ++lineIdx) {
                auto& line = par->lines [lineIdx];

                n = line->len;

                if (line->hyphenated && lineIdx + 1 < par->lines.size ()) {
                    --n;
                }

                s = new GString ();
                encodeFragment (line->text.data (), n, uMap, primaryLR, s);

                if (lineIdx + 1 < par->lines.size () && !line->hyphenated) {
                    s->append (space, spaceLen);
                }

                (*outputFunc) (outputStream, s->c_str (), s->getLength ());

                delete s;
            }
            (*outputFunc) (outputStream, eol, eolLen);
        }
        (*outputFunc) (outputStream, eol, eolLen);
    }

    deleteGList (columns, TextColumn);
}

GList* TextPage::makeColumns () {
    TextBlock* tree;
    GList* columns;

    tree = splitChars (chars);
    if (!tree) {
        // no text
        return new GList ();
    }
    columns = buildColumns (tree);
    delete tree;
    return columns;
}

// This handles both physical layout and table layout modes.
void TextPage::writePhysLayout (
    void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
    char* space, int spaceLen, char* eol, int eolLen) {
    TextBlock* tree;
    GString** out;
    int* outLen;
    TextColumn* col;
    TextParagraph* par;
    GList* columns;
    bool primaryLR;
    int ph, colIdx, parIdx, lineIdx, rot, y, i;

#if 0 //~debug
  dumpChars(chars);
#endif

  rot = rotateChars (chars);
    primaryLR = checkPrimaryLR (chars);
    tree = splitChars (chars);

#if 0 //~debug
  dumpTree(tree);
#endif

  if (!tree) {
        // no text
        unrotateChars (chars, rot);
        return;
    }
    columns = buildColumns (tree);
    delete tree;
    unrotateChars (chars, rot);
    ph = assignPhysLayoutPositions (columns);

#if 0 //~debug
  dumpColumns(columns);
#endif

    out = (GString**)calloc (ph, sizeof (GString*));
    outLen = (int*)calloc (ph, sizeof (int));
    for (i = 0; i < ph; ++i) {
        out[i] = NULL;
        outLen[i] = 0;
    }

    columns->sort (&TextColumn::cmpPX);
    for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
        col = (TextColumn*)columns->get (colIdx);
        y = col->py;
        for (parIdx = 0; parIdx < col->paragraphs->getLength () && y < ph;
             ++parIdx) {
            par = (TextParagraph*)col->paragraphs->get (parIdx);
            for (lineIdx = 0; lineIdx < par->lines.size () && y < ph;
                 ++lineIdx) {
                auto& line = par->lines [lineIdx];
                if (!out[y]) { out[y] = new GString (); }
                while (outLen[y] < col->px + line->px) {
                    out[y]->append (space, spaceLen);
                    ++outLen[y];
                }
                encodeFragment (line->text.data (), line->len, uMap, primaryLR, out[y]);
                outLen[y] += line->pw;
                ++y;
            }
            if (parIdx + 1 < col->paragraphs->getLength ()) { ++y; }
        }
    }

    for (i = 0; i < ph; ++i) {
        if (out[i]) {
            (*outputFunc) (
                outputStream, out[i]->c_str (), out[i]->getLength ());
            delete out[i];
        }
        (*outputFunc) (outputStream, eol, eolLen);
    }

    free (out);
    free (outLen);

    deleteGList (columns, TextColumn);
}

void TextPage::writeLinePrinter (
    void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
    char* space, int spaceLen, char* eol, int eolLen) {
    TextChar *ch, *ch2;
    GList* line;
    GString* s;
    char buf[8];
    double pitch, lineSpacing, delta;
    double yMin0, yShift, xMin0, xShift;
    double y, x;
    int rot, n, i, j, k;

    rot = rotateChars (chars);
    chars->sort (&TextChar::cmpX);
    removeDuplicates (chars, 0);
    chars->sort (&TextChar::cmpY);

    // get character pitch
    if (control.fixedPitch > 0) { pitch = control.fixedPitch; }
    else {
        // compute (approximate) character pitch
        pitch = pageWidth;
        for (i = 0; i < chars->getLength (); ++i) {
            ch = (TextChar*)chars->get (i);
            for (j = i + 1; j < chars->getLength (); ++j) {
                ch2 = (TextChar*)chars->get (j);
                if (ch2->ymin + ascentAdjustFactor * (ch2->ymax - ch2->ymin) <
                        ch->ymax -
                            descentAdjustFactor * (ch->ymax - ch->ymin) &&
                    ch->ymin + ascentAdjustFactor * (ch->ymax - ch->ymin) <
                        ch2->ymax -
                            descentAdjustFactor * (ch2->ymax - ch2->ymin)) {
                    delta = fabs (ch2->xmin - ch->xmin);
                    if (delta > 0 && delta < pitch) { pitch = delta; }
                }
            }
        }
    }

    // get line spacing
    if (control.fixedLineSpacing > 0) {
        lineSpacing = control.fixedLineSpacing;
    }
    else {
        // compute (approximate) line spacing
        lineSpacing = pageHeight;
        i = 0;
        while (i < chars->getLength ()) {
            ch = (TextChar*)chars->get (i);
            // look for the first char that does not (substantially)
            // vertically overlap this one
            delta = 0;
            for (++i; delta == 0 && i < chars->getLength (); ++i) {
                ch2 = (TextChar*)chars->get (i);
                if (ch2->ymin + ascentAdjustFactor * (ch2->ymax - ch2->ymin) >
                    ch->ymax - descentAdjustFactor * (ch->ymax - ch->ymin)) {
                    delta = ch2->ymin - ch->ymin;
                }
            }
            if (delta > 0 && delta < lineSpacing) { lineSpacing = delta; }
        }
    }

    // shift the grid to avoid problems with floating point accuracy --
    // for fixed line spacing, this avoids problems with
    // dropping/inserting blank lines
    if (chars->getLength ()) {
        yMin0 = ((TextChar*)chars->get (0))->ymin;
        yShift = yMin0 - (int)(yMin0 / lineSpacing + 0.5) * lineSpacing -
                 0.5 * lineSpacing;
    }
    else {
        yShift = 0;
    }

    // for each line...
    i = 0;
    j = chars->getLength () - 1;
    for (y = yShift; y < pageHeight; y += lineSpacing) {
        // get the characters in this line
        line = new GList;
        while (i < chars->getLength () &&
               ((TextChar*)chars->get (i))->ymin < y + lineSpacing) {
            line->append (chars->get (i++));
        }
        line->sort (&TextChar::cmpX);

        // shift the grid to avoid problems with floating point accuracy
        // -- for fixed char spacing, this avoids problems with
        // dropping/inserting spaces
        if (line->getLength ()) {
            xMin0 = ((TextChar*)line->get (0))->xmin;
            xShift = xMin0 - (int)(xMin0 / pitch + 0.5) * pitch - 0.5 * pitch;
        }
        else {
            xShift = 0;
        }

        // write the line
        s = new GString ();
        x = xShift;
        k = 0;
        while (k < line->getLength ()) {
            ch = (TextChar*)line->get (k);
            if (ch->xmin < x + pitch) {
                n = uMap->mapUnicode (ch->c, buf, sizeof (buf));
                s->append (buf, n);
                ++k;
            }
            else {
                s->append (space, spaceLen);
                n = spaceLen;
            }
            x += (uMap->isUnicode () ? 1 : n) * pitch;
        }
        s->append (eol, eolLen);
        (*outputFunc) (outputStream, s->c_str (), s->getLength ());
        delete s;
        delete line;
    }

    unrotateChars (chars, rot);
}

void TextPage::writeRaw (
    void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
    char* space, int spaceLen, char* eol, int eolLen) {
    TextChar *ch, *ch2;
    GString* s;
    char buf[8];
    int n, i;

    s = new GString ();

    for (i = 0; i < chars->getLength (); ++i) {
        // process one char
        ch = (TextChar*)chars->get (i);
        n = uMap->mapUnicode (ch->c, buf, sizeof (buf));
        s->append (buf, n);

        // check for space or eol
        if (i + 1 < chars->getLength ()) {
            ch2 = (TextChar*)chars->get (i + 1);
            if (ch2->rot != ch->rot) { s->append (eol, eolLen); }
            else {
                switch (ch->rot) {
                case 0:
                default:
                    if (fabs (ch2->ymin - ch->ymin) >
                            rawModeLineDelta * ch->fontSize ||
                        ch2->xmin - ch->xmax <
                            -rawModeCharOverlap * ch->fontSize) {
                        s->append (eol, eolLen);
                    }
                    else if (
                        ch2->xmin - ch->xmax >
                        rawModeWordSpacing * ch->fontSize) {
                        s->append (space, spaceLen);
                    }
                    break;
                case 1:
                    if (fabs (ch->xmax - ch2->xmax) >
                            rawModeLineDelta * ch->fontSize ||
                        ch2->ymin - ch->ymax <
                            -rawModeCharOverlap * ch->fontSize) {
                        s->append (eol, eolLen);
                    }
                    else if (
                        ch2->ymin - ch->ymax >
                        rawModeWordSpacing * ch->fontSize) {
                        s->append (space, spaceLen);
                    }
                    break;
                case 2:
                    if (fabs (ch->ymax - ch2->ymax) >
                            rawModeLineDelta * ch->fontSize ||
                        ch->xmin - ch2->xmax <
                            -rawModeCharOverlap * ch->fontSize) {
                        s->append (eol, eolLen);
                    }
                    else if (
                        ch->xmin - ch2->xmax >
                        rawModeWordSpacing * ch->fontSize) {
                        s->append (space, spaceLen);
                    }
                    break;
                case 3:
                    if (fabs (ch2->xmin - ch->xmin) >
                            rawModeLineDelta * ch->fontSize ||
                        ch->ymin - ch2->ymax <
                            -rawModeCharOverlap * ch->fontSize) {
                        s->append (eol, eolLen);
                    }
                    else if (
                        ch->ymin - ch2->ymax >
                        rawModeWordSpacing * ch->fontSize) {
                        s->append (space, spaceLen);
                    }
                    break;
                }
            }
        }
        else {
            s->append (eol, eolLen);
        }

        if (s->getLength () > 1000) {
            (*outputFunc) (outputStream, s->c_str (), s->getLength ());
            s->clear ();
        }
    }

    if (s->getLength () > 0) {
        (*outputFunc) (outputStream, s->c_str (), s->getLength ());
    }
    delete s;
}

void TextPage::encodeFragment (
    Unicode* text, int len, UnicodeMap* uMap, bool primaryLR, GString* s) {
    char lre[8], rle[8], popdf[8], buf[8];
    int lreLen, rleLen, popdfLen, n;
    int i, j, k;

    if (uMap->isUnicode ()) {
        lreLen = uMap->mapUnicode (0x202a, lre, sizeof (lre));
        rleLen = uMap->mapUnicode (0x202b, rle, sizeof (rle));
        popdfLen = uMap->mapUnicode (0x202c, popdf, sizeof (popdf));

        if (primaryLR) {
            i = 0;
            while (i < len) {
                // output a left-to-right section
                for (j = i; j < len && !unicodeTypeR (text[j]); ++j)
                    ;
                for (k = i; k < j; ++k) {
                    n = uMap->mapUnicode (text[k], buf, sizeof (buf));
                    s->append (buf, n);
                }
                i = j;
                // output a right-to-left section
                for (j = i; j < len && !(unicodeTypeL (text[j]) ||
                                         unicodeTypeNum (text[j]));
                     ++j)
                    ;
                if (j > i) {
                    s->append (rle, rleLen);
                    for (k = j - 1; k >= i; --k) {
                        n = uMap->mapUnicode (text[k], buf, sizeof (buf));
                        s->append (buf, n);
                    }
                    s->append (popdf, popdfLen);
                    i = j;
                }
            }
        }
        else {
            // Note: This code treats numeric characters (European and
            // Arabic/Indic) as left-to-right, which isn't strictly correct
            // (incurs extra LRE/POPDF pairs), but does produce correct
            // visual formatting.
            s->append (rle, rleLen);
            i = len - 1;
            while (i >= 0) {
                // output a right-to-left section
                for (j = i; j >= 0 && !(unicodeTypeL (text[j]) ||
                                        unicodeTypeNum (text[j]));
                     --j)
                    ;
                for (k = i; k > j; --k) {
                    n = uMap->mapUnicode (text[k], buf, sizeof (buf));
                    s->append (buf, n);
                }
                i = j;
                // output a left-to-right section
                for (j = i; j >= 0 && !unicodeTypeR (text[j]); --j)
                    ;
                if (j < i) {
                    s->append (lre, lreLen);
                    for (k = j + 1; k <= i; ++k) {
                        n = uMap->mapUnicode (text[k], buf, sizeof (buf));
                        s->append (buf, n);
                    }
                    s->append (popdf, popdfLen);
                    i = j;
                }
            }
            s->append (popdf, popdfLen);
        }
    }
    else {
        for (i = 0; i < len; ++i) {
            n = uMap->mapUnicode (text[i], buf, sizeof (buf));
            s->append (buf, n);
        }
    }
}

//
// TextPage: layout analysis
//
// Determine primary (most common) rotation value.  Rotate all chars
// to that primary rotation.
int TextPage::rotateChars (GList* charsA) {
    TextChar* ch;
    int nChars[4];
    double xMin, yMin, xMax, yMax, t;
    int rot, i;

    // determine primary rotation
    nChars[0] = nChars[1] = nChars[2] = nChars[3] = 0;
    for (i = 0; i < charsA->getLength (); ++i) {
        ch = (TextChar*)charsA->get (i);
        ++nChars[ch->rot];
    }
    rot = 0;
    for (i = 1; i < 4; ++i) {
        if (nChars[i] > nChars[rot]) { rot = i; }
    }

    // rotate
    switch (rot) {
    case 0:
    default: break;
    case 1:
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            xMin = ch->ymin;
            xMax = ch->ymax;
            yMin = pageWidth - ch->xmax;
            yMax = pageWidth - ch->xmin;
            ch->xmin = xMin;
            ch->xmax = xMax;
            ch->ymin = yMin;
            ch->ymax = yMax;
            ch->rot = (ch->rot + 3) & 3;
        }
        t = pageWidth;
        pageWidth = pageHeight;
        pageHeight = t;
        break;
    case 2:
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            xMin = pageWidth - ch->xmax;
            xMax = pageWidth - ch->xmin;
            yMin = pageHeight - ch->ymax;
            yMax = pageHeight - ch->ymin;
            ch->xmin = xMin;
            ch->xmax = xMax;
            ch->ymin = yMin;
            ch->ymax = yMax;
            ch->rot = (ch->rot + 2) & 3;
        }
        break;
    case 3:
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            xMin = pageHeight - ch->ymax;
            xMax = pageHeight - ch->ymin;
            yMin = ch->xmin;
            yMax = ch->xmax;
            ch->xmin = xMin;
            ch->xmax = xMax;
            ch->ymin = yMin;
            ch->ymax = yMax;
            ch->rot = (ch->rot + 1) & 3;
        }
        t = pageWidth;
        pageWidth = pageHeight;
        pageHeight = t;
        break;
    }

    return rot;
}

// Rotate the TextUnderlines and TextLinks to match the transform
// performed by rotateChars().
void TextPage::rotateUnderlinesAndLinks (int rot) {
    TextUnderline* underline;
    TextLink* link;
    double xMin, yMin, xMax, yMax;
    int i;

    switch (rot) {
    case 0:
    default: break;
    case 1:
        for (i = 0; i < underlines->getLength (); ++i) {
            underline = (TextUnderline*)underlines->get (i);
            xMin = underline->y0;
            xMax = underline->y1;
            yMin = pageWidth - underline->x1;
            yMax = pageWidth - underline->x0;
            underline->x0 = xMin;
            underline->x1 = xMax;
            underline->y0 = yMin;
            underline->y1 = yMax;
            underline->horiz = !underline->horiz;
        }
        for (i = 0; i < links->getLength (); ++i) {
            link = (TextLink*)links->get (i);
            xMin = link->yMin;
            xMax = link->yMax;
            yMin = pageWidth - link->xMax;
            yMax = pageWidth - link->xMin;
            link->xMin = xMin;
            link->xMax = xMax;
            link->yMin = yMin;
            link->yMax = yMax;
        }
        break;
    case 2:
        for (i = 0; i < underlines->getLength (); ++i) {
            underline = (TextUnderline*)underlines->get (i);
            xMin = pageWidth - underline->x1;
            xMax = pageWidth - underline->x0;
            yMin = pageHeight - underline->y1;
            yMax = pageHeight - underline->y0;
            underline->x0 = xMin;
            underline->x1 = xMax;
            underline->y0 = yMin;
            underline->y1 = yMax;
        }
        for (i = 0; i < links->getLength (); ++i) {
            link = (TextLink*)links->get (i);
            xMin = pageWidth - link->xMax;
            xMax = pageWidth - link->xMin;
            yMin = pageHeight - link->yMax;
            yMax = pageHeight - link->yMin;
            link->xMin = xMin;
            link->xMax = xMax;
            link->yMin = yMin;
            link->yMax = yMax;
        }
        break;
    case 3:
        for (i = 0; i < underlines->getLength (); ++i) {
            underline = (TextUnderline*)underlines->get (i);
            xMin = pageHeight - underline->y1;
            xMax = pageHeight - underline->y0;
            yMin = underline->x0;
            yMax = underline->x1;
            underline->x0 = xMin;
            underline->x1 = xMax;
            underline->y0 = yMin;
            underline->y1 = yMax;
            underline->horiz = !underline->horiz;
        }
        for (i = 0; i < links->getLength (); ++i) {
            link = (TextLink*)links->get (i);
            xMin = pageHeight - link->yMax;
            xMax = pageHeight - link->yMin;
            yMin = link->xMin;
            yMax = link->xMax;
            link->xMin = xMin;
            link->xMax = xMax;
            link->yMin = yMin;
            link->yMax = yMax;
        }
        break;
    }
}

// Undo the coordinate transform performed by rotateChars().
void TextPage::unrotateChars (GList* charsA, int rot) {
    TextChar* ch;
    double xMin, yMin, xMax, yMax, t;
    int i;

    switch (rot) {
    case 0:
    default:
        // no transform
        break;
    case 1:
        t = pageWidth;
        pageWidth = pageHeight;
        pageHeight = t;
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            xMin = pageWidth - ch->ymax;
            xMax = pageWidth - ch->ymin;
            yMin = ch->xmin;
            yMax = ch->xmax;
            ch->xmin = xMin;
            ch->xmax = xMax;
            ch->ymin = yMin;
            ch->ymax = yMax;
            ch->rot = (ch->rot + 1) & 3;
        }
        break;
    case 2:
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            xMin = pageWidth - ch->xmax;
            xMax = pageWidth - ch->xmin;
            yMin = pageHeight - ch->ymax;
            yMax = pageHeight - ch->ymin;
            ch->xmin = xMin;
            ch->xmax = xMax;
            ch->ymin = yMin;
            ch->ymax = yMax;
            ch->rot = (ch->rot + 2) & 3;
        }
        break;
    case 3:
        t = pageWidth;
        pageWidth = pageHeight;
        pageHeight = t;
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            xMin = ch->ymin;
            xMax = ch->ymax;
            yMin = pageHeight - ch->xmax;
            yMax = pageHeight - ch->xmin;
            ch->xmin = xMin;
            ch->xmax = xMax;
            ch->ymin = yMin;
            ch->ymax = yMax;
            ch->rot = (ch->rot + 3) & 3;
        }
        break;
    }
}

// Undo the coordinate transform performed by rotateChars().
void TextPage::unrotateColumns (GList* columns, int rot) {
    TextColumn* col;
    TextParagraph* par;
    double xMin, yMin, xMax, yMax, t;
    int colIdx, parIdx, lineIdx, wordIdx, i;

    switch (rot) {
    case 0:
    default:
        // no transform
        break;
    case 1:
        t = pageWidth;
        pageWidth = pageHeight;
        pageHeight = t;
        for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
            col = (TextColumn*)columns->get (colIdx);
            xMin = pageWidth - col->yMax;
            xMax = pageWidth - col->yMin;
            yMin = col->xMin;
            yMax = col->xMax;
            col->xMin = xMin;
            col->xMax = xMax;
            col->yMin = yMin;
            col->yMax = yMax;
            for (parIdx = 0; parIdx < col->paragraphs->getLength (); ++parIdx) {
                par = (TextParagraph*)col->paragraphs->get (parIdx);
                xMin = pageWidth - par->yMax;
                xMax = pageWidth - par->yMin;
                yMin = par->xMin;
                yMax = par->xMax;
                par->xMin = xMin;
                par->xMax = xMax;
                par->yMin = yMin;
                par->yMax = yMax;
                for (lineIdx = 0; lineIdx < par->lines.size ();
                     ++lineIdx) {
                    auto& line = par->lines [lineIdx];
                    xMin = pageWidth - line->yMax;
                    xMax = pageWidth - line->yMin;
                    yMin = line->xMin;
                    yMax = line->xMax;
                    line->xMin = xMin;
                    line->xMax = xMax;
                    line->yMin = yMin;
                    line->yMax = yMax;
                    line->rot = (line->rot + 1) & 3;
                    for (wordIdx = 0; wordIdx < line->words.size ();
                         ++wordIdx) {
                        auto& word = line->words [wordIdx];
                        xMin = pageWidth - word->yMax;
                        xMax = pageWidth - word->yMin;
                        yMin = word->xMin;
                        yMax = word->xMax;
                        word->xMin = xMin;
                        word->xMax = xMax;
                        word->yMin = yMin;
                        word->yMax = yMax;
                        word->rot = (word->rot + 1) & 3;
                    }
                }
            }
        }
        break;
    case 2:
        for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
            col = (TextColumn*)columns->get (colIdx);
            xMin = pageWidth - col->xMax;
            xMax = pageWidth - col->xMin;
            yMin = pageHeight - col->yMax;
            yMax = pageHeight - col->yMin;
            col->xMin = xMin;
            col->xMax = xMax;
            col->yMin = yMin;
            col->yMax = yMax;
            for (parIdx = 0; parIdx < col->paragraphs->getLength (); ++parIdx) {
                par = (TextParagraph*)col->paragraphs->get (parIdx);
                xMin = pageWidth - par->xMax;
                xMax = pageWidth - par->xMin;
                yMin = pageHeight - par->yMax;
                yMax = pageHeight - par->yMin;
                par->xMin = xMin;
                par->xMax = xMax;
                par->yMin = yMin;
                par->yMax = yMax;
                for (lineIdx = 0; lineIdx < par->lines.size ();
                     ++lineIdx) {
                    auto& line = par->lines [lineIdx];
                    xMin = pageWidth - line->xMax;
                    xMax = pageWidth - line->xMin;
                    yMin = pageHeight - line->yMax;
                    yMax = pageHeight - line->yMin;
                    line->xMin = xMin;
                    line->xMax = xMax;
                    line->yMin = yMin;
                    line->yMax = yMax;
                    line->rot = (line->rot + 2) & 3;
                    for (i = 0; i <= line->len; ++i) {
                        line->edge[i] = pageWidth - line->edge[i];
                    }
                    for (wordIdx = 0; wordIdx < line->words.size ();
                         ++wordIdx) {
                        auto& word = line->words [wordIdx];
                        xMin = pageWidth - word->xMax;
                        xMax = pageWidth - word->xMin;
                        yMin = pageHeight - word->yMax;
                        yMax = pageHeight - word->yMin;
                        word->xMin = xMin;
                        word->xMax = xMax;
                        word->yMin = yMin;
                        word->yMax = yMax;
                        word->rot = (word->rot + 2) & 3;
                        for (i = 0; i <= word->size (); ++i) {
                            word->edge[i] = pageWidth - word->edge[i];
                        }
                    }
                }
            }
        }
        break;
    case 3:
        t = pageWidth;
        pageWidth = pageHeight;
        pageHeight = t;
        for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
            col = (TextColumn*)columns->get (colIdx);
            xMin = col->yMin;
            xMax = col->yMax;
            yMin = pageHeight - col->xMax;
            yMax = pageHeight - col->xMin;
            col->xMin = xMin;
            col->xMax = xMax;
            col->yMin = yMin;
            col->yMax = yMax;
            for (parIdx = 0; parIdx < col->paragraphs->getLength (); ++parIdx) {
                par = (TextParagraph*)col->paragraphs->get (parIdx);
                xMin = par->yMin;
                xMax = par->yMax;
                yMin = pageHeight - par->xMax;
                yMax = pageHeight - par->xMin;
                par->xMin = xMin;
                par->xMax = xMax;
                par->yMin = yMin;
                par->yMax = yMax;
                for (lineIdx = 0; lineIdx < par->lines.size ();
                     ++lineIdx) {
                    auto& line = par->lines [lineIdx];
                    xMin = line->yMin;
                    xMax = line->yMax;
                    yMin = pageHeight - line->xMax;
                    yMax = pageHeight - line->xMin;
                    line->xMin = xMin;
                    line->xMax = xMax;
                    line->yMin = yMin;
                    line->yMax = yMax;
                    line->rot = (line->rot + 3) & 3;
                    for (i = 0; i <= line->len; ++i) {
                        line->edge[i] = pageHeight - line->edge[i];
                    }
                    for (wordIdx = 0; wordIdx < line->words.size ();
                         ++wordIdx) {
                        auto& word = line->words [wordIdx];
                        xMin = word->yMin;
                        xMax = word->yMax;
                        yMin = pageHeight - word->xMax;
                        yMax = pageHeight - word->xMin;
                        word->xMin = xMin;
                        word->xMax = xMax;
                        word->yMin = yMin;
                        word->yMax = yMax;
                        word->rot = (word->rot + 3) & 3;
                        for (i = 0; i <= word->size (); ++i) {
                            word->edge[i] = pageHeight - word->edge[i];
                        }
                    }
                }
            }
        }
        break;
    }
}

void TextPage::unrotateWords (
    TextWords& words, int rot) {

    double xMin, yMin, xMax, yMax;
    int i, j;

    switch (rot) {
    case 0:
    default:
        // no transform
        break;
    case 1:
        for (i = 0; i < words.size (); ++i) {
            auto& word = words [i];
            xMin = pageWidth - word->yMax;
            xMax = pageWidth - word->yMin;
            yMin = word->xMin;
            yMax = word->xMax;
            word->xMin = xMin;
            word->xMax = xMax;
            word->yMin = yMin;
            word->yMax = yMax;
            word->rot = (word->rot + 1) & 3;
        }
        break;
    case 2:
        for (i = 0; i < words.size (); ++i) {
            auto& word = words [i];
            xMin = pageWidth - word->xMax;
            xMax = pageWidth - word->xMin;
            yMin = pageHeight - word->yMax;
            yMax = pageHeight - word->yMin;
            word->xMin = xMin;
            word->xMax = xMax;
            word->yMin = yMin;
            word->yMax = yMax;
            word->rot = (word->rot + 2) & 3;
            for (j = 0; j <= word->size (); ++j) {
                word->edge[j] = pageWidth - word->edge[j];
            }
        }
        break;
    case 3:
        for (i = 0; i < words.size (); ++i) {
            auto& word = words [i];
            xMin = word->yMin;
            xMax = word->yMax;
            yMin = pageHeight - word->xMax;
            yMax = pageHeight - word->xMin;
            word->xMin = xMin;
            word->xMax = xMax;
            word->yMin = yMin;
            word->yMax = yMax;
            word->rot = (word->rot + 3) & 3;
            for (j = 0; j <= word->size (); ++j) {
                word->edge[j] = pageHeight - word->edge[j];
            }
        }
        break;
    }
}

// Determine the primary text direction (LR vs RL).  Returns true for
// LR, false for RL.
bool TextPage::checkPrimaryLR (GList* charsA) {
    TextChar* ch;
    int i, lrCount;

    lrCount = 0;
    for (i = 0; i < charsA->getLength (); ++i) {
        ch = (TextChar*)charsA->get (i);
        if (unicodeTypeL (ch->c)) { ++lrCount; }
        else if (unicodeTypeR (ch->c)) {
            --lrCount;
        }
    }
    return lrCount >= 0;
}

// Remove duplicate characters.  The list of chars has been sorted --
// by x for rot=0,2; by y for rot=1,3.
void TextPage::removeDuplicates (GList* charsA, int rot) {
    TextChar *ch, *ch2;
    double xDelta, yDelta;
    int i, j;

    if (rot & 1) {
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            xDelta = dupMaxSecDelta * ch->fontSize;
            yDelta = dupMaxPriDelta * ch->fontSize;
            j = i + 1;
            while (j < charsA->getLength ()) {
                ch2 = (TextChar*)charsA->get (j);
                if (ch2->ymin - ch->ymin >= yDelta) { break; }
                if (ch2->c == ch->c && fabs (ch2->xmin - ch->xmin) < xDelta &&
                    fabs (ch2->xmax - ch->xmax) < xDelta &&
                    fabs (ch2->ymax - ch->ymax) < yDelta) {
                    charsA->del (j);
                }
                else {
                    ++j;
                }
            }
        }
    }
    else {
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            xDelta = dupMaxPriDelta * ch->fontSize;
            yDelta = dupMaxSecDelta * ch->fontSize;
            j = i + 1;
            while (j < charsA->getLength ()) {
                ch2 = (TextChar*)charsA->get (j);
                if (ch2->xmin - ch->xmin >= xDelta) { break; }
                if (ch2->c == ch->c && fabs (ch2->xmax - ch->xmax) < xDelta &&
                    fabs (ch2->ymin - ch->ymin) < yDelta &&
                    fabs (ch2->ymax - ch->ymax) < yDelta) {
                    charsA->del (j);
                }
                else {
                    ++j;
                }
            }
        }
    }
}

// Split the characters into trees of TextBlocks, one tree for each
// rotation.  Merge into a single tree (with the primary rotation).
TextBlock* TextPage::splitChars (GList* charsA) {
    TextBlock* tree[4];
    TextBlock* blk;
    GList *chars2, *clippedChars;
    TextChar* ch;
    int rot, i;

    // split: build a tree of TextBlocks for each rotation
    clippedChars = new GList ();
    for (rot = 0; rot < 4; ++rot) {
        chars2 = new GList ();
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            if (ch->rot == rot) { chars2->append (ch); }
        }
        tree[rot] = NULL;
        if (chars2->getLength () > 0) {
            chars2->sort ((rot & 1) ? &TextChar::cmpY : &TextChar::cmpX);
            removeDuplicates (chars2, rot);
            if (control.clipText) {
                i = 0;
                while (i < chars2->getLength ()) {
                    ch = (TextChar*)chars2->get (i);
                    if (ch->clipped) {
                        ch = (TextChar*)chars2->del (i);
                        clippedChars->append (ch);
                    }
                    else {
                        ++i;
                    }
                }
            }
            if (chars2->getLength () > 0) { tree[rot] = split (chars2, rot); }
        }
        delete chars2;
    }

    // if the page contains no (unclipped) text, just leave an empty
    // column list
    if (!tree[0]) {
        delete clippedChars;
        return NULL;
    }

    // if the main tree is not a multicolumn node, insert one so that
    // rotated text has somewhere to go
    if (tree[0]->tag != blkTagMulticolumn) {
        blk = new TextBlock (blkHorizSplit, 0);
        blk->addChild (tree[0]);
        blk->tag = blkTagMulticolumn;
        tree[0] = blk;
    }

    // merge non-primary-rotation text into the primary-rotation tree
    for (rot = 1; rot < 4; ++rot) {
        if (tree[rot]) {
            insertIntoTree (tree[rot], tree[0]);
            tree[rot] = NULL;
        }
    }

    if (clippedChars->getLength ()) {
        insertClippedChars (clippedChars, tree[0]);
    }
    delete clippedChars;

#if 0 //~debug
  dumpTree(tree[0]);
#endif

    return tree[0];
}

// Generate a tree of TextBlocks, marked as columns, lines, and words.
TextBlock* TextPage::split (GList* charsA, int rot) {
    TextBlock* blk;
    GList *chars2, *chars3;
    int *horizProfile, *vertProfile;
    double xMin, yMin, xMax, yMax;
    int xMinI, yMinI, xMaxI, yMaxI;
    int xMinI2, yMinI2, xMaxI2, yMaxI2;
    TextChar* ch;
    double minFontSize, avgFontSize, splitPrecision;
    double nLines, vertGapThreshold, ascentAdjust, descentAdjust, minChunk;
    int horizGapSize, vertGapSize;
    double horizGapSize2, vertGapSize2;
    int minHorizChunkWidth, minVertChunkWidth, nHorizGaps, nVertGaps;
    double largeCharSize;
    int nLargeChars;
    bool doHorizSplit, doVertSplit, smallSplit;
    int i, x, y, prev, start;

    //----- compute bbox, min font size, average font size, and
    //      split precision for this block

    xMin = yMin = xMax = yMax = 0; // make gcc happy
    minFontSize = avgFontSize = 0;
    for (i = 0; i < charsA->getLength (); ++i) {
        ch = (TextChar*)charsA->get (i);
        if (i == 0 || ch->xmin < xMin) { xMin = ch->xmin; }
        if (i == 0 || ch->ymin < yMin) { yMin = ch->ymin; }
        if (i == 0 || ch->xmax > xMax) { xMax = ch->xmax; }
        if (i == 0 || ch->ymax > yMax) { yMax = ch->ymax; }
        avgFontSize += ch->fontSize;
        if (i == 0 || ch->fontSize < minFontSize) {
            minFontSize = ch->fontSize;
        }
    }
    avgFontSize /= charsA->getLength ();
    splitPrecision = splitPrecisionMul * minFontSize;
    if (splitPrecision < minSplitPrecision) {
        splitPrecision = minSplitPrecision;
    }

    //----- compute the horizontal and vertical profiles

    // add some slack to the array bounds to avoid floating point
    // precision problems
    xMinI = (int)floor (xMin / splitPrecision) - 1;
    yMinI = (int)floor (yMin / splitPrecision) - 1;
    xMaxI = (int)floor (xMax / splitPrecision) + 1;
    yMaxI = (int)floor (yMax / splitPrecision) + 1;
    horizProfile = (int*)calloc (yMaxI - yMinI + 1, sizeof (int));
    vertProfile = (int*)calloc (xMaxI - xMinI + 1, sizeof (int));
    memset (horizProfile, 0, (yMaxI - yMinI + 1) * sizeof (int));
    memset (vertProfile, 0, (xMaxI - xMinI + 1) * sizeof (int));
    for (i = 0; i < charsA->getLength (); ++i) {
        ch = (TextChar*)charsA->get (i);
        // yMinI2 and yMaxI2 are adjusted to allow for slightly overlapping lines
        switch (rot) {
        case 0:
        default:
            xMinI2 = (int)floor (ch->xmin / splitPrecision);
            xMaxI2 = (int)floor (ch->xmax / splitPrecision);
            ascentAdjust = ascentAdjustFactor * (ch->ymax - ch->ymin);
            yMinI2 = (int)floor ((ch->ymin + ascentAdjust) / splitPrecision);
            descentAdjust = descentAdjustFactor * (ch->ymax - ch->ymin);
            yMaxI2 = (int)floor ((ch->ymax - descentAdjust) / splitPrecision);
            break;
        case 1:
            descentAdjust = descentAdjustFactor * (ch->xmax - ch->xmin);
            xMinI2 = (int)floor ((ch->xmin + descentAdjust) / splitPrecision);
            ascentAdjust = ascentAdjustFactor * (ch->xmax - ch->xmin);
            xMaxI2 = (int)floor ((ch->xmax - ascentAdjust) / splitPrecision);
            yMinI2 = (int)floor (ch->ymin / splitPrecision);
            yMaxI2 = (int)floor (ch->ymax / splitPrecision);
            break;
        case 2:
            xMinI2 = (int)floor (ch->xmin / splitPrecision);
            xMaxI2 = (int)floor (ch->xmax / splitPrecision);
            descentAdjust = descentAdjustFactor * (ch->ymax - ch->ymin);
            yMinI2 = (int)floor ((ch->ymin + descentAdjust) / splitPrecision);
            ascentAdjust = ascentAdjustFactor * (ch->ymax - ch->ymin);
            yMaxI2 = (int)floor ((ch->ymax - ascentAdjust) / splitPrecision);
            break;
        case 3:
            ascentAdjust = ascentAdjustFactor * (ch->xmax - ch->xmin);
            xMinI2 = (int)floor ((ch->xmin + ascentAdjust) / splitPrecision);
            descentAdjust = descentAdjustFactor * (ch->xmax - ch->xmin);
            xMaxI2 = (int)floor ((ch->xmax - descentAdjust) / splitPrecision);
            yMinI2 = (int)floor (ch->ymin / splitPrecision);
            yMaxI2 = (int)floor (ch->ymax / splitPrecision);
            break;
        }
        for (y = yMinI2; y <= yMaxI2; ++y) { ++horizProfile[y - yMinI]; }
        for (x = xMinI2; x <= xMaxI2; ++x) { ++vertProfile[x - xMinI]; }
    }

    //----- find the largest gaps in the horizontal and vertical profiles

    horizGapSize = 0;
    for (start = yMinI; start < yMaxI && !horizProfile[start - yMinI]; ++start)
        ;
    for (y = start; y < yMaxI; ++y) {
        if (horizProfile[y - yMinI] && !horizProfile[y + 1 - yMinI]) {
            start = y;
        }
        else if (!horizProfile[y - yMinI] && horizProfile[y + 1 - yMinI]) {
            if (y - start > horizGapSize) { horizGapSize = y - start; }
        }
    }
    vertGapSize = 0;
    for (start = xMinI; start < xMaxI && !vertProfile[start - xMinI]; ++start)
        ;
    for (x = start; x < xMaxI; ++x) {
        if (vertProfile[x - xMinI] && !vertProfile[x + 1 - xMinI]) {
            start = x;
        }
        else if (!vertProfile[x - xMinI] && vertProfile[x + 1 - xMinI]) {
            if (x - start > vertGapSize) { vertGapSize = x - start; }
        }
    }
    horizGapSize2 = horizGapSize - splitGapSlack * avgFontSize / splitPrecision;
    if (horizGapSize2 < 0.99) { horizGapSize2 = 0.99; }
    vertGapSize2 = vertGapSize - splitGapSlack * avgFontSize / splitPrecision;
    if (vertGapSize2 < 0.99) { vertGapSize2 = 0.99; }

    //----- count horiz/vert gaps equivalent to largest gaps

    minHorizChunkWidth = yMaxI - yMinI;
    nHorizGaps = 0;
    for (start = yMinI; start < yMaxI && !horizProfile[start - yMinI]; ++start)
        ;
    prev = start - 1;
    for (y = start; y < yMaxI; ++y) {
        if (horizProfile[y - yMinI] && !horizProfile[y + 1 - yMinI]) {
            start = y;
        }
        else if (!horizProfile[y - yMinI] && horizProfile[y + 1 - yMinI]) {
            if (y - start > horizGapSize2) {
                ++nHorizGaps;
                if (start - prev < minHorizChunkWidth) {
                    minHorizChunkWidth = start - prev;
                }
                prev = y;
            }
        }
    }
    minVertChunkWidth = xMaxI - xMinI;
    nVertGaps = 0;
    for (start = xMinI; start < xMaxI && !vertProfile[start - xMinI]; ++start)
        ;
    prev = start - 1;
    for (x = start; x < xMaxI; ++x) {
        if (vertProfile[x - xMinI] && !vertProfile[x + 1 - xMinI]) {
            start = x;
        }
        else if (!vertProfile[x - xMinI] && vertProfile[x + 1 - xMinI]) {
            if (x - start > vertGapSize2) {
                ++nVertGaps;
                if (start - prev < minVertChunkWidth) {
                    minVertChunkWidth = start - prev;
                }
                prev = x;
            }
        }
    }

    //----- compute splitting parameters

    // approximation of number of lines in block
    if (fabs (avgFontSize) < 0.001) { nLines = 1; }
    else if (rot & 1) {
        nLines = (xMax - xMin) / avgFontSize;
    }
    else {
        nLines = (yMax - yMin) / avgFontSize;
    }

    // compute the minimum allowed vertical gap size
    // (this is a horizontal gap threshold for rot=1,3
    if (control.mode == textOutTableLayout) {
        vertGapThreshold =
            vertGapThresholdTableMax + vertGapThresholdTableSlope * nLines;
        if (vertGapThreshold < vertGapThresholdTableMin) {
            vertGapThreshold = vertGapThresholdTableMin;
        }
    }
    else {
        vertGapThreshold = vertGapThresholdMax + vertGapThresholdSlope * nLines;
        if (vertGapThreshold < vertGapThresholdMin) {
            vertGapThreshold = vertGapThresholdMin;
        }
    }
    vertGapThreshold = vertGapThreshold * avgFontSize / splitPrecision;

    // compute the minimum allowed chunk width
    if (control.mode == textOutTableLayout) { minChunk = 0; }
    else {
        minChunk = vertSplitChunkThreshold * avgFontSize / splitPrecision;
    }

    // look for large chars
    // -- this kludge (multiply by 256, convert to int, divide by 256.0)
    //    prevents floating point stability issues on x86 with gcc, where
    //    largeCharSize could otherwise have slightly different values
    //    here and where it's used below to do the large char partition
    //    (because it gets truncated from 80 to 64 bits when spilled)
    largeCharSize = (int)(largeCharThreshold * avgFontSize * 256) / 256.0;
    nLargeChars = 0;
    for (i = 0; i < charsA->getLength (); ++i) {
        ch = (TextChar*)charsA->get (i);
        if (ch->fontSize > largeCharSize) { ++nLargeChars; }
    }

    // figure out which type of split to do
    doHorizSplit = doVertSplit = false;
    smallSplit = false;
    if (rot & 1) {
        if (nHorizGaps > 0 &&
            (horizGapSize > vertGapSize ||
             control.mode == textOutTableLayout) &&
            horizGapSize > vertGapThreshold && minHorizChunkWidth > minChunk) {
            doHorizSplit = true;
        }
        else if (nVertGaps > 0) {
            doVertSplit = true;
        }
        else if (nLargeChars == 0 && nHorizGaps > 0) {
            doHorizSplit = true;
            smallSplit = true;
        }
    }
    else {
        if (nVertGaps > 0 &&
            (vertGapSize > horizGapSize ||
             control.mode == textOutTableLayout) &&
            vertGapSize > vertGapThreshold && minVertChunkWidth > minChunk) {
            doVertSplit = true;
        }
        else if (nHorizGaps > 0) {
            doHorizSplit = true;
        }
        else if (nLargeChars == 0 && nVertGaps > 0) {
            doVertSplit = true;
            smallSplit = true;
        }
    }

    //----- split the block

    //~ this could use "other content" (vector graphics, rotated text) --
    //~ presence of other content in a gap means we should definitely split

    // split vertically
    if (doVertSplit) {
        blk = new TextBlock (blkVertSplit, rot);
        blk->smallSplit = smallSplit;
        for (start = xMinI; start < xMaxI && !vertProfile[start - xMinI];
             ++start)
            ;
        prev = start - 1;
        for (x = start; x < xMaxI; ++x) {
            if (vertProfile[x - xMinI] && !vertProfile[x + 1 - xMinI]) {
                start = x;
            }
            else if (!vertProfile[x - xMinI] && vertProfile[x + 1 - xMinI]) {
                if (x - start > vertGapSize2) {
                    chars2 = getChars (
                        charsA, (prev + 0.5) * splitPrecision, yMin - 1,
                        (start + 1.5) * splitPrecision, yMax + 1);
                    blk->addChild (split (chars2, rot));
                    delete chars2;
                    prev = x;
                }
            }
        }
        chars2 = getChars (
            charsA, (prev + 0.5) * splitPrecision, yMin - 1, xMax + 1,
            yMax + 1);
        blk->addChild (split (chars2, rot));
        delete chars2;

        // split horizontally
    }
    else if (doHorizSplit) {
        blk = new TextBlock (blkHorizSplit, rot);
        blk->smallSplit = smallSplit;
        for (start = yMinI; start < yMaxI && !horizProfile[start - yMinI];
             ++start)
            ;
        prev = start - 1;
        for (y = start; y < yMaxI; ++y) {
            if (horizProfile[y - yMinI] && !horizProfile[y + 1 - yMinI]) {
                start = y;
            }
            else if (!horizProfile[y - yMinI] && horizProfile[y + 1 - yMinI]) {
                if (y - start > horizGapSize2) {
                    chars2 = getChars (
                        charsA, xMin - 1, (prev + 0.5) * splitPrecision,
                        xMax + 1, (start + 1.5) * splitPrecision);
                    blk->addChild (split (chars2, rot));
                    delete chars2;
                    prev = y;
                }
            }
        }
        chars2 = getChars (
            charsA, xMin - 1, (prev + 0.5) * splitPrecision, xMax + 1,
            yMax + 1);
        blk->addChild (split (chars2, rot));
        delete chars2;

        // split into larger and smaller chars
    }
    else if (nLargeChars > 0) {
        chars2 = new GList ();
        chars3 = new GList ();
        for (i = 0; i < charsA->getLength (); ++i) {
            ch = (TextChar*)charsA->get (i);
            if (ch->fontSize > largeCharSize) { chars2->append (ch); }
            else {
                chars3->append (ch);
            }
        }
        blk = split (chars3, rot);
        insertLargeChars (chars2, blk);
        delete chars2;
        delete chars3;

        // create a leaf node
    }
    else {
        blk = new TextBlock (blkLeaf, rot);
        for (i = 0; i < charsA->getLength (); ++i) {
            blk->addChild ((TextChar*)charsA->get (i));
        }
    }

    free (horizProfile);
    free (vertProfile);

    tagBlock (blk);

    return blk;
}

// Return the subset of chars inside a rectangle.
GList* TextPage::getChars (
    GList* charsA, double xMin, double yMin, double xMax, double yMax) {
    GList* ret;
    TextChar* ch;
    double x, y;
    int i;

    ret = new GList ();
    for (i = 0; i < charsA->getLength (); ++i) {
        ch = (TextChar*)charsA->get (i);
        // because of {ascent,descent}AdjustFactor, the y coords (or x
        // coords for rot 1,3) for the gaps will be a little bit tight --
        // so we use the center of the character here
        x = 0.5 * (ch->xmin + ch->xmax);
        y = 0.5 * (ch->ymin + ch->ymax);
        if (x > xMin && x < xMax && y > yMin && y < yMax) { ret->append (ch); }
    }
    return ret;
}

// Decide whether this block is a line, column, or multiple columns:
// - all leaf nodes are lines
// - horiz split nodes whose children are lines or columns are columns
// - other horiz split nodes are multiple columns
// - vert split nodes, with small gaps, whose children are lines are lines
// - other vert split nodes are multiple columns
// (for rot=1,3: the horiz and vert splits are swapped)
// In table layout mode:
// - all leaf nodes are lines
// - vert split nodes, with small gaps, whose children are lines are lines
// - everything else is multiple columns
void TextPage::tagBlock (TextBlock* blk) {
    TextBlock* child;
    int i;

    if (control.mode == textOutTableLayout) {
        if (blk->type == blkLeaf) { blk->tag = blkTagLine; }
        else if (
            blk->type == ((blk->rot & 1) ? blkHorizSplit : blkVertSplit) &&
            blk->smallSplit) {
            blk->tag = blkTagLine;
            for (i = 0; i < blk->children->getLength (); ++i) {
                child = (TextBlock*)blk->children->get (i);
                if (child->tag != blkTagLine) {
                    blk->tag = blkTagMulticolumn;
                    break;
                }
            }
        }
        else {
            blk->tag = blkTagMulticolumn;
        }
        return;
    }

    if (blk->type == blkLeaf) { blk->tag = blkTagLine; }
    else {
        if (blk->type == ((blk->rot & 1) ? blkVertSplit : blkHorizSplit)) {
            blk->tag = blkTagColumn;
            for (i = 0; i < blk->children->getLength (); ++i) {
                child = (TextBlock*)blk->children->get (i);
                if (child->tag != blkTagColumn && child->tag != blkTagLine) {
                    blk->tag = blkTagMulticolumn;
                    break;
                }
            }
        }
        else {
            if (blk->smallSplit) {
                blk->tag = blkTagLine;
                for (i = 0; i < blk->children->getLength (); ++i) {
                    child = (TextBlock*)blk->children->get (i);
                    if (child->tag != blkTagLine) {
                        blk->tag = blkTagMulticolumn;
                        break;
                    }
                }
            }
            else {
                blk->tag = blkTagMulticolumn;
            }
        }
    }
}

// Insert a list of large characters into a tree.
void TextPage::insertLargeChars (GList* largeChars, TextBlock* blk) {
    TextChar *ch, *ch2;
    bool singleLine;
    double xLimit, yLimit, minOverlap;
    int i;

    //~ this currently works only for characters in the primary rotation

    // check to see if the large chars are a single line, in the
    // upper-left corner of blk (this is just a rough estimate)
    xLimit = blk->xMin + 0.5 * (blk->xMin + blk->xMax);
    yLimit = blk->yMin + 0.5 * (blk->yMin + blk->yMax);
    singleLine = true;
    // note: largeChars are already sorted by x
    for (i = 0; i < largeChars->getLength (); ++i) {
        ch2 = (TextChar*)largeChars->get (i);
        if (ch2->xmax > xLimit || ch2->ymax > yLimit) {
            singleLine = false;
            break;
        }
        if (i > 0) {
            ch = (TextChar*)largeChars->get (i - 1);
            minOverlap = 0.5 * (ch->fontSize < ch2->fontSize ? ch->fontSize
                                                             : ch2->fontSize);
            if (ch->ymax - ch2->ymin < minOverlap ||
                ch2->ymax - ch->ymin < minOverlap) {
                singleLine = false;
                break;
            }
        }
    }

    if (singleLine) {
        // if the large chars are a single line, prepend them to the first
        // leaf node in blk
        insertLargeCharsInFirstLeaf (largeChars, blk);
    }
    else {
        // if the large chars are not a single line, prepend each one to
        // the appropriate leaf node -- this handles cases like bullets
        // drawn in a large font, on the left edge of a column
        for (i = largeChars->getLength () - 1; i >= 0; --i) {
            ch = (TextChar*)largeChars->get (i);
            insertLargeCharInLeaf (ch, blk);
        }
    }
}

// Find the first leaf (in depth-first order) in blk, and prepend a
// list of large chars.
void TextPage::insertLargeCharsInFirstLeaf (GList* largeChars, TextBlock* blk) {
    TextChar* ch;
    int i;

    if (blk->type == blkLeaf) {
        for (i = largeChars->getLength () - 1; i >= 0; --i) {
            ch = (TextChar*)largeChars->get (i);
            blk->prependChild (ch);
        }
    }
    else {
        insertLargeCharsInFirstLeaf (
            largeChars, (TextBlock*)blk->children->get (0));
        blk->updateBounds (0);
    }
}

// Find the leaf in <blk> where large char <ch> belongs, and prepend
// it.
void TextPage::insertLargeCharInLeaf (TextChar* ch, TextBlock* blk) {
    TextBlock* child;
    double y;
    int i;

    //~ this currently works only for characters in the primary rotation

    //~ this currently just looks down the left edge of blk
    //~   -- it could be extended to do more

    // estimate the baseline of ch
    y = ch->ymin + 0.75 * (ch->ymax - ch->ymin);

    if (blk->type == blkLeaf) { blk->prependChild (ch); }
    else if (blk->type == blkHorizSplit) {
        for (i = 0; i < blk->children->getLength (); ++i) {
            child = (TextBlock*)blk->children->get (i);
            if (y < child->yMax || i == blk->children->getLength () - 1) {
                insertLargeCharInLeaf (ch, child);
                blk->updateBounds (i);
                break;
            }
        }
    }
    else {
        insertLargeCharInLeaf (ch, (TextBlock*)blk->children->get (0));
        blk->updateBounds (0);
    }
}

// Merge blk (rot != 0) into primaryTree (rot == 0).
void TextPage::insertIntoTree (TextBlock* blk, TextBlock* primaryTree) {
    TextBlock* child;

    // we insert a whole column at a time - so call insertIntoTree
    // recursively until we get to a column (or line)

    if (blk->tag == blkTagMulticolumn) {
        while (blk->children->getLength ()) {
            child = (TextBlock*)blk->children->del (0);
            insertIntoTree (child, primaryTree);
        }
        delete blk;
    }
    else {
        insertColumnIntoTree (blk, primaryTree);
    }
}

// Insert a column (as an atomic subtree) into tree.
// Requirement: tree is not a leaf node.
void TextPage::insertColumnIntoTree (TextBlock* column, TextBlock* tree) {
    TextBlock* child;
    int i;

    for (i = 0; i < tree->children->getLength (); ++i) {
        child = (TextBlock*)tree->children->get (i);
        if (child->tag == blkTagMulticolumn && column->xMin >= child->xMin &&
            column->yMin >= child->yMin && column->xMax <= child->xMax &&
            column->yMax <= child->yMax) {
            insertColumnIntoTree (column, child);
            tree->tag = blkTagMulticolumn;
            return;
        }
    }

    if (tree->type == blkVertSplit) {
        if (tree->rot == 1 || tree->rot == 2) {
            for (i = 0; i < tree->children->getLength (); ++i) {
                child = (TextBlock*)tree->children->get (i);
                if (column->xMax > 0.5 * (child->xMin + child->xMax)) { break; }
            }
        }
        else {
            for (i = 0; i < tree->children->getLength (); ++i) {
                child = (TextBlock*)tree->children->get (i);
                if (column->xMin < 0.5 * (child->xMin + child->xMax)) { break; }
            }
        }
    }
    else if (tree->type == blkHorizSplit) {
        if (tree->rot >= 2) {
            for (i = 0; i < tree->children->getLength (); ++i) {
                child = (TextBlock*)tree->children->get (i);
                if (column->yMax > 0.5 * (child->yMin + child->yMax)) { break; }
            }
        }
        else {
            for (i = 0; i < tree->children->getLength (); ++i) {
                child = (TextBlock*)tree->children->get (i);
                if (column->yMin < 0.5 * (child->yMin + child->yMax)) { break; }
            }
        }
    }
    else {
        // this should never happen
        return;
    }
    tree->children->insert (i, column);
    tree->tag = blkTagMulticolumn;
}

// Insert clipped characters back into the TextBlock tree.
void TextPage::insertClippedChars (GList* clippedChars, TextBlock* tree) {
    TextChar *ch, *ch2;
    TextBlock* leaf;
    double y;
    int i;

    //~ this currently works only for characters in the primary rotation

    clippedChars->sort (TextChar::cmpX);
    while (clippedChars->getLength ()) {
        ch = (TextChar*)clippedChars->del (0);
        if (ch->rot != 0) { continue; }
        if (!(leaf = findClippedCharLeaf (ch, tree))) { continue; }
        leaf->addChild (ch);
        i = 0;
        while (i < clippedChars->getLength ()) {
            ch2 = (TextChar*)clippedChars->get (i);
            if (ch2->xmin > ch->xmax + clippedTextMaxWordSpace * ch->fontSize) {
                break;
            }
            y = 0.5 * (ch2->ymin + ch2->ymax);
            if (y > leaf->yMin && y < leaf->yMax) {
                ch2 = (TextChar*)clippedChars->del (i);
                leaf->addChild (ch2);
                ch = ch2;
            }
            else {
                ++i;
            }
        }
    }
}

// Find the leaf in <tree> to which clipped char <ch> can be appended.
// Returns NULL if there is no appropriate append point.
TextBlock* TextPage::findClippedCharLeaf (TextChar* ch, TextBlock* tree) {
    TextBlock *ret, *child;
    double y;
    int i;

    //~ this currently works only for characters in the primary rotation

    y = 0.5 * (ch->ymin + ch->ymax);
    if (tree->type == blkLeaf) {
        if (tree->rot == 0) {
            if (y > tree->yMin && y < tree->yMax &&
                ch->xmin <=
                    tree->xMax + clippedTextMaxWordSpace * ch->fontSize) {
                return tree;
            }
        }
    }
    else {
        for (i = 0; i < tree->children->getLength (); ++i) {
            child = (TextBlock*)tree->children->get (i);
            if ((ret = findClippedCharLeaf (ch, child))) { return ret; }
        }
    }
    return NULL;
}

// Convert the tree of TextBlocks into a list of TextColumns.
GList* TextPage::buildColumns (TextBlock* tree) {
    GList* columns;

    columns = new GList ();
    buildColumns2 (tree, columns);
    return columns;
}

void TextPage::buildColumns2 (TextBlock* blk, GList* columns) {
    TextColumn* col;
    int i;

    switch (blk->tag) {
    case blkTagLine:
    case blkTagColumn:
        col = buildColumn (blk);
        columns->append (col);
        break;
    case blkTagMulticolumn:
        for (i = 0; i < blk->children->getLength (); ++i) {
            buildColumns2 ((TextBlock*)blk->children->get (i), columns);
        }
        break;
    }
}

TextColumn* TextPage::buildColumn (TextBlock* blk) {
    GList* paragraphs;
    double spaceThresh, indent0, indent1, fontSize0, fontSize1;
    int i;

    TextLines lines;
    buildLines (blk, lines);

    spaceThresh = paragraphSpacingThreshold * getAverageLineSpacing (lines);

    //~ could look for bulleted lists here: look for the case where
    //~   all out-dented lines start with the same char

    // build the paragraphs
    paragraphs = new GList ();
    i = 0;
    while (i < lines.size ()) {
        // get the first line of the paragraph
        TextLines parLines;

        auto& line0 = lines [i];
        parLines.push_back (line0);

        ++i;

        if (i < lines.size ()) {
            auto& line1 = lines [i];

            indent0 = getLineIndent (*line0, blk);
            indent1 = getLineIndent (*line1, blk);

            fontSize0 = line0->fontSize;
            fontSize1 = line1->fontSize;

            // inverted indent
            if (   indent1 - indent0 > minParagraphIndent * fontSize0
                && fabs (fontSize0 - fontSize1) <= paragraphFontSizeDelta
                && getLineSpacing (*line0, *line1) <= spaceThresh) {

                parLines.push_back (line1);
                indent0 = indent1;

                for (++i; i < lines.size (); ++i) {
                    auto& line1 = lines [i];

                    indent1 = getLineIndent (*line1, blk);
                    fontSize1 = line1->fontSize;

                    if (indent0 - indent1 > minParagraphIndent * fontSize0) {
                        break;
                    }

                    if (fabs (fontSize0 - fontSize1) > paragraphFontSizeDelta) {
                        break;
                    }

                    if (getLineSpacing (*lines [i - 1], *line1) > spaceThresh) {
                        break;
                    }

                    parLines.push_back (line1);
                }

                // drop cap
            }
            else if (
                fontSize0 > largeCharThreshold * fontSize1 &&
                indent1 - indent0 > minParagraphIndent * fontSize1 &&
                getLineSpacing (*line0, *line1) < 0) {
                parLines.push_back (line1);
                fontSize0 = fontSize1;
                for (++i; i < lines.size (); ++i) {
                    auto& line1 = lines [i];
                    indent1 = getLineIndent (*line1, blk);
                    if (indent1 - indent0 <= minParagraphIndent * fontSize0) {
                        break;
                    }
                    if (getLineSpacing (*lines [i - 1], *line1) > spaceThresh) {
                        break;
                    }
                    parLines.push_back (line1);
                }
                for (; i < lines.size (); ++i) {
                    auto& line1 = lines [i];

                    indent1 = getLineIndent (*line1, blk);
                    fontSize1 = line1->fontSize;

                    if (indent1 - indent0 > minParagraphIndent * fontSize0) {
                        break;
                    }

                    if (fabs (fontSize0 - fontSize1) > paragraphFontSizeDelta) {
                        break;
                    }

                    if (getLineSpacing (*lines [i - 1], *line1) > spaceThresh) {
                        break;
                    }

                    parLines.push_back (line1);
                }

                // regular indent or no indent
            }
            else if (   fabs (fontSize0 - fontSize1) <= paragraphFontSizeDelta
                     && getLineSpacing (*line0, *line1) <= spaceThresh) {

                parLines.push_back (line1);
                indent0 = indent1;

                for (++i; i < lines.size (); ++i) {
                    auto& line1 = lines [i];

                    indent1 = getLineIndent (*line1, blk);
                    fontSize1 = line1->fontSize;

                    if (indent1 - indent0 > minParagraphIndent * fontSize0) {
                        break;
                    }

                    if (fabs (fontSize0 - fontSize1) > paragraphFontSizeDelta) {
                        break;
                    }

                    if (getLineSpacing (*lines [i - 1], *line1) > spaceThresh) {
                        break;
                    }

                    parLines.push_back (line1);
                }
            }
        }

        paragraphs->append (new TextParagraph (parLines));
    }

    return new TextColumn (
        paragraphs, blk->xMin, blk->yMin, blk->xMax, blk->yMax);
}

double
TextPage::getLineIndent (const TextLine& line, TextBlock* blk) const {
    double indent;

    switch (line.rot) {
    case 0:
    default: indent = line.xMin - blk->xMin; break;
    case 1:  indent = line.yMin - blk->yMin; break;
    case 2:  indent = blk->xMax - line.xMax; break;
    case 3:  indent = blk->yMax - line.yMax; break;
    }

    return indent;
}

// Compute average line spacing in column.
double
TextPage::getAverageLineSpacing (
    const TextLines& lines) const {

    double avg = 0, sp;
    size_t n = 0;

    for (size_t i = 1; i < lines.size (); ++i) {
        sp = getLineSpacing (*lines [i - 1], *lines [i]);

        if (sp > 0) {
            avg += sp;
            ++n;
        }
    }

    if (n > 0) {
        avg /= n;
    }

    return avg;
}

// Compute the space between two lines.
double TextPage::getLineSpacing (const TextLine& lhs, const TextLine& rhs) const {
    double sp;

    switch (lhs.rot) {
    case 0:
    default: sp = rhs.yMin - lhs.yMax; break;
    case 1:  sp = lhs.xMin - rhs.xMax; break;
    case 2:  sp = lhs.yMin - rhs.yMin; break;
    case 3:  sp = rhs.xMin - rhs.xMax; break;
    }

    return sp;
}

void
TextPage::buildLines (
    TextBlock* blk, TextLines& lines) {

    switch (blk->tag) {
    case blkTagLine: {
        auto line = buildLine (blk);

        if (blk->rot == 1 || blk->rot == 2) {
            lines.insert (lines.begin (), line);
        }
        else {
            lines.push_back (line);
        }
    }
        break;

    case blkTagColumn:
    case blkTagMulticolumn: {
        // multicolumn should never happen here
        for (size_t i = 0; i < blk->children->getLength (); ++i) {
            buildLines ((TextBlock*)blk->children->get (i), lines);
        }
    }
        break;
    }
}

TextLinePtr
TextPage::buildLine (TextBlock* blk) {
    GList* charsA;
    TextWords words;

    TextChar *ch, *ch2;

    double wordSp, lineFontSize, sp;
    bool spaceAfter, spaceAfter2;

    charsA = new GList ();
    getLineChars (blk, charsA);

    wordSp = computeWordSpacingThreshold (charsA, blk->rot);

    lineFontSize = 0;
    spaceAfter = false;

    for (size_t i = 0, j; i < charsA->getLength ();) {
        sp = wordSp - 1;

        for (j = i + 1; j < charsA->getLength (); ++j) {
            ch = (TextChar*)charsA->get (j - 1);
            ch2 = (TextChar*)charsA->get (j);

            sp = (blk->rot & 1) ? (ch2->ymin - ch->ymax)
                                : (ch2->xmin - ch->xmax);

            if (sp > wordSp || ch->font != ch2->font ||
                fabs (ch->fontSize - ch2->fontSize) > 0.01 ||
                (control.mode == textOutRawOrder &&
                 ch2->charPos != ch->charPos + ch->charLen)) {
                break;
            }

            sp = wordSp - 1;
        }

        spaceAfter2 = spaceAfter;
        spaceAfter = sp > wordSp;

        auto word = std::make_shared< TextWord > (
            charsA, i, j - i, blk->rot, (blk->rot >= 2) ? spaceAfter2 : spaceAfter);

        i = j;

        if (blk->rot >= 2) {
            words.insert (words.begin (), word);
        }
        else {
            words.push_back (word);
        }

        if (0 == i || word->fontSize > lineFontSize) {
            lineFontSize = word->fontSize;
        }
    }

    delete charsA;

    return std::make_shared< TextLine > (
        std::move (words),
        blk->xMin, blk->yMin,
        blk->xMax, blk->yMax,
        lineFontSize);
}

void TextPage::getLineChars (TextBlock* blk, GList* charsA) {
    if (blk->type == blkLeaf) {
        charsA->append (blk->children);
    }
    else {
        for (size_t i = 0; i < blk->children->getLength (); ++i) {
            getLineChars ((TextBlock*)blk->children->get (i), charsA);
        }
    }
}

// Compute the inter-word spacing threshold for a line of chars.
// Spaces greater than this threshold will be considered inter-word
// spaces.
double TextPage::computeWordSpacingThreshold (GList* charsA, int rot) {
    TextChar *ch, *ch2;
    double avgFontSize, minSp, maxSp, sp;
    int i;

    avgFontSize = 0;
    minSp = maxSp = 0;
    for (i = 0; i < charsA->getLength (); ++i) {
        ch = (TextChar*)charsA->get (i);
        avgFontSize += ch->fontSize;
        if (i < charsA->getLength () - 1) {
            ch2 = (TextChar*)charsA->get (i + 1);
            sp = (rot & 1) ? (ch2->ymin - ch->ymax) : (ch2->xmin - ch->xmax);
            if (i == 0 || sp < minSp) { minSp = sp; }
            if (sp > maxSp) { maxSp = sp; }
        }
    }
    avgFontSize /= charsA->getLength ();
    if (minSp < 0) { minSp = 0; }

    // if spacing is completely uniform, assume it's a single word
    // (technically it could be either "ABC" or "A B C", but it's
    // essentially impossible to tell)
    if (maxSp - minSp < uniformSpacing * avgFontSize) {
        return maxSp + 1;

        // if there is some variation in spacing, but it's small, assume
        // there are some inter-word spaces
    }
    else if (maxSp - minSp < wordSpacing * avgFontSize) {
        return 0.5 * (minSp + maxSp);

        // otherwise, assume a reasonable threshold for inter-word spacing
        // (we can't use something like 0.5*(minSp+maxSp) here because there
        // can be outliers at the high end)
    }
    else {
        return minSp + wordSpacing * avgFontSize;
    }
}

int TextPage::assignPhysLayoutPositions (GList* columns) {
    assignLinePhysPositions (columns);
    return assignColumnPhysPositions (columns);
}

// Assign a physical x coordinate for each TextLine (relative to the
// containing TextColumn).  This also computes TextColumn width and
// height.
void TextPage::assignLinePhysPositions (GList* columns) {
    TextColumn* col;
    TextParagraph* par;
    UnicodeMap* uMap;
    int colIdx, parIdx, lineIdx;

    if (!(uMap = globalParams->getTextEncoding ())) { return; }

    for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
        col = (TextColumn*)columns->get (colIdx);
        col->pw = col->ph = 0;
        for (parIdx = 0; parIdx < col->paragraphs->getLength (); ++parIdx) {
            par = (TextParagraph*)col->paragraphs->get (parIdx);
            for (lineIdx = 0; lineIdx < par->lines.size (); ++lineIdx) {
                auto& line = par->lines [lineIdx];
                computeLinePhysWidth (*line, uMap);
                if (control.fixedPitch > 0) {
                    line->px =
                        (int)((line->xMin - col->xMin) / control.fixedPitch);
                }
                else if (fabs (line->fontSize) < 0.001) {
                    line->px = 0;
                }
                else {
                    line->px =
                        (int)((line->xMin - col->xMin) / (physLayoutSpaceWidth * line->fontSize));
                }
                if (line->px + line->pw > col->pw) {
                    col->pw = line->px + line->pw;
                }
            }
            col->ph += par->lines.size ();
        }
        col->ph += col->paragraphs->getLength () - 1;
    }

    uMap->decRefCnt ();
}

void TextPage::computeLinePhysWidth (TextLine& line, UnicodeMap* uMap) {
    char buf[8];
    int n, i;

    if (uMap->isUnicode ()) { line.pw = line.len; }
    else {
        line.pw = 0;
        for (i = 0; i < line.len; ++i) {
            n = uMap->mapUnicode (line.text[i], buf, sizeof (buf));
            line.pw += n;
        }
    }
}

// Assign physical x and y coordinates for each TextColumn.  Returns
// the text height (max physical y + 1).
int TextPage::assignColumnPhysPositions (GList* columns) {
    TextColumn *col, *col2;
    double slack, xOverlap, yOverlap;
    int ph, i, j;

    if (control.mode == textOutTableLayout) { slack = tableCellOverlapSlack; }
    else {
        slack = 0;
    }

    // assign x positions
    columns->sort (&TextColumn::cmpX);
    for (i = 0; i < columns->getLength (); ++i) {
        col = (TextColumn*)columns->get (i);
        if (control.fixedPitch) {
            col->px = (int)(col->xMin / control.fixedPitch);
        }
        else {
            col->px = 0;
            for (j = 0; j < i; ++j) {
                col2 = (TextColumn*)columns->get (j);
                xOverlap = col2->xMax - col->xMin;
                if (xOverlap < slack * (col2->xMax - col2->xMin)) {
                    if (col2->px + col2->pw + 2 > col->px) {
                        col->px = col2->px + col2->pw + 2;
                    }
                }
                else {
                    yOverlap =
                        (col->yMax < col2->yMax ? col->yMax : col2->yMax) -
                        (col->yMin > col2->yMin ? col->yMin : col2->yMin);
                    if (yOverlap > 0 && xOverlap < yOverlap) {
                        if (col2->px + col2->pw > col->px) {
                            col->px = col2->px + col2->pw;
                        }
                    }
                    else {
                        if (col2->px > col->px) { col->px = col2->px; }
                    }
                }
            }
        }
    }

    // assign y positions
    ph = 0;
    columns->sort (&TextColumn::cmpY);
    for (i = 0; i < columns->getLength (); ++i) {
        col = (TextColumn*)columns->get (i);
        col->py = 0;
        for (j = 0; j < i; ++j) {
            col2 = (TextColumn*)columns->get (j);
            yOverlap = col2->yMax - col->yMin;
            if (yOverlap < slack * (col2->yMax - col2->yMin)) {
                if (col2->py + col2->ph + 1 > col->py) {
                    col->py = col2->py + col2->ph + 1;
                }
            }
            else {
                xOverlap = (col->xMax < col2->xMax ? col->xMax : col2->xMax) -
                           (col->xMin > col2->xMin ? col->xMin : col2->xMin);
                if (xOverlap > 0 && yOverlap < xOverlap) {
                    if (col2->py + col2->ph > col->py) {
                        col->py = col2->py + col2->ph;
                    }
                }
                else {
                    if (col2->py > col->py) { col->py = col2->py; }
                }
            }
        }
        if (col->py + col->ph > ph) { ph = col->py + col->ph; }
    }

    return ph;
}

void TextPage::generateUnderlinesAndLinks (GList* columns) {
    TextColumn* col;
    TextParagraph* par;
    TextUnderline* underline;
    TextLink* link;
    double base, uSlack, ubSlack, hSlack;
    int colIdx, parIdx, lineIdx, wordIdx, i;

    for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
        col = (TextColumn*)columns->get (colIdx);
        for (parIdx = 0; parIdx < col->paragraphs->getLength (); ++parIdx) {
            par = (TextParagraph*)col->paragraphs->get (parIdx);
            for (lineIdx = 0; lineIdx < par->lines.size (); ++lineIdx) {
                auto& line = par->lines [lineIdx];
                for (wordIdx = 0; wordIdx < line->words.size ();
                     ++wordIdx) {
                    auto& word = line->words [wordIdx];
                    base = word->getBaseline ();
                    uSlack = underlineSlack * word->fontSize;
                    ubSlack = underlineBaselineSlack * word->fontSize;
                    hSlack = hyperlinkSlack * word->fontSize;

                    //----- handle underlining
                    for (i = 0; i < underlines->getLength (); ++i) {
                        underline = (TextUnderline*)underlines->get (i);
                        if (underline->horiz) {
                            if (word->rot == 0 || word->rot == 2) {
                                if (fabs (underline->y0 - base) < ubSlack &&
                                    underline->x0 < word->xMin + uSlack &&
                                    word->xMax - uSlack < underline->x1) {
                                    word->underlined = true;
                                }
                            }
                        }
                        else {
                            if (word->rot == 1 || word->rot == 3) {
                                if (fabs (underline->x0 - base) < ubSlack &&
                                    underline->y0 < word->yMin + uSlack &&
                                    word->yMax - uSlack < underline->y1) {
                                    word->underlined = true;
                                }
                            }
                        }
                    }

                    //----- handle links
                    for (i = 0; i < links->getLength (); ++i) {
                        link = (TextLink*)links->get (i);
                        if (link->xMin < word->xMin + hSlack &&
                            word->xMax - hSlack < link->xMax &&
                            link->yMin < word->yMin + hSlack &&
                            word->yMax - hSlack < link->yMax) {
                        }
                    }
                }
            }
        }
    }
}

//
// TextPage: access
//
bool TextPage::findText (
    Unicode* s, int len, bool startAtTop, bool stopAtBottom,
    bool startAtLast, bool stopAtLast, bool caseSensitive, bool backward,
    bool wholeWord, double* xMin, double* yMin, double* xMax, double* yMax) {
    TextBlock* tree;
    TextColumn* column;
    TextParagraph* par;
    Unicode *s2, *txt;
    Unicode* p;
    double xStart, yStart, xStop, yStop;
    double xMin0, yMin0, xMax0, yMax0;
    double xMin1, yMin1, xMax1, yMax1;
    bool found;
    int txtSize, m, rot, colIdx, parIdx, lineIdx, i, j, k;

    //~ need to handle right-to-left text

    if (!findCols) {
        rot = rotateChars (chars);
        if ((tree = splitChars (chars))) {
            findCols = buildColumns (tree);
            delete tree;
        }
        else {
            // no text
            findCols = new GList ();
        }
        unrotateChars (chars, rot);
        unrotateColumns (findCols, rot);
    }

    // convert the search string to uppercase
    if (!caseSensitive) {
        s2 = (Unicode*)calloc (len, sizeof (Unicode));
        for (i = 0; i < len; ++i) { s2[i] = unicodeToUpper (s[i]); }
    }
    else {
        s2 = s;
    }

    txt = NULL;
    txtSize = 0;

    xStart = yStart = xStop = yStop = 0;
    if (startAtLast && haveLastFind) {
        xStart = lastFindXMin;
        yStart = lastFindYMin;
    }
    else if (!startAtTop) {
        xStart = *xMin;
        yStart = *yMin;
    }
    if (stopAtLast && haveLastFind) {
        xStop = lastFindXMin;
        yStop = lastFindYMin;
    }
    else if (!stopAtBottom) {
        xStop = *xMax;
        yStop = *yMax;
    }

    found = false;
    xMin0 = xMax0 = yMin0 = yMax0 = 0; // make gcc happy
    xMin1 = xMax1 = yMin1 = yMax1 = 0; // make gcc happy

    for (colIdx = backward ? findCols->getLength () - 1 : 0;
         backward ? colIdx >= 0 : colIdx < findCols->getLength ();
         colIdx += backward ? -1 : 1) {
        column = (TextColumn*)findCols->get (colIdx);

        // check: is the column above the top limit?
        if (!startAtTop &&
            (backward ? column->yMin > yStart : column->yMax < yStart)) {
            continue;
        }

        // check: is the column below the bottom limit?
        if (!stopAtBottom &&
            (backward ? column->yMax < yStop : column->yMin > yStop)) {
            continue;
        }

        for (parIdx = backward ? column->paragraphs->getLength () - 1 : 0;
             backward ? parIdx >= 0 : parIdx < column->paragraphs->getLength ();
             parIdx += backward ? -1 : 1) {
            par = (TextParagraph*)column->paragraphs->get (parIdx);

            // check: is the paragraph above the top limit?
            if (!startAtTop &&
                (backward ? par->yMin > yStart : par->yMax < yStart)) {
                continue;
            }

            // check: is the paragraph below the bottom limit?
            if (!stopAtBottom &&
                (backward ? par->yMax < yStop : par->yMin > yStop)) {
                continue;
            }

            for (lineIdx = backward ? par->lines.size () - 1 : 0;
                 backward ? lineIdx >= 0 : lineIdx < par->lines.size ();
                 lineIdx += backward ? -1 : 1) {
                auto& line = par->lines [lineIdx];

                // check: is the line above the top limit?
                if (!startAtTop &&
                    (backward ? line->yMin > yStart : line->yMax < yStart)) {
                    continue;
                }

                // check: is the line below the bottom limit?
                if (!stopAtBottom &&
                    (backward ? line->yMax < yStop : line->yMin > yStop)) {
                    continue;
                }

                // convert the line to uppercase
                m = line->len;
                if (!caseSensitive) {
                    if (m > txtSize) {
                        txt = (Unicode*)reallocarray (txt, m, sizeof (Unicode));
                        txtSize = m;
                    }
                    for (k = 0; k < m; ++k) {
                        txt[k] = unicodeToUpper (line->text[k]);
                    }
                }
                else {
                    txt = line->text.data ();
                }

                // search each position in this line
                j = backward ? m - len : 0;
                p = txt + j;
                while (backward ? j >= 0 : j <= m - len) {
                    if (!wholeWord ||
                        ((j == 0 || !unicodeTypeWord (txt[j - 1])) &&
                         (j + len == m || !unicodeTypeWord (txt[j + len])))) {
                        // compare the strings
                        for (k = 0; k < len; ++k) {
                            if (p[k] != s2[k]) { break; }
                        }

                        // found it
                        if (k == len) {
                            switch (line->rot) {
                            case 0:
                                xMin1 = line->edge[j];
                                xMax1 = line->edge[j + len];
                                yMin1 = line->yMin;
                                yMax1 = line->yMax;
                                break;
                            case 1:
                                xMin1 = line->xMin;
                                xMax1 = line->xMax;
                                yMin1 = line->edge[j];
                                yMax1 = line->edge[j + len];
                                break;
                            case 2:
                                xMin1 = line->edge[j + len];
                                xMax1 = line->edge[j];
                                yMin1 = line->yMin;
                                yMax1 = line->yMax;
                                break;
                            case 3:
                                xMin1 = line->xMin;
                                xMax1 = line->xMax;
                                yMin1 = line->edge[j + len];
                                yMax1 = line->edge[j];
                                break;
                            }
                            if (backward) {
                                if ((startAtTop || yMin1 < yStart ||
                                     (yMin1 == yStart && xMin1 < xStart)) &&
                                    (stopAtBottom || yMin1 > yStop ||
                                     (yMin1 == yStop && xMin1 > xStop))) {
                                    if (!found || yMin1 > yMin0 ||
                                        (yMin1 == yMin0 && xMin1 > xMin0)) {
                                        xMin0 = xMin1;
                                        xMax0 = xMax1;
                                        yMin0 = yMin1;
                                        yMax0 = yMax1;
                                        found = true;
                                    }
                                }
                            }
                            else {
                                if ((startAtTop || yMin1 > yStart ||
                                     (yMin1 == yStart && xMin1 > xStart)) &&
                                    (stopAtBottom || yMin1 < yStop ||
                                     (yMin1 == yStop && xMin1 < xStop))) {
                                    if (!found || yMin1 < yMin0 ||
                                        (yMin1 == yMin0 && xMin1 < xMin0)) {
                                        xMin0 = xMin1;
                                        xMax0 = xMax1;
                                        yMin0 = yMin1;
                                        yMax0 = yMax1;
                                        found = true;
                                    }
                                }
                            }
                        }
                    }
                    if (backward) {
                        --j;
                        --p;
                    }
                    else {
                        ++j;
                        ++p;
                    }
                }
            }
        }
    }

    if (!caseSensitive) {
        free (s2);
        free (txt); // TODO: memory management
    }

    if (found) {
        *xMin = xMin0;
        *xMax = xMax0;
        *yMin = yMin0;
        *yMax = yMax0;
        lastFindXMin = xMin0;
        lastFindYMin = yMin0;
        haveLastFind = true;
        return true;
    }

    return false;
}

GString*
TextPage::getText (double xMin, double yMin, double xMax, double yMax) {
    UnicodeMap* uMap;
    char space[8], eol[16];
    int spaceLen, eolLen;
    GList* chars2;
    GString** out;
    int* outLen;
    TextColumn* col;
    TextParagraph* par;
    TextChar* ch;
    bool primaryLR;
    TextBlock* tree;
    GList* columns;
    GString* ret;
    double xx, yy;
    int rot, colIdx, parIdx, lineIdx, ph, y, i;

    // get the output encoding
    if (!(uMap = globalParams->getTextEncoding ())) { return NULL; }
    spaceLen = uMap->mapUnicode (0x20, space, sizeof (space));
    eolLen = 0; // make gcc happy
    switch (globalParams->getTextEOL ()) {
    case eolUnix: eolLen = uMap->mapUnicode (0x0a, eol, sizeof (eol)); break;
    case eolDOS:
        eolLen = uMap->mapUnicode (0x0d, eol, sizeof (eol));
        eolLen += uMap->mapUnicode (0x0a, eol + eolLen, sizeof (eol) - eolLen);
        break;
    case eolMac: eolLen = uMap->mapUnicode (0x0d, eol, sizeof (eol)); break;
    }

    // get all chars in the rectangle
    // (i.e., all chars whose center lies inside the rectangle)
    chars2 = new GList ();
    for (i = 0; i < chars->getLength (); ++i) {
        ch = (TextChar*)chars->get (i);
        xx = 0.5 * (ch->xmin + ch->xmax);
        yy = 0.5 * (ch->ymin + ch->ymax);
        if (xx > xMin && xx < xMax && yy > yMin && yy < yMax) {
            chars2->append (ch);
        }
    }
#if 0 //~debug
  dumpChars(chars2);
#endif

    rot = rotateChars (chars2);
    primaryLR = checkPrimaryLR (chars2);
    tree = splitChars (chars2);
    if (!tree) {
        unrotateChars (chars2, rot);
        delete chars2;
        return new GString ();
    }
#if 0 //~debug
  dumpTree(tree);
#endif
    columns = buildColumns (tree);
    delete tree;
    ph = assignPhysLayoutPositions (columns);
#if 0 //~debug
  dumpColumns(columns);
#endif
    unrotateChars (chars2, rot);
    delete chars2;

    out = (GString**)calloc (ph, sizeof (GString*));
    outLen = (int*)calloc (ph, sizeof (int));
    for (i = 0; i < ph; ++i) {
        out[i] = NULL;
        outLen[i] = 0;
    }

    columns->sort (&TextColumn::cmpPX);
    for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
        col = (TextColumn*)columns->get (colIdx);
        y = col->py;
        for (parIdx = 0; parIdx < col->paragraphs->getLength () && y < ph;
             ++parIdx) {
            par = (TextParagraph*)col->paragraphs->get (parIdx);
            for (lineIdx = 0; lineIdx < par->lines.size () && y < ph;
                 ++lineIdx) {
                auto& line = par->lines [lineIdx];
                if (!out[y]) { out[y] = new GString (); }
                while (outLen[y] < col->px + line->px) {
                    out[y]->append (space, spaceLen);
                    ++outLen[y];
                }
                encodeFragment (line->text.data (), line->len, uMap, primaryLR, out[y]);
                outLen[y] += line->pw;
                ++y;
            }
            if (parIdx + 1 < col->paragraphs->getLength ()) { ++y; }
        }
    }

    ret = new GString ();
    for (i = 0; i < ph; ++i) {
        if (out[i]) {
            ret->append (*out[i]);
            delete out[i];
        }
        if (ph > 1) { ret->append (eol, eolLen); }
    }

    free (out);
    free (outLen);
    deleteGList (columns, TextColumn);
    uMap->decRefCnt ();

    return ret;
}

bool TextPage::findCharRange (
    int pos, int length, double* xMin, double* yMin, double* xMax,
    double* yMax) {
    TextChar* ch;
    double xMin2, yMin2, xMax2, yMax2;
    bool first;
    int i;

    //~ this doesn't correctly handle ranges split across multiple lines
    //~ (the highlighted region is the bounding box of all the parts of
    //~ the range)

    xMin2 = yMin2 = xMax2 = yMax2 = 0;
    first = true;
    for (i = 0; i < chars->getLength (); ++i) {
        ch = (TextChar*)chars->get (i);
        if (ch->charPos >= pos && ch->charPos < pos + length) {
            if (first || ch->xmin < xMin2) { xMin2 = ch->xmin; }
            if (first || ch->ymin < yMin2) { yMin2 = ch->ymin; }
            if (first || ch->xmax > xMax2) { xMax2 = ch->xmax; }
            if (first || ch->ymax > yMax2) { yMax2 = ch->ymax; }
            first = false;
        }
    }
    if (first) { return false; }
    *xMin = xMin2;
    *yMin = yMin2;
    *xMax = xMax2;
    *yMax = yMax2;
    return true;
}

TextWords
TextPage::makeWordList () {
    TextBlock* tree;
    GList* columns;
    TextColumn* col;
    TextParagraph* par;
    TextWords words;
    int rot, colIdx, parIdx, lineIdx, wordIdx;

    rot = rotateChars (chars);

    if (0 == (tree = splitChars (chars))) {
        unrotateChars (chars, rot);
        return { };
    }

    columns = buildColumns (tree);
    delete tree;

    unrotateChars (chars, rot);

    for (colIdx = 0; colIdx < columns->getLength (); ++colIdx) {
        col = (TextColumn*)columns->get (colIdx);

        for (parIdx = 0; parIdx < col->paragraphs->getLength (); ++parIdx) {
            par = (TextParagraph*)col->paragraphs->get (parIdx);

            for (lineIdx = 0; lineIdx < par->lines.size (); ++lineIdx) {
                auto& line = par->lines [lineIdx];

                for (wordIdx = 0; wordIdx < line->words.size (); ++wordIdx) {
                    words.push_back (line->words [wordIdx]);
                }
            }
        }
    }

    switch (control.mode) {
    case textOutReadingOrder:
        // already in reading order
        break;

    case textOutPhysLayout:
    case textOutTableLayout:
    case textOutLinePrinter:
        actions::sort (words, lessYX);
        break;

    case textOutRawOrder:
        actions::sort (words, lessCharPos);
        break;
    }

    // this has to be done after sorting with cmpYX
    unrotateColumns (columns, rot);
    unrotateWords (words, rot);

    deleteGList (columns, TextColumn);

    return words;
}

//
// TextPage: debug
//
#if 0 //~debug

void TextPage::dumpChars(GList *charsA) {
    TextChar *ch;
    int i;

    for (i = 0; i < charsA->getLength(); ++i) {
        ch = (TextChar *)charsA->get(i);
        printf("char: U+%04x '%c' xMin=%g yMin=%g xMax=%g yMax=%g fontSize=%g rot=%d\n",
               ch->c, ch->c & 0xff, ch->xmin, ch->ymin, ch->xmax, ch->ymax,
               ch->fontSize, ch->rot);
    }
}

void TextPage::dumpTree(TextBlock *tree, int indent) {
    TextChar *ch;
    int i;

    printf("%*sblock: type=%s tag=%s small=%d rot=%d xMin=%g yMin=%g xMax=%g yMax=%g\n",
           indent, "",
           tree->type == blkLeaf ? "leaf" :
           tree->type == blkHorizSplit ? "horiz" : "vert",
           tree->tag == blkTagMulticolumn ? "multicolumn" :
           tree->tag == blkTagColumn ? "column" : "line",
           tree->smallSplit,
           tree->rot, tree->xMin, tree->yMin, tree->xMax, tree->yMax);
    if (tree->type == blkLeaf) {
        for (i = 0; i < tree->children->getLength(); ++i) {
            ch = (TextChar *)tree->children->get(i);
            printf("%*schar: '%c' xMin=%g yMin=%g xMax=%g yMax=%g font=%d.%d\n",
                   indent + 2, "", ch->c & 0xff,
                   ch->xmin, ch->ymin, ch->xmax, ch->ymax,
                   ch->font->fontID.num, ch->font->fontID.gen);
        }
    } else {
        for (i = 0; i < tree->children->getLength(); ++i) {
            dumpTree((TextBlock *)tree->children->get(i), indent + 2);
        }
    }
}

void TextPage::dumpColumns(GList *columns) {
    TextColumn *col;
    TextParagraph *par;
    TextLine *line;
    int colIdx, parIdx, lineIdx, i;

    for (colIdx = 0; colIdx < columns->getLength(); ++colIdx) {
        col = (TextColumn *)columns->get(colIdx);
        printf("column: xMin=%g yMin=%g xMax=%g yMax=%g px=%d py=%d pw=%d ph=%d\n",
               col->xMin, col->yMin, col->xMax, col->yMax,
               col->px, col->py, col->pw, col->ph);
        for (parIdx = 0; parIdx < col->paragraphs->getLength(); ++parIdx) {
            par = (TextParagraph *)col->paragraphs->get(parIdx);
            printf("  paragraph:\n");
            for (lineIdx = 0; lineIdx < par->lines.size (); ++lineIdx) {
                line = (TextLine *)par->lines->get(lineIdx);
                printf("    line: xMin=%g yMin=%g xMax=%g yMax=%g px=%d pw=%d rot=%d\n",
                       line->xMin, line->yMin, line->xMax, line->yMax,
                       line->px, line->pw, line->rot);
                printf("          ");
                for (i = 0; i < line->len; ++i) {
                    printf("%c", line->text[i] & 0xff);
                }
                printf("\n");
            }
        }
    }
}

#endif //~debug

//
// TextOutputDev
//
static void outputToFile (void* stream, const char* text, int len) {
    fwrite (text, 1, len, (FILE*)stream);
}

TextOutputDev::TextOutputDev (
    const char* fileName, TextOutputControl* controlA, bool append) {
    text = NULL;
    control = *controlA;
    ok = true;

    // open file
    needClose = false;
    if (fileName) {
        if (!strcmp (fileName, "-")) { outputStream = stdout; }
        else if ((outputStream = fopen (fileName, append ? "ab" : "wb"))) {
            needClose = true;
        }
        else {
            error (errIO, -1, "Couldn't open text file '{0:s}'", fileName);
            ok = false;
            return;
        }
        outputFunc = &outputToFile;
    }
    else {
        outputStream = NULL;
    }

    // set up text object
    text = new TextPage (&control);
}

TextOutputDev::TextOutputDev (
    TextOutputFunc func, void* stream, TextOutputControl* controlA) {
    outputFunc = func;
    outputStream = stream;
    needClose = false;
    control = *controlA;
    text = new TextPage (&control);
    ok = true;
}

TextOutputDev::~TextOutputDev () {
    if (needClose) { fclose ((FILE*)outputStream); }
    if (text) { delete text; }
}

void TextOutputDev::startPage (int pageNum, GfxState* state) {
    text->startPage (state);
}

void TextOutputDev::endPage () {
    if (outputStream) { text->write (outputStream, outputFunc); }
}

void TextOutputDev::restoreState (GfxState* state) { text->updateFont (state); }

void TextOutputDev::updateFont (GfxState* state) { text->updateFont (state); }

void TextOutputDev::beginString (GfxState* state, GString* s) {}

void TextOutputDev::endString (GfxState* state) {}

void TextOutputDev::drawChar (
    GfxState* state, double x, double y, double dx, double dy, double originX,
    double originY, CharCode c, int nBytes, Unicode* u, int uLen) {
    text->addChar (state, x, y, dx, dy, c, nBytes, u, uLen);
}

void TextOutputDev::incCharCount (int nChars) { text->incCharCount (nChars); }

void TextOutputDev::beginActualText (GfxState* state, Unicode* u, int uLen) {
    text->beginActualText (state, u, uLen);
}

void TextOutputDev::endActualText (GfxState* state) {
    text->endActualText (state);
}

bool TextOutputDev::findText (
    Unicode* s, int len, bool startAtTop, bool stopAtBottom,
    bool startAtLast, bool stopAtLast, bool caseSensitive, bool backward,
    bool wholeWord, double* xMin, double* yMin, double* xMax, double* yMax) {
    return text->findText (
        s, len, startAtTop, stopAtBottom, startAtLast, stopAtLast,
        caseSensitive, backward, wholeWord, xMin, yMin, xMax, yMax);
}

GString*
TextOutputDev::getText (double xMin, double yMin, double xMax, double yMax) {
    return text->getText (xMin, yMin, xMax, yMax);
}

bool TextOutputDev::findCharRange (
    int pos, int length, double* xMin, double* yMin, double* xMax,
    double* yMax) {
    return text->findCharRange (pos, length, xMin, yMin, xMax, yMax);
}

TextWords
TextOutputDev::makeWordList () {
    return text->makeWordList ();
}

TextPage* TextOutputDev::takeText () {
    TextPage* ret;

    ret = text;
    text = new TextPage (&control);
    return ret;
}

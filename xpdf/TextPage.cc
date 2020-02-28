// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cmath>
#include <cctype>

#include <iostream>
#include <memory>
#include <variant>
#include <vector>

#include <utils/string.hh>

#include <xpdf/bbox.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/TextBlock.hh>
#include <xpdf/TextChar.hh>
#include <xpdf/TextPage.hh>
#include <xpdf/TextParagraph.hh>
#include <xpdf/TextWord.hh>
#include <xpdf/UnicodeTypeTable.hh>

#include <range/v3/all.hpp>
using namespace ranges;

#include <boost/scope_exit.hpp>

////////////////////////////////////////////////////////////////////////

struct char_t {
    wchar_t value;
    xpdf::bbox_t box;
};

template< typename T > xpdf::bbox_t bbox_from (const T&);

template< >
inline xpdf::bbox_t
bbox_from< TextChar > (const TextChar& ch) {
    return xpdf::bbox_t{ ch.xmin, ch.ymin, ch.xmax, ch.ymax };
}

template< >
inline xpdf::bbox_t
bbox_from< TextCharPtr > (const TextCharPtr& p) {
    return bbox_from (*p);
}

template< >
inline xpdf::bbox_t
bbox_from< char_t > (const char_t& ch) {
    return ch.box;
}

template< >
inline xpdf::bbox_t
bbox_from< xpdf::bbox_t > (const xpdf::bbox_t& arg) {
    return arg;
}

inline char_t make_char (TextChar& ch) {
    return { wchar_t (ch.c), bbox_from (ch) };
}

////////////////////////////////////////////////////////////////////////

//
// Character/column comparison objects:
//
const auto lessX = [](const auto& lhs, const auto& rhs) {
    return lhs->xmin < rhs->xmin;
};

const auto lessY = [](const auto & lhs, const auto& rhs) {
    return lhs->ymin < rhs->ymin;
};

//
// X-coordinate column position comparison object:
//
const auto lessPosX = [](const auto& lhs, const auto& rhs) {
    return lhs->px < rhs->px;
};

//
// Word comparison objects:
//
const auto lessYX = [](auto& lhs, auto& rhs) {
    return
        lhs->ymin  < rhs->ymin || (
        lhs->ymin == rhs->ymin && lhs->xmin < rhs->xmin);
};

const auto lessCharPos = [](auto& lhs, auto& rhs) {
    return lhs->charPos [0] < rhs->charPos [0];
};

////////////////////////////////////////////////////////////////////////

struct TextUnderline {
    TextUnderline (double x0A, double y0A, double x1A, double y1A) {
        x0 = x0A;
        y0 = y0A;
        x1 = x1A;
        y1 = y1A;
        horiz = y0 == y1;
    }

    double x0, y0, x1, y1;
    bool horiz;
};

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

    ~TextLink () {
        if (uri) { delete uri; }
    }

    double xMin, yMin, xMax, yMax;
    GString* uri;
};

////////////////////////////////////////////////////////////////////////

TextPage::TextPage (TextOutputControl* controlA) {
    control = *controlA;
    pageWidth = pageHeight = 0;
    charPos = 0;
    curFont = { };
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
    findLR = true;
    lastFindXMin = lastFindYMin = 0;
    haveLastFind = false;
}

TextPage::~TextPage () {
    clear ();
}

template< xpdf::rotation_t >
bool do_reading_order (double, double, double, double);

#define XPDF_READING_ORDER_DEF(rot, a, b, c, d)                     \
template< > inline bool do_reading_order< xpdf::rotation_t::rot > ( \
    double a_x0, double a_y0, double b_x0, double b_y0) {           \
    return a < b || (a == b && c < d);                              \
}

XPDF_READING_ORDER_DEF (               none, a_y0, b_y0, a_x0, b_x0)
XPDF_READING_ORDER_DEF (       quarter_turn, b_x0, a_x0, a_y0, b_y0)
XPDF_READING_ORDER_DEF (          half_turn, b_y0, a_y0, b_x0, a_x0)
XPDF_READING_ORDER_DEF (three_quarters_turn, a_x0, b_x0, b_y0, a_y0)

#undef XPDF_READING_ORDER_DEF

template< xpdf::rotation_t R, typename T >
inline bool reading_order (const T& lhs, const T& rhs) {
    const auto& a = bbox_from (lhs).arr;
    const auto& b = bbox_from (rhs).arr;
    return do_reading_order< R > (a [0], a[1], b [0], b [1]);
}

inline xpdf::bbox_t
dilate_horizontally (xpdf::bbox_t x, double factor = .10) {
    auto delta = factor * height_of (x);
    x.arr [0] -= delta; x.arr [2] += delta;
    return x;
}

inline xpdf::bbox_t
dilate_vertically (xpdf::bbox_t x, double factor = .15) {
    auto delta = factor * height_of (x);
    x.arr [1] -= delta; x.arr [3] += delta;
    return x;
}

template< typename T >
std::vector< xpdf::bbox_t >
simple_aggregate (const std::vector< xpdf::bbox_t >& xs, T test) {
    if (xs.size () < 2) {
        return xs;
    }

    std::vector< xpdf::bbox_t > Xs;

    for (auto& x : xs) {
        bool coalesced = false;

        for (auto& X : Xs | views::reverse) {
            if (test (X, x)) {
                X = coalesce (X, x);
                coalesced = true;
                break;
            }
        }

        if (!coalesced) {
            Xs.push_back (x);
        }
    }

    return Xs;
}

template< typename T >
std::vector< xpdf::bbox_t >
aggregate (std::vector< xpdf::bbox_t > xs, T test) {
    if (xs.size () < 2) {
        return xs;
    }

    std::vector< xpdf::bbox_t > Xs;

    for (;; xs = std::move (Xs)) {
        Xs = simple_aggregate (xs, test);

        if (xs.size () == Xs.size ()) {
            break;
        }
    }

    return Xs;
}

std::vector< xpdf::bbox_t >
aggregate (std::vector< xpdf::bbox_t > letters) {
    if (letters.size () < 2) {
        return letters;
    }

    auto test = [](double factor = .10) {
        return [=](auto& lhs, auto& rhs) {
            return overlapping (lhs, dilate_horizontally (rhs, factor));
        };
    };

    auto words = aggregate (letters, test ());
    auto lines = aggregate (words, test (.50));

    auto test2 = [](double factor = .15) {
        return [=](auto& lhs, auto& rhs) {
            return overlapping (lhs, dilate_vertically (rhs, factor));
        };
    };

    auto paras = simple_aggregate (lines, test2 ());

    return paras;
}

inline auto
rotate_by (int rotation) {
    using namespace xpdf;
    switch (rotation) {
    default:
    case 0: return xpdf::rotate< rotation_t::none, double >;
    case 3: return xpdf::rotate< rotation_t::quarter_turn, double >;
    case 2: return xpdf::rotate< rotation_t::half_turn, double >;
    case 1: return xpdf::rotate< rotation_t::three_quarters_turn, double >;
    }
}

inline auto
undo_rotate_by (int rotation) {
    using namespace xpdf;
    switch (rotation) {
    default:
    case 0: return unrotate< rotation_t::none, double >;
    case 3: return unrotate< rotation_t::quarter_turn, double >;
    case 2: return unrotate< rotation_t::half_turn, double >;
    case 1: return unrotate< rotation_t::three_quarters_turn, double >;
    }
}

template< typename Fn >
inline void
do_upright (std::vector< xpdf::bbox_t >& xs, const xpdf::bbox_t& X, Fn fn) {
    for_each (xs, [&](auto& x) { x = fn (x, X); });
}

inline void
upright (std::vector< xpdf::bbox_t >& xs, int rot, const xpdf::bbox_t& X) {
    do_upright (xs, X, rotate_by (rot));
};

inline void
undo_upright (std::vector< xpdf::bbox_t >& xs, int rot, const xpdf::bbox_t& X) {
    do_upright (xs, X, undo_rotate_by (rot));
};

std::vector< xpdf::bbox_t >
TextPage::segment () const {
    using namespace xpdf;

    const xpdf::bbox_t superbox{ 0, 0, pageWidth, pageHeight };
    std::vector< xpdf::bbox_t > boxes;

    for (int rotation : { 0, 1, 2, 3 }) {
        std::vector< xpdf::bbox_t > xs;

        auto rng = chars | views::filter ([=](auto& ch) {
            return rotation == ch->rot;
        });

        transform (rng, back_inserter (xs), [](auto& ch) {
            return bbox_from (ch);
        });

        upright (xs, rotation, superbox);

        xs = aggregate (xs);
        undo_upright (xs, rotation, superbox);

        boxes.insert (boxes.end (), xs.begin (), xs.end ());
    }

    return boxes;
}

////////////////////////////////////////////////////////////////////////

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
    curFont = { };
    curFontSize = 0;
    curRot = 0;
    nTinyChars = 0;
    free (actualText);
    actualText = NULL;
    actualTextLen = 0;
    actualTextNBytes = 0;
    chars.clear ();
    fonts.clear ();
    underlines.clear ();
    links.clear ();
    findLR = true;
    lastFindXMin = lastFindYMin = 0;
    haveLastFind = false;
    findCols.clear ();
}

void TextPage::updateFont (GfxState* state) {
    GfxFont* gfxFont;
    double* fm;
    char* name;
    int code, mCode, letterCode, anyCode;
    double w, m[4], m2[4];

    auto iter = find_if (fonts, [&](auto& x) { return x->matches (state); });

    if (iter == fonts.end ()) {
        curFont = std::make_shared< TextFontInfo > (state);
        fonts.push_back (curFont);
    }
    else {
        curFont = *iter;
    }

    // adjust the font size
    gfxFont     = state->getFont ();
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
    if (!globalParams->getTextKeepTinyChars () && fabs (w1) < 3 && fabs (h1) < 3) {
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

            chars.push_back (
                std::make_shared< TextChar > (
                    TextChar {
                        curFont, curFontSize,
                        xMin, yMin, xMax, yMax,
                        xpdf::to_double (rgb.r),
                        xpdf::to_double (rgb.g),
                        xpdf::to_double (rgb.b),
                        u [j], charPos,
                        uint8_t (nBytes), uint8_t (curRot), clipped,
                        state->getRender () == 3
                    }));
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
    underlines.push_back (std::make_shared< TextUnderline > (x0, y0, x1, y1));
}

void TextPage::addLink (
    double xMin, double yMin, double xMax, double yMax, Link* link) {
    GString* uri;

    if (link && link->getAction () && link->getAction ()->getKind () == actionURI) {
        uri = ((LinkURI*)link->getAction ())->getURI ()->copy ();
        links.push_back (std::make_shared< TextLink > (xMin, yMin, xMax, yMax, uri));
    }
}

void TextPage::write (void* pstr, TextOutputFunc pfun) {
    UnicodeMap* uMap;

    char space [8], eol [16], eop [8];
    int spaceLen, eolLen, eopLen;

    bool pageBreaks;

    // get the output encoding
    if (!(uMap = globalParams->getTextEncoding ())) {
        return;
    }

    spaceLen = uMap->mapUnicode (0x20, space, sizeof (space));
    eolLen = 0; // make gcc happy

    switch (globalParams->getTextEOL ()) {
    case eolUnix:
        eolLen = uMap->mapUnicode (0x0a, eol, sizeof (eol));
        break;

    case eolDOS:
        eolLen  = uMap->mapUnicode (0x0d, eol, sizeof (eol));
        eolLen += uMap->mapUnicode (0x0a, eol + eolLen, sizeof (eol) - eolLen);
        break;

    case eolMac:
        eolLen = uMap->mapUnicode (0x0d, eol, sizeof (eol));
        break;
    }

    eopLen = uMap->mapUnicode (0x0c, eop, sizeof (eop));
    pageBreaks = globalParams->getTextPageBreaks ();

    switch (control.mode) {
    case textOutReadingOrder:
        writeReadingOrder (pstr, pfun, uMap, space, spaceLen, eol, eolLen);
        break;
    case textOutPhysLayout: case textOutTableLayout:
        writePhysLayout (pstr, pfun, uMap, space, spaceLen, eol, eolLen);
        break;
    case textOutLinePrinter:
        writeLinePrinter ( pstr, pfun, uMap, space, spaceLen, eol, eolLen);
        break;
    case textOutRawOrder:
        writeRaw (pstr, pfun, uMap, space, spaceLen, eol, eolLen);
        break;
    }

    // end of page
    if (pageBreaks) {
        (*pfun) (pstr, eop, eopLen);
    }

    uMap->decRefCnt ();
}

void TextPage::writeReadingOrder (
    void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
    char* space, int spaceLen, char* eol, int eolLen) {
    TextBlockPtr tree;
    bool primaryLR;
    GString* s;
    int lineIdx, rot, n;

    rot = rotateChars (chars);
    primaryLR = isPrevalentLeftToRight (chars);
    tree = splitChars (chars);

    if (!tree) {
        // no text
        unrotateChars (chars, rot);
        return;
    }

    auto columns = buildColumns (tree);
    unrotateChars (chars, rot);

    for (auto& col : columns) {
        for (auto& par : col->paragraphs) {
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
}

// This handles both physical layout and table layout modes.
void TextPage::writePhysLayout (
    void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
    char* space, int spaceLen, char* eol, int eolLen) {
    GString** out;
    int* outLen;
    bool primaryLR;
    int ph, parIdx, rot, y, i;

    rot = rotateChars (chars);
    primaryLR = isPrevalentLeftToRight (chars);

    TextBlockPtr tree = splitChars (chars);

    if (!tree) {
        // no text
        unrotateChars (chars, rot);
        return;
    }

    auto columns = buildColumns (tree);
    unrotateChars (chars, rot);

    ph = assignPhysLayoutPositions (columns);

    out = (GString**)calloc (ph, sizeof (GString*));
    outLen = (int*)calloc (ph, sizeof (int));

    for (i = 0; i < ph; ++i) {
        out[i] = NULL;
        outLen[i] = 0;
    }

    sort (columns, lessPosX);

    for (auto& col : columns) {
        y = col->py;

        for (parIdx = 0; parIdx < col->paragraphs.size () && y < ph; ++parIdx) {
            auto& par = col->paragraphs [parIdx];

            for (auto& line : par->lines ) {
                if (!out[y]) {
                    out[y] = new GString ();
                }

                while (outLen[y] < col->px + line->px) {
                    out[y]->append (space, spaceLen);
                    ++outLen[y];
                }

                encodeFragment (line->text.data (), line->len, uMap, primaryLR, out[y]);
                outLen[y++] += line->pw;
            }

            if (parIdx + 1 < col->paragraphs.size ()) {
                ++y;
            }
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
}

inline bool
vertical_overlapping (TextChar& lhs, TextChar& rhs) {
    const auto &af = ascentAdjustFactor, &df = descentAdjustFactor;

    return lhs.ymax - df * (lhs.ymax - lhs.ymin) > rhs.ymin + af * (rhs.ymax - rhs.ymin)
        && lhs.ymin + af * (lhs.ymax - lhs.ymin) < rhs.ymax - df * (rhs.ymax - rhs.ymin);
}

inline double
pitch_of (TextChars& chars) {
    ASSERT (!chars.empty ());

    //
    // Compute the (approximate) character pitch:
    //
    auto n = (std::numeric_limits< double >::max) ();

    //
    // An O(n²) algorithm:
    //
    for (size_t i = 0; i < chars.size (); ++i) {
        auto& a = *chars [i];

        for (size_t j = i + 1; j < chars.size (); ++j) {
            //
            // Compute minimum pitch between any two characters that
            // slightly overlap on the vertical:
            //
            auto& b = *chars [j];

            if (vertical_overlapping (a, b)) {
                auto delta = fabs (b.xmin - a.xmin);

                if (n > delta) {
                    n = delta;
                }
            }
        }
    }

    ASSERT (n > 0);
    return n;
}

inline double
linespacing_of (TextChars& chars) {
    //
    // Compute (approximate) line spacing
    //
    auto n = (std::numeric_limits< double >::max) ();

    for (size_t i = 0; i < chars.size (); ) {
        auto& a = *chars [i];

        //
        // Find the first (significantly) non-overlapping character and compute
        // the vertical spacing between the two:
        //
        auto delta = 0.;

        for (++i; delta && i < chars.size (); ++i) {
            auto& b = *chars [i];

            if (b.ymin +  ascentAdjustFactor * (b.ymax - b.ymin) >
                a.ymax - descentAdjustFactor * (a.ymax - a.ymin)) {
                delta = b.ymin - a.ymin;
            }
        }

        if (delta > 0 && delta < n) {
            n = delta;
        }
    }

    return n;
}

void TextPage::writeLinePrinter (
    void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
    char* space, int spaceLen, char* eol, int eolLen) {
    char buf[8];
    double yMin0, yShift, xMin0, xShift;
    double y, x;
    int rot, n, i, k;

    rot = rotateChars (chars);
    sort (chars, lessX);

    removeDuplicates (chars, 0);
    sort (chars, lessY);

    //
    // Character pitch:
    //
    double pitch = control.fixedPitch;

    if (0 == pitch) {
        pitch = pitch_of (chars);
    }

    //
    // Line spacing:
    //
    double lineSpacing = control.fixedLineSpacing;

    if (0 == lineSpacing) {
        lineSpacing = linespacing_of (chars);
    }

    //
    // Shift the grid to avoid problems with floating point accuracy -- for
    // fixed line spacing, this avoids problems with dropping/inserting blank
    // lines:
    //
    if (!chars.empty ()) {
        yMin0 = (chars [0])->ymin;
        yShift = yMin0 - (int)(yMin0 / lineSpacing + 0.5) * lineSpacing -
                 0.5 * lineSpacing;
    }
    else {
        yShift = 0;
    }

    for (y = yShift; y < pageHeight; y += lineSpacing) {
        // get the characters in this line
        TextChars line;

        for (i = 0; i < chars.size () && chars [i]->ymin < y + lineSpacing; ) {
            line.push_back (chars [i++]);
        }

        sort (line, lessX);

        if (!line.empty ()) {
            //
            // Shift the grid to avoid problems with floating point accuracy -- for
            // fixed char spacing, this avoids problems with dropping/inserting
            // space:
            //
            xMin0 = line [0]->xmin;
            xShift = xMin0 - (int)(xMin0 / pitch + 0.5) * pitch - 0.5 * pitch;
        }
        else {
            xShift = 0;
        }

        // write the line
        GString s;

        x = xShift;
        k = 0;

        while (k < line.size ()) {
            auto& ch = line [k];

            if (ch->xmin < x + pitch) {
                n = uMap->mapUnicode (ch->c, buf, sizeof (buf));
                s.append (buf, n);
                ++k;
            }
            else {
                s.append (space, spaceLen);
                n = spaceLen;
            }

            x += (uMap->isUnicode () ? 1 : n) * pitch;
        }

        s.append (eol, eolLen);
        (*outputFunc) (outputStream, s.c_str (), s.getLength ());
    }

    unrotateChars (chars, rot);
}

//
// If the vertical distance is too large or the next character is too much to
// the left of the first, they belong on separate lines:
//
inline bool linebreak_between (const TextChar& a, const TextChar& b) {
    ASSERT (a.rot == b.rot);

    const auto size = a.size, delta = rawModeLineDelta * size,
        overlap = rawModeCharOverlap * size;

    switch (a.rot) {
    case 0: return fabs (b.ymin - a.ymin) > delta || b.xmin - a.xmax < -overlap;
    case 1: return fabs (a.xmax - b.xmax) > delta || b.ymin - a.ymax < -overlap;
    case 2: return fabs (a.ymax - b.ymax) > delta || a.xmin - b.xmax < -overlap;
    case 3: return fabs (b.xmin - a.xmin) > delta || a.ymin - b.ymax < -overlap;

    default:
        ASSERT (0);
        break;
    }
}

inline bool space_between (const TextChar& a, const TextChar& b) {
    ASSERT (a.rot == b.rot);

    const auto size = a.size, spacing = rawModeWordSpacing * size;

    switch (a.rot) {
    case 0: return b.xmin - a.xmax > spacing;
    case 1: return b.ymin - a.ymax > spacing;
    case 2: return a.xmin - b.xmax > spacing;
    case 3: return a.ymin - b.ymax > spacing;
    default:
        ASSERT (0);
        break;
    }
}

void TextPage::writeRaw (
    void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap,
    char* space, int spaceLen, char* eol, int eolLen) {

    std::string s;
    char buf [8];

    for (size_t i = 0; i < chars.size (); ++i) {
        auto& ch1 = *chars [i];

        const int len = uMap->mapUnicode (ch1.c, buf, sizeof (buf));
        s.append (buf, len);

        if (i + 1 < chars.size ()) {
            auto& ch2 = *chars [i + 1];

            if (ch1.rot != ch2.rot || linebreak_between (ch1, ch2)) {
                s.append (eol, eolLen);
            }
            else if (space_between (ch1, ch2)) {
                s.append (space, spaceLen);
            }
        }
        else {
            s.append (eol, eolLen);
        }

        if (s.size () > 1000UL) {
            //
            // Arbitrary buffering here:
            //
            (*outputFunc) (outputStream, s.c_str (), s.size ());
            s.clear ();
        }
    }

    if (!s.empty ()) {
        //
        // Print-out the remaining characters:
        //
        (*outputFunc) (outputStream, s.c_str (), s.size ());
    }
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

inline void
do_rotate (TextChar& c, double x0, double y0, double x1, double y1, int n) {
    c.xmin = x0; c.xmax = x1; c.ymin = y0; c.ymax = y1;
    c.rot = (c.rot + n) & 3;
}

inline void
rotate90 (TextChar& c, int w, int) {
    do_rotate (c, c.ymin, w - c.xmax, c.ymax, w - c.xmin, 3);
};

inline void
rotate180 (TextChar& c, int w, int h) {
    do_rotate (c, w - c.xmax, h - c.ymax, w - c.xmin, h - c.ymin, 2);
}

inline void
rotate270 (TextChar& c, int, int h) {
    do_rotate (c, h - c.ymax, c.xmin, h - c.ymin, c.xmax, 1);
}

inline int
prevalent_rotation_amongst (TextChars& chars) {
    std::array< int, 4 > counters{ 0, 0, 0, 0 };

    for (auto& ch : chars) {
        ++counters [ch->rot];
    }

    return std::distance (counters.begin (), max_element (counters));
}

//
// Determine most prevalent rotation value.  Rotate all characters to that
// primary rotation.
//
int TextPage::rotateChars (TextChars& chars) {
    //
    // Count the numbers of characters for each rotation:
    //
    const int rot = prevalent_rotation_amongst (chars);

    // rotate
    switch (rot) {
    case 1:
        for (auto& c : chars) { rotate90 (*c, pageWidth, 0); }
        std::swap (pageWidth, pageHeight);
        break;

    case 2:
        for (auto& c : chars) { rotate180 (*c, pageWidth, pageHeight); }
        break;

    case 3:
        for (auto& c : chars) { rotate270 (*c, 0, pageHeight); }
        std::swap (pageWidth, pageHeight);
        break;

    case 0:
    default: break;
    }

    return rot;
}

// Rotate the TextUnderlines and TextLinks to match the transform
// performed by rotateChars().
void TextPage::rotateUnderlinesAndLinks (int rot) {
    double xMin, yMin, xMax, yMax;

    switch (rot) {
    case 1:
        for (auto& underline : underlines) {
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

        for (auto& link : links) {
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
        for (auto& underline : underlines) {
            xMin = pageWidth - underline->x1;
            xMax = pageWidth - underline->x0;
            yMin = pageHeight - underline->y1;
            yMax = pageHeight - underline->y0;
            underline->x0 = xMin;
            underline->x1 = xMax;
            underline->y0 = yMin;
            underline->y1 = yMax;
        }

        for (auto& link : links) {
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
        for (auto& underline : underlines) {
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

        for (auto& link : links) {
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

    case 0:
    default:
        break;
    }
}

template< typename T >
inline void
do_unrotate (T& t, double x0, double y0, double x1, double y1, int n) {
    t.xmin = x0; t.xmax = x1; t.ymin = y0; t.ymax = y1;
    t.rot = (t.rot + n) & 3;
}

template< >
inline void do_unrotate< TextColumn > (
    TextColumn& t, double x0, double y0, double x1, double y1, int) {
    t.xmin = x0; t.xmax = x1; t.ymin = y0; t.ymax = y1;
}

template< >
inline void do_unrotate< TextParagraph > (
    TextParagraph& t, double x0, double y0, double x1, double y1, int) {
    t.xmin = x0; t.xmax = x1; t.ymin = y0; t.ymax = y1;
}

template< typename T >
inline void unrotate90 (T& t, int w, int) {
    do_unrotate (t, w - t.ymax, t.xmin, w - t.ymin, t.xmax, 1);
};

template< typename T >
inline void unrotate180 (T& t, int w, int h) {
    do_unrotate (t, w - t.xmax, h - t.ymax, w - t.xmin, h - t.ymin, 2);
}

template< typename T >
inline void unrotate270 (T& t, int, int h) {
    do_unrotate (t, t.ymin, h - t.xmax, t.ymax, h - t.xmin, 3);
}

//
// Undo the coordinate transform performed by TextPage::rotateChars:
//
void
TextPage::unrotateChars (TextChars& chars, int rot) {
    switch (rot) {
    case 1:
        std::swap (pageWidth, pageHeight);
        for (auto& ch : chars) {
            unrotate90 (*ch, pageWidth, pageHeight);
        }
        break;

    case 2:
        for (auto& ch : chars) {
            unrotate180 (*ch, pageWidth, pageHeight);
        }
        break;

    case 3:
        std::swap (pageWidth, pageHeight);
        for (auto& ch : chars) {
            unrotate270 (*ch, pageWidth, pageHeight);
        }
        break;

    default:
        break;
    }
}

// Undo the coordinate transform performed by rotateChars().
void TextPage::unrotateColumns (TextColumns& columns, int rot) {
    auto w = pageWidth, h = pageHeight;

    switch (rot) {
    case 0:
    default:
        // no transform
        break;
    case 1:
        std::swap (pageWidth, pageHeight);

        for (auto& col : columns) {
            unrotate90 (*col, w, h);

            for (auto& par : col->paragraphs) {
                unrotate90 (*par, w, h);

                for (auto& line : par->lines) {
                    unrotate90 (*line, w, h);

                    for (auto& word : line->words) {
                        unrotate90 (*word, w, h);
                    }
                }
            }
        }
        break;

    case 2:
        for (auto& col : columns) {
            unrotate180 (*col, w, h);

            for (auto& par : col->paragraphs) {
                unrotate180 (*par, w, h);

                for (auto& line : par->lines) {
                    unrotate180 (*line, w, h);

                    actions::transform (
                        line->edge, [=](auto& x) { return w - x; });

                    for (auto& word : line->words) {
                        unrotate180 (*word, w, h);

                        actions::transform (
                            word->edge, [=](auto& x) { return w - x; });
                    }
                }
            }
        }
        break;

    case 3:
        std::swap (pageWidth, pageHeight);

        for (auto& col : columns) {
            unrotate90 (*col, w, h);

            for (auto& par : col->paragraphs) {
                unrotate90 (*par, w, h);

                for (auto& line : par->lines) {
                    unrotate90 (*line, w, h);

                    actions::transform (
                        line->edge, [=](auto& x) { return w - x; });

                    for (auto& word : line->words) {
                        unrotate90 (*word, w, h);

                        for (size_t i = 0; i <= word->size (); ++i) {
                            word->edge[i] = pageHeight - word->edge[i];
                        }
                    }
                }
            }
        }
        break;
    }
}

void
TextPage::unrotateWords (TextWords& words, int rot) {
    switch (rot) {
    case 0:
    default:
        // no transform
        break;
    case 1:
        for (auto& w : words) {
            unrotate90 (*w, pageWidth, pageHeight);
        }
        break;

    case 2:
        for (auto& w : words) {
            unrotate180 (*w, pageWidth, pageHeight);
            actions::transform (w->edge, [this](auto& x) { return pageWidth - x; });
        }
        break;

    case 3:
        for (auto& w : words) {
            unrotate270 (*w, pageWidth, pageHeight);
            actions::transform (w->edge, [this](auto& x) { return pageHeight - x; });
        }
        break;
    }
}

// Determine the primary text direction (LR vs RL).  Returns true for
// LR, false for RL.
bool TextPage::isPrevalentLeftToRight (TextChars& chars) {
    long n = 0;

    for (auto& c : chars) {
        const Unicode val = c->c;
        n += unicodeTypeL (val) ? 1 : unicodeTypeR (val) ? -1 : 0;
    }

    return n >= 0;
}

inline bool
duplicated (const TextChar& a, const TextChar& b, double x, double y) {
    return a.c == b.c &&
        fabs (a.xmin - b.xmin) < x &&
        fabs (a.ymax - b.ymax) < y;
}

//
// Remove duplicate characters.
// The list of characters has been sorted by X coordinate for rot ∈ { 0, 2 } and
// by Y coordinate for rot ∈ { 1, 3 }:
//
void TextPage::removeDuplicates (TextChars& chars, int rot) {
    if (rot & 1) {
        bool found = false;
        std::vector< bool > mask (chars.size ());

        //
        // Another O(n²) algorithm:
        //
        for (size_t i = 0; i < chars.size (); ++i) {
            auto& a = *chars [i];

            const double xdelta = dupMaxSecDelta * a.size;
            const double ydelta = dupMaxPriDelta * a.size;

            for (size_t j = i + 1; j < chars.size (); ) {
                auto& b = *chars [j];

                if (b.ymin - a.ymin >= ydelta) {
                    //
                    // Stop if characters are sufficiently apart, vertically:
                    //
                    break;
                }

                if (duplicated (a, b, xdelta, ydelta)) {
                    found = true;
                    mask [j] = true;
                }
                else {
                    ++j;
                }
            }
        }

        if (found) {
            TextChars other;
            other.reserve (chars.size ());

            for (size_t i = 0; i < mask.size (); ++i) {
                if (!mask [i]) {
                    other.push_back (chars [i]);
                }
            }

            std::swap (chars, other);
        }
    }
    else {
        //
        // YAOSA: Yet Another O-Squared Algorithm:
        //
        bool found = false;
        std::vector< bool > mask (chars.size ());

        //
        // Another O(n²) algorithm:
        //
        for (size_t i = 0; i < chars.size (); ++i) {
            auto& a = *chars [i];

            const double xdelta = dupMaxPriDelta * a.size;
            const double ydelta = dupMaxSecDelta * a.size;

            for (size_t j = i + 1; j < chars.size (); ) {
                auto& b = *chars [j];

                if (b.xmin - a.xmin >= xdelta) {
                    //
                    // Stop if characters are sufficiently apart, horizontally:
                    //
                    break;
                }

                if (duplicated (a, b, xdelta, ydelta)) {
                    found = true;
                    mask [j] = true;
                }
                else {
                    ++j;
                }
            }
        }

        if (found) {
            TextChars other;
            other.reserve (chars.size ());

            for (size_t i = 0; i < mask.size (); ++i) {
                if (!mask [i]) {
                    other.push_back (chars [i]);
                }
            }

            std::swap (chars, other);
        }
    }
}

//
// Split the characters into a tree of TextBlocks, one tree for each
// rotation. Merge into a single tree (with the primary rotation).
//
TextBlockPtr
TextPage::splitChars (TextChars& charsA) {
    TextBlockPtr tree [4], blk;

    //
    // Split: build a tree of TextBlocks for each rotation
    //
    TextChars clippedChars;

    for (const auto rot : { 0, 1, 2, 3 }) {
        TextChars chars2;
        chars2.reserve (charsA.size ());

        copy_if (charsA, back_insert_iterator (chars2), [&](auto& c) {
            return c->rot == rot;
        });

        tree [rot] = 0;

        if (chars2.size () > 0) {
            if (rot & 1) {
                sort (chars2, lessY);
            }
            else {
                sort (chars2, lessX);
            }

            removeDuplicates (chars2, rot);

            if (control.clipText) {
                TextChars otherChars;

                for (size_t i = 0; i < chars2.size (); ) {
                    auto& ch = chars2 [i];

                    if (ch->clipped) {
                        clippedChars.push_back (ch);
                    }
                    else {
                        otherChars.push_back (ch);
                        ++i;
                    }
                }

                std::swap (chars2, otherChars);
            }

            if (!chars2.empty ()) {
                tree [rot] = split (chars2, rot);
            }
        }
    }

    //
    // If the page contains no (unclipped) text, just leave an empty column
    // list:
    //
    if (0 == tree [0]) {
        return 0;
    }

    //
    // If the main tree is not a multicolumn node, insert one so that rotated
    // text has somewhere to go:
    //
    if (tree [0]->tag != blkTagMulticolumn) {
        blk = std::make_shared< TextBlock > (blkHorizSplit, 0);

        blk->addChild (tree [0]);
        blk->tag = blkTagMulticolumn;

        tree [0] = blk;
    }

    // merge non-primary-rotation text into the primary-rotation tree
    for (const auto rot : { 1, 2, 3 }) {
        if (tree [rot]) {
            insertIntoTree (tree [rot], tree [0]);
            tree [rot] = 0;
        }
    }

    if (!clippedChars.empty ()) {
        insertClippedChars (clippedChars, tree [0]);
    }

    return tree [0];
}

//
// Generate a tree of TextBlocks, marked as columns, lines, and words.
//
TextBlockPtr TextPage::split (TextChars& charsA, int rot) {
    TextBlockPtr blk;
    int xMinI, yMinI, xMaxI, yMaxI;
    int xMinI2, yMinI2, xMaxI2, yMaxI2;
    double nLines, vertGapThreshold, ascentAdjust, descentAdjust, minChunk;
    int horizGapSize, vertGapSize;
    double horizGapSize2, vertGapSize2;
    int minHorizChunkWidth, minVertChunkWidth, nHorizGaps, nVertGaps;
    double largeCharSize;
    int nLargeChars;
    bool doHorizSplit, doVertSplit, smallSplit;
    int i, x, y, prev, start;

    //
    // Compute minimum and maximum bbox, minimum and average font size:
    //
    double xMin = 0, yMin = 0, xMax = 0, yMax = 0;
    double minFontSize = 0, avgFontSize = 0;

    for (size_t i = 0; i < charsA.size (); ++i) {
        const auto& ch = charsA [i];

        if (i == 0 || ch->xmin < xMin) { xMin = ch->xmin; }
        if (i == 0 || ch->ymin < yMin) { yMin = ch->ymin; }
        if (i == 0 || ch->xmax > xMax) { xMax = ch->xmax; }
        if (i == 0 || ch->ymax > yMax) { yMax = ch->ymax; }

        avgFontSize += ch->size;

        if (i == 0 || ch->size < minFontSize) {
            minFontSize = ch->size;
        }
    }

    avgFontSize /= charsA.size ();

    //
    // Split precision is 5% of minimum font size:
    //
    double splitPrecision = splitPrecisionMul * minFontSize;

    if (splitPrecision < minSplitPrecision) {
        splitPrecision = minSplitPrecision;
    }

    //
    // The core algorithm for detecting the layout of text in a page is based on
    // static analysis of horizontal and vertical stripes of page. Stripes that
    // are not intersecting any characters are gaps and the size of the gaps is
    // interpreted as column, paragraph, and line separators.
    //

    //
    // Add some slack to the array bounds to avoid floating point precision
    // `problems':
    //
    xMinI = (int)floor (xMin / splitPrecision) - 1;
    yMinI = (int)floor (yMin / splitPrecision) - 1;
    xMaxI = (int)floor (xMax / splitPrecision) + 1;
    yMaxI = (int)floor (yMax / splitPrecision) + 1;

    std::vector< int > hprofile (yMaxI - yMinI + 1);
    std::vector< int > vprofile (xMaxI - xMinI + 1);

    for (auto& p : charsA) {
        const auto& c = *p;

        //
        // yMinI2 and yMaxI2 are adjusted to allow for slightly overlapping
        // lines
        //
        switch (rot) {
        case 0:
        default:
            xMinI2 = (int)floor (c.xmin / splitPrecision);
            xMaxI2 = (int)floor (c.xmax / splitPrecision);

            ascentAdjust = ascentAdjustFactor * (c.ymax - c.ymin);
            yMinI2 = (int)floor ((c.ymin + ascentAdjust) / splitPrecision);

            descentAdjust = descentAdjustFactor * (c.ymax - c.ymin);
            yMaxI2 = (int)floor ((c.ymax - descentAdjust) / splitPrecision);
            break;

        case 1:
            descentAdjust = descentAdjustFactor * (c.xmax - c.xmin);
            xMinI2 = (int)floor ((c.xmin + descentAdjust) / splitPrecision);

            ascentAdjust = ascentAdjustFactor * (c.xmax - c.xmin);
            xMaxI2 = (int)floor ((c.xmax - ascentAdjust) / splitPrecision);

            yMinI2 = (int)floor (c.ymin / splitPrecision);
            yMaxI2 = (int)floor (c.ymax / splitPrecision);
            break;

        case 2:
            xMinI2 = (int)floor (c.xmin / splitPrecision);
            xMaxI2 = (int)floor (c.xmax / splitPrecision);

            descentAdjust = descentAdjustFactor * (c.ymax - c.ymin);
            yMinI2 = (int)floor ((c.ymin + descentAdjust) / splitPrecision);

            ascentAdjust = ascentAdjustFactor * (c.ymax - c.ymin);
            yMaxI2 = (int)floor ((c.ymax - ascentAdjust) / splitPrecision);
            break;

        case 3:
            ascentAdjust = ascentAdjustFactor * (c.xmax - c.xmin);
            xMinI2 = (int)floor ((c.xmin + ascentAdjust) / splitPrecision);

            descentAdjust = descentAdjustFactor * (c.xmax - c.xmin);
            xMaxI2 = (int)floor ((c.xmax - descentAdjust) / splitPrecision);

            yMinI2 = (int)floor (c.ymin / splitPrecision);
            yMaxI2 = (int)floor (c.ymax / splitPrecision);
            break;
        }

        for (y = yMinI2; y <= yMaxI2; ++y) { ++hprofile [y - yMinI]; }
        for (x = xMinI2; x <= xMaxI2; ++x) { ++vprofile [x - xMinI]; }
    }

    //
    // Find the largest gaps in the horizontal and vertical profiles:
    //
    horizGapSize = 0;

    //
    // Skip initial empty slices:
    //
    for (start = yMinI; start < yMaxI && !hprofile [start - yMinI]; ++start) ;

    //
    // Scan the horizontal `profile':
    //
    for (y = start; y < yMaxI; ++y) {
        if (hprofile [y - yMinI] && !hprofile[y + 1 - yMinI]) {
            //
            // Potential start of a new horizontal gap:
            //
            start = y;
        }
        else if (!hprofile [y - yMinI] && hprofile [y + 1 - yMinI]) {
            //
            // End of a horizontal gap ...
            //
            if (y - start > horizGapSize) {
                //
                // ... if this was the largest gap so far, record it:
                //
                horizGapSize = y - start;
            }
        }
    }

    vertGapSize = 0;

    //
    // Skip initial empty slices:
    //
    for (start = xMinI; start < xMaxI && !vprofile[start - xMinI]; ++start) ;

    //
    // Scan the vertical `profile':
    //
    for (x = start; x < xMaxI; ++x) {
        if (vprofile [x - xMinI] && !vprofile [x + 1 - xMinI]) {
            //
            // Potential start of a new vertical gap:
            //
            start = x;
        }
        else if (!vprofile [x - xMinI] && vprofile [x + 1 - xMinI]) {
            //
            // End of a vertical gap ...
            //
            if (vertGapSize < x - start) {
                //
                // ... if this was the largest gap so far, record it:
                //
                vertGapSize = x - start;
            }
        }
    }

    //
    // {horiz,vert}GapSize2 is the largest gap size in __slices__, adjusted down
    // with a slack amount (20% of the font size):
    //
    horizGapSize2 = horizGapSize - splitGapSlack * avgFontSize / splitPrecision;

    if (horizGapSize2 < 0.99) {
        horizGapSize2 = 0.99;
    }

    vertGapSize2 = vertGapSize - splitGapSlack * avgFontSize / splitPrecision;

    if (vertGapSize2 < 0.99) {
        vertGapSize2 = 0.99;
    }

    //
    // Count all gaps that are `equivalent' to the (computed) largest gaps:
    //
    minHorizChunkWidth = yMaxI - yMinI;
    nHorizGaps = 0;

    //
    // Skip the initial empty slices:
    //
    for (start = yMinI; start < yMaxI && !hprofile [start - yMinI]; ++start) ;
    prev = start - 1;

    for (y = start; y < yMaxI; ++y) {
        if (hprofile [y - yMinI] && !hprofile [y + 1 - yMinI]) {
            start = y;
        }
        else if (!hprofile [y - yMinI] && hprofile [y + 1 - yMinI]) {
            if (y - start > horizGapSize2) {
                //
                // Count the gap:
                //
                ++nHorizGaps;

                if (minHorizChunkWidth > start - prev) {
                    //
                    // If the smalles gap so far, record it:
                    //
                    minHorizChunkWidth = start - prev;
                }

                prev = y;
            }
        }
    }

    minVertChunkWidth = xMaxI - xMinI;
    nVertGaps = 0;

    for (start = xMinI; start < xMaxI && !vprofile[start - xMinI]; ++start) ;
    prev = start - 1;

    for (x = start; x < xMaxI; ++x) {
        if (vprofile [x - xMinI] && !vprofile [x + 1 - xMinI]) {
            start = x;
        }
        else if (!vprofile [x - xMinI] && vprofile [x + 1 - xMinI]) {
            if (x - start > vertGapSize2) {
                //
                // Count the gap:
                //
                ++nVertGaps;

                if (minVertChunkWidth > start - prev) {
                    //
                    // If the smalles gap so far, record it:
                    //
                    minVertChunkWidth = start - prev;
                }

                prev = x;
            }
        }
    }

    //
    // Compute splitting parameters:
    //

    // approximation of number of lines in block
    if (fabs (avgFontSize) < 0.001) {
        nLines = 1;
    }
    else if (rot & 1) {
        nLines = (xMax - xMin) / avgFontSize;
    }
    else {
        nLines = (yMax - yMin) / avgFontSize;
    }

    //
    // Compute the minimum allowed vertical gap size (this is a horizontal gap)
    // threshold for rot ∈ { 1, 3 }
    //
    if (control.mode == textOutTableLayout) {
        vertGapThreshold = vertGapThresholdTableMax + vertGapThresholdTableSlope * nLines;
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

    //
    // Compute the minimum allowed chunk width:
    //
    if (control.mode == textOutTableLayout) {
        minChunk = 0;
    }
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

    for (i = 0; i < charsA.size (); ++i) {
        auto& ch = *charsA [i];

        if (ch.size > largeCharSize) {
            ++nLargeChars;
        }
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

    //
    // Split the block:
    //
    //~ this could use "other content" (vector graphics, rotated text) --
    //~ presence of other content in a gap means we should definitely split
    //

    if (doVertSplit) {
        //
        // Split vertically:
        //
        blk = std::make_shared< TextBlock > (blkVertSplit, rot);
        blk->smallSplit = smallSplit;

        for (start = xMinI; start < xMaxI && !vprofile[start - xMinI]; ++start) ;
        prev = start - 1;

        for (x = start; x < xMaxI; ++x) {
            if (vprofile [x - xMinI] && !vprofile [x + 1 - xMinI]) {
                start = x;
            }
            else if (!vprofile [x - xMinI] && vprofile [x + 1 - xMinI]) {
                if (x - start > vertGapSize2) {
                    auto chars2 = charsIn (
                        charsA,
                        xpdf::bbox_t{
                            ( prev + 0.5) * splitPrecision, yMin - 1,
                            (start + 1.5) * splitPrecision, yMax + 1
                        });

                    blk->addChild (split (chars2, rot));

                    prev = x;
                }
            }
        }

        auto chars2 = charsIn (
            charsA, xpdf::bbox_t{
                (prev + 0.5) * splitPrecision, yMin - 1, xMax + 1, yMax + 1
            });

        blk->addChild (split (chars2, rot));
    }
    else if (doHorizSplit) {
        //
        // Split horizontally:
        //
        blk = std::make_shared< TextBlock > (blkHorizSplit, rot);
        blk->smallSplit = smallSplit;

        for (start = yMinI; start < yMaxI && !hprofile[start - yMinI]; ++start) ;
        prev = start - 1;

        for (y = start; y < yMaxI; ++y) {
            if (hprofile[y - yMinI] && !hprofile[y + 1 - yMinI]) {
                start = y;
            }
            else if (!hprofile[y - yMinI] && hprofile[y + 1 - yMinI]) {
                if (y - start > horizGapSize2) {
                    auto chars2 = charsIn (
                        charsA, xpdf::bbox_t{
                            xMin - 1, ( prev + 0.5) * splitPrecision,
                            xMax + 1, (start + 1.5) * splitPrecision
                        });

                    blk->addChild (split (chars2, rot));

                    prev = y;
                }
            }
        }

        auto chars2 = charsIn (
            charsA, xpdf::bbox_t{
                xMin - 1, (prev + 0.5) * splitPrecision, xMax + 1, yMax + 1
            });

        blk->addChild (split (chars2, rot));
    }
    else if (nLargeChars > 0) {
        //
        // Split into larger and smaller chars:
        //
        TextChars chars2, chars3;

        for (i = 0; i < charsA.size (); ++i) {
            auto ch = charsA [i];

            if (ch->size > largeCharSize) {
                chars2.push_back (ch);
            }
            else {
                chars3.push_back (ch);
            }
        }

        blk = split (chars3, rot);
        insertLargeChars (chars2, blk);
    }
    else {
        //
        // Create a leaf node:
        //
        blk = std::make_shared< TextBlock > (blkLeaf, rot);

        for (auto& ch : charsA) {
            blk->addChild (ch);
        }
    }

    tagBlock (blk);

    return blk;
}

// Return the subset of chars inside a rectangle.
TextChars
TextPage::charsIn (TextChars& charsA, const xpdf::bbox_t& box) const {
    TextChars chars;

    const auto& [ x0, y0, x1, y1 ] = box.arr;

    for (auto& ch : charsA) {
        //
        // Because of {ascent,descent}AdjustFactor, the y coords (or x
        // coords for rot 1,3) for the gaps will be a little bit tight --
        // so we use the center of the character here:
        //
        double x = 0.5 * (ch->xmin + ch->xmax);
        double y = 0.5 * (ch->ymin + ch->ymax);

        if (x0 < x && x < x1 && y0 < y && y < y1) {
            chars.push_back (ch);
        }
    }

    return chars;
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
void TextPage::tagBlock (TextBlockPtr blk) {
    TextBlockPtr child;

    if (control.mode == textOutTableLayout) {
        if (blk->type == blkLeaf) {
            blk->tag = blkTagLine;
        }
        else if (blk->type == ((blk->rot & 1) ? blkHorizSplit : blkVertSplit) && blk->smallSplit) {
            blk->tag = blkTagLine;

            for (auto& block : blk->as_blocks ()) {
                if (block->tag != blkTagLine) {
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

    if (blk->type == blkLeaf) {
        blk->tag = blkTagLine;
    }
    else {
        if (blk->type == ((blk->rot & 1) ? blkVertSplit : blkHorizSplit)) {
            blk->tag = blkTagColumn;

            for (auto& block : blk->as_blocks ()) {
                if (block->tag != blkTagColumn &&
                    block->tag != blkTagLine) {
                    blk->tag = blkTagMulticolumn;
                    break;
                }
            }
        }
        else {
            if (blk->smallSplit) {
                blk->tag = blkTagLine;

                for (auto& block : blk->as_blocks ()) {
                    if (block->tag != blkTagLine) {
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
void TextPage::insertLargeChars (TextChars& largeChars, TextBlockPtr blk) {
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
    for (i = 0; i < largeChars.size (); ++i) {
        auto& ch2 = largeChars [i];

        if (ch2->xmax > xLimit || ch2->ymax > yLimit) {
            singleLine = false;
            break;
        }

        if (i > 0) {
            auto& ch = largeChars [i - 1];

            minOverlap = 0.5 * (ch->size < ch2->size ? ch->size : ch2->size);

            if (ch->ymax - ch2->ymin < minOverlap || ch2->ymax - ch->ymin < minOverlap) {
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
        for (auto& ch : largeChars | views::reverse) {
            insertLargeCharInLeaf (ch, blk);
        }
    }
}

//
// Find the first leaf (in depth-first order) in blk, and prepend a
// list of large chars.
//
void
TextPage::insertLargeCharsInFirstLeaf (TextChars& largeChars, TextBlockPtr blk) {
    if (blk->type == blkLeaf) {
        blk->prependChildren (largeChars.begin (), largeChars.end ());
    }
    else {
        insertLargeCharsInFirstLeaf (largeChars, blk->as_blocks ().front ());
        blk->updateBounds (0);
    }
}

// Find the leaf in <blk> where large char <ch> belongs, and prepend
// it.
void
TextPage::insertLargeCharInLeaf (TextCharPtr ch, TextBlockPtr blk) {
    //~ this currently works only for characters in the primary rotation
    //~ this currently just looks down the left edge of blk
    //~   -- it could be extended to do more

    // estimate the baseline of ch
    auto y = ch->ymin + 0.75 * (ch->ymax - ch->ymin);

    if (blk->type == blkLeaf) {
        blk->prependChild (ch);
    }
    else if (blk->type == blkHorizSplit) {
        auto& children = blk->as_blocks ();

        for (size_t i = 0; i < children.size (); ++i) {
            auto& child = children [i];

            if (y < child->yMax || i == children.size () - 1) {
                insertLargeCharInLeaf (ch, child);
                blk->updateBounds (i);
                break;
            }
        }
    }
    else {
        insertLargeCharInLeaf (ch, blk->as_blocks ().front ());
        blk->updateBounds (0);
    }
}

// Merge blk (rot != 0) into primaryTree (rot == 0).
void
TextPage::insertIntoTree (TextBlockPtr blk, TextBlockPtr primaryTree) {
    //
    // We insert a whole column at a time - so call insertIntoTree
    // recursively until we get to a column (or line):
    //
    if (blk->tag == blkTagMulticolumn) {
        auto& blocks = blk->as_blocks ();

        for (auto& block : blocks) {
            insertIntoTree (block, primaryTree);
        }
    }
    else {
        insertColumnIntoTree (blk, primaryTree);
    }
}

// Insert a column (as an atomic subtree) into tree.
// Requirement: tree is not a leaf node.
void
TextPage::insertColumnIntoTree (TextBlockPtr column, TextBlockPtr tree) {
    auto& blocks = tree->as_blocks ();

    for (auto& block : blocks) {
        if (block->tag == blkTagMulticolumn &&
            column->xMin >= block->xMin &&
            column->yMin >= block->yMin &&
            column->xMax <= block->xMax &&
            column->yMax <= block->yMax) {

            insertColumnIntoTree (column, block);
            tree->tag = blkTagMulticolumn;

            return;
        }
    }

    size_t i = 0;

    if (tree->type == blkVertSplit) {
        if (tree->rot == 1 || tree->rot == 2) {
            for (i = 0; i < blocks.size (); ++i) {
                auto& x = blocks [i];

                if (column->xMax > 0.5 * (x->xMin + x->xMax)) {
                    break;
                }
            }
        }
        else {
            for (i = 0; i < blocks.size (); ++i) {
                auto& x = blocks [i];

                if (column->xMin < 0.5 * (x->xMin + x->xMax)) {
                    break;
                }
            }
        }
    }
    else if (tree->type == blkHorizSplit) {
        if (tree->rot >= 2) {
            for (i = 0; i < blocks.size (); ++i) {
                auto& x = blocks [i];

                if (column->yMax > 0.5 * (x->yMin + x->yMax)) {
                    break;
                }
            }
        }
        else {
            for (i = 0; i < blocks.size (); ++i) {
                auto& x = blocks [i];

                if (column->yMin < 0.5 * (x->yMin + x->yMax)) {
                    break;
                }
            }
        }
    }
    else {
        // this should never happen
        return;
    }

    blocks.insert (blocks.begin () + i, column);
    tree->tag = blkTagMulticolumn;
}

// Insert clipped characters back into the TextBlock tree.
void
TextPage::insertClippedChars (TextChars& clippedChars, TextBlockPtr tree) {
    //~ this currently works only for characters in the primary rotation
    sort (clippedChars, lessX);

    while (!clippedChars.empty ()) {
        auto& ch = clippedChars.front ();

        // TODO: O(N)
        clippedChars.erase (clippedChars.begin ());

        if (ch->rot != 0) {
            continue;
        }

        TextBlockPtr leaf = findClippedCharLeaf (ch, tree);

        if (!leaf) {
            continue;
        }

        leaf->addChild (ch);

        for (size_t i = 0; i < clippedChars.size (); ) {
            auto& ch2 = clippedChars [i];

            if (ch2->xmin > ch->xmax + clippedTextMaxWordSpace * ch->size) {
                break;
            }

            double y = 0.5 * (ch2->ymin + ch2->ymax);

            if (y > leaf->yMin && y < leaf->yMax) {
                auto& ch2 = clippedChars [i];

                // TODO: O(N)
                clippedChars.erase (clippedChars.begin () + i);
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
TextBlockPtr
TextPage::findClippedCharLeaf (TextCharPtr ch, TextBlockPtr tree) {
    //~ this currently works only for characters in the primary rotation

    double y = 0.5 * (ch->ymin + ch->ymax);

    if (tree->type == blkLeaf) {
        if (tree->rot == 0) {
            if (y > tree->yMin && y < tree->yMax &&
                ch->xmin <= tree->xMax + clippedTextMaxWordSpace * ch->size) {
                return tree;
            }
        }
    }
    else {
        auto& xs = std::get< TextBlocks > (tree->xs);

        for (auto& x : xs) {
            auto p = findClippedCharLeaf (ch, x);

            if (p) {
                return p;
            }
        }
    }

    return { };
}

TextColumnPtr TextPage::buildColumn (TextBlockPtr blk) {
    TextParagraphs paragraphs;
    double spaceThresh, indent0, indent1, fontSize0, fontSize1;
    int i;

    TextLines lines;
    makeLines (blk, lines);

    spaceThresh = paragraphSpacingThreshold * getAverageLineSpacing (lines);

    //~ could look for bulleted lists here: look for the case where
    //~   all out-dented lines start with the same char

    // build the paragraphs
    for (i = 0; i < lines.size (); ) {
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

        paragraphs.push_back (std::make_shared< TextParagraph > (parLines));
    }

    return std::make_shared< TextColumn > (
        std::move (paragraphs),
        blk->xMin, blk->yMin, blk->xMax, blk->yMax);
}

//
// (TODO) Flatten the tree of TextBlocks into a list of TextColumns:
//
TextColumns TextPage::buildColumns (TextBlockPtr tree) {
    if (!tree) {
        return { };
    }

    switch (tree->tag) {
    case blkTagLine:
    case blkTagColumn:
        return TextColumns{ buildColumn (tree) };

    case blkTagMulticolumn: {
        TextColumns columns;

        for (auto& block : tree->as_blocks ()) {
            TextColumns other = buildColumns (block);
            columns.insert (
                columns.end (),
                std::make_move_iterator (other.begin ()),
                std::make_move_iterator (other.end ()));
        }

        return columns;
    }

    default:
        ASSERT (0);
        break;
    }
}

double
TextPage::getLineIndent (const TextLine& line, TextBlockPtr blk) const {
    double indent;

    switch (line.rot) {
    case 0:
    default: indent = line.xmin - blk->xMin; break;
    case 1:  indent = line.ymin - blk->yMin; break;
    case 2:  indent = blk->xMax - line.xmax; break;
    case 3:  indent = blk->yMax - line.ymax; break;
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
    default: sp = rhs.ymin - lhs.ymax; break;
    case 1:  sp = lhs.xmin - rhs.xmax; break;
    case 2:  sp = lhs.ymin - rhs.ymin; break;
    case 3:  sp = rhs.xmin - rhs.xmax; break;
    }

    return sp;
}

void TextPage::makeLines (TextBlockPtr blk, TextLines& lines) {
    switch (blk->tag) {
    case blkTagLine: {
        auto line = makeLine (blk);

        if (blk->rot == 1 || blk->rot == 2) {
            //
            // In 90° and 180° orientations, insert at front:
            //
            lines.insert (lines.begin (), line);
        }
        else {
            lines.push_back (line);
        }
    }
        break;

    case blkTagColumn:
    case blkTagMulticolumn:
        //
        // Multi-column should never happen here:
        //
        for (auto& x : blk->as_blocks ()) {
            makeLines (x, lines);
        }

        break;
    }
}

TextLinePtr
TextPage::makeLine (TextBlockPtr blk) {
    double wordSp, lineFontSize, sp;
    bool spaceAfter, spaceAfter2;

    TextChars charsA = getLineOfChars (blk);
    TextWords words;

    wordSp = computeWordSpacingThreshold (charsA, blk->rot);

    lineFontSize = 0;
    spaceAfter = false;

    for (size_t i = 0, j; i < charsA.size ();) {
        sp = wordSp - 1;

        for (j = i + 1; j < charsA.size (); ++j) {
            auto& ch  = charsA [j - 1];
            auto& ch2 = charsA [j];

            sp = (blk->rot & 1)
                ? (ch2->ymin - ch->ymax)
                : (ch2->xmin - ch->xmax);

            if (sp > wordSp || ch->font != ch2->font || fabs (ch->size - ch2->size) > 0.01 ||
                (control.mode == textOutRawOrder && ch2->charPos != ch->charPos + ch->charLen)) {
                break;
            }

            sp = wordSp - 1;
        }

        spaceAfter2 = spaceAfter;
        spaceAfter = sp > wordSp;

        auto word = std::make_shared< TextWord > (
            charsA, i, j - i, int (blk->rot), (blk->rot >= 2) ? spaceAfter2 : spaceAfter);

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

    return std::make_shared< TextLine > (
        std::move (words),
        blk->xMin, blk->yMin,
        blk->xMax, blk->yMax,
        lineFontSize);
}

TextChars
TextPage::getLineOfChars (TextBlockPtr blk) {
    if (blk->type == blkLeaf) {
        return std::get< TextChars > (blk->xs);
    }
    else {
        TextChars chars;

        for (auto& block : blk->as_blocks ()) {
            auto line = getLineOfChars (block);
            chars.insert (
                chars.end (),
                std::make_move_iterator (line.begin ()),
                std::make_move_iterator (line.end ()));
        }

        return chars;
    }
}

// Compute the inter-word spacing threshold for a line of chars.
// Spaces greater than this threshold will be considered inter-word
// spaces.
double TextPage::computeWordSpacingThreshold (TextChars& charsA, int rot) {
    double avgFontSize = 0, minSp = 0, maxSp = 0, sp = 0;

    for (size_t i = 0; i < charsA.size (); ++i) {
        auto& ch = charsA [i];
        avgFontSize += ch->size;

        if (i < charsA.size () - 1) {
            auto& ch2 = charsA [i];

            sp = (rot & 1)
                ? (ch2->ymin - ch->ymax)
                : (ch2->xmin - ch->xmax);

            if (i == 0 || sp < minSp) {
                minSp = sp;
            }

            if (sp > maxSp) {
                maxSp = sp;
            }
        }
    }

    avgFontSize /= charsA.size ();

    if (minSp < 0) {
        minSp = 0;
    }

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

int TextPage::assignPhysLayoutPositions (TextColumns& columns) {
    assignLinePhysPositions (columns);
    return assignColumnPhysPositions (columns);
}

// Assign a physical x coordinate for each TextLine (relative to the
// containing TextColumn).  This also computes TextColumn width and
// height.
void TextPage::assignLinePhysPositions (TextColumns& columns) {
    UnicodeMap* uMap;

    if (!(uMap = globalParams->getTextEncoding ())) {
        return;
    }

    for (auto& col : columns) {
        col->pw = col->ph = 0;

        for (auto& par : col->paragraphs) {
            for (auto& line : par->lines) {
                computeLinePhysWidth (*line, uMap);

                if (control.fixedPitch > 0) {
                    line->px = (line->xmin - col->xmin) / control.fixedPitch;
                }
                else if (fabs (line->fontSize) < 0.001) {
                    line->px = 0;
                }
                else {
                    line->px =
                        (line->xmin - col->xmin) /
                        (physLayoutSpaceWidth * line->fontSize);
                }

                if (line->px + line->pw > col->pw) {
                    col->pw = line->px + line->pw;
                }
            }

            col->ph += par->lines.size ();
        }

        col->ph += col->paragraphs.size () - 1;
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
int TextPage::assignColumnPhysPositions (TextColumns& columns) {
    double slack, xOverlap, yOverlap;
    int ph, i, j;

    if (control.mode == textOutTableLayout) { slack = tableCellOverlapSlack; }
    else {
        slack = 0;
    }

    // assign x positions
    sort (columns, lessX);

    for (i = 0; i < columns.size (); ++i) {
        auto& col = columns [i];

        if (control.fixedPitch) {
            col->px = (int)(col->xmin / control.fixedPitch);
        }
        else {
            col->px = 0;
            for (j = 0; j < i; ++j) {
                auto& col2 = columns [j];
                xOverlap = col2->xmax - col->xmin;
                if (xOverlap < slack * (col2->xmax - col2->xmin)) {
                    if (col2->px + col2->pw + 2 > col->px) {
                        col->px = col2->px + col2->pw + 2;
                    }
                }
                else {
                    yOverlap =
                        (col->ymax < col2->ymax ? col->ymax : col2->ymax) -
                        (col->ymin > col2->ymin ? col->ymin : col2->ymin);
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

    sort (columns, lessY);

    // assign y positions
    for (ph = 0, i = 0; i < columns.size (); ++i) {
        auto& col = columns [i];
        col->py = 0;

        for (j = 0; j < i; ++j) {
            auto& col2 = columns [j];
            yOverlap = col2->ymax - col->ymin;
            if (yOverlap < slack * (col2->ymax - col2->ymin)) {
                if (col2->py + col2->ph + 1 > col->py) {
                    col->py = col2->py + col2->ph + 1;
                }
            }
            else {
                xOverlap =
                    (col->xmax < col2->xmax ? col->xmax : col2->xmax) -
                    (col->xmin > col2->xmin ? col->xmin : col2->xmin);

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

        if (col->py + col->ph > ph) {
            ph = col->py + col->ph;
        }
    }

    return ph;
}

void TextPage::generateUnderlinesAndLinks (TextColumns& columns) {
    double base, uSlack, ubSlack, hSlack;

    for (auto& col : columns) {
        for (auto& par : col->paragraphs) {
            for (auto& line : par->lines) {
                for (auto& word : line->words) {
                    base = word->getBaseline ();

                    uSlack  = underlineSlack * word->fontSize;
                    ubSlack = underlineBaselineSlack * word->fontSize;
                    hSlack  = hyperlinkSlack * word->fontSize;

                    // handle underlining
                    for (auto& underline : underlines) {
                        if (underline->horiz) {
                            if (word->rot == 0 || word->rot == 2) {
                                if (fabs (underline->y0 - base) < ubSlack &&
                                    underline->x0 < word->xmin + uSlack &&
                                    word->xmax - uSlack < underline->x1) {
                                    word->underlined = true;
                                }
                            }
                        }
                        else {
                            if (word->rot == 1 || word->rot == 3) {
                                if (fabs (underline->x0 - base) < ubSlack &&
                                    underline->y0 < word->ymin + uSlack &&
                                    word->ymax - uSlack < underline->y1) {
                                    word->underlined = true;
                                }
                            }
                        }
                    }

                    // handle links
                    for (auto& link : links) {
                        if (link->xMin < word->xmin + hSlack &&
                            word->xmax - hSlack < link->xMax &&
                            link->yMin < word->ymin + hSlack &&
                            word->ymax - hSlack < link->yMax) {
                        }
                    }
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////

inline std::wstring to_wstring (Unicode* p, size_t n) {
    std::wstring w;
    transform (p, p + n, back_inserter (w), [](auto c) { return wchar_t (c); });
    return w;
}

inline std::string to_string (const std::wstring& wstr) {
    std::string s;
    transform (wstr, back_inserter (s), [](auto c) { return char (c); });
    return s;
}

inline auto reading_order_of (int rotation) {
    using namespace xpdf;

    switch (rotation) {
    default:
    case 0: return reading_order< rotation_t::none, char_t >;
    case 1: return reading_order< rotation_t::quarter_turn, char_t >;
    case 2: return reading_order< rotation_t::quarter_turn, char_t >;
    case 3: return reading_order< rotation_t::three_quarters_turn, char_t >;
    }
};

std::vector< xpdf::bbox_t >
search_all (const std::vector< char_t >& cs, std::wregex& regex) {
    std::wstring wstr;
    transform (cs, back_inserter (wstr), &char_t::value);

    auto iter = std::wsregex_iterator (wstr.begin (), wstr.end (), regex);
    auto last = std::wsregex_iterator ();

    std::vector< xpdf::bbox_t > xs;

    for (; iter != last; ++iter) {
        auto match = (*iter) [0];

        auto pos = std::distance (wstr.cbegin (), match.first);
        auto len = match.length ();

        xs.push_back (
            accumulate (
                cs.begin () + pos, cs.begin () + pos + len,
                cs [pos].box, std::plus< xpdf::bbox_t > { },
                &char_t::box));
    }

    return xs;
}

bool TextPage::findText (
    Unicode* p, int len,
    bool startAtTop,  bool stopAtBottom, bool startAtLast, bool stopAtLast,
    bool caseSensitive, bool backward, bool wholeWord,
    xpdf::bbox_t& box) {

    std::wregex regex (to_wstring (p, len));

    auto rotated_by = [](int rot) {
        return views::filter ([=](auto& ch) { return ch->rot == rot; });
    };

    std::vector< xpdf::bbox_t > boxes;

    for (int rot : { 0, 1, 2, 3 }) {
        std::vector< char_t > cs;

        transform (chars | rotated_by (rot), back_inserter (cs), [](auto& ch) {
            return make_char (*ch);
        });

        sort (cs, reading_order_of (rot));

        auto matches = search_all (cs, regex);
        boxes.insert (boxes.end (), matches.begin (), matches.end ());
    }

    sort (boxes, reading_order< xpdf::rotation_t::none, xpdf::bbox_t >);

    xpdf::bbox_t search_area;

    {
        double x0, y0, x1, y1;

        if (startAtTop) {
            x0 = y0 = 0;
        }
        else {
            if (startAtLast && haveLastFind) {
                x0 = lastFindXMin;
                y0 = lastFindYMin;
            }
            else {
                x0 = box.point [0].x;
                y0 = box.point [0].y;
            }
        }

        if (stopAtBottom) {
            x1 = y1 = (std::numeric_limits< double >::max) ();
        }
        else {
            if (stopAtLast && haveLastFind) {
                x1 = lastFindXMin;
                y1 = lastFindYMin;
            }
            else {
                x1 = box.point [1].x;
                y1 = box.point [1].y;
            }
        }

        search_area = xpdf::bbox_t{ x0, y0, x1, y1 };
    }

    auto iter2 = find_if (boxes, [&](auto& box) {
        return
            box.point [0].y  > search_area.point [0].y ||
            box.point [0].y == search_area.point [0].y &&
            box.point [0].x  > search_area.point [0].x;
    });

    if (iter2 != boxes.end ()) {
        box = *iter2;

        auto& corner = box.point [0];

        lastFindXMin = corner.x;
        lastFindYMin = corner.y;

        return haveLastFind = true;
    }

    return false;
}

GString*
TextPage::getText (const xpdf::bbox_t& box) {
    UnicodeMap* uMap;
    char space[8], eol[16];
    int spaceLen, eolLen;
    GString** out;
    int* outLen;
    bool primaryLR;
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
    TextChars chars2;

    for (auto& ch : chars) {
        xx = 0.5 * (ch->xmin + ch->xmax);
        yy = 0.5 * (ch->ymin + ch->ymax);

        if (box.arr [0] < xx && xx < box.arr [2] &&
            box.arr [1] < yy && yy < box.arr [3]) {
            chars2.push_back (ch);
        }
    }

    rot = rotateChars (chars2);
    primaryLR = isPrevalentLeftToRight (chars2);

    TextColumns columns;

    {
        auto tree = splitChars (chars2);

        if (!tree) {
            unrotateChars (chars2, rot);
            return new GString ();
        }

        columns = buildColumns (tree);
    }

    ph = assignPhysLayoutPositions (columns);

    unrotateChars (chars2, rot);

    out = (GString**)calloc (ph, sizeof (GString*));
    outLen = (int*)calloc (ph, sizeof (int));

    for (i = 0; i < ph; ++i) {
        out[i] = NULL;
        outLen[i] = 0;
    }

    sort (columns, lessPosX);

    for (colIdx = 0; colIdx < columns.size (); ++colIdx) {
        auto& col = columns [colIdx];
        y = col->py;

        for (parIdx = 0; parIdx < col->paragraphs.size () && y < ph; ++parIdx) {
            auto& par = col->paragraphs [parIdx];

            for (lineIdx = 0; lineIdx < par->lines.size () && y < ph; ++lineIdx) {
                auto& line = par->lines [lineIdx];

                if (!out[y]) {
                    out[y] = new GString;
                }

                while (outLen[y] < col->px + line->px) {
                    out[y]->append (space, spaceLen);
                    ++outLen[y];
                }

                encodeFragment (line->text.data (), line->len, uMap, primaryLR, out[y]);
                outLen[y] += line->pw;

                ++y;
            }

            if (parIdx + 1 < col->paragraphs.size ()) {
                ++y;
            }
        }
    }

    ret = new GString;

    for (i = 0; i < ph; ++i) {
        if (out[i]) {
            ret->append (*out[i]);
            delete out[i];
        }
        if (ph > 1) { ret->append (eol, eolLen); }
    }

    free (out);
    free (outLen);

    uMap->decRefCnt ();

    return ret;
}

TextWords
TextPage::makeWordList () {
    int rot = rotateChars (chars);

    auto tree = splitChars (chars);

    if (!tree) {
        unrotateChars (chars, rot);
        return { };
    }

    auto columns = buildColumns (tree);

    unrotateChars (chars, rot);

    TextWords words;

    for (auto& col : columns) {
        for (auto& par : col->paragraphs) {
            for (auto& line : par->lines) {
                for (auto& word : line->words) {
                    words.push_back (word);
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
        sort (words, lessYX);
        break;

    case textOutRawOrder:
        sort (words, lessCharPos);
        break;
    }

    // this has to be done after sorting with cmpYX
    unrotateColumns (columns, rot);
    unrotateWords (words, rot);

    return words;
}

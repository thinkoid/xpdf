// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

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

template< typename T >
inline bool lessX (const T& lhs, const T& rhs) {
    return lhs->box.xmin < rhs->box.xmin;
};

template< typename T >
inline bool lessY (const T& lhs, const T& rhs) {
    return lhs->box.ymin < rhs->box.ymin;
};

template< typename T >
inline bool lessYX (const T& lhs, const T& rhs) {
    return
        lhs->box.ymin  < rhs->box.ymin || (
        lhs->box.ymin == rhs->box.ymin && lhs->box.xmin < rhs->box.xmin);
};

//
// X-coordinate column position comparison object:
//
inline bool
lessPosX (const TextColumnPtr& lhs, const TextColumnPtr& rhs) {
    return lhs->px < rhs->px;
};

inline bool
lessCharPos (const TextWordPtr& lhs, const TextWordPtr& rhs) {
    return lhs->charPos [0] < rhs->charPos [0];
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

//
// Add a character to the TextPage text. Process `ActualText' spans separately:
//
void TextPage::addChar (
    GfxState* state, double x, double y, double dx, double dy, CharCode c,
    int nBytes, Unicode* u, int uLen) {
    double x1, y1, x2, y2, w1, h1, dx2, dy2, ascent, descent, sp;
    double xMin, yMin, xMax, yMax;
    double clipXMin, clipYMin, clipXMax, clipYMax;
    GfxRGB rgb;
    bool clipped, rtl;
    int i, j;

    if (actualText) {
        //
        // If we're in an ActualText span, save the position info (the
        // ActualText chars will be added by TextPage::endActualText):
        //
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
    if (actualText) {
        //
        // ActualText cannot be nested:
        //
        free (actualText);
    }

    actualText = (Unicode*)calloc (uLen, sizeof (Unicode));
    memcpy (actualText, u, uLen * sizeof (Unicode));

    actualTextLen = uLen;
    actualTextNBytes = 0;
}

void TextPage::endActualText (GfxState* state) {
    Unicode* u = actualText;

    //
    // Zero, such that calling TextPage::addChar will not add to ActualText
    // text:
    //
    actualText = 0;

    if (actualTextNBytes) {
        //
        // We have the position info for all of the text inside the marked
        // content span, we feed the `ActualText' back through TextPage::addChar:
        //
        addChar (
            state,
            actualTextX0, actualTextY0,
            actualTextX1 - actualTextX0,
            actualTextY1 - actualTextY0,
            0, actualTextNBytes,
            u, actualTextLen);

    }

    free (u);

    actualText = 0;
    actualTextLen = 0;

    actualTextNBytes = false;
}

void TextPage::addUnderline (double x0, double y0, double x1, double y1) {
    underlines.push_back (TextUnderline{ x0, y0, x1, y1 });
}

void TextPage::addLink (
    double xMin, double yMin, double xMax, double yMax, Link* link) {

    if (link && link->getAction () && link->getAction ()->getKind () == actionURI) {
        std::string s (*((LinkURI*)link->getAction ())->getURI ());
        links.push_back (TextLink{ { xMin, yMin, xMax, yMax }, s });
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

template< typename T >
inline void
do_rotate (T& t, double x0, double y0, double x1, double y1) {
    t.box = { x0, y0, x1, y1 };
}

template< typename T >
inline void
rotate90 (T& t, int w, int) {
    const auto& [ x0, y0, x1, y1 ] = t.box.arr;
    do_rotate (t, y0, w - x1, y1, w - x0);
};

template< typename T >
inline void
rotate180 (T& t, int w, int h) {
    const auto& [ x0, y0, x1, y1 ] = t.box.arr;
    do_rotate (t, w - x1, h - y1, w - x0, h - y0);
}

template< typename T >
inline void
rotate270 (T& t, int, int h) {
    const auto& [ x0, y0, x1, y1 ] = t.box.arr;
    do_rotate (t, h - y1, x0, h - y0, x1);
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
        for (auto& ch : chars) {
            rotate90 (*ch, pageWidth, 0);
            ch->rot = (ch->rot + 3) & 3;
        }
        std::swap (pageWidth, pageHeight);
        break;

    case 2:
        for (auto& ch : chars) {
            rotate180 (*ch, pageWidth, pageHeight);
            ch->rot = (ch->rot + 2) & 3;
        }
        break;

    case 3:
        for (auto& ch : chars) {
            rotate270 (*ch, 0, pageHeight);
            ch->rot = (ch->rot + 1) & 3;
        }
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
    switch (rot) {
    case 1:
        for (auto& underline : underlines) {
            rotate90 (underline, pageWidth, 0);
        }

        for (auto& link : links) {
            rotate90 (link, pageWidth, 0);
        }
        break;

    case 2:
        for (auto& underline : underlines) {
            rotate180 (underline, pageWidth, pageHeight);
        }

        for (auto& link : links) {
            rotate180 (link, pageWidth, pageHeight);
        }
        break;

    case 3:
        for (auto& underline : underlines) {
            rotate270 (underline, 0, pageHeight);
        }

        for (auto& link : links) {
            rotate270 (link, 0, pageHeight);
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
    t.box = { x0, y0, x1, y1 };
    t.rot = (t.rot + n) & 3;
}

template< >
inline void do_unrotate< TextColumn > (
    TextColumn& t, double x0, double y0, double x1, double y1, int) {
    t.box = { x0, y0, x1, y1 };
}

template< >
inline void do_unrotate< TextParagraph > (
    TextParagraph& t, double x0, double y0, double x1, double y1, int) {
    t.box = { x0, y0, x1, y1 };
}

template< typename T >
inline void unrotate90 (T& t, int w, int) {
    const auto& [ x0, y0, x1, y1 ] = t.box.arr;
    do_unrotate (t, w - y1, x0, w - y0, x1, 1);
};

template< typename T >
inline void unrotate180 (T& t, int w, int h) {
    const auto& [ x0, y0, x1, y1 ] = t.box.arr;
    do_unrotate (t, w - x1, h - y1, w - x0, h - y0, 2);
}

template< typename T >
inline void unrotate270 (T& t, int, int h) {
    const auto& [ x0, y0, x1, y1 ] = t.box.arr;
    do_unrotate (t, y0, h - x1, y1, h - x0, 3);
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
    return a.c == b.c
        && fabs (a.box.xmin - b.box.xmin) < x
        && fabs (a.box.ymax - b.box.ymax) < y;
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

                if (b.box.ymin - a.box.ymin >= ydelta) {
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

                if (b.box.xmin - a.box.xmin >= xdelta) {
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
                sort (chars2, lessY< TextCharPtr >);
            }
            else {
                sort (chars2, lessX< TextCharPtr >);
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
    xpdf::bbox_t box = charsA [0]->box;
    double minFontSize = 0, avgFontSize = 0;

    for (size_t i = 0; i < charsA.size (); ++i) {
        const auto& ch = charsA [i];
        box = coalesce (box, ch->box);

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
    xMinI = (int)floor (box.xmin / splitPrecision) - 1;
    yMinI = (int)floor (box.ymin / splitPrecision) - 1;
    xMaxI = (int)floor (box.xmax / splitPrecision) + 1;
    yMaxI = (int)floor (box.ymax / splitPrecision) + 1;

    std::vector< int > hprofile (yMaxI - yMinI + 1);
    std::vector< int > vprofile (xMaxI - xMinI + 1);

    for (auto& ch : charsA) {
        const auto& [x0, y0, x1, y1] = ch->box.arr;

        //
        // yMinI2 and yMaxI2 are adjusted to allow for slightly overlapping
        // lines
        //
        switch (rot) {
        case 0:
        default:
            xMinI2 = (int)floor (x0 / splitPrecision);
            xMaxI2 = (int)floor (x1 / splitPrecision);

            ascentAdjust = ascentAdjustFactor * (y1 - y0);
            yMinI2 = (int)floor ((y0 + ascentAdjust) / splitPrecision);

            descentAdjust = descentAdjustFactor * (y1 - y0);
            yMaxI2 = (int)floor ((y1 - descentAdjust) / splitPrecision);
            break;

        case 1:
            descentAdjust = descentAdjustFactor * (x1 - x0);
            xMinI2 = (int)floor ((x0 + descentAdjust) / splitPrecision);

            ascentAdjust = ascentAdjustFactor * (x1 - x0);
            xMaxI2 = (int)floor ((x1 - ascentAdjust) / splitPrecision);

            yMinI2 = (int)floor (y0 / splitPrecision);
            yMaxI2 = (int)floor (y1 / splitPrecision);
            break;

        case 2:
            xMinI2 = (int)floor (x0 / splitPrecision);
            xMaxI2 = (int)floor (x1 / splitPrecision);

            descentAdjust = descentAdjustFactor * (y1 - y0);
            yMinI2 = (int)floor ((y0 + descentAdjust) / splitPrecision);

            ascentAdjust = ascentAdjustFactor * (y1 - y0);
            yMaxI2 = (int)floor ((y1 - ascentAdjust) / splitPrecision);
            break;

        case 3:
            ascentAdjust = ascentAdjustFactor * (x1 - x0);
            xMinI2 = (int)floor ((x0 + ascentAdjust) / splitPrecision);

            descentAdjust = descentAdjustFactor * (x1 - x0);
            xMaxI2 = (int)floor ((x1 - descentAdjust) / splitPrecision);

            yMinI2 = (int)floor (y0 / splitPrecision);
            yMaxI2 = (int)floor (y1 / splitPrecision);
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
        nLines = width_of (box) / avgFontSize;
    }
    else {
        nLines = height_of (box) / avgFontSize;
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
                            ( prev + 0.5) * splitPrecision, box.ymin - 1,
                            (start + 1.5) * splitPrecision, box.ymax + 1
                        });

                    blk->addChild (split (chars2, rot));

                    prev = x;
                }
            }
        }

        auto chars2 = charsIn (
            charsA, xpdf::bbox_t{
                (prev + 0.5) * splitPrecision,
                    box.ymin - 1,
                    box.xmax + 1,
                    box.ymax + 1
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
                            box.xmin - 1, ( prev + 0.5) * splitPrecision,
                            box.xmax + 1, (start + 1.5) * splitPrecision
                        });

                    blk->addChild (split (chars2, rot));

                    prev = y;
                }
            }
        }

        auto chars2 = charsIn (
            charsA, xpdf::bbox_t{
                box.xmin - 1, (prev + 0.5) * splitPrecision,
                box.xmax + 1, box.ymax + 1
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
    const auto& a = box;

    TextChars chars;

    for (auto& ch : charsA) {
        const auto& b = ch->box;

        //
        // Because of {ascent,descent}AdjustFactor, the y coords (or x
        // coords for rot 1,3) for the gaps will be a little bit tight --
        // so we use the center of the character here:
        //

        double x = (b.xmin + b.xmax) / 2;
        double y = (b.ymin + b.ymax) / 2;

        if (a.xmin < x && x < a.xmax && a.ymin < y && y < a.ymax) {
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

void
TextPage::doInsertLargeChars (TextChars& chars, TextBlockPtr pblock) {
    //~ this currently works only for characters in the primary rotation

    //
    // Check to see if the large chars are a single line, in the
    // upper-left corner of block (this is just a rough estimate):
    //
    const auto [ xLimit, yLimit ] = center_of (pblock->box);

    bool singleLine = true;

    for (auto iter = chars.begin (); iter != chars.end (); ++iter) {
        auto& lhs = (*iter)->box;

        if (lhs.point [1].x > xLimit || lhs.point [1].y > yLimit) {
            //
            // This heuristic fails if the bottom right corner of the large
            // character is past the center of the block:
            //
            singleLine = false;
            break;
        }

        auto next = iter;
        std::advance (next, 1);

        if (next != chars.end ()) {
            auto& rhs = (*next)->box;

            if (vertical_overlap (lhs, rhs) < min_height_of (lhs, rhs) / 2) {
                //
                // This heuristic fails if the overlap between the two adjacent
                // characters in the large chars array is less than half the
                // smalest of the two:
                //
                singleLine = false;
                break;
            }
        }
    }

    if (singleLine) {
        insertLargeCharsInFirstLeaf (chars, pblock);
    }
    else {
        for (auto& ch : chars | views::reverse) {
            insertLargeCharInLeaf (ch, pblock);
        }
    }
}

//
// Insert a list of large characters (like large drop caps) into a tree:
//
void
TextPage::insertLargeChars (TextChars& chars, TextBlockPtr pblock) {
    switch (chars.size ()) {
    case 0:
        break;

    case 1:
        insertLargeCharsInFirstLeaf (chars, pblock);
        break;

    default:
        doInsertLargeChars (chars, pblock);
        break;
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
    const auto ylimit = ch->box.xmin + 0.75 * height_of (ch->box);

    if (blk->type == blkLeaf) {
        blk->prependChild (ch);
    }
    else if (blk->type == blkHorizSplit) {
        auto& children = blk->as_blocks ();

        for (size_t i = 0; i < children.size (); ++i) {
            auto& child = children [i];

            if (ylimit < child->box.ymax || i == children.size () - 1) {
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
            column->box.xmin >= block->box.xmin &&
            column->box.ymin >= block->box.ymin &&
            column->box.xmax <= block->box.xmax &&
            column->box.ymax <= block->box.ymax) {

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

                if (column->box.xmax > 0.5 * (x->box.xmin + x->box.xmax)) {
                    break;
                }
            }
        }
        else {
            for (i = 0; i < blocks.size (); ++i) {
                auto& x = blocks [i];

                if (column->box.xmin < 0.5 * (x->box.xmin + x->box.xmax)) {
                    break;
                }
            }
        }
    }
    else if (tree->type == blkHorizSplit) {
        if (tree->rot >= 2) {
            for (i = 0; i < blocks.size (); ++i) {
                auto& x = blocks [i];

                if (column->box.ymax > 0.5 * (x->box.ymin + x->box.ymax)) {
                    break;
                }
            }
        }
        else {
            for (i = 0; i < blocks.size (); ++i) {
                auto& x = blocks [i];

                if (column->box.ymin < 0.5 * (x->box.ymin + x->box.ymax)) {
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
    sort (clippedChars, lessX< TextCharPtr >);

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

            if (ch2->box.xmin > ch->box.xmax + clippedTextMaxWordSpace * ch->size) {
                break;
            }

            double y = 0.5 * (ch2->box.ymin + ch2->box.ymax);

            if (y > leaf->box.ymin && y < leaf->box.ymax) {
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

    double y = 0.5 * (ch->box.ymin + ch->box.ymax);

    if (tree->type == blkLeaf) {
        if (tree->rot == 0) {
            if (y > tree->box.ymin && y < tree->box.ymax &&
                ch->box.xmin <= tree->box.xmax + clippedTextMaxWordSpace * ch->size) {
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
        blk->box.xmin, blk->box.ymin, blk->box.xmax, blk->box.ymax);
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
    default: indent = line.box.xmin - blk->box.xmin; break;
    case 1:  indent = line.box.ymin - blk->box.ymin; break;
    case 2:  indent = blk->box.xmax - line.box.xmax; break;
    case 3:  indent = blk->box.ymax - line.box.ymax; break;
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
    default: sp = rhs.box.ymin - lhs.box.ymax; break;
    case 1:  sp = lhs.box.xmin - rhs.box.xmax; break;
    case 2:  sp = lhs.box.ymin - rhs.box.ymin; break;
    case 3:  sp = rhs.box.xmin - rhs.box.xmax; break;
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

            sp = (blk->rot & 1) ? height_of (ch2->box) : width_of (ch->box);

            if (sp > wordSp || ch->font != ch2->font ||
                fabs (ch->size - ch2->size) > 0.01 ||
                (control.mode == textOutRawOrder &&
                 ch2->charPos != ch->charPos + ch->charLen)) {
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
        blk->box.xmin, blk->box.ymin,
        blk->box.xmax, blk->box.ymax,
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

            sp = (rot & 1) ? height_of (ch2->box) : width_of (ch2->box);

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
                    line->px = (line->box.xmin - col->box.xmin) / control.fixedPitch;
                }
                else if (fabs (line->fontSize) < 0.001) {
                    line->px = 0;
                }
                else {
                    line->px =
                        (line->box.xmin - col->box.xmin) /
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
    sort (columns, lessX< TextColumnPtr >);

    for (i = 0; i < columns.size (); ++i) {
        auto& col = columns [i];

        if (control.fixedPitch) {
            col->px = (int)(col->box.xmin / control.fixedPitch);
        }
        else {
            col->px = 0;
            for (j = 0; j < i; ++j) {
                auto& col2 = columns [j];
                xOverlap = col2->box.xmax - col->box.xmin;
                if (xOverlap < slack * (col2->box.xmax - col2->box.xmin)) {
                    if (col2->px + col2->pw + 2 > col->px) {
                        col->px = col2->px + col2->pw + 2;
                    }
                }
                else {
                    yOverlap =
                        (col->box.ymax < col2->box.ymax ? col->box.ymax : col2->box.ymax) -
                        (col->box.ymin > col2->box.ymin ? col->box.ymin : col2->box.ymin);
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

    sort (columns, lessY< TextColumnPtr >);

    // assign y positions
    for (ph = 0, i = 0; i < columns.size (); ++i) {
        auto& col = columns [i];
        col->py = 0;

        for (j = 0; j < i; ++j) {
            auto& col2 = columns [j];
            yOverlap = height_of (col2->box);

            if (yOverlap < slack * height_of (col2->box)) {
                if (col2->py + col2->ph + 1 > col->py) {
                    col->py = col2->py + col2->ph + 1;
                }
            }
            else {
                xOverlap =
                    (col->box.xmax < col2->box.xmax ? col->box.xmax : col2->box.xmax) -
                    (col->box.xmin > col2->box.xmin ? col->box.xmin : col2->box.xmin);

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
                        if (0 == height_of (underline.box)) {
                            if (word->rot == 0 || word->rot == 2) {
                                if (fabs (underline.box.ymin - base) < ubSlack &&
                                    underline.box.xmin < word->box.xmin + uSlack &&
                                    word->box.xmax - uSlack < underline.box.xmax) {
                                    word->underlined = true;
                                }
                            }
                        }
                        else {
                            if (word->rot == 1 || word->rot == 3) {
                                if (fabs (underline.box.xmin - base) < ubSlack &&
                                    underline.box.ymin < word->box.ymin + uSlack &&
                                    word->box.ymax - uSlack < underline.box.ymax) {
                                    word->underlined = true;
                                }
                            }
                        }
                    }

                    // handle links
                    for (auto& link : links) {
                        if (link.box.xmin < word->box.xmin + hSlack && word->box.xmax - hSlack < link.box.xmax &&
                            link.box.ymin < word->box.ymin + hSlack && word->box.ymax - hSlack < link.box.ymax) {
                            word->link = link;
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
        xx = 0.5 * (ch->box.xmin + ch->box.xmax);
        yy = 0.5 * (ch->box.ymin + ch->box.ymax);

        if (box.xmin < xx && xx < box.xmax &&
            box.ymin < yy && yy < box.ymax) {
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

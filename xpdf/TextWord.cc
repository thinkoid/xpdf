// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>

#include <xpdf/TextChar.hh>
#include <xpdf/TextFontInfo.hh>
#include <xpdf/TextWord.hh>

//
// Build a TextWord object, using chars[start .. start+len-1].
// (If rot >= 2, the chars list is in reverse order.)
//
TextWord::TextWord (
    TextChars& chars, int start, int lenA, int rotA, bool spaceAfterA) {
    TextCharPtr ch;
    int i;

    rot = rotA;

    const auto len = lenA;

    text.resize (len);
    edge.resize (len + 1);
    charPos.resize (len + 1);

    switch (rot) {
    case 0:
    default:
        ch = chars [start];
        xmin = ch->xmin;
        ymin = ch->ymin;
        ymax = ch->ymax;
        ch = chars [start + len - 1];
        xmax = ch->xmax;
        break;
    case 1:
        ch = chars [start];
        xmin = ch->xmin;
        xmax = ch->xmax;
        ymin = ch->ymin;
        ch = chars [start + len - 1];
        ymax = ch->ymax;
        break;
    case 2:
        ch = chars [start];
        xmax = ch->xmax;
        ymin = ch->ymin;
        ymax = ch->ymax;
        ch = chars [start + len - 1];
        xmin = ch->xmin;
        break;
    case 3:
        ch = chars [start];
        xmin = ch->xmin;
        xmax = ch->xmax;
        ymax = ch->ymax;
        ch = chars [start + len - 1];
        ymin = ch->ymin;
        break;
    }

    for (i = 0; i < len; ++i) {
        ch = chars [rot >= 2 ? start + len - 1 - i : start + i];
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

    ch = chars [start];

    font = ch->font;
    fontSize = ch->size;

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
    int charIdx, double* xminA, double* yminA, double* xmaxA, double* ymaxA) {
    if (charIdx < 0 || charIdx >= text.size ()) { return; }
    switch (rot) {
    case 0:
        *xminA = edge[charIdx];
        *xmaxA = edge[charIdx + 1];
        *yminA = ymin;
        *ymaxA = ymax;
        break;
    case 1:
        *xminA = xmin;
        *xmaxA = xmax;
        *yminA = edge[charIdx];
        *ymaxA = edge[charIdx + 1];
        break;
    case 2:
        *xminA = edge[charIdx + 1];
        *xmaxA = edge[charIdx];
        *yminA = ymin;
        *ymaxA = ymax;
        break;
    case 3:
        *xminA = xmin;
        *xmaxA = xmax;
        *yminA = edge[charIdx + 1];
        *ymaxA = edge[charIdx];
        break;
    }
}

double TextWord::getBaseline () {
    switch (rot) {
    case 0:
    default: return ymax + fontSize * font->descent;
    case 1: return xmin - fontSize * font->descent;
    case 2: return ymin - fontSize * font->descent;
    case 3: return xmax + fontSize * font->descent;
    }
}


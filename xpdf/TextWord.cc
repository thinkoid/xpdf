// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>

#include <xpdf/TextChar.hh>
#include <xpdf/TextFontInfo.hh>
#include <xpdf/TextWord.hh>

//
// Build a TextWord object, using chars[start .. start+len-1].
// (If rot >= 2, the chars list is in reverse order.)
//
TextWord::TextWord(TextChars &chars, int start, int lenA, int rotA,
                   bool spaceAfterA)
{
    int i;

    rot = rotA;

    const auto len = lenA;

    text.resize(len);
    edge.resize(len + 1);
    charPos.resize(len + 1);

    box = chars.front()->box;

    {
        const auto &ch = chars.back();

        switch (rot) {
        default:
        case 0:
            box.xmax = ch->box.xmax;
            break;
        case 1:
            box.ymax = ch->box.ymax;
            break;
        case 2:
            box.xmin = ch->box.xmin;
            break;
        case 3:
            box.ymin = ch->box.ymin;
            break;
        }
    }

    for (i = 0; i < len; ++i) {
        const auto &ch = chars[rot >= 2 ? start + len - 1 - i : start + i];

        text[i] = ch->c;
        charPos[i] = ch->charPos;

        if (i == len - 1) {
            charPos[len] = ch->charPos + ch->charLen;
        }

        switch (rot) {
        case 0:
        default:
            edge[i] = ch->box.xmin;
            if (i == len - 1) {
                edge[len] = ch->box.xmax;
            }
            break;
        case 1:
            edge[i] = ch->box.ymin;
            if (i == len - 1) {
                edge[len] = ch->box.ymax;
            }
            break;
        case 2:
            edge[i] = ch->box.xmax;
            if (i == len - 1) {
                edge[len] = ch->box.xmin;
            }
            break;
        case 3:
            edge[i] = ch->box.ymax;
            if (i == len - 1) {
                edge[len] = ch->box.ymin;
            }
            break;
        }
    }

    const auto &ch = chars.front();

    font = ch->font;
    fontSize = ch->size;

    spaceAfter = spaceAfterA;
    underlined = false;

    invisible = ch->invisible;
}

GString *TextWord::getFontName() const
{
    return font->name;
}

double TextWord::getBaseline()
{
    switch (rot) {
    default:
    case 0:
        return box.ymax + fontSize * font->descent;
    case 1:
        return box.xmin - fontSize * font->descent;
    case 2:
        return box.ymin - fontSize * font->descent;
    case 3:
        return box.xmax + fontSize * font->descent;
    }
}

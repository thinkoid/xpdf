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

#include <utils/memory.hh>
#include <utils/string.hh>
#include <utils/GList.hh>

#include <xpdf/Error.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/Link.hh>
#include <xpdf/TextOutputControl.hh>
#include <xpdf/TextOutputDev.hh>
#include <xpdf/TextPage.hh>
#include <xpdf/UnicodeTypeTable.hh>
#include <xpdf/bbox.hh>
#include <xpdf/unicode_map.hh>

#include <range/v3/all.hpp>
using namespace ranges;

////////////////////////////////////////////////////////////////////////

TextOutputDev::TextOutputDev(TextOutputControl control)
    : text(std::make_shared< TextPage >(&control))
    , control(control)
{
}

void TextOutputDev::startPage(int pageNum, GfxState *state)
{
    text->startPage(state);
}

void TextOutputDev::restoreState(GfxState *state)
{
    text->updateFont(state);
}

void TextOutputDev::updateFont(GfxState *state)
{
    text->updateFont(state);
}

void TextOutputDev::drawChar(GfxState *state, double x, double y, double dx,
                             double dy, double originX, double originY,
                             CharCode c, int nBytes, Unicode *u, int uLen)
{
    text->addChar(state, x, y, dx, dy, c, nBytes, u, uLen);
}

void TextOutputDev::incCharCount(int nChars)
{
    text->incCharCount(nChars);
}

void TextOutputDev::beginActualText(GfxState *state, Unicode *u, int uLen)
{
    text->beginActualText(state, u, uLen);
}

void TextOutputDev::endActualText(GfxState *state)
{
    text->endActualText(state);
}

bool TextOutputDev::findText(Unicode *s, int len, bool startAtTop,
                             bool stopAtBottom, bool startAtLast, bool stopAtLast,
                             bool caseSensitive, bool backward, bool wholeWord,
                             xpdf::bbox_t &box)
{
    return text->findText(s, len, startAtTop, stopAtBottom, startAtLast,
                          stopAtLast, caseSensitive, backward, wholeWord, box);
}

GString *TextOutputDev::getText(const xpdf::bbox_t &box)
{
    return text->getText(box);
}

TextPagePtr TextOutputDev::takeText()
{
    auto other = std::make_shared< TextPage >(&control);
    return std::swap(other, text), other;
}

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

#include <xpdf/bbox.hh>
#include <xpdf/Error.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/Link.hh>
#include <xpdf/TextOutputControl.hh>
#include <xpdf/TextOutputDev.hh>
#include <xpdf/TextPage.hh>
#include <xpdf/UnicodeMap.hh>
#include <xpdf/UnicodeTypeTable.hh>

#include <range/v3/all.hpp>
using namespace ranges;

////////////////////////////////////////////////////////////////////////

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
    text = std::make_shared< TextPage > (&control);
}

TextOutputDev::TextOutputDev (
    TextOutputFunc func, void* stream, TextOutputControl* controlA) {
    outputFunc = func;
    outputStream = stream;
    needClose = false;
    control = *controlA;
    text = std::make_shared< TextPage > (&control);
    ok = true;
}

TextOutputDev::~TextOutputDev () {
    if (needClose) {
        fclose ((FILE*)outputStream);
    }
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
    Unicode* s, int len,
    bool startAtTop, bool stopAtBottom, bool startAtLast, bool stopAtLast,
    bool caseSensitive, bool backward, bool wholeWord,
    xpdf::bbox_t& box) {
    return text->findText (
        s, len,
        startAtTop, stopAtBottom, startAtLast, stopAtLast,
        caseSensitive, backward, wholeWord,
        box);
}

GString*
TextOutputDev::getText (const xpdf::bbox_t& box) {
    return text->getText (box);
}

bool TextOutputDev::findCharRange (int pos, int length, xpdf::bbox_t& box) {
    return text->findCharRange (pos, length, box);
}

TextWords
TextOutputDev::makeWordList () {
    return text->makeWordList ();
}

TextPagePtr TextOutputDev::takeText () {
    auto other = std::make_shared< TextPage > (&control);
    return std::swap (other, text), other;
}

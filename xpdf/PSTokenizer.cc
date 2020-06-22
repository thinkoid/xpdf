// -*- mode: c++; -*-
// Copyright 2002-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <cstdlib>
#include <xpdf/PSTokenizer.hh>

//------------------------------------------------------------------------

// A '1' in this array means the character is white space.  A '1' or
// '2' means the character ends a name or command.
static char specialChars[256] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, // 0x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
    1, 0, 0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 2, // 2x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, // 3x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 4x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, // 5x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, // 7x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9x
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // ax
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // bx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // cx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // dx
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // ex
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // fx
};

//------------------------------------------------------------------------

PSTokenizer::PSTokenizer(int (*getCharFuncA)(void *), void *dataA)
{
    getCharFunc = getCharFuncA;
    data = dataA;
    charBuf = -1;
}

PSTokenizer::~PSTokenizer() { }

bool PSTokenizer::getToken(char *buf, int size, int *length)
{
    bool comment, backslash;
    int  c;
    int  i;

    // skip whitespace and comments
    comment = false;
    while (1) {
        if ((c = get()) == EOF) {
            buf[0] = '\0';
            *length = 0;
            return false;
        }
        if (comment) {
            if (c == '\x0a' || c == '\x0d') {
                comment = false;
            }
        } else if (c == '%') {
            comment = true;
        } else if (specialChars[c] != 1) {
            break;
        }
    }

    // read a token
    i = 0;
    buf[i++] = c;
    if (c == '(') {
        backslash = false;
        while ((c = peek()) != EOF) {
            if (i < size - 1) {
                buf[i++] = c;
            }
            get();
            if (c == '\\') {
                backslash = true;
            } else if (!backslash && c == ')') {
                break;
            } else {
                backslash = false;
            }
        }
    } else if (c == '<') {
        while ((c = peek()) != EOF) {
            get();
            if (i < size - 1 && specialChars[c] != 1) {
                buf[i++] = c;
            }
            if (c == '>') {
                break;
            }
        }
    } else if (c != '[' && c != ']') {
        while ((c = peek()) != EOF && !specialChars[c]) {
            get();
            if (i < size - 1) {
                buf[i++] = c;
            }
        }
    }
    buf[i] = '\0';
    *length = i;

    return true;
}

int PSTokenizer::peek()
{
    if (charBuf < 0) {
        charBuf = (*getCharFunc)(data);
    }
    return charBuf;
}

int PSTokenizer::get()
{
    int c;

    if (charBuf < 0) {
        charBuf = (*getCharFunc)(data);
    }
    c = charBuf;
    charBuf = -1;
    return c;
}

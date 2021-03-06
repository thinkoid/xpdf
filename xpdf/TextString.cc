// -*- mode: c++; -*-
// Copyright 2011-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <cstring>

#include <utils/memory.hh>
#include <utils/string.hh>

#include <xpdf/PDFDocEncoding.hh>
#include <xpdf/TextString.hh>

//------------------------------------------------------------------------

TextString::TextString()
{
    u = NULL;
    len = size = 0;
}

TextString::TextString(GString *s)
{
    u = NULL;
    len = size = 0;
    append(s);
}

TextString::TextString(TextString *s)
{
    len = size = s->len;
    if (len) {
        u = (Unicode *)calloc(size, sizeof(Unicode));
        memcpy(u, s->u, len * sizeof(Unicode));
    } else {
        u = NULL;
    }
}

TextString::~TextString()
{
    free(u);
}

TextString *TextString::append(Unicode c)
{
    expand(1);
    u[len] = c;
    ++len;
    return this;
}

TextString *TextString::append(GString *s)
{
    int n, i;

    if ((s->front() & 0xff) == 0xfe && ((*s)[1] & 0xff) == 0xff) {
        n = (s->getLength() - 2) / 2;
        expand(n);
        for (i = 0; i < n; ++i) {
            u[len + i] = (((*s)[2 + 2 * i] & 0xff) << 8) |
                         ((*s)[3 + 2 * i] & 0xff);
        }
        len += n;
    } else {
        n = s->getLength();
        expand(n);
        for (i = 0; i < n; ++i) {
            u[len + i] = pdfDocEncoding[(*s)[i] & 0xff];
        }
        len += n;
    }
    return this;
}

TextString *TextString::insert(int idx, Unicode c)
{
    if (idx >= 0 && idx <= len) {
        expand(1);
        if (idx < len) {
            memmove(u + idx + 1, u + idx, (len - idx) * sizeof(Unicode));
        }
        u[idx] = c;
        ++len;
    }
    return this;
}

TextString *TextString::insert(int idx, GString *s)
{
    int n, i;

    if (idx >= 0 && idx <= len) {
        if ((s->front() & 0xff) == 0xfe && ((*s)[1] & 0xff) == 0xff) {
            n = (s->getLength() - 2) / 2;
            expand(n);
            if (idx < len) {
                memmove(u + idx + n, u + idx, (len - idx) * sizeof(Unicode));
            }
            for (i = 0; i < n; ++i) {
                u[idx + i] = (((*s)[2 + 2 * i] & 0xff) << 8) |
                             ((*s)[3 + 2 * i] & 0xff);
            }
            len += n;
        } else {
            n = s->getLength();
            expand(n);
            if (idx < len) {
                memmove(u + idx + n, u + idx, (len - idx) * sizeof(Unicode));
            }
            for (i = 0; i < n; ++i) {
                u[idx + i] = pdfDocEncoding[(*s)[i] & 0xff];
            }
            len += n;
        }
    }
    return this;
}

void TextString::expand(int delta)
{
    int newLen;

    newLen = len + delta;
    if (delta > INT_MAX - len) {
        // trigger an out-of-memory error
        size = -1;
    } else if (newLen <= size) {
        return;
    } else if (size > 0 && size <= INT_MAX / 2 && size * 2 >= newLen) {
        size *= 2;
    } else {
        size = newLen;
    }
    u = (Unicode *)reallocarray(u, size, sizeof(Unicode));
}

GString *TextString::toPDFTextString()
{
    GString *s;
    bool     useUnicode;
    int      i;

    useUnicode = false;
    for (i = 0; i < len; ++i) {
        if (u[i] >= 0x80) {
            useUnicode = true;
            break;
        }
    }
    s = new GString();
    if (useUnicode) {
        s->append(1UL, (char)0xfe);
        s->append(1UL, (char)0xff);
        for (i = 0; i < len; ++i) {
            s->append(1UL, (char)(u[i] >> 8));
            s->append(1UL, (char)u[i]);
        }
    } else {
        for (i = 0; i < len; ++i) {
            s->append(1UL, (char)u[i]);
        }
    }
    return s;
}

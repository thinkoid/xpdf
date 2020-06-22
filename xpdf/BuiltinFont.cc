// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdlib>
#include <cstring>
#include <xpdf/FontEncodingTables.hh>
#include <xpdf/BuiltinFont.hh>

//------------------------------------------------------------------------

BuiltinFontWidths::BuiltinFontWidths(BuiltinFontWidth *widths, int sizeA)
{
    int i, h;

    size = sizeA;
    tab = (BuiltinFontWidth **)calloc(size, sizeof(BuiltinFontWidth *));
    for (i = 0; i < size; ++i) {
        tab[i] = NULL;
    }
    for (i = 0; i < sizeA; ++i) {
        h = hash(widths[i].name);
        widths[i].next = tab[h];
        tab[h] = &widths[i];
    }
}

BuiltinFontWidths::~BuiltinFontWidths()
{
    free(tab);
}

bool BuiltinFontWidths::getWidth(const char *name, unsigned short *width)
{
    int               h;
    BuiltinFontWidth *p;

    h = hash(name);
    for (p = tab[h]; p; p = p->next) {
        if (!strcmp(p->name, name)) {
            *width = p->width;
            return true;
        }
    }
    return false;
}

int BuiltinFontWidths::hash(const char *name)
{
    const char * p;
    unsigned int h;

    h = 0;
    for (p = name; *p; ++p) {
        h = 17 * h + (int)(*p & 0xff);
    }
    return (int)(h % size);
}

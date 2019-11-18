//========================================================================
//
// BuiltinFont.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef BUILTINFONT_H
#define BUILTINFONT_H

#include <defs.hh>


struct BuiltinFont;
class BuiltinFontWidths;

//------------------------------------------------------------------------

struct BuiltinFont {
    const char* name;
    const char** defaultBaseEnc;
    short ascent;
    short descent;
    short bbox[4];
    BuiltinFontWidths* widths;
};

//------------------------------------------------------------------------

struct BuiltinFontWidth {
    const char* name;
    unsigned short width;
    BuiltinFontWidth* next;
};

class BuiltinFontWidths {
public:
    BuiltinFontWidths (BuiltinFontWidth* widths, int sizeA);
    ~BuiltinFontWidths ();
    bool getWidth (const char* name, unsigned short* width);

private:
    int hash (const char* name);

    BuiltinFontWidth** tab;
    int size;
};

#endif

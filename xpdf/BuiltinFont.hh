// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_BUILTINFONT_HH
#define XPDF_XPDF_BUILTINFONT_HH

#include <defs.hh>

struct BuiltinFont;
class BuiltinFontWidths;

//------------------------------------------------------------------------

struct BuiltinFont
{
    const char *       name;
    const char **      defaultBaseEnc;
    short              ascent;
    short              descent;
    short              bbox[4];
    BuiltinFontWidths *widths;
};

//------------------------------------------------------------------------

struct BuiltinFontWidth
{
    const char *      name;
    unsigned short    width;
    BuiltinFontWidth *next;
};

class BuiltinFontWidths
{
public:
    BuiltinFontWidths(BuiltinFontWidth *widths, int sizeA);
    ~BuiltinFontWidths();
    bool getWidth(const char *name, unsigned short *width);

private:
    int hash(const char *name);

    BuiltinFontWidth **tab;
    int                size;
};

#endif // XPDF_XPDF_BUILTINFONT_HH

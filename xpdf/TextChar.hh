// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_TEXTCHAR_HH
#define XPDF_XPDF_TEXTCHAR_HH

#include <defs.hh>

#include <xpdf/bbox.hh>
#include <xpdf/xpdf.hh>
#include <xpdf/CharTypes.hh>
#include <xpdf/TextOutput.hh>

struct TextChar
{
    TextFontInfoPtr font;

    double       size;
    xpdf::bbox_t box;

    Unicode c;
    int     charPos;

    unsigned char charLen : 4, rot : 2, clipped : 1, invisible : 1;
};

struct char_t
{
    wchar_t      value;
    xpdf::bbox_t box;
};

template< typename T > xpdf::bbox_t bbox_from(const T &);

template<> inline xpdf::bbox_t bbox_from< TextChar >(const TextChar &ch)
{
    return ch.box;
}

template<> inline xpdf::bbox_t bbox_from< TextCharPtr >(const TextCharPtr &p)
{
    return bbox_from(*p);
}

template<> inline xpdf::bbox_t bbox_from< char_t >(const char_t &ch)
{
    return ch.box;
}

template<> inline xpdf::bbox_t bbox_from< xpdf::bbox_t >(const xpdf::bbox_t &arg)
{
    return arg;
}

inline char_t make_char(TextChar &ch)
{
    return { wchar_t(ch.c), bbox_from(ch) };
}

#endif // XPDF_XPDF_TEXTCHAR_HH

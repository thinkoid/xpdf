// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_XPDF_BUILTINFONT_HH
#define XPDF_XPDF_BUILTINFONT_HH

#include <defs.hh>

#include <map>
#include <string>
#include <vector>

#include <xpdf/bbox.hh>
#include <xpdf/obj.hh>

namespace xpdf {

struct builtin_font_t
{
    std::string name;
    xpdf::ref_t ref;

    const char **base_encoding;

    struct {
        int ascent, descent;
        bbox_t bbox;
    } metric;

    std::map< std::string_view, int > widths;
};

const builtin_font_t *builtin_font(const char *);
const builtin_font_t *builtin_substitute_font(size_t);

} // namespace xpdf

#endif // XPDF_XPDF_BUILTINFONT_HH

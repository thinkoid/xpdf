// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC

#ifndef XPDF_XPDF_TEXTLINK_HH
#define XPDF_XPDF_TEXTLINK_HH

#include <defs.hh>

#include <string>
#include <xpdf/bbox.hh>

struct TextLink {
    xpdf::bbox_t box;
    std::string uri;
};

#endif // XPDF_XPDF_TEXTLINK_HH

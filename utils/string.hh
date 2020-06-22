// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC

#ifndef XPDF_UTILS_STRING_HH
#define XPDF_UTILS_STRING_HH

#include <defs.hh>

#include <optional>
#include <string>

namespace xpdf {

using string_t = std::string;
using optional_string_t = std::optional< std::string >;

} // namespace xpdf

#include <utils/GString.hh>

#endif // XPDF_UTILS_STRING_HH

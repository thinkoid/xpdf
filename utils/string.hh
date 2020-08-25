// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_UTILS_STRING_HH
#define XPDF_UTILS_STRING_HH

#include <defs.hh>

#include <optional>
#include <string>
#include <vector>

namespace xpdf {

using string_t = std::string;
using optional_string_t = std::optional< std::string >;

std::vector< std::string >
split(const std::string &s, const std::string &delims = " \t\r\n");

} // namespace xpdf

#include <utils/GString.hh>

#endif // XPDF_UTILS_STRING_HH

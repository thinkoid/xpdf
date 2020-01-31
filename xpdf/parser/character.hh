// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_CHARACTER_HH
#define XPDF_XPDF_PARSER_CHARACTER_HH

#include <defs.hh>

#include <xpdf/parser/ctype.hh>

namespace xpdf::parser {

inline bool is_regular (char c) {
    return 0 == ctype_of (c);
}

inline bool is_delimiter (char c) {
    return 1 == ctype_of (c);
}

inline bool is_space (char c) {
    return 2 == ctype_of (c);
}

} // xpdf::parser

#endif // XPDF_XPDF_PARSER_CHARACTER_HH

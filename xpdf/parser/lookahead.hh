// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_LOOKAHEAD_HH
#define XPDF_XPDF_PARSER_LOOKAHEAD_HH

#include <defs.hh>

#include <climits>

namespace xpdf::parser {

template< typename Iterator >
inline typename Iterator::value_type
lookahead (Iterator& iter, Iterator last) {
    return (iter == last) ? CHAR_MAX + 1 : *iter;
}

template< typename Iterator >
inline bool
lookahead (Iterator& iter, Iterator last, typename Iterator::value_type c) {
    return c == lookahead (iter, last);
}

template< typename Iterator >
bool lookahead (Iterator, Iterator, const std::string&);

} // xpdf::parser

#include <xpdf/parser/lookahead.cc>

#endif // XPDF_XPDF_PARSER_LOOKAHEAD_HH

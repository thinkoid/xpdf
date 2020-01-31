// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_STRING_HH
#define XPDF_XPDF_PARSER_STRING_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool parenthesized_string (Iterator, Iterator&, Iterator, std::string&);

template< typename Iterator >
bool angular_string (Iterator, Iterator&, Iterator, std::string&);

template< typename Iterator >
bool string_ (Iterator, Iterator&, Iterator, std::string&);

} // xpdf::parser

#include <xpdf/parser/string.cc>

#endif // XPDF_XPDF_PARSER_STRING_HH

// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_ARRAY_HH
#define XPDF_XPDF_PARSER_ARRAY_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool array (Iterator, Iterator&, Iterator, ast::array_t&);

} // xpdf::parser

#include <xpdf/parser/array.cc>

#endif // XPDF_XPDF_PARSER_ARRAY_HH

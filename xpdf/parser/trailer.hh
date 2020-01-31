// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_TRAILER_HH
#define XPDF_XPDF_PARSER_TRAILER_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool trailer (Iterator, Iterator&, Iterator, ast::dict_t&);

} // xpdf::parser

#include <xpdf/parser/trailer.cc>

#endif // XPDF_XPDF_PARSER_TRAILER_HH

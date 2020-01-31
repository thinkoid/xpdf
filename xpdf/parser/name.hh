// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_NAME_HH
#define XPDF_XPDF_PARSER_NAME_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool name (Iterator, Iterator&, Iterator, ast::name_t&);

} // xpdf::parser

#include <xpdf/parser/name.cc>

#endif // XPDF_XPDF_PARSER_NAME_HH

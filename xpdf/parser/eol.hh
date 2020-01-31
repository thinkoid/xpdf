// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_EOL_HH
#define XPDF_XPDF_PARSER_EOL_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool eol (Iterator, Iterator&, Iterator);

} // xpdf::parser

#include <xpdf/parser/eol.cc>

#endif // XPDF_XPDF_PARSER_EOL_HH

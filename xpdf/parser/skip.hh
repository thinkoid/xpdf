// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_SKIP_HH
#define XPDF_XPDF_PARSER_SKIP_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool skipws (Iterator, Iterator&, Iterator);

} // xpdf::parser

#include <xpdf/parser/skip.cc>

#define SKIP skipws (first, iter, last)

#endif // XPDF_XPDF_PARSER_SKIP_HH

// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_LIT_HH
#define XPDF_XPDF_PARSER_LIT_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool lit (Iterator, Iterator&, Iterator, const std::string&);

template< typename Iterator >
bool lit (Iterator, Iterator&, Iterator, char);

} // xpdf::parser

#include <xpdf/parser/lit.cc>

#endif // XPDF_XPDF_PARSER_LIT_HH

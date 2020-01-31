// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_NUMERIC_HH
#define XPDF_XPDF_PARSER_NUMERIC_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool bool_ (Iterator, Iterator&, Iterator, bool&);

template< typename Iterator >
bool digit (Iterator, Iterator&, Iterator, int&);

template< typename Iterator >
bool int_ (Iterator, Iterator&, Iterator, int&);

template< typename Iterator >
bool double_ (Iterator, Iterator&, Iterator, double&);

template< typename Iterator >
bool ints (Iterator, Iterator&, Iterator, int&, int&);

} // xpdf::parser

#include <xpdf/parser/numeric.cc>

#endif // XPDF_XPDF_PARSER_NUMERIC_HH

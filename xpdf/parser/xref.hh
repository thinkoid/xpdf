// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_XREF_HH
#define XPDF_XPDF_PARSER_XREF_HH

#include <defs.hh>

#include <map>

namespace xpdf::parser {

template< typename Iterator >
bool startxref (Iterator, Iterator&, Iterator);

template< typename Iterator >
bool xref (Iterator, Iterator&, Iterator, std::map< size_t, off_t >&);

} // xpdf::parser

#include <xpdf/parser/xref.cc>

#endif // XPDF_XPDF_PARSER_XREF_HH

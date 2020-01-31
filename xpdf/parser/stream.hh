// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_STREAM_HH
#define XPDF_XPDF_PARSER_STREAM_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool streambuf_ (Iterator, Iterator&, Iterator, ast::stream_t&);

template< typename Iterator >
bool stream_ (Iterator, Iterator&, Iterator, ast::stream_t&);

} // xpdf::parser

#include <xpdf/parser/stream.cc>

#endif // XPDF_XPDF_PARSER_STREAM_HH

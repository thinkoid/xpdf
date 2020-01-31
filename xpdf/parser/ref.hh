// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_REF_HH
#define XPDF_XPDF_PARSER_REF_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool ref (Iterator, Iterator&, Iterator, ast::ref_t&);

} // xpdf::parser

#include <xpdf/parser/ref.cc>

#endif // XPDF_XPDF_PARSER_REF_HH

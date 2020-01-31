// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_ANY_HH
#define XPDF_XPDF_PARSER_ANY_HH

#include <defs.hh>
#include <xpdf/parser/ast.hh>

namespace xpdf::parser {

template< typename Iterator >
bool any (Iterator, Iterator&, Iterator, ast::obj_t&);

} // xpdf::parser

#include <xpdf/parser/any.cc>

#endif // XPDF_XPDF_PARSER_ANY_HH

// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_OBJ_HH
#define XPDF_XPDF_PARSER_OBJ_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
bool object (Iterator, Iterator&, Iterator, std::tuple< int, ast::obj_t >&);

} // xpdf::parser

#include <xpdf/parser/obj.cc>

#endif // XPDF_XPDF_PARSER_OBJ_HH

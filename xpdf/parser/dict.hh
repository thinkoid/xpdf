// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_DICT_HH
#define XPDF_XPDF_PARSER_DICT_HH

#include <defs.hh>
#include <xpdf/parser/any.hh>

#include <string>
#include <tuple>

namespace xpdf::parser {

template< typename Iterator >
bool definition (Iterator, Iterator&, Iterator,
                 std::tuple< std::string, ast::obj_t >&);

template< typename Iterator >
bool dictionary (Iterator, Iterator&, Iterator, ast::dict_t&);

} // xpdf::parser

#include <xpdf/parser/dict.cc>

#endif // XPDF_XPDF_PARSER_DICT_HH

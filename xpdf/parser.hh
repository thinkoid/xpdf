// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_HH
#define XPDF_XPDF_PARSER_HH

#include <defs.hh>

#include <xpdf/parser/any.hh>
#include <xpdf/parser/array.hh>
#include <xpdf/parser/ast.hh>
#include <xpdf/parser/character.hh>
#include <xpdf/parser/comment.hh>
#include <xpdf/parser/dict.hh>
#include <xpdf/parser/eol.hh>
#include <xpdf/parser/error.hh>
#include <xpdf/parser/lit.hh>
#include <xpdf/parser/lookahead.hh>
#include <xpdf/parser/name.hh>
#include <xpdf/parser/numeric.hh>
#include <xpdf/parser/obj.hh>
#include <xpdf/parser/ref.hh>
#include <xpdf/parser/skip.hh>
#include <xpdf/parser/stream.hh>
#include <xpdf/parser/string.hh>
#include <xpdf/parser/trailer.hh>
#include <xpdf/parser/xref.hh>

namespace xpdf {

template< typename Iterator >
bool parse (Iterator, Iterator&, Iterator, parser::ast::doc_t&);

} // namespace xpdf

#include <xpdf/parser.cc>

#endif // XPDF_XPDF_PARSER_HH

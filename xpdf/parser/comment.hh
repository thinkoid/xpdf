// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_COMMENT_HH
#define XPDF_XPDF_PARSER_COMMENT_HH

#include <defs.hh>
#include <xpdf/parser/lit.hh>

namespace xpdf::parser {

template< typename Iterator >
bool comment (Iterator, Iterator&, Iterator);

template< typename Iterator >
inline bool eof (Iterator first, Iterator& iter, Iterator last) {
    return lit (first, iter, last, "%%EOF");
}

template< typename Iterator >
bool version (Iterator, Iterator&, Iterator, std::tuple< int, int >&);

} // xpdf::parser

#include <xpdf/parser/comment.cc>

#endif // XPDF_XPDF_PARSER_COMMENT_HH

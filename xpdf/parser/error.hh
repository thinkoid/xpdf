// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_ERROR_HH
#define XPDF_XPDF_PARSER_ERROR_HH

#include <defs.hh>

#include <iostream>

namespace xpdf::parser {

template< typename Iterator >
void expected (Iterator, Iterator, Iterator, Iterator,
               const std::string&, std::ostream& out = std::cerr);

template< typename Iterator >
inline void
expected (Iterator first, Iterator start, Iterator iter, Iterator last,
          char what, std::ostream& out = std::cerr) {
    return expected (first, start, iter, last, std::string (1U, what), out);
}

} // xpdf::parser

#include <xpdf/parser/error.cc>

#endif // XPDF_XPDF_PARSER_ERROR_HH

// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/comment.hh>
#include <xpdf/parser/eol.hh>
#include <xpdf/parser/lit.hh>
#include <xpdf/parser/lookahead.hh>
#include <xpdf/parser/numeric.hh>

namespace xpdf::parser {

template< typename Iterator >
bool comment (Iterator first, Iterator& iter, Iterator last) {
    bool b = false;

    if (iter != last && *iter == '%') {
        for (b = true; iter != last && !eol (first, iter, last); ++iter) ;
    }

    return b;
}

template< typename Iterator >
bool version (Iterator first, Iterator& iter, Iterator last,
              std::tuple< int, int >& attr) {
    int major = 0;

    if (lit (first, iter, last, "%PDF-") &&
        int_ (first, iter, last, major)) {

        int minor = 0;

        if (lit (first, iter, last, '.') && digit (first, iter, last, minor)) {
            return attr = { 1, minor }, true;
        }
    }

    return false;
}

} // xpdf::parser

// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/trailer.hh>

namespace xpdf::parser {

template< typename Iterator >
bool trailer (Iterator first, Iterator& iter, Iterator last,
              ast::dict_t& attr) {
    if (lit (first, iter, last, "trailer")) {
        skipws (first, iter, last);

        ast::dict_t trailer;

        if (dictionary (first, iter, last, trailer)) {
            return attr = std::move (trailer), true;
        }
    }

    return false;
}

} // xpdf::parser

// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/array.hh>
#include <xpdf/parser/lit.hh>
#include <xpdf/parser/skip.hh>

namespace xpdf::parser {

template< typename Iterator >
bool array (Iterator first, Iterator& iter, Iterator last,
            ast::array_t& attr) {
    ast::array_t arr;

    if (lit (first, iter, last, '[')) {
        skipws (first, iter, last);

        for (; iter != last && !lit (first, iter, last, ']');) {
            ast::obj_t obj;

            if (any (first, iter, last, obj)) {
                arr.emplace_back (std::move (obj));
            }
            else
                return false;

            skipws (first, iter, last);
        }

        attr = std::move (arr);
        return true;
    }

    return false;
}

} // xpdf::parser

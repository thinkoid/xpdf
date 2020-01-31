// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/dict.hh>
#include <xpdf/parser/comment.hh>
#include <xpdf/parser/lit.hh>
#include <xpdf/parser/name.hh>
#include <xpdf/parser/skip.hh>

namespace xpdf::parser {

template< typename Iterator >
bool definition (Iterator first, Iterator& iter, Iterator last,
                 std::tuple< ast::name_t, ast::obj_t >& attr) {
    std::tuple< ast::name_t, ast::obj_t > def;

    if (name (first, iter, last, std::get< 0 >(def))) {
        skipws (first, iter, last);

        if (any (first, iter, last, std::get< 1 >(def))) {
            return attr = std::move (def), true;
        }
    }

    return false;
}

template< typename Iterator >
bool dictionary (Iterator first, Iterator& iter, Iterator last,
                 ast::dict_t& attr) {
    if (lit (first, iter, last, "<<")) {
        skipws (first, iter, last);
        comment (first, iter, last);

        skipws (first, iter, last);

        ast::dict_t dict;

        for (; iter != last;) {
            switch (*iter) {
            case '/': {
                std::tuple< ast::name_t, ast::obj_t > def;

                if (!definition (first, iter, last, def)) {
                    return false;
                }

                dict.push_back (std::move (def));
            }
                break;

            case '>':
                if (lit (first, iter, last, ">>")) {
                    return attr = std::move (dict), true;
                }

            default:
                return false;
            }

            skipws (first, iter, last);
            comment (first, iter, last);

            skipws (first, iter, last);
        }
    }

    return false;
}

} // xpdf::parser

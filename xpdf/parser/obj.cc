// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/obj.hh>
#include <xpdf/parser/skip.hh>
#include <xpdf/parser/stream.hh>

namespace xpdf::parser {

template< typename Iterator >
bool object (Iterator first, Iterator& iter, Iterator last,
             std::tuple< int, ast::obj_t >& attr) {
    int id;

    if (int_ (first, iter, last, id)) {
        skipws (first, iter, last);

        int ignore = 0;

        if (int_ (first, iter, last, ignore)) {
            skipws (first, iter, last);

            if (lit (first, iter, last, "obj")) {
                skipws (first, iter, last);

                std::vector< ast::obj_t > objs;

                skipws (first, iter, last);
                comment (first, iter, last);

                for (; iter != last;) {
                    if (lookahead (iter, last, "endobj")) {
                        lit (first, iter, last, "endobj");

                        switch (objs.size ()) {
                        case 0:
                            return attr = { id, ast::obj_t{ } }, true;

                        case 1:
                            return attr = {
                                id, std::move (objs.front ()) }, true;

                        default:
                            return attr = {
                                id, std::make_shared< ast::array_t > (
                                    std::move (objs)) }, true;
                        }
                    }
                    else {
                        ast::obj_t obj;

                        if (!any (first, iter, last, obj)) {
                            return false;
                        }

                        objs.emplace_back (std::move (obj));
                    }

                    skipws (first, iter, last);
                    comment (first, iter, last);
                }
            }
        }
    }

    return false;
}

} // xpdf::parser


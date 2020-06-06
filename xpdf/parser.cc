// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>
#include <xpdf/parser.hh>

namespace xpdf {

template< typename Iterator >
bool parse (Iterator first, Iterator& iter, Iterator last,
            xpdf::parser::ast::doc_t& attr) {
    using namespace xpdf::parser;

    ast::doc_t doc;

    for (SKIP; iter != last; SKIP) {
        const auto start = iter;

        switch (*iter) {
        case '%':
            if (!comment (first, iter, last))
                return false;

            break;

        case 's': {
            //
            // startxref
            //
            int ignore;

            if (!startxref (first, iter, last, ignore)) {
                expected (first, start, iter, last, "startxref");
                return false;
            }
        }
            break;

        case 't': {
            //
            // trailer
            //
            if (!trailer (first, iter, last, doc.trailer)) {
                expected (first, start, iter, last, "trailer");
                return false;
            }
        }
            break;

        case 'x': {
            //
            // xref
            //
            std::map< size_t, off_t > ignore;

            if (!xrefs (first, iter, last, ignore)) {
                expected (first, start, iter, last, "xref");
                return false;
            }
        }
            break;

        default:
            if (std::isdigit (*iter)) {
                std::tuple< int, ast::obj_t > obj;

                if (!object (first, iter, last, obj)) {
                    expected (first, start, iter, last, "object");
                    return false;
                }

                doc.objs.emplace_back (std::move (obj));
            }
            else {
                expected (first, start, iter, last, "anything but this");
                return false;
            }

            break;
        }
    }

    return attr = std::move (doc), true;
}

} // namespace xpdf

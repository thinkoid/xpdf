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
        switch (*iter) {
        case '%':
            if (!comment (first, iter, last)) {
                return false;
            }

            break;

        case 's': {
            //
            // startxref
            //
            int ignore;

            if (!startxref (first, iter, last, ignore)) {
                return expected (first, iter, last, "startxref"), false;
            }
        }
            break;

        case 't': {
            //
            // trailer
            //
            if (!trailer (first, iter, last, doc.trailer)) {
                return expected (first, iter, last, "trailer"), false;
            }
        }
            break;

        case 'x': {
            //
            // xref
            //
            std::map< size_t, off_t > ignore;

            if (!xrefs (first, iter, last, ignore)) {
                return expected (first, iter, last, "xref"), false;
            }
        }
            break;

        default:
            if (std::isdigit (*iter)) {
                std::tuple< int, ast::obj_t > obj;

                if (!object (first, iter, last, obj)) {
                    return expected (first, iter, last, "object"), false;
                }

                doc.objs.emplace_back (std::move (obj));
            }
            else {
                return expected (first, iter, last, "anything but this"), false;
            }

            break;
        }
    }

    return attr = std::move (doc), true;
}

} // namespace xpdf

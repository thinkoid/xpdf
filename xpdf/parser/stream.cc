// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/stream.hh>
#include <xpdf/parser/eol.hh>

namespace xpdf::parser {

template< typename Iterator >
bool streambuf_ (Iterator first, Iterator& iter, Iterator last, ast::stream_t& attr) {
    ast::stream_t str;

    for (; iter != last; ++iter) {
        {
            // TODO: eval
            XPDF_ITERATOR_GUARD (iter);

            if (eol (first, iter, last) && lit (first, iter, last, "endstream")) {
                attr = std::move (str);
                XPDF_PARSE_SUCCESS;
            }
        }

        str.emplace_back (*iter);
    }

    return false;
}

template< typename Iterator >
bool stream_ (Iterator first, Iterator& iter, Iterator last, ast::stream_t& attr) {
    {
        // TODO: eval
        XPDF_ITERATOR_GUARD (iter);

        if (!lit (first, iter, last, "stream"))
            return true;

        XPDF_XPDF_ITERATOR_RELEASE;
    }

    return eol (first, iter, last) && streambuf_ (first, iter, last, attr);
}

} // xpdf::parser

// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/xref.hh>
#include <xpdf/parser/lit.hh>
#include <xpdf/parser/numeric.hh>
#include <xpdf/parser/skip.hh>

namespace xpdf::parser {

template< typename Iterator >
bool startxref (Iterator first, Iterator& iter, Iterator last, int& attr) {
    int n = 0;

    if (lit  (first, iter, last, "startxref") && SKIP &&
        int_ (first, iter, last, n)) {
        return attr = n, true;
    }

    return false;
}

namespace detail {

template< typename Iterator >
bool xref (Iterator first, Iterator& iter, Iterator last,
           off_t& off, int& type) {
    int a, b;

    if (ints (first, iter, last, a, b) && SKIP &&
        iter != last && (*iter == 'n' || *iter == 'f')) {
        return off = a, type = *iter++, true;
    }

    return false;
}

template< typename Iterator >
bool xrefs (Iterator first, Iterator& iter, Iterator last,
            int a, int b, std::map< size_t, off_t >& xs) {
    int i = a;

    for (; i < a + b; ++i, SKIP) {
        int type;
        off_t off;

        if (xref (first, iter, last, off, type)) {
            switch (type) {
            case 'n':
                //
                // Avoid id 0:
                //
                if (i) {
                    xs.emplace (i, off);
                }
                break;

            case 'f':
                // TODO: free entries
                break;

            default:
                return false;
            }
        }
        else {
            break;
        }
    }

    return i == a + b;
}

template< typename Iterator >
bool xrefs (Iterator first, Iterator& iter, Iterator last,
            std::map< size_t, off_t >& attr) {
    std::map< size_t, off_t > xs;

    for (int a, b, c = 0; iter != last && std::isdigit (*iter); ++c, SKIP) {
        // TODO: parsing is too relaxed
        if (ints (first, iter, last, a, b)) {
            if (b <= 0 || !SKIP || !xrefs (first, iter, last, a, b, xs)) {
                return false;
            }
        }
        else {
            if (0 == c) {
                return false;
            }

            break;
        }
    }

    return attr = xs, true;
}

} // namespace detail

template< typename Iterator >
bool xrefs (Iterator first, Iterator& iter, Iterator last,
            std::map< size_t, off_t >& attr) {
    if (lit (first, iter, last, "xref")) {
        return SKIP && detail::xrefs (first, iter, last, attr);
    }

    return false;
}

} // xpdf::parser

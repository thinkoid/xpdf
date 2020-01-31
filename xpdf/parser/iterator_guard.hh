// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_ITERATOR_GUARD_HH
#define XPDF_XPDF_PARSER_ITERATOR_GUARD_HH

#include <defs.hh>

namespace xpdf::parser {

template< typename Iterator >
struct iterator_guard_t {
    iterator_guard_t (Iterator& iter)
        : iter (iter), save (iter), restore (true)
        { }

    ~iterator_guard_t () {
        if (restore) {
            iter = save;
        }
    }

    void release () {
        restore = false;
    }

    Iterator &iter, save;
    bool restore;
};

#define XPDF_ITERATOR_GUARD(x) iterator_guard_t iterator_guard (x)
#define XPDF_XPDF_ITERATOR_RELEASE  iterator_guard.release ()
#define XPDF_PARSE_SUCCESS     XPDF_XPDF_ITERATOR_RELEASE; return true

} // xpdf::parser

#endif // XPDF_XPDF_PARSER_ITERATOR_GUARD_HH

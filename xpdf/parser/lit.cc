// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/lit.hh>
#include <xpdf/parser/iterator_guard.hh>

namespace xpdf::parser {

template< typename Iterator >
bool lit (Iterator, Iterator& iter, Iterator last, const std::string& s) {
    auto other = s.begin ();

    for (; iter != last && other != s.end () && *iter == *other;
         ++iter, ++other) ;

    return other == s.end ();
}

template< typename Iterator >
bool lit (Iterator, Iterator& iter, Iterator last, char c) {
    if (iter != last && *iter == c) {
        return ++iter, true;
    }

    return false;
}

} // xpdf::parser

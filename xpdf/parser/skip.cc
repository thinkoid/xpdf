// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/skip.hh>
#include <xpdf/parser/character.hh>

namespace xpdf::parser {

template< typename Iterator >
bool skipws (Iterator, Iterator& iter, Iterator last) {
    bool b = false;

    if (iter != last && is_space (*iter)) {
        b = true;
        for (++iter; iter != last && is_space (*iter); ++iter) ;
    }

    return b;
}

} // xpdf::parser

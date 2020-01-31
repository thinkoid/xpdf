// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/error.hh>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace xpdf::parser {

template< typename Iterator >
void expected (Iterator first, Iterator iter, Iterator last,
               const std::string& what, std::ostream& out) {
    off_t line = std::count (first, iter, '\n') + 1;

    first = std::find (
        std::make_reverse_iterator (iter),
        std::make_reverse_iterator (first), '\n').base ();

    off_t col = std::distance (first, iter) + 1;

    std::stringstream ss;
    ss.unsetf (std::ios_base::skipws);

    auto print = [&](unsigned char c) {
        if (std::isprint (c) || c == '\n')
            ss << c;
        else
            ss << '\\' << std::setw (3) << std::setfill ('0') << std::oct
               << int (c);
    };

    const auto distance = std::distance (first, iter);

    if (distance > 64) {
        for (size_t i = 0; i < 30 && first != iter; ++first, ++i)
            print (*first);

        ss << "[...]";

        auto length_of = [](unsigned char c) {
            return (std::isprint (c) || c == '\n') ? 1 : 4;
        };

        auto riter = std::make_reverse_iterator (iter);
        const auto rlast = std::make_reverse_iterator (first);

        for (size_t i = 0; i < 30 && riter != rlast; ++riter)
            i += length_of (*riter);

        for (first = riter.base (); first != iter; ++first)
            print (*first);
    }
    else {
        std::for_each (first, iter, print);
    }

    const auto n = ss.tellp ();

    for (size_t i = 0; i < 16 && iter != last && *iter != '\n'; ++iter, ++i) {
        print (*iter);
    }

    out << "error:" << line << ":" << col
        << ": parsing " << what << ":\n" << ss.rdbuf () << "[...]\n"
        << std::string (n, '-') << '^' << std::endl;
}

} // xpdf::parser

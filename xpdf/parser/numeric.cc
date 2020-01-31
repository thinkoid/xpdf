// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/lit.hh>
#include <xpdf/parser/error.hh>
#include <xpdf/parser/numeric.hh>

#include <string>

namespace xpdf::parser {

template< typename Iterator >
bool bool_ (Iterator first, Iterator& iter, Iterator last, bool& b) {
    if (iter != last) {
        switch (*iter) {
        case 't':
            if (lit (first, iter, last, "true"))
                return b = true;
            break;

        case 'f':
            if (lit (first, iter, last, "false"))
                return !(b = false);
            break;

        default:
            break;
        }
    }

    return false;
}

template< typename Iterator >
bool digit (Iterator, Iterator& iter, Iterator last, int& attr) {
    if (iter != last) {
        if (std::isdigit (*iter)) {
            return attr = *iter++ - '0', true;
        }
    }

    return false;
}

template< typename Iterator >
bool int_ (Iterator first, Iterator& iter, Iterator last, int& attr) {
    if (iter != last) {
        std::stringstream ss;

        if (*iter == '+' || *iter == '-') {
            ss << *iter++;
        }

        bool empty = true;

        for (; iter != last && std::isdigit (*iter); ++iter, empty = false) {
            ss << *iter;
        }

        if (!empty) {
            return attr = std::stoi (ss.str ()), true;
        }
    }

    return false;
}

template< typename Iterator >
bool double_ (Iterator first, Iterator& iter, Iterator last, double& attr) {
    if (iter != last) {
        std::stringstream ss;

        //
        // Consume sign, if any:
        //
        if (*iter == '+' || *iter == '-') {
            ss << *iter++;
        }

        bool empty = true;

        //
        // Consume digits leading to decimal point:
        //
        for (; iter != last && std::isdigit (*iter); ++iter, empty = false) {
            ss << *iter;
        }

        //
        // Expect a decimal point, required by format:
        //
        if (iter == last || *iter != '.')
            return false;

        ss << *iter++;

        //
        // Consume trailing digits, if any:
        //
        for (; iter != last && std::isdigit (*iter); ++iter, empty = false) {
            ss << *iter;
        }

        //
        // Non-conforming exponential format:
        //
        if (iter != last && (*iter == 'e' || *iter == 'E')) {
            ss << *iter++;

            if (iter != last && (*iter == '+' || *iter == '-')) {
                ss << *iter++;
            }

            //
            // Consume trailing digits, if any:
            //
            for (; iter != last && std::isdigit (*iter); ++iter) {
                ss << *iter;
            }
        }

        if (!empty) {
            return attr = std::stod (ss.str ()), true;
        }
    }

    return false;
}

template< typename Iterator >
bool ints (Iterator first, Iterator& iter, Iterator last, int& a, int& b) {
    int x, y;

    if (int_ (first, iter, last, x) && skipws (first, iter, last) &&
        int_ (first, iter, last, y)) {
        return a = x, b = y, true;
    }

    return false;
}

} // xpdf::parser

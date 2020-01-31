// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <xpdf/parser/name.hh>

namespace xpdf::parser {

template< typename Iterator >
bool name (Iterator first, Iterator& iter, Iterator last, ast::name_t& attr) {
    if (iter != last && *iter == '/') {
        std::stringstream ss;
        ss << *iter;

        bool empty = true;

        for (++iter; iter != last && *iter; ++iter) {
            if (*iter == '#') {
                ss << *iter;

                if (++iter == last || !std::isxdigit (*iter))
                    return false;

                ss << *iter;

                if (++iter == last || !std::isxdigit (*iter))
                    return false;

                ss << *iter;
                empty = false;
            }
            else if (is_regular (*iter)) {
                ss << *iter;
                empty = false;
            }
            else {
                break;
            }
        }

        if (!empty) {
            return attr = ss.str (), true;
        }
    }

    return false;
}

} // xpdf::parser

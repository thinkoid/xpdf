// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#include <defs.hh>

#include <string>
#include <vector>

namespace xpdf {

std::vector< std::string >
split(const std::string &s, const std::string &delims)
{
    std::vector< std::string > xs;

    for (size_t first = 0, second; first < s.size(); first = second + 1) {
        second = s.find_first_of(delims, first);

        if (first != second)
            xs.emplace_back(s.substr(first, second - first));

        if (second == std::string::npos)
            break;
    }

    return xs;
}

} // namespace xpdf

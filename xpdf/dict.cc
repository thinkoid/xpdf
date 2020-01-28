// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstddef>
#include <cstring>

#include <goo/memory.hh>

#include <xpdf/obj.hh>
#include <xpdf/XRef.hh>
#include <xpdf/dict.hh>

#include <range/v3/algorithm/find_if.hpp>
using namespace ranges;

const auto sequential_find = [](auto& xs, auto& key) {
    return find_if (xs, [&](auto& x) { return std::get< 0 > (x) == key; });
};

////////////////////////////////////////////////////////////////////////

namespace xpdf {

void dict_t::emplace (const std::string& key, obj_t obj) {
    auto iter = sequential_find (*this, key);

    if (iter == end ()) {
        emplace_back (std::move (key), std::move (obj));
    }
    else {
        std::get< 1 > (*iter) = std::move (obj);
    }
}

bool dict_t::has_key (const std::string& s) const {
    return sequential_find (*this, s) != end ();
}

bool dict_t::has_type (const std::string& s) const {
    return has_key ("Type") && at ("Type").is_name (s);
}

xpdf::obj_t& dict_t::operator[] (const char* s) {
    auto iter = sequential_find (*this, s);

    if (iter == end ()) {
        emplace_back (std::string (s), xpdf::obj_t{ });
        iter = --end ();
    }

    return std::get< 1 > (*iter);
}

xpdf::obj_t& dict_t::at (const char* s) {
    auto iter = sequential_find (*this, s);

    if (iter == end ()) {
        throw std::out_of_range ("dict_t::at");
    }

    return std::get< 1 > (*iter);
}

const std::string& dict_t::key_at (size_t n) const {
    ASSERT (n < size ());
    return std::get< 0 > (this->operator[] (n));
}

obj_t& dict_t::val_at (size_t n) {
    ASSERT (n < size ());
    return std::get< 1 > (this->operator[] (n));
}

} // namespace xpdf

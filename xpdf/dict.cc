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

bool dict_t::has (const std::string& s) const {
    auto iter = sequential_find (*this, s);
    return iter != end () && std::get< 1 > (*iter).is_name (s);
}

bool dict_t::is (const char* type) {
    auto iter = sequential_find (*this, type);
    return iter != end () && std::get< 1 > (*iter).is_name (type);
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

obj_t* dict_t::lookupNF (const char* key, obj_t* pobj) {
    auto iter = sequential_find (*this, key);

    if (iter != end ()) {
        return *pobj = std::get< 1 > (*iter), pobj;
    }
    else {
        *pobj = { };
        return pobj;
    }
}

char* dict_t::getKey (int i) const {
    ASSERT (size_t (i) < size ());

    auto iter = begin ();
    std::advance (iter, size_t (i));

    return (char*)std::get< 0 > (*iter).c_str ();
}

obj_t* dict_t::getVal (int i, obj_t* pobj) {
    ASSERT (size_t (i) < size ());

    auto iter = begin ();
    std::advance (iter, size_t (i));

    auto& obj = std::get< 1 > (*iter);
    return *pobj = resolve (obj), pobj;
}

obj_t* dict_t::getValNF (int i, obj_t* pobj) {
    ASSERT (size_t (i) < size ());

    auto iter = begin ();
    std::advance (iter, size_t (i));

    return *pobj = std::get< 1 > (*iter), pobj;
}

} // namespace xpdf

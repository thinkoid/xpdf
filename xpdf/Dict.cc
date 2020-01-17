// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstddef>
#include <cstring>

#include <goo/memory.hh>

#include <xpdf/object.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Dict.hh>

#include <range/v3/algorithm/find_if.hpp>
using namespace ranges;

const auto sequential_find = [](auto& xs, auto& key) {
    return find_if (xs, [&](auto& x) { return std::get< 0 > (x) == key; });
};

////////////////////////////////////////////////////////////////////////

void Dict::add (const std::string& key, Object& obj) {
    auto iter = sequential_find (xs, key);

    if (iter == xs.end ()) {
        xs.emplace_back (std::string (key), obj);
    }
    else {
        std::get< 1 > (*iter) = obj;
    }
}

bool Dict::is (const char* type) {
    auto iter = sequential_find (xs, type);
    return iter != xs.end () && std::get< 1 > (*iter).isName (type);
}

Object* Dict::lookup (const char* key, Object* pobj, int recursion) {
    auto iter = sequential_find (xs, key);

    if (iter != xs.end ()) {
        return std::get< 1 > (*iter).fetch (xref, pobj, recursion);
    }
    else {
        return pobj->initNull ();
    }
}

Object* Dict::lookupNF (const char* key, Object* pobj) {
    auto iter = sequential_find (xs, key);

    if (iter != xs.end ()) {
        return *pobj = std::get< 1 > (*iter), pobj;
    }
    else {
        return pobj->initNull ();
    }
}

char* Dict::getKey (int i) const {
    ASSERT (size_t (i) < xs.size ());

    auto iter = xs.begin ();
    std::advance (iter, size_t (i));

    return (char*)std::get< 0 > (*iter).c_str ();
}

Object* Dict::getVal (int i, Object* pobj) {
    ASSERT (size_t (i) < xs.size ());

    auto iter = xs.begin ();
    std::advance (iter, size_t (i));

    return std::get< 1 > (*iter).fetch (xref, pobj);
}

Object* Dict::getValNF (int i, Object* pobj) {
    ASSERT (size_t (i) < xs.size ());

    auto iter = xs.begin ();
    std::advance (iter, size_t (i));

    return *pobj = std::get< 1 > (*iter), pobj;
}

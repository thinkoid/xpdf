// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdlib>
#include <cstddef>

#include <goo/memory.hh>

#include <xpdf/obj.hh>
#include <xpdf/array.hh>

#include <fmt/format.h>
using fmt::format;

namespace xpdf {

Object* array_t::get (int i, Object* obj) {
    if (size_t (i) < size ()) {
        auto iter = begin ();
        std::advance (iter, i);
        return iter->fetch (p_, obj);
    }
    else {
        return *obj = { }, obj;
    }
}

Object* array_t::getNF (int i, Object* obj) {
    if (size_t (i) < size ()) {
        auto iter = begin ();
        std::advance (iter, i);
        return *obj = *iter, obj;
    }
    else {
        return *obj = { }, obj;
    }
}

} // namespace xpdf

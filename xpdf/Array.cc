// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdlib>
#include <cstddef>

#include <goo/memory.hh>

#include <xpdf/obj.hh>
#include <xpdf/Array.hh>

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

Array::Array (XRef* xrefA) {
    xref = xrefA;
}

void Array::push_back (const Object& obj) {
    xs.push_back (obj);
}

void Array::push_back (Object&& obj) {
    xs.push_back (std::move (obj));
}

Object* Array::get (int i, Object* obj) {
    if (size_t (i) < xs.size ()) {
        auto iter = xs.begin ();
        std::advance (iter, i);
        return iter->fetch (xref, obj);
    }
    else {
        *obj = { };
        return obj;
    }
}

Object* Array::getNF (int i, Object* obj) {
    if (size_t (i) < xs.size ()) {
        auto iter = xs.begin ();
        std::advance (iter, i);
        return *obj = *iter, obj;
    }
    else {
        *obj = { };
        return obj;
    }
}

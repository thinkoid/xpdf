// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdlib>
#include <cstddef>

#include <goo/memory.hh>

#include <xpdf/object.hh>
#include <xpdf/Array.hh>

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

Array::Array (XRef* xrefA) {
    xref = xrefA;
}

void Array::add (Object* elem) {
    xs.push_back (*elem);
}

Object* Array::get (int i, Object* obj) {
    if (size_t (i) < xs.size ()) {
        auto iter = xs.begin ();
        std::advance (iter, i);
        return iter->fetch (xref, obj);
    }
    else {
        return obj->initNull ();
    }
}

Object* Array::getNF (int i, Object* obj) {
    if (size_t (i) < xs.size ()) {
        auto iter = xs.begin ();
        std::advance (iter, i);
        return iter->copy (obj);
    }
    else {
        return obj->initNull ();
    }
}

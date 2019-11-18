//========================================================================
//
// Array.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <cstdlib>
#include <cstddef>
#include <xpdf/Object.hh>
#include <xpdf/Array.hh>

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

Array::Array (XRef* xrefA) {
    xref = xrefA;
    elems = NULL;
    size = length = 0;
    ref = 1;
}

Array::~Array () {
    int i;

    for (i = 0; i < length; ++i) elems[i].free ();
    free (elems);
}

void Array::add (Object* elem) {
    if (length == size) {
        if (length == 0) { size = 8; }
        else {
            size *= 2;
        }
        elems = (Object*)reallocarray (elems, size, sizeof (Object));
    }
    elems[length] = *elem;
    ++length;
}

Object* Array::get (int i, Object* obj) {
    if (i < 0 || i >= length) {
        return obj->initNull ();
    }

    return elems[i].fetch (xref, obj);
}

Object* Array::getNF (int i, Object* obj) {
    if (i < 0 || i >= length) {
        return obj->initNull ();
    }
    return elems[i].copy (obj);
}

// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_ARRAY_HH
#define XPDF_XPDF_ARRAY_HH

#include <defs.hh>

#include <xpdf/ast_fwd.hh>
#include <xpdf/xpdf.hh>

#include <list>

class XRef;

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

class Array {
public:
    // Constructor.
    Array (XRef* xrefA);

    size_t size () const { return xs.size (); }

    // Add an element.
    void add (Object* elem);

    // Accessors.
    Object* get (int i, Object* obj);
    Object* getNF (int i, Object* obj);

private:
    XRef* xref;    // the xref table for this PDF file
    std::list< Object > xs;
};

#endif // XPDF_XPDF_ARRAY_HH

//========================================================================
//
// Array.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDF_XPDF_ARRAY_HH
#define XPDF_XPDF_ARRAY_HH

#include <defs.hh>

#include <boost/iterator/iterator_facade.hpp>
#include <range/v3/view/subrange.hpp>

class Object;
class XRef;

#include <xpdf/xpdf.hh>

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

class Array {
public:
    // Constructor.
    Array (XRef* xrefA);

    // Destructor.
    ~Array ();

    // Reference counting.
    int incRef () { return ++ref; }
    int decRef () { return --ref; }

    // Get number of elements.
    int getLength () { return length; }

    // Add an element.
    void add (Object* elem);

    // Accessors.
    Object* get (int i, Object* obj);
    Object* getNF (int i, Object* obj);

private:
    XRef* xref;    // the xref table for this PDF file
    Object* elems; // array of elements
    int size;      // size of <elems> array
    int length;    // number of elements in array
    int ref;       // reference count
};

#endif // XPDF_XPDF_ARRAY_HH

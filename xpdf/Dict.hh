// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_DICT_HH
#define XPDF_XPDF_DICT_HH

#include <defs.hh>

#include <string>
#include <tuple>
#include <vector>

#include <xpdf/obj_fwd.hh>

class XRef;
struct DictEntry;

//------------------------------------------------------------------------
// Dict
//------------------------------------------------------------------------

class Dict {
public:
    // Constructor.
    Dict (XRef* p) : xref (p) { }

    // Get number of entries.
    int getLength () const { return xs.size (); }

    //
    // Add an entry (taking ownership of key pointer):
    //
    void add (const char*, const Object&);
    void add (const char*, Object&&);

    void add (const char* key, Object* pobj) {
        add (key, *pobj);
    }

    //
    // Check if dictionary is of specified type:
    //
    bool is (const char* type);

    xpdf::obj_t& operator[] (const char*);

    xpdf::obj_t& at (const char*);
    const xpdf::obj_t& at (const char* s) const {
        return const_cast< Dict* > (this)->at (s);
    }

    //
    // Look up an entry and return the value.  Returns a null object
    // if <key> is not in the dictionary:
    //
    Object* lookupNF (const char*, Object*);

    //
    // Iterative accessors.
    //
    char* getKey (int i) const;

    Object* getVal (int i, Object* obj);
    Object* getValNF (int i, Object* obj);

    //
    // Set the xref pointer.  This is only used in one special case: the
    // trailer dictionary, which is read before the xref table is
    // parsed.
    //
    void setXRef (XRef* xrefA) { xref = xrefA; }

private:
    XRef* xref;          // the xref table for this PDF file
    std::vector< std::tuple< std::string, Object > > xs;
};

#endif // XPDF_XPDF_DICT_HH

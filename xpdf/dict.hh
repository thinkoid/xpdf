// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_DICT_HH
#define XPDF_XPDF_DICT_HH

#include <defs.hh>

#include <string>
#include <tuple>
#include <vector>

#include <xpdf/obj.hh>

namespace xpdf {

struct dict_t : std::vector< std::tuple< std::string, Object > > {
    using base_type = std::vector<
        std::tuple< std::string, Object > >;

    using base_type::base_type;
    using base_type::operator=;

    //
    // Same semantics with std::map::operator[]
    //
    xpdf::obj_t& operator[] (const char*);

    //
    // Same semantics with std::map::at
    //
    xpdf::obj_t& at (const char*);

    const xpdf::obj_t& at (const char* s) const {
        return const_cast< dict_t* > (this)->at (s);
    }

    //
    // Legacy interface:
    //

    // Get number of entries.
    size_t getLength () const { return size (); }

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
};

} // namespace xpdf

using Dict = xpdf::dict_t;

#endif // XPDF_XPDF_DICT_HH

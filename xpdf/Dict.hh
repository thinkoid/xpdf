//========================================================================
//
// Dict.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDF_XPDF_DICT_HH
#define XPDF_XPDF_DICT_HH

#include <defs.hh>

#include <cassert>

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <goo/unique_resource.hh>

class Object;
class XRef;

namespace xpdf {

struct dictionary_t {
    explicit dictionary_t (XRef*);

    // TODO: remove
    size_t incref () { return ++refcount_; }
    size_t decref () { return --refcount_; }

    void insert (const char*, Object*);

    // TODO: optional result
    Object* get (const char*, Object*);
    Object* fetch (const char*, Object*, int = 0);

    // TODO: remove
    Object* lookup (const char* s, Object* pobj, int recursion = 0) {
        assert (s && s [0]);
        assert (pobj);
        assert (0 <= recursion);

        return fetch (s, pobj, recursion);
    }

    Object* lookupNF (const char* s, Object* pobj) {
        assert (s && s [0]);
        assert (pobj);

        return get (s, pobj);
    }

    bool type_is (const char*) const;

    size_t size () const { return xs_.size (); }

    const char* key_at (size_t) const;

    // TODO: optional result
    Object* get_at (size_t, Object* pobj);
    Object* fetch_at (size_t, Object* pobj);

    // TODO: remove
    Object* getVal (size_t n, Object* pobj) {
        return fetch_at (n, pobj);
    }

    Object* getValNF (size_t n, Object* pobj) {
        return get_at (n, pobj);
    }

    void setXRef (XRef* pxref) { pxref_ = pxref; }

private:
    //
    // Xref table for this PDF file
    //
    XRef* pxref_;

    using object_deleter = std::function< void(Object&) >;
    using object_resource = std::unique_resource< Object, object_deleter >;

    std::vector< std::tuple< std::string, object_resource > > xs_;

    size_t refcount_;
};

} // namespace xpdf

using Dict = xpdf::dictionary_t;

#endif // XPDF_XPDF_DICT_HH

//========================================================================
//
// Dict.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <cstddef>
#include <cstring>

#include <xpdf/Object.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Dict.hh>

#include <range/v3/all.hpp>

namespace xpdf {
namespace detail {

template< typename T >
inline auto make_c_array_guard (T* p) {
    return std::unique_ptr< T, std::function< void(T*) > > (
        p, [&](T* p) { if (p) free ((void*)p); });
};

} // namespace detail

dictionary_t::dictionary_t (XRef* p) : pxref_ (p), refcount_ (1U) { }

void
dictionary_t::insert (const char* s, Object* pobj) {
    assert (s && s [0]);
    assert (pobj);

    auto guard = detail::make_c_array_guard (s);

    auto iter = ranges::find_if (xs_, [&](auto& xs) {
        return std::get< 0 >(xs) == s;
    });

    auto deleter = [](Object& obj) { obj.free (); };

    if (iter == xs_.end ()) {
        xs_.emplace_back (s, object_resource (*pobj, deleter));
    }
    else {
        auto& val = std::get<1> (*iter);
        val = object_resource (*pobj, deleter);
    }
}

bool
dictionary_t::type_is (const char* type) const {
    return ranges::any_of (xs_, [&](auto& xs) {
        const auto& [key, value] = xs;
        return key == "Type" && value.get ().isName (type);
    });
}

Object*
dictionary_t::fetch (const char* s, Object* pobj, int recursion) {
    assert (s && s [0]);

    auto iter = ranges::find_if (xs_, [&](auto& xs) {
        return std::get< 0 >(xs) == s; });

    assert (pobj);

    if (iter == xs_.end ()) {
        return pobj->initNull ();
    }

    auto& val = std::get< 1 > (*iter);

    assert (pxref_);
    return val.get ().fetch (pxref_, pobj, recursion);
}

Object*
dictionary_t::get (const char* s, Object* pobj) {
    assert (s && s [0]);

    auto iter = ranges::find_if (xs_, [&](auto& xs) {
        return std::get< 0 >(xs) == s; });

    assert (pobj);

    if (iter == xs_.end ()) {
        return pobj->initNull ();
    }

    auto& val = std::get< 1 > (*iter);
    return val.get ().copy (pobj);
}

const char*
dictionary_t::key_at (size_t n) const {
    return std::get< 0 > (xs_ [n]).c_str ();
}

Object*
dictionary_t::get_at (size_t n, Object* obj) {
    assert (n < xs_.size ());

    auto& [key, value] = xs_ [n];
    return value.get ().copy (obj);
}

Object*
dictionary_t::fetch_at (size_t n, Object* obj) {
    assert (pxref_);
    assert (n < xs_.size ());

    auto& [key, value] = xs_ [n];
    return value.get ().fetch (pxref_, obj);
}

} // namespace xpdf

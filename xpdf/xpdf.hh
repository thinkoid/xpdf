// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_XPDF_HH
#define XPDF_XPDF_XPDF_HH

#include <defs.hh>

#include <memory>
#include <tuple>
#include <vector>

#include <xpdf/array.hh>
#include <xpdf/dict.hh>
#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>

#include <fmt/format.h>
using fmt::format;

namespace xpdf {

template< typename T, typename ... U >
inline bool contains (T&& t, U&& ... u) { return ((t == u) || ...); }

template< typename T >
inline T as (Dict& dict, const char* s) {
    return dict.at (s).cast< T > ();
}

template< typename T >
inline T array_get (Object& arr, size_t i) {
    return arr [i].cast< T > ();
}

template< typename T >
inline auto as_array (Object& obj) {
    auto& src = obj.as_array ();

    std::vector< T > xs;

    for (size_t i = 0, n = src.size (); i < n; ++i) {
        xs.emplace_back (src [i].cast< T > ());
    }

    return xs;
}

template< >
inline auto as_array< size_t > (Object& obj) {
    auto& src = obj.as_array ();

    std::vector< size_t > xs;
    xs.reserve (src.size ());

    for (size_t i = 0, n = src.size (); i < n; ++i) {
        xs.emplace_back (src [i].cast< int > ());
    }

    return xs;
}

template< >
inline auto as_array< std::tuple< double, double > > (Object& obj) {
    using tuple_type = std::tuple< double, double >;

    if (!obj.is_array ()) {
        throw std::runtime_error ("not an array");
    }

    auto& src = obj.as_array ();

    if (src.empty ()) {
        return std::vector< tuple_type >{ };
    }

    std::vector< tuple_type > xs;

    auto iter = src.begin (), next = iter, last = src.end ();

    for (++next; iter != last && next != last; iter += 2, next += 2) {
        xs.emplace_back (iter->cast< double > (), next->cast< double > ());
    }

    if (iter != last) {
        xs.emplace_back (iter->cast< double > (), double{ });
    }

    return xs;
}

template< typename T >
inline auto as_array (Dict& dict, const char* s) {
    auto obj = resolve (dict [s]);

    if (obj.is_null ()) {
        throw std::runtime_error (format ("missing array \"{}\"", s));
    }

    return as_array< T > (obj);
}

template< typename T >
std::vector< T > maybe_array (Dict& dict, const char* s) {
    auto obj = resolve (dict [s]);

    if (obj.is_null ()) {
        return { };
    }

    return { as_array< T > (obj) };
}

} // namespace xpdf

#endif // XPDF_XPDF_XPDF_HH

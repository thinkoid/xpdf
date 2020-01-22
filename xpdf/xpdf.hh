// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_XPDF_HH
#define XPDF_XPDF_XPDF_HH

#include <defs.hh>

#include <memory>
#include <tuple>
#include <vector>

#include <xpdf/array_fwd.hh>
#include <xpdf/ArrayIterator.hh>
#include <xpdf/Dict.hh>
#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>

#include <range/v3/all.hpp>
using namespace ranges;

#include <fmt/format.h>
using fmt::format;

namespace xpdf {

template< typename T, typename ... U >
inline bool contains (T&& t, U&& ... u) { return ((t == u) || ...); }

template< typename T >
inline T as (Dict& dict, const char* s) {
    Object obj;

    if (0 == dict.lookup (s, &obj)) {
        throw std::runtime_error (format ("missing key {}", s));
    }

    return obj.cast< T > ();
}

template< typename T >
inline T array_get (Object& arr, size_t i) {
    Object tmp;

    if (0 == arr.arrayGet (i, &tmp)) {
        throw std::runtime_error (format ("invalid array or index {}", i));
    }

    return tmp.cast< T > ();
}

template< typename T >
inline auto as_array (Object& src) {
    std::vector< T > xs;

    for (size_t i = 0, n = src.arrayGetLength (); i < n; ++i) {
        xs.emplace_back (array_get< T > (src, i));
    }

    return xs;
}

template< >
inline auto as_array< size_t > (Object& src) {
    std::vector< size_t > xs;

    for (size_t i = 0, n = src.arrayGetLength (); i < n; ++i) {
        xs.emplace_back (array_get< int > (src, i));
    }

    return xs;
}

template< >
inline auto as_array< std::tuple< double, double > > (Object& src) {
    std::vector< std::tuple< double, double > > xs;

    if (!src.is_array ()) {
        throw std::runtime_error ("not an array");
    }

    auto rng = xpdf::make_array_subrange< double > (src);

    transform (rng | views::chunk (2), back_inserter (xs), [](auto arg) {
        return std::make_tuple (arg [0], arg [1]);
    });

    return xs;
}

template< typename T >
inline auto as_array (Dict& dict, const char* s) {
    Object obj;

    if (0 == dict.lookup (s, &obj) || obj.is_null ()) {
        throw std::runtime_error (format ("missing array \"{}\"", s));
    }

    return as_array< T > (obj);
}

template< typename T >
std::vector< T > maybe_array (Dict& dict, const char* s) {
    Object obj;

    if (0 == dict.lookup (s, &obj) || obj.is_null ()) {
        return { };
    }

    return { as_array< T > (obj) };
}

} // namespace xpdf

#endif // XPDF_XPDF_XPDF_HH

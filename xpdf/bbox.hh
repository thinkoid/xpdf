// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_BBOX_HH
#define XPDF_XPDF_BBOX_HH

#include <defs.hh>
#include <cmath>

namespace xpdf {
namespace detail {

//
// Bounding box, described by 4 coordinates of two points, a `bottom-left' and a
// `top-right' (if (0,0) is at bottom-left), or `top-left' and `bottom-right'
// (when (0,0) is at top-left):
//
template< typename T >
struct bbox_t {
    using value_type = T;
    union {
        value_type arr [4];
        struct { value_type x, y; } point [2];
    };
};

} // namespace detail

template<
    typename T, typename U,
    typename std::enable_if_t< std::is_constructible_v< T, U > >* = nullptr
    >
inline detail::bbox_t< T >
to (const detail::bbox_t< U >& x) {
    return detail::bbox_t< T >{
        T (x.arr [0]), T (x.arr [1]), T (x.arr [2]), T (x.arr [3])
    };
}

template<
    typename T,
    typename std::enable_if_t< std::is_floating_point_v< T > >* = nullptr
    >
inline detail::bbox_t< T >
ceil (const detail::bbox_t< T >& x) {
    //
    // Stretches the box to the nearest integer margins:
    //
    return detail::bbox_t< T >{
        std::floor (x.arr [0]),
        std::floor (x.arr [1]),
        std::ceil  (x.arr [2]),
        std::ceil  (x.arr [3])
    };
}

template<
    typename T,
    typename std::enable_if_t< std::is_floating_point_v< T > >* = nullptr
    >
inline detail::bbox_t< T >
floor (const detail::bbox_t< T >& x) {
    //
    // Shrinks the box to the nearest integer margins:
    //
    return detail::bbox_t< T >{
        std::ceil  (x.arr [0]),
        std::ceil  (x.arr [1]),
        std::floor (x.arr [2]),
        std::floor (x.arr [3])
    };
}

//
// The default bounding box type is the floating point specialization for
// double:
//
using bbox_t  = detail::bbox_t< double >;

using bboxi_t = detail::bbox_t< int >;
using bboxu_t = detail::bbox_t< int unsigned >;

} // namespace xpdf

#endif // XPDF_XPDF_BBOX_HH

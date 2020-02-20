// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_BBOX_HH
#define XPDF_XPDF_BBOX_HH

#include <defs.hh>

#include <cmath>
#include <algorithm>

namespace xpdf {

enum struct rotation_t {
    none, quarter_turn, half_turn, three_quarters_turn
};

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

template< typename T >
inline bool
operator== (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return std::equal (
        lhs.arr, lhs.arr + sizeof lhs.arr / sizeof *lhs.arr, rhs.arr);
}

template< typename T >
inline bool
operator!= (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return !(lhs == rhs);
}

template< typename T >
inline std::ostream&
operator<< (std::ostream& ss, const bbox_t< T >& box) {
    return ss
        << box.arr [0] << ","
        << box.arr [1] << ","
        << box.arr [2] << ","
        << box.arr [3];
}

template< xpdf::rotation_t, typename > struct rotate_t;

template< typename T >
struct rotate_t< xpdf::rotation_t::none, T > {
    using box_type = bbox_t< T >;
    box_type operator() (box_type x, const box_type&) const { return x; }
};

#define XPDF_ROTATE_DEF(type, a, b, c, d)                       \
template< typename T >                                          \
struct rotate_t< xpdf::rotation_t::type, T > {                  \
    using box_type = bbox_t< T >;                               \
    box_type operator() (box_type x, const box_type& X) const { \
        auto& [ x0, y0, x1, y1 ] = x.arr;                       \
        auto& [ X0, Y0, X1, Y1 ] = X.arr;                       \
        return { a, b, c, d };                                  \
    }                                                           \
}

XPDF_ROTATE_DEF (       quarter_turn,      y0, X1 - x1,      y1, X1 - x0);
XPDF_ROTATE_DEF (          half_turn, X1 - x1, Y1 - y1, X1 - x0, Y1 - y0);
XPDF_ROTATE_DEF (three_quarters_turn, Y1 - y1,      x0, Y1 - y0,      x1);

#undef XPDF_ROTATE_DEF

} // namespace detail

////////////////////////////////////////////////////////////////////////

//
// The default bounding box type is the floating point specialization for
// double:
//
using bbox_t  = detail::bbox_t< double >;

using bboxi_t = detail::bbox_t< int >;
using bboxu_t = detail::bbox_t< int unsigned >;

////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////

template< rotation_t rotation, typename T >
inline detail::bbox_t< T >
rotate (detail::bbox_t< T > box, const detail::bbox_t< T >& superbox) {
    //
    // Rotation around origin followed by a translation, brings the box
    // `upright':
    //
    return detail::rotate_t< rotation, T > ()(box, superbox);
}

} // namespace xpdf

#endif // XPDF_XPDF_BBOX_HH

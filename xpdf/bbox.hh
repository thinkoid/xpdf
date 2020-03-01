// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_BBOX_HH
#define XPDF_XPDF_BBOX_HH

#include <defs.hh>
#include <cmath>

#include <iostream>
#include <vector>

namespace xpdf {

enum struct rotation_t {
    none, quarter_turn, half_turn, three_quarters_turn
};

namespace detail {

template< typename > struct bbox_t;

template< typename T >
struct point_t {
    using value_t = T;
    value_t x, y;

    template< typename U >
    bool in (const bbox_t< U >&) const;
};

//
// Bounding box, described by 4 coordinates of two points, a `bottom-left' and a
// `top-right' (if (0,0) is at bottom-left), or `top-left' and `bottom-right'
// (when (0,0) is at top-left):
//
template< typename T >
struct bbox_t {
    using value_type = T;
    using point_type = point_t< T >;

    union {
        value_type arr [4];
        point_type point [2];
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
inline bbox_t< T >
operator+ (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return {
        (std::min) (lhs.arr [0], rhs.arr [0]),
        (std::min) (lhs.arr [1], rhs.arr [1]),
        (std::max) (lhs.arr [2], rhs.arr [2]),
        (std::max) (lhs.arr [3], rhs.arr [3])
    };
}

template< typename T >
inline bbox_t< T >&
operator+= (bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return (
        lhs = {
            (std::min) (lhs.arr [0], rhs.arr [0]),
            (std::min) (lhs.arr [1], rhs.arr [1]),
            (std::max) (lhs.arr [2], rhs.arr [2]),
            (std::max) (lhs.arr [3], rhs.arr [3])
        });
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

////////////////////////////////////////////////////////////////////////

template< typename T >
inline bbox_t< T >
normalize (bbox_t< T > x) {
    if (x.arr [0] > x.arr [2]) { std::swap (x.arr [0], x.arr [2]); }
    if (x.arr [1] > x.arr [3]) { std::swap (x.arr [1], x.arr [3]); }
    return x;
}

template< typename T >
inline T width_of (const bbox_t< T >& x) { return x.arr [2] - x.arr [0]; }

template< typename T >
inline T height_of (const bbox_t< T >& x) { return x.arr [3] - x.arr [1]; }

template< typename T >
inline T
horizontal_overlap (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    const auto dist =
        (std::min) (lhs.arr [2], rhs.arr [2]) -
        (std::max) (lhs.arr [0], rhs.arr [0]);
    return dist > 0 ? dist : 0;
}

template< typename T >
inline double
vertical_overlap (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    const auto dist =
        (std::min) (lhs.arr [3], rhs.arr [3]) -
        (std::max) (lhs.arr [1], rhs.arr [1]);
    return dist > 0 ? dist : 0;
}

template< typename T >
inline bool
overlapping (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return horizontal_overlap (lhs, rhs) && vertical_overlap (lhs, rhs);
}

template< typename T >
inline T
horizontal_distance (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return lhs.arr [2] < rhs.arr [0]
        ? rhs.arr [0] - lhs.arr [2]
        : rhs.arr [2] < lhs.arr [0] ? lhs.arr [0] - rhs.arr [2] : 0;
}

template< typename T >
inline T
vertical_distance (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return lhs.arr [3] < rhs.arr [1]
        ? rhs.arr [1] - lhs.arr [3]
        : rhs.arr [3] < lhs.arr [1] ? lhs.arr [1] - rhs.arr [3] : 0;
}

template< typename T >
inline T
min_height_of (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return (std::min) (height_of (lhs), height_of (rhs));
}

template< typename T >
inline bbox_t< T >
coalesce (const bbox_t< T >& lhs, const bbox_t< T >& rhs) {
    return bbox_t< T >{
        (std::min) (lhs.arr [0], rhs.arr [0]),
        (std::min) (lhs.arr [1], rhs.arr [1]),
        (std::max) (lhs.arr [2], rhs.arr [2]),
        (std::max) (lhs.arr [3], rhs.arr [3])
    };
}

template< typename T >
template< typename U >
bool point_t< T >::in (const bbox_t< U >& box) const {
    return
        box.arr [0] <= x && x < box.arr [2] &&
        box.arr [1] <= y && y < box.arr [3];
}

template< xpdf::rotation_t, typename >
struct rotate_t;

template< typename T >
struct rotate_t< xpdf::rotation_t::none, T > {
    using box_type = bbox_t< T >;
    box_type operator() (const box_type& x, const box_type&) const {
        return x;
    }
};

//
// In the definition of the specialization `x' stands for the box that is
// rotated, and `X' stands for the `superbox', i.e., the page or sheet that
// hosts the box. E.g., under a 90° rotation the new x_min is the former y_min,
// but y_min is the width of the page less the former y_max, etc.:
//
#define XPDF_ROTATE_DEF(type, a, b, c, d)                               \
template< typename T >                                                  \
struct rotate_t< xpdf::rotation_t::type, T > {                          \
    using box_type = bbox_t< T >;                                       \
    box_type operator() (const box_type& x, const box_type& X) const {  \
        const auto& [ x0, y0, x1, y1 ] = x.arr;                         \
        const auto& [ X0, Y0, X1, Y1 ] = X.arr;                         \
        return { a, b, c, d };                                          \
    }                                                                   \
}

XPDF_ROTATE_DEF (       quarter_turn,      y0, X1 - x1,      y1, X1 - x0);
XPDF_ROTATE_DEF (          half_turn, X1 - x1, Y1 - y1, X1 - x0, Y1 - y0);
XPDF_ROTATE_DEF (three_quarters_turn, Y1 - y1,      x0, Y1 - y0,      x1);

#undef XPDF_ROTATE_DEF

template< xpdf::rotation_t, typename >
struct unrotate_t;

template< typename T >
struct unrotate_t< xpdf::rotation_t::none, T > {
    using box_type = bbox_t< T >;
    box_type operator() (const box_type& x, const box_type&) const {
        return x;
    }
};

#define XPDF_UNROTATE_DEF(type, a, b, c, d)                             \
template< typename T >                                                  \
struct unrotate_t< xpdf::rotation_t::type, T > {                        \
    using box_type = bbox_t< T >;                                       \
    box_type operator() (const box_type& x, const box_type& X) const {  \
        const auto& [ x0, y0, x1, y1 ] = x.arr;                         \
        const auto& [ X0, Y0, X1, Y1 ] = X.arr;                         \
        return { a, b, c, d };                                          \
    }                                                                   \
}

XPDF_UNROTATE_DEF (       quarter_turn, X1 - y1,      x0, X1 - y0,      x1);
XPDF_UNROTATE_DEF (          half_turn, X1 - x1, Y1 - y1, X1 - x0, Y1 - y0);
XPDF_UNROTATE_DEF (three_quarters_turn,      y0, Y1 - x1,      y1, Y1 - x0);

#undef XPDF_UNROTATE_DEF

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

template< typename T >
using enable_if_floating_point = std::enable_if< std::is_floating_point_v< T > >;

template< typename T >
using enable_if_floating_point_t = typename enable_if_floating_point< T >::type;

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

template< typename T, enable_if_floating_point_t< T >* = nullptr >
inline detail::bbox_t< T >
ceil (const detail::bbox_t< T >& x) {
    const auto& [ a, b, c, d ] = x.arr;
    return detail::bbox_t< T >{
        std::floor (a), std::floor (b), std::ceil (c), std::ceil (d)
    };
}

template< typename T, enable_if_floating_point_t< T >* = nullptr >
inline detail::bbox_t< T >
floor (const detail::bbox_t< T >& x) {
    const auto& [ a, b, c, d ] = x.arr;
    return detail::bbox_t< T >{
        std::ceil (a), std::ceil (b), std::floor (c), std::floor (d)
    };
}

////////////////////////////////////////////////////////////////////////

template< rotation_t rotation, typename T >
inline detail::bbox_t< T >
rotate (const detail::bbox_t< T >& box, const detail::bbox_t< T >& superbox) {
    //
    // Rotation around origin followed by a translation, brings the box
    // `upright':
    //
    return detail::rotate_t< rotation, T > ()(box, superbox);
}

template< rotation_t rotation, typename T >
inline detail::bbox_t< T >
unrotate (const detail::bbox_t< T >& box, const detail::bbox_t< T >& superbox) {
    //
    // Rotation around origin followed by a translation, brings the box
    // `upright':
    //
    return detail::unrotate_t< rotation, T > ()(box, superbox);
}

} // namespace xpdf

#endif // XPDF_XPDF_BBOX_HH

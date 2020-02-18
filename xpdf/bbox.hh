// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_BBOX_HH
#define XPDF_XPDF_BBOX_HH

#include <defs.hh>

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

//
// The default bounding box is the floating point specialization:
//
using bbox_t  = detail::bbox_t< double >;

using bboxi_t = detail::bbox_t< int >;
using bboxu_t = detail::bbox_t< int unsigned >;

} // namespace xpdf

#endif // XPDF_XPDF_BBOX_HH

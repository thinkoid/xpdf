// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_BBOX_HH
#define XPDF_XPDF_BBOX_HH

#include <defs.hh>

namespace xpdf {

//
// Bounding box, described by 4 coordinates of two points, a `bottom-left' and a
// `top-right' (if (0,0) is at bottom-left), or `top-left' and `bottom-right'
// (when (0,0) is at top-left):
//
struct bbox_t {
    using value_type = double;
    union {
        value_type arr [4];
        struct { value_type x, y; } point [2];
    };
};

} // namespace xpdf

#endif // XPDF_XPDF_BBOX_HH

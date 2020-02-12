// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_BBOX_HH
#define XPDF_XPDF_BBOX_HH

#include <defs.hh>

namespace xpdf {

//
// Bounding box, described by 4 floating-point coordinates of two points, a
// bottom-left and a top-right:
//
struct bbox_t {
    union {
        double arr [4];
        struct { double x, y; } point [2];
    };
};

} // namespace xpdf

#endif // XPDF_XPDF_BBOX_HH

// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_OBJECT_FWD_HH
#define XPDF_XPDF_OBJECT_FWD_HH

#include <defs.hh>

namespace xpdf {

struct ref_t;
struct object_t;

} // namespace xpdf

using Object = xpdf::object_t;
using Ref = xpdf::ref_t;

#endif // XPDF_XPDF_OBJECT_FWD_HH

// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_ARRAY_HH
#define XPDF_XPDF_ARRAY_HH

#include <defs.hh>

#include <vector>
#include <xpdf/obj_fwd.hh>

namespace xpdf {

struct array_t : std::vector< obj_t > {
    using base_type = std::vector< obj_t >;

    using base_type::base_type;
    using base_type::operator=;

    using base_type::operator[];
};

} // namespace xpdf

#endif // XPDF_XPDF_ARRAY_HH

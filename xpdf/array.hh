// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_ARRAY_HH
#define XPDF_XPDF_ARRAY_HH

#include <defs.hh>

#include <xpdf/obj_fwd.hh>
#include <xpdf/xpdf.hh>

#include <list>

class XRef;

namespace xpdf {

struct array_t : std::vector< obj_t > {
    using base_type = std::vector< obj_t >;

    using base_type::base_type;
    using base_type::operator=;

    explicit array_t (XRef* p) : p_ (p) { }

    //
    // Legacy accessors:
    //
    Object* get (int i, Object* obj);
    Object* getNF (int i, Object* obj);

private:
    XRef* p_;    // the xref table for this PDF file
};

} // namespace xpdf

#endif // XPDF_XPDF_ARRAY_HH

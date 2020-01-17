// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_FUNCTION_HH
#define XPDF_XPDF_FUNCTION_HH

#include <defs.hh>
#include <memory>

#include <xpdf/object_fwd.hh>

namespace xpdf {

struct function_t {
    struct impl_t;

    static constexpr size_t max_recursion  =  8UL;

    static constexpr size_t max_arity          = 32UL;
    static constexpr size_t max_sampled_arity  = 16UL;

public:
    function_t () { }

    void operator() (const double*, const double* const, double*) const;

    size_t   arity () const;
    size_t coarity () const;

    std::string to_ps () const;

    operator bool () const { return bool (p_); }

private:
    std::shared_ptr< impl_t > p_;

private:
    function_t (std::shared_ptr< impl_t > p) : p_ (p) { }
    friend function_t make_function (Object&);
};

function_t make_function (Object&);

} // namespace xpdf

using Function = xpdf::function_t;

#endif // XPDF_XPDF_FUNCTION_HH

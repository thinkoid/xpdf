// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_OBJECT_FWD_HH
#define XPDF_XPDF_OBJECT_FWD_HH

#include <defs.hh>

namespace xpdf {
namespace ast {

struct ref_t;
struct object_t;

}} // namespace xpdf::ast

using Object = xpdf::ast::object_t;
using Ref = xpdf::ast::ref_t;

#endif // XPDF_XPDF_OBJECT_FWD_HH

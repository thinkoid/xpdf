// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_DEFS_HH
#define XPDF_DEFS_HH

#include <config.hh>

#define TO_S(x) #x

#define XPDF_DO_CAT(a, b) a ## b
#define XPDF_CAT(a, b) XPDF_DO_CAT(a, b)

#include <boost/assert.hpp>

#define XPDF_ASSERT BOOST_ASSERT
#define ASSERT XPDF_ASSERT

#endif // XPDF_DEFS_HH

// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_BITPACK_HH
#define XPDF_XPDF_BITPACK_HH

#include <defs.hh>

#include <vector>

namespace xpdf {

std::vector< double >
unpack (const char*, const char* const, size_t, size_t);

} // namespace xpdf

#endif // XPDF_XPDF_BITPACK_HH

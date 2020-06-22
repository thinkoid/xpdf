// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC

#ifndef XPDF_UTILS_MEMORY_HH
#define XPDF_UTILS_MEMORY_HH

#include <defs.hh>
#include <stddef.h>

#if defined(__APPLE__)
void *reallocarray(void *, size_t, size_t);
#endif // __APPLE__

#endif // XPDF_UTILS_MEMORY_HH

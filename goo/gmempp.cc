//========================================================================
//
// gmempp.cc
//
// Use gmalloc/gfree for C++ new/delete operators.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>
#include <goo/gmem.hh>

#ifdef DEBUG_MEM

void *operator new(size_t size) {
  return gmalloc((int)size);
}

void *operator new[](size_t size) {
  return gmalloc((int)size);
}

void operator delete(void *p) {
  gfree(p);
}

void operator delete[](void *p) {
  gfree(p);
}

#endif

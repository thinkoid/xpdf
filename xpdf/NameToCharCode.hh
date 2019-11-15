//========================================================================
//
// NameToCharCode.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef NAMETOCHARCODE_H
#define NAMETOCHARCODE_H

#include <config.hh>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <xpdf/CharTypes.hh>

struct NameToCharCodeEntry;

//------------------------------------------------------------------------

class NameToCharCode {
public:

  NameToCharCode();
  ~NameToCharCode();

  void add(const char *name, CharCode c);
  CharCode lookup(const char *name);

private:

  int hash(const char *name);

  NameToCharCodeEntry *tab;
  int size;
  int len;
};

#endif
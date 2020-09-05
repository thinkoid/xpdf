// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_NAMETOCHARCODE_HH
#define XPDF_XPDF_NAMETOCHARCODE_HH

#include <defs.hh>

#include <xpdf/CharTypes.hh>

#include <boost/noncopyable.hpp>

struct NameToCharCodeEntry;

class NameToCharCode : boost::noncopyable
{
public:
    NameToCharCode();
    ~NameToCharCode();

    void     add(const char *name, CharCode c);
    CharCode lookup(const char *name);

private:
    int hash(const char *name);

    NameToCharCodeEntry *tab;
    int                  size;
    int                  len;
};

#endif // XPDF_XPDF_NAMETOCHARCODE_HH

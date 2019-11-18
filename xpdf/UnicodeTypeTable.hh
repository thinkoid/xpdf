//========================================================================
//
// UnicodeTypeTable.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#ifndef UNICODETYPETABLE_H
#define UNICODETYPETABLE_H


extern bool unicodeTypeL (Unicode c);

extern bool unicodeTypeR (Unicode c);

extern bool unicodeTypeNum (Unicode c);

extern bool unicodeTypeAlphaNum (Unicode c);

extern bool unicodeTypeWord (Unicode c);

extern Unicode unicodeToUpper (Unicode c);

#endif

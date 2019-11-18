//========================================================================
//
// FoFiEncodings.h
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef FOFIENCODINGS_H
#define FOFIENCODINGS_H

#include <defs.hh>


//------------------------------------------------------------------------
// Type 1 and 1C font data
//------------------------------------------------------------------------

extern const char* fofiType1StandardEncoding[256];
extern const char* fofiType1ExpertEncoding[256];

//------------------------------------------------------------------------
// Type 1C font data
//------------------------------------------------------------------------

extern const char* fofiType1CStdStrings[391];
extern unsigned short fofiType1CISOAdobeCharset[229];
extern unsigned short fofiType1CExpertCharset[166];
extern unsigned short fofiType1CExpertSubsetCharset[87];

#endif

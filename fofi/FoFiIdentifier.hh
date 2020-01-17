// -*- mode: c++; -*-
// Copyright 2009 Glyph & Cog, LLC

#ifndef XPDF_FOFI_FOFIIDENTIFIER_HH
#define XPDF_FOFI_FOFIIDENTIFIER_HH

#include <defs.hh>

class GList;

//------------------------------------------------------------------------
// FoFiIdentifier
//------------------------------------------------------------------------

enum FoFiIdentifierType {
    fofiIdType1PFA,           // Type 1 font in PFA format
    fofiIdType1PFB,           // Type 1 font in PFB format
    fofiIdCFF8Bit,            // 8-bit CFF font
    fofiIdCFFCID,             // CID CFF font
    fofiIdTrueType,           // TrueType font
    fofiIdTrueTypeCollection, // TrueType collection
    fofiIdOpenTypeCFF8Bit,    // OpenType wrapper with 8-bit CFF font
    fofiIdOpenTypeCFFCID,     // OpenType wrapper with CID CFF font
    fofiIdDfont,              // Mac OS X dfont
    fofiIdUnknown,            // unknown type
    fofiIdError               // error in reading the file
};

class FoFiIdentifier {
public:
    // Identify a font file.
    static FoFiIdentifierType identifyMem (const char*, int);
    static FoFiIdentifierType identifyFile (const char*);
    static FoFiIdentifierType identifyStream (int (*) (void*), void*);
};

#endif // XPDF_FOFI_FOFIIDENTIFIER_HH

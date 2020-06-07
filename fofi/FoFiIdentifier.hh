// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_FOFI_FOFIIDENTIFIER_HH
#define XPDF_FOFI_FOFIIDENTIFIER_HH

#include <defs.hh>

#include <cstddef>

namespace xpdf {

enum font_type {
    FONT_TYPE1_PFA,           // Type 1 font in PFA format
    FONT_TYPE1_PFB,           // Type 1 font in PFB format
    FONT_CFF_8BIT,            // 8-bit CFF font
    FONT_CFF_CID,             // CID CFF font
    FONT_TRUETYPE,           // TrueType font
    FONT_TRUETYPE_COLLECTION, // TrueType collection
    FONT_OPENTYPE_CFF_8BIT,    // OpenType wrapper with 8-bit CFF font
    FONT_OPENTYPE_CFF_CID,     // OpenType wrapper with CID CFF font
    FONT_DFONT,              // Mac OS X dfont
    FONT_UNKNOWN,            // unknown type
    FONT_ERROR               // error in reading the file
};

bool font_identify_byextension (const char*, font_type&);
bool font_identify_bycontent (const char*, font_type&);
bool font_identify (const char*, font_type&);
bool font_identify (const char*, size_t, font_type&);

} // namespace xpdf

#endif // XPDF_FOFI_FOFIIDENTIFIER_HH

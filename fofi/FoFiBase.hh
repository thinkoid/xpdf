// -*- mode: c++; -*-
// Copyright 1999-2003 Glyph & Cog, LLC

#ifndef XPDF_FOFI_FOFIBASE_HH
#define XPDF_FOFI_FOFIBASE_HH

#include <defs.hh>

//------------------------------------------------------------------------

typedef void (*FoFiOutputFunc) (void* stream, const char* data, int len);

//------------------------------------------------------------------------
// FoFiBase
//------------------------------------------------------------------------

class FoFiBase {
public:
    virtual ~FoFiBase ();

protected:
    FoFiBase (char* fileA, int lenA, bool freeFileDataA);
    // TODO: std buffer
    static char* readFile (const char* fileName, int* fileLen);

    // S = signed / U = unsigned
    // 8/16/32/Var = word length, in bytes
    // BE = big endian
    int getS8 (int pos, bool* ok);
    int getU8 (int pos, bool* ok);
    int getS16BE (int pos, bool* ok);
    int getU16BE (int pos, bool* ok);
    int getS32BE (int pos, bool* ok);
    unsigned getU32BE (int pos, bool* ok);
    unsigned getU32LE (int pos, bool* ok);
    unsigned getUVarBE (int pos, int size, bool* ok);

    bool checkRegion (int pos, int size);

    unsigned char* fileData;
    unsigned char* file;
    int len;
    bool freeFileData;
};

#endif // XPDF_FOFI_FOFIBASE_HH

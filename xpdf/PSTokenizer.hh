// -*- mode: c++; -*-
// Copyright 2002-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_PSTOKENIZER_HH
#define XPDF_XPDF_PSTOKENIZER_HH

#include <defs.hh>

//------------------------------------------------------------------------

class PSTokenizer {
public:
    PSTokenizer (int (*getCharFuncA) (void*), void* dataA);
    ~PSTokenizer ();

    // Get the next PostScript token.  Returns false at end-of-stream.
    bool getToken (char* buf, int size, int* length);

private:
    int peek ();
    int get ();

    int (*getCharFunc) (void*);
    void* data;
    int charBuf;
};

#endif // XPDF_XPDF_PSTOKENIZER_HH

// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_LEGACY_PARSER_HH
#define XPDF_XPDF_LEGACY_PARSER_HH

#include <defs.hh>

#include <xpdf/Lexer.hh>

//------------------------------------------------------------------------
// Parser
//------------------------------------------------------------------------

class Parser
{
public:
    // Constructor.
    Parser(XRef *xrefA, Lexer *lexerA, bool allowStreamsA);

    // Destructor.
    ~Parser();

    // Get the next object from the input stream.  If <simpleOnly> is
    // true, do not parse compound objects (arrays, dictionaries, or
    // streams).
    Object *getObj(Object *obj, bool simpleOnly = false,
                   unsigned char *fileKey = NULL,
                   CryptAlgorithm encAlgorithm = cryptRC4, int keyLength = 0,
                   int objNum = 0, int objGen = 0, int recursion = 0);

    // Get stream.
    Stream *as_stream() { return lexer->as_stream(); }

    // Get current position in file.
    off_t tellg() { return lexer->tellg(); }

private:
    XRef *xref; // the xref table for this PDF file

    Lexer *        lexer;
    Lexer::token_t buf1, buf2; // next two tokens

    bool allowStreams; // parse stream objects?
    int  inlineImg; // set when inline image data is encountered

    Stream *makeStream(Object *dict, unsigned char *fileKey,
                       CryptAlgorithm encAlgorithm, int keyLength, int objNum,
                       int objGen, int recursion);
    void    shift();
};

#endif // XPDF_XPDF_LEGACY_PARSER_HH

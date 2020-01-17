// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_LEXER_HH
#define XPDF_XPDF_LEXER_HH

#include <defs.hh>

#include <xpdf/object.hh>
#include <xpdf/Stream.hh>

class XRef;

//------------------------------------------------------------------------
// lexer_t
//------------------------------------------------------------------------

namespace xpdf {

struct lexer_t {
    struct token_t {
        enum {
            ERROR_,
            EOF_,
            NULL_,
            BOOL_,
            INT_,
            REAL_,
            STRING_,
            NAME_,
            KEYWORD_
        } type;
        std::string s;
    };

    // Construct a lexer for a single stream.  Deletes the stream when
    // lexer is deleted.
    lexer_t (XRef* xref, Stream* str);

    // Construct a lexer for a stream or array of streams (assumes obj
    // is either a stream or array of streams).
    lexer_t (XRef* xref, Object* obj);

    // Destructor.
    ~lexer_t ();

    // Get the next object from the input stream.
    token_t next ();

    // Skip to the beginning of the next line in the input stream.
    void skipToNextLine ();

    // Skip over one character.
    void skipChar () { getChar (); }

    // Get stream.
    Stream* getStream () {
        return curStr.isNone () ? (Stream*)NULL : curStr.getStream ();
    }

    // Get current position in file.
    GFileOffset getPos () {
        return curStr.isNone () ? -1 : curStr.streamGetPos ();
    }

    // Set position in file.
    void setPos (GFileOffset pos, int dir = 0) {
        if (!curStr.isNone ()) curStr.streamSetPos (pos, dir);
    }

    // Returns true if <c> is a whitespace character.
    static bool isSpace (int c);

private:
    int getChar ();
    int lookChar ();

    Array* streams;          // array of input streams
    int strPtr;              // index of current stream
    Object curStr;           // current stream
    bool freeArray;         // should lexer free the streams array?

#define tokBufSize 128 // size of token buffer
    char tokBuf[tokBufSize]; // temporary token buffer
};

} // namespace xpdf

#endif // XPDF_XPDF_LEXER_HH

// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_LEXER_HH
#define XPDF_XPDF_LEXER_HH

#include <defs.hh>

#include <xpdf/obj.hh>
#include <xpdf/array.hh>
#include <xpdf/Stream.hh>

class XRef;

//------------------------------------------------------------------------
// Lexer
//------------------------------------------------------------------------

struct Lexer
{
    struct token_t
    {
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
    Lexer(Stream *str);

    // Construct a lexer for a stream or array of streams (assumes obj
    // is either a stream or array of streams).
    Lexer(Object *obj);

    // Destructor.
    ~Lexer();

    // Get the next object from the input stream.
    token_t next();

    // Skip to the beginning of the next line in the input stream.
    void skipToNextLine();

    // Skip over one character.
    void skipChar() { get(); }

    // Get stream.
    Stream *as_stream()
    {
        return curStr.is_none() ? (Stream *)NULL : curStr.as_stream();
    }

    // Get current position in file.
    off_t tellg() { return curStr.is_none() ? -1 : curStr.streamGetPos(); }

    // Set position in file.
    void seekg(off_t pos, int dir = 0)
    {
        if (!curStr.is_none())
            curStr.streamSetPos(pos, dir);
    }

    // Returns true if <c> is a whitespace character.
    static bool isSpace(int c);

private:
    int get();
    int peek();

private:
    Array  streams; // array of input streams
    Object curStr; // current stream

    size_t strPtr; // index of current stream
};

#endif // XPDF_XPDF_LEXER_HH

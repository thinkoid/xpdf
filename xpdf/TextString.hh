// -*- mode: c++; -*-
// Copyright 2011-2013 Glyph & Cog, LLC

#ifndef XPDF_XPDF_TEXTSTRING_HH
#define XPDF_XPDF_TEXTSTRING_HH

#include <defs.hh>

#include <xpdf/CharTypes.hh>

class TextString {
public:
    // Create an empty TextString.
    TextString ();

    // Create a TextString from a PDF text string.
    TextString (GString* s);

    // Copy a TextString.
    TextString (TextString* s);

    ~TextString ();

    // Append a Unicode character or PDF text string to this TextString.
    TextString* append (Unicode c);
    TextString* append (GString* s);

    // Insert a Unicode character or PDF text string in this TextString.
    TextString* insert (int idx, Unicode c);
    TextString* insert (int idx, GString* s);

    // Get the Unicode characters in the TextString.
    int getLength () { return len; }
    Unicode* getUnicode () { return u; }

    // Create a PDF text string from a TextString.
    GString* toPDFTextString ();

private:
    void expand (int delta);

    Unicode* u; // NB: not null-terminated
    int len;
    int size;
};

#endif // XPDF_XPDF_TEXTSTRING_HH

//========================================================================
//
// GString.h
//
// Simple variable-length string type.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef GSTRING_H
#define GSTRING_H

#include <defs.hh>

#include <climits> // for LLONG_MAX and ULLONG_MAX
#include <cstdarg>

#include <string>


class GString : private std::string {
public:
    // Create an empty string.
    GString () = default;

    // Destructor.
    ~GString () = default;

    GString (GString&& other) = default;
    GString& operator= (GString&& other) = default;

    GString (const GString& other) = delete;
    GString& operator= (const GString& other) = delete;

    // Create a string from a C string.
    explicit GString (const char* sA) : std::string (sA ? sA : "") {}

    // Zero-cost conversion from and to std::string
    explicit GString (const std::string& str) : std::string (str) {}
    explicit GString (std::string&& str) : std::string (std::move (str)) {}

    // Create a string from <lengthA> chars at <sA>.  This string
    // can contain null characters.
    GString (const char* sA, int lengthA)
        : std::string (sA ? sA : "", sA ? lengthA : 0) {}

    // Create a string from <lengthA> chars at <idx> in <str>.
    GString (const GString* str, int idx, int lengthA)
        : std::string (*str, idx, lengthA) {}

    // Set content of a string to <newStr>.
    GString* Set (const GString* newStr) {
        assign (
            newStr ? static_cast< const std::string& > (*newStr)
                   : std::string{});
        return this;
    }
    GString* Set (const char* newStr) {
        assign (newStr ? newStr : "");
        return this;
    }
    GString* Set (const char* newStr, int newLen) {
        assign (newStr ? newStr : "", newStr ? newLen : 0);
        return this;
    }

    // Copy a string.
    explicit GString (const GString* str)
        : std::string (
              str ? static_cast< const std::string& > (*str) : std::string{}) {}
    GString* copy () const { return new GString (this); }

    // Concatenate two strings.
    GString (const GString* str1, const GString* str2) {
        reserve (str1->size () + str2->size ());
        static_cast< std::string& > (*this).append (*str1);
        static_cast< std::string& > (*this).append (*str2);
    }

    // Convert an integer to a string.
    static GString* fromInt (int x);

    // Create a formatted string.  Similar to printf, but without the
    // string overflow issues.  Formatting elements consist of:
    //     {<arg>:[<width>][.<precision>]<type>}
    // where:
    // - <arg> is the argument number (arg 0 is the first argument
    //   following the format string) -- NB: args must be first used in
    //   order; they can be reused in any order
    // - <width> is the field width -- negative to reverse the alignment;
    //   starting with a leading zero to zero-fill (for integers)
    // - <precision> is the number of digits to the right of the decimal
    //   point (for floating point numbers)
    // - <type> is one of:
    //     d, x, X, o, b -- int in decimal, lowercase hex, uppercase hex, octal, binary
    //     ud, ux, uX, uo, ub -- unsigned int
    //     ld, lx, lX, lo, lb, uld, ulx, ulX, ulo, ulb -- long, unsigned long
    //     lld, llx, llX, llo, llb, ulld, ullx, ullX, ullo, ullb
    //         -- long long, unsigned long long
    //     f, g, gs -- floating point (float or double)
    //         f  -- always prints trailing zeros (eg 1.0 with .2f will print 1.00)
    //         g  -- omits trailing zeros and, if possible, the dot (eg 1.0 shows up as 1)
    //         gs -- is like g, but treats <precision> as number of significant
    //               digits to show (eg 0.0123 with .2gs will print 0.012)
    //     c -- character (char, short or int)
    //     s -- string (char *)
    //     t -- GString *
    //     w -- blank space; arg determines width
    // To get literal curly braces, use {{ or }}.
    static GString* format (const char* fmt, ...);
    static GString* formatv (const char* fmt, va_list argList);

    // Get length.
    int getLength () const { return size (); }

    // Get C string.
    using std::string::c_str;

    // Get <i>th character.
    char getChar (int i) const { return (*this)[i]; }

    // Change <i>th character.
    void setChar (int i, char c) { (*this)[i] = c; }

    // Clear string to zero length.
    GString* clear () {
        static_cast< std::string& > (*this).clear ();
        return this;
    }

    // Append a character or string.
    GString* append (char c) {
        push_back (c);
        return this;
    }
    GString* append (const GString* str) {
        static_cast< std::string& > (*this).append (*str);
        return this;
    }
    GString* append (const char* str) {
        static_cast< std::string& > (*this).append (str);
        return this;
    }
    GString* append (const char* str, int lengthA) {
        static_cast< std::string& > (*this).append (str, lengthA);
        return this;
    }

    // Append a formatted string.
    GString* appendf (const char* fmt, ...);
    GString* appendfv (const char* fmt, va_list argList);

    // Insert a character or string.
    GString* insert (int i, char c) {
        static_cast< std::string& > (*this).insert (i, 1, c);
        return this;
    }
    GString* insert (int i, const GString* str) {
        static_cast< std::string& > (*this).insert (i, *str);
        return this;
    }
    GString* insert (int i, const char* str) {
        static_cast< std::string& > (*this).insert (i, str);
        return this;
    }
    GString* insert (int i, const char* str, int lengthA) {
        static_cast< std::string& > (*this).insert (i, str, lengthA);
        return this;
    }

    // Delete a character or range of characters.
    GString* del (int i, int n = 1) {
        erase (i, n);
        return this;
    }

    // Convert string to all-upper/all-lower case.
    GString* upperCase ();
    GString* lowerCase ();

    // Compare two strings:  -1:<  0:=  +1:>
    int cmp (const GString* str) const { return compare (*str); }
    int cmp (const std::string& str) const { return compare (str); }
    int cmpN (GString* str, int n) const { return compare (0, n, *str); }
    int cmp (const char* sA) const { return compare (sA); }
    int cmpN (const char* sA, int n) const { return compare (0, n, sA); }

    // Return true if strings starts with prefix
    bool startsWith (const char* prefix) const;
    // Return true if string ends with suffix
    bool endsWith (const char* suffix) const;

    bool hasUnicodeMarker () const {
        return size () >= 2 && (*this)[0] == '\xfe' && (*this)[1] == '\xff';
    }
    bool hasUnicodeMarkerLE () const {
        return size () >= 2 && (*this)[0] == '\xff' && (*this)[1] == '\xfe';
    }
    bool hasJustUnicodeMarker () const {
        return size () == 2 && hasUnicodeMarker ();
    }

    void prependUnicodeMarker ();

    // Sanitizes the string so that it does
    // not contain any ( ) < > [ ] { } / %
    // The postscript mode also has some more strict checks
    // The caller owns the return value
    GString* sanitizedName (bool psmode) const;
};

#endif

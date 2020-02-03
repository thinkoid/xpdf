// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_UTILS_GSTRING_HH
#define XPDF_UTILS_GSTRING_HH

#include <defs.hh>

#include <climits> // for LLONG_MAX and ULLONG_MAX
#include <cstdarg>

#include <string>

struct GString : std::string {
    using base_type = std::string;

    GString () = default;
    ~GString () = default;

    using base_type::base_type;
    using base_type::operator=;

    // Zero-cost conversion from and to std::string
    explicit GString (const std::string& s) : std::string (s) { }
    explicit GString (std::string&& s) : std::string (std::move (s)) { }

    // Create a string from <lengthA> chars at <idx> in <str>.
    GString (const GString* str, int idx, int lengthA)
        : std::string (*str, idx, lengthA) { }

    // Copy a string.
    explicit GString (const GString* p)
        : std::string (p ? reinterpret_cast< const base_type& > (*p) : std::string{ })
        { }

    GString* copy () const { return new GString (this); }

    // Concatenate two strings.
    GString (const GString* str1, const GString* str2) {
        reserve (str1->size () + str2->size ());
        static_cast< std::string& > (*this).append (*str1);
        static_cast< std::string& > (*this).append (*str2);
    }

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
    using std::string::operator[];

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

#endif // XPDF_UTILS_GSTRING_HH

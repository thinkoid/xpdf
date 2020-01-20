// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_OBJECT_HH
#define XPDF_XPDF_OBJECT_HH

#include <defs.hh>

#include <cstdio>
#include <cstring>

#include <memory>
#include <variant>

#include <goo/gfile.hh>
#include <goo/GString.hh>

#include <xpdf/ast_fwd.hh>

class XRef;
class Array;
class Dict;
class Stream;

////////////////////////////////////////////////////////////////////////

namespace xpdf {
namespace ast {
//
// Used in parsing:
//
struct null_t { };
struct error_t { };
struct eof_t { };

struct name_t : std::string {
    name_t () = default;

    name_t (const std::string& s) : std::string (s) { }
    name_t (std::string&& s) : std::string (std::move (s)) { }

    using std::string::operator=;
};

struct command_t : std::string {
    command_t () = default;

    command_t (const std::string& s) : std::string (s) { }
    command_t (std::string&& s) : std::string (std::move (s)) { }

    using std::string::operator=;
};

struct ref_t {
    int num; // object number
    int gen; // generation number
};

struct object_t {
    object_t () noexcept : var_ (null_t{ }) { }

    object_t (eof_t)   noexcept : var_ (eof_t{ })   { }
    object_t (error_t) noexcept : var_ (error_t{ }) { }

    object_t (bool   arg) noexcept : var_ (arg) { }
    object_t (int    arg) noexcept : var_ (arg) { }
    object_t (double arg) noexcept : var_ (arg) { }

    object_t (GString*) noexcept;
    object_t (const std::string&);

    object_t (const name_t&    arg) : var_ (arg) { }
    object_t (const command_t& arg) : var_ (arg) { }

    object_t (const ref_t& arg) noexcept : var_ (arg) { }

    object_t (Array* ) noexcept;
    object_t (Dict*  ) noexcept;
    object_t (Stream*) noexcept;

    //
    // Legacy type-checking interface:
    //
    bool isNull   () const { return var_.index () == 0; }

    // TODO: might be a bad equivalence
    bool isNone   () const { return isNull (); }

    bool isEOF    () const { return var_.index () == 1; }
    bool isError  () const { return var_.index () == 2; }

    bool isBool   () const { return var_.index () == 3; }

    bool isInt    () const { return var_.index () == 4; }
    bool isReal   () const { return var_.index () == 5; }
    bool isNum    () const { return isInt () || isReal (); }

    bool isString () const { return var_.index () == 6; }

    bool isName   () const { return var_.index () == 7; }
    bool isName   (const char* s) const {
        return isName () && 0 == strcmp (getName (), s);
    }

    bool isCmd    () const { return var_.index () == 8; }
    bool isCmd    (const char* s) const {
        return isCmd () && 0 == strcmp (getCmd (), s);
    }

    bool isRef    () const { return var_.index () == 9; }
    bool isArray  () const { return var_.index () == 10; }

    bool isDict   () const { return var_.index () == 11; }
    bool isDict   (const char*) const;

    bool isStream () const { return var_.index () == 12; }
    bool isStream (const char*) const;

    //
    // Legacy accessors:
    //
    bool getBool () const { return std::get< bool > (var_); }

    int    getInt  () const { return std::get<    int > (var_); }
    double getReal () const { return std::get< double > (var_); }

    double getNum () const {
        return isInt () ? getInt () : getReal ();
    }

    GString* getString () const {
        using pointer = std::shared_ptr< GString >;
        return std::get< pointer > (var_).get ();
    }

    const char* getName () const {
        return std::get< name_t > (var_).c_str ();
    }

    const char* getCmd  () const {
        return std::get< command_t > (var_).c_str ();
    }

    Array& getArray () const {
        using pointer = std::shared_ptr< Array >;
        return *std::get< pointer > (var_);
    }

    Dict* getDict () const {
        using pointer = std::shared_ptr< Dict >;
        return std::get< pointer > (var_).get ();
    }

    Stream* getStream () const {
        using pointer = std::shared_ptr< Stream >;
        return std::get< pointer > (var_).get ();
    }

    const ref_t& getRef () const { return std::get< ref_t > (var_); }
    int getRefNum () const { return std::get< ref_t > (var_).num; }
    int getRefGen () const { return std::get< ref_t > (var_).gen; }

    //
    // Legacy initialization interface:
    //
    object_t* initBool (bool arg) { return var_ = arg, this; }

    object_t* initInt (int arg) { return var_ = arg, this; }

    object_t* initReal (double arg) { return var_ = arg, this; }

    object_t* initString (const std::string& s) {
        return var_ = std::make_shared< GString > (s), this;
    }

    object_t* initString (GString* p) {
        return var_ = std::shared_ptr< GString > (p), this;
    }

    object_t* initName (const std::string& arg) {
        return var_ = name_t (arg), this;
    }

    object_t* initNull () { return var_ = null_t{ }, this; }

    object_t* initArray  (XRef*);
    object_t* initDict   (XRef*);
    object_t* initDict   (Dict*);
    object_t* initStream (Stream*);

    object_t* initRef (int x, int y) {
        return var_ = ref_t{ x, y }, this;
    }

    object_t* initCmd (const char* arg) {
        return var_ = command_t (arg), this;
    }

    object_t* initCmd (const std::string& arg) {
        return var_ = command_t (arg), this;
    }

    object_t* initError () noexcept {
        return var_ = error_t{ }, this;
    }

    object_t* initEOF () noexcept {
        return var_ = eof_t{ }, this;
    }

    //
    // Legacy misc:
    //
    const char* getTypeName () const;
    void print (FILE* = stdout);

    //
    // Fetch referenced objects if object_t is a reference. Otherwise, copy this
    // object:
    //
    object_t* fetch (XRef*, object_t*, int recursion = 0);

    //
    // Array accessors:
    //
    int arrayGetLength ();
    void arrayAdd (object_t* elem);
    object_t* arrayGet (int i, object_t* obj);
    object_t* arrayGetNF (int i, object_t* obj);

    //
    // Dict accessors:
    //
    int dictGetLength ();
    void dictAdd (const char* key, object_t* val);
    void dictAdd (const std::string& key, object_t* val);
    bool dictIs (const char* dictType) const;
    object_t* dictLookup (const char* key, object_t* obj, int recursion = 0);
    object_t* dictLookupNF (const char* key, object_t* obj);
    char* dictGetKey (int i);
    object_t* dictGetVal (int i, object_t* obj);
    object_t* dictGetValNF (int i, object_t* obj);

    //
    // Stream accessors:
    //
    bool streamIs (const char* dictType) const;
    void streamReset ();
    void streamClose ();
    int streamGetChar ();
    int streamLookChar ();
    int streamGetBlock (char* blk, int size);
    char* streamGetLine (char* buf, int size);
    GFileOffset streamGetPos ();
    void streamSetPos (GFileOffset pos, int dir = 0);
    Dict* streamGetDict ();

private:
    std::variant<
        null_t,                        //  0
        eof_t,                         //  1
        error_t,                       //  2
        bool,                          //  3
        int,                           //  4
        double,                        //  5
        std::shared_ptr< GString >,    //  6
        name_t,                        //  7
        command_t,                     //  8
        ref_t,                         //  9
        std::shared_ptr< Array >,      // 10
        std::shared_ptr< Dict >,       // 11
        std::shared_ptr< Stream >      // 12
    > var_;
};

} // namespace ast

//
// Formerly object_t::fetch
//
ast::object_t fetch (ast::object_t&, XRef&, int recursion = 0);

//
// Convenience factories:
//
inline ast::object_t make_null_object () {
    return ast::object_t ();
}

inline ast::object_t make_error_object () {
    return ast::object_t (ast::error_t{ });
}

inline ast::object_t make_eof_object () {
    return ast::object_t (ast::eof_t{ });
}

inline ast::object_t make_string_object (const std::string& arg) {
    return ast::object_t (arg);
}

inline ast::object_t make_name_object (const std::string& arg) {
    return ast::object_t (ast::name_t (arg));
}

inline ast::object_t make_command_object (const std::string& arg) {
    return ast::object_t (ast::command_t (arg));
}

inline ast::object_t make_ref_object (int a, int b) {
    return ast::object_t (ast::ref_t{ a, b });
}

inline ast::object_t make_ref_object (ast::ref_t arg) {
    return ast::object_t (arg);
}

ast::object_t make_array_object (XRef*);
ast::object_t make_dictionary_object (XRef*);

} // namespace xpdf

using Object = xpdf::ast::object_t;
using Ref = xpdf::ast::ref_t;

#endif // XPDF_XPDF_OBJECT_HH

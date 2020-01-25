// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_OBJ_HH
#define XPDF_XPDF_OBJ_HH

#include <defs.hh>

#include <cstdio>
#include <cstring>

#include <memory>
#include <variant>

#include <goo/gfile.hh>
#include <goo/GString.hh>

#include <xpdf/array_fwd.hh>
#include <xpdf/obj_fwd.hh>

class Dict;
class Stream;
class XRef;

////////////////////////////////////////////////////////////////////////

namespace xpdf {
namespace detail {

template< typename T, typename U, typename Enable = void >
struct cast_t {
    U operator() (const T&) const { throw_ (); return { }; }
    U operator() (T&&) const { throw_ (); return { }; }
private:
    void throw_ () const { throw std::runtime_error ("invalid cast"); }
};

template< typename T, typename U>
using is_convertible_t = typename std::enable_if<
    std::is_convertible< T, U >::value >::type;

template< typename T, typename U >
struct cast_t< T, U, is_convertible_t< T, U > >  {
    U operator() (const T& arg) const { return U (arg); }
    U operator() (T&& arg) const { return U (std::move (arg)); }
};

} // namespace detail

struct null_t { };

//
// Used in parsing:
//
struct err_t { };
struct eof_t { };

struct name_t : std::string {
    name_t () = default;

    name_t (const std::string& s) : std::string (s) { }
    name_t (std::string&& s) : std::string (std::move (s)) { }

    using std::string::operator=;
};

struct cmd_t : std::string {
    cmd_t () = default;

    cmd_t (const std::string& s) : std::string (s) { }
    cmd_t (std::string&& s) : std::string (std::move (s)) { }

    using std::string::operator=;
};

struct ref_t {
    XRef* xref;
    int num, gen;
};

inline bool operator== (const ref_t& lhs, const ref_t& rhs) {
    return lhs.num == rhs.num && lhs.gen == rhs.gen;
}

inline bool operator!= (const ref_t& lhs, const ref_t& rhs) {
    return !(lhs == rhs);
}

struct obj_t {
    obj_t () noexcept : var_ (null_t{ }) { }

    obj_t (eof_t)   noexcept : var_ (eof_t{ })   { }
    obj_t (err_t) noexcept : var_ (error_t{ }) { }

    obj_t (bool   arg) noexcept : var_ (arg) { }
    obj_t (int    arg) noexcept : var_ (arg) { }
    obj_t (double arg) noexcept : var_ (arg) { }

    obj_t (GString*) noexcept;

    obj_t (const char*);
    obj_t (const std::string&);

    obj_t (const name_t&    arg) : var_ (arg) { }
    obj_t (const cmd_t& arg) : var_ (arg) { }

    obj_t (const ref_t& arg) noexcept : var_ (arg) { }

    obj_t (Array* ) noexcept;
    obj_t (Dict*  ) noexcept;
    obj_t (Stream*) noexcept;

    template< typename T >
    bool is () const {
        return std::holds_alternative< T > (var_);
    }

    //
    // Legacy type-checking interface:
    //
    bool is_null () const { return is< null_t > (); }
    bool is_none () const { return is< null_t > (); }

    bool is_eof  () const { return is< eof_t > (); }
    bool is_err  () const { return is< err_t > (); }

    bool is_bool () const { return is< bool > (); }

    bool is_int  () const { return is< int > (); }
    bool is_real () const { return is< double > (); }
    bool is_num  () const { return is_int () || is_real (); }

    bool is_string () const { return is< std::shared_ptr< GString > > (); }

    bool is_name () const { return is< name_t > (); }
    bool is_name (const char* s) const {
        return is_name () && 0 == strcmp (as_name (), s);
    }

    bool is_cmd () const { return is< cmd_t > (); }
    bool is_cmd (const char* s) const {
        return is_cmd () && 0 == strcmp (as_cmd (), s);
    }

    bool is_ref () const { return is< ref_t > (); }

    bool is_array () const { return is< std::shared_ptr< Array > > (); }

    bool is_dict () const { return is< std::shared_ptr< Dict > > (); }
    bool is_dict (const char*) const;

    bool is_stream () const { return is< std::shared_ptr< Stream > > (); }
    bool is_stream (const char*) const;

    //
    // Accessors:
    //
    template< typename T >
    T& as () { return std::get< T > (var_); }

    template< typename T >
    const T& as () const { return std::get< T > (var_); }

    //
    // Conversion between types because the parser has no semantic information
    // about the numeric values:
    //
    template< typename U >
    U cast () const {
#define CAST(T) detail::cast_t< T, U >{ }(as< T > ());
        switch (var_.index ()) {
        case  0: return CAST (null_t);
        case  1: return CAST (eof_t);
        case  2: return CAST (err_t);
        case  3: return CAST (bool);
        case  4: return CAST (int);
        case  5: return CAST (double);
        case  6: return CAST (std::shared_ptr< GString >);
        case  7: return CAST (name_t);
        case  8: return CAST (cmd_t);
        case  9: return CAST (ref_t);
        case 10: return CAST (std::shared_ptr< Array >);
        case 11: return CAST (std::shared_ptr< Dict >);
        case 12: return CAST (std::shared_ptr< Stream >);
#undef CAST
        default:
            ASSERT (0);
        }
    }

    //
    // Legacy accessors:
    //
    bool as_bool () { return as< bool > (); }

    int    as_int  () { return as< int > (); }
    double as_real () { return as< double > (); }
    double as_num  () { return is_int () ? as_int () : as_real (); }

    GString* as_string () const {
        using pointer = std::shared_ptr< GString >;
        return as< pointer > ().get ();
    }

    const char* as_name () const {
        return as< name_t > ().c_str ();
    }

    const char* as_cmd  () const {
        return as< cmd_t > ().c_str ();
    }

    Array& as_array () const {
        using pointer = std::shared_ptr< Array >;
        return *as< pointer > ();
    }

    Dict* as_dict () const {
        using pointer = std::shared_ptr< Dict >;
        return as< pointer > ().get ();
    }

    Stream* as_stream () const {
        using pointer = std::shared_ptr< Stream >;
        return as< pointer > ().get ();
    }

    const ref_t& as_ref () const { return as< ref_t > (); }

    //
    // Legacy misc:
    //
    const char* getTypeName () const;
    void print (FILE* = stdout);

    //
    // Array accessors:
    //
    obj_t& operator[] (size_t);

    const obj_t& operator[] (size_t n) const {
        return const_cast< obj_t* > (this)->operator[] (n);
    }

    //
    // Dict accessors:
    //
    int dictGetLength ();

    void dictAdd (const char*, const obj_t&);
    void dictAdd (const char*, obj_t&&);
    void dictAdd (const char* key, obj_t* val);

    bool dictIs (const char* dictType) const;
    obj_t* dictLookup (const char* key, obj_t* obj, int recursion = 0);
    obj_t* dictLookupNF (const char* key, obj_t* obj);
    char* dictGetKey (int i);
    obj_t* dictGetVal (int i, obj_t* obj);
    obj_t* dictGetValNF (int i, obj_t* obj);

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

    //
    // Ref accessors:
    //
    int getRefNum () const { return std::get< ref_t > (var_).num; }
    int getRefGen () const { return std::get< ref_t > (var_).gen; }

private:
    std::variant<
        null_t,                        //  0
        eof_t,                         //  1
        err_t,                         //  2
        bool,                          //  3
        int,                           //  4
        double,                        //  5
        std::shared_ptr< GString >,    //  6
        name_t,                        //  7
        cmd_t,                         //  8
        ref_t,                         //  9
        std::shared_ptr< Array >,      // 10
        std::shared_ptr< Dict >,       // 11
        std::shared_ptr< Stream >      // 12
    > var_;
};

obj_t resolve (const obj_t&, int = 0);

//
// Convenience factories:
//
inline obj_t make_null_obj () {
    return obj_t ();
}

inline obj_t make_err_obj () {
    return obj_t (err_t{ });
}

inline obj_t make_eof_obj () {
    return obj_t (eof_t{ });
}

inline obj_t make_bool_obj (bool arg) {
    return obj_t (arg);
}

inline obj_t make_int_obj (int arg) {
    return obj_t (arg);
}

inline obj_t make_real_obj (double arg) {
    return obj_t (arg);
}

inline obj_t make_string_obj (const std::string& arg) {
    return obj_t (arg);
}

inline obj_t make_name_obj (const std::string& arg) {
    return obj_t (name_t (arg));
}

inline obj_t make_cmd_obj (const std::string& arg) {
    return obj_t (cmd_t (arg));
}

inline obj_t make_ref_obj (int a, int b, XRef* p) {
    return obj_t (ref_t{ p, a, b });
}

inline obj_t make_ref_obj (ref_t arg) {
    return obj_t (arg);
}

obj_t make_arr_obj ();
obj_t make_dict_obj (Dict*);
obj_t make_dict_obj (XRef*);
obj_t make_stream_obj (Stream*);

} // namespace xpdf

using Object = xpdf::obj_t;
using Ref = xpdf::ref_t;

#endif // XPDF_XPDF_OBJ_HH

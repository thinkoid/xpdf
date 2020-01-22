// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#include <defs.hh>

#include <cstddef>
#include <cstdlib>

#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/ast.hh>
#include <xpdf/Stream.hh>
#include <xpdf/XRef.hh>

namespace xpdf {

namespace ast {

obj_t::obj_t (GString* p) noexcept
    : var_ (std::shared_ptr< GString > (p))
{ }

obj_t::obj_t (const char* s)
    : var_ (std::make_shared< GString > (s))
{ }

obj_t::obj_t (const std::string& s)
    : var_ (std::make_shared< GString > (s))
{ }

obj_t::obj_t (Array*  p) noexcept
    : var_ (std::shared_ptr<  Array > (p))
{ }

obj_t::obj_t (Dict*   p) noexcept
    : var_ (std::shared_ptr<   Dict > (p))
{ }

obj_t::obj_t (Stream* p) noexcept
    : var_ (std::shared_ptr< Stream > (p))
{ }

obj_t* obj_t::fetch (XRef* xref, obj_t* obj, int recursion) {
    return *obj = xpdf::fetch (*this, *xref, recursion), obj;
}

//------------------------------------------------------------------------
// Array accessors.
//------------------------------------------------------------------------

int obj_t::arrayGetLength () {
    return as_array ().size ();
}

void obj_t::arrayAdd (obj_t* pobj) {
    as_array ().add (pobj);
}

void obj_t::arrayAdd (const obj_t& obj) {
    as_array ().add (obj);
}

void obj_t::arrayAdd (obj_t&& obj) {
    as_array ().add (std::move (obj));
}

obj_t* obj_t::arrayGet (int i, obj_t* pobj) {
    return as_array ().get (i, pobj);
}

obj_t* obj_t::arrayGetNF (int i, obj_t* pobj) {
    return as_array ().getNF (i, pobj);
}

//------------------------------------------------------------------------
// Dict accessors.
//------------------------------------------------------------------------

int obj_t::dictGetLength () {
    return as_dict ()->getLength ();
}

void obj_t::dictAdd (const char* key, const obj_t& val) {
    as_dict ()->add (key, val);
}

void obj_t::dictAdd (const char* key, obj_t&& val) {
    as_dict ()->add (key, std::move (val));
}

void obj_t::dictAdd (const char* key, obj_t* val) {
    as_dict ()->add (key, val);
}

bool obj_t::dictIs (const char* dictType) const {
    return as_dict ()->is (dictType);
}

bool obj_t::is_dict (const char* dictType) const {
    return is_dict () && dictIs (dictType);
}

obj_t*
obj_t::dictLookup (const char* key, obj_t* obj, int recursion) {
    return as_dict ()->lookup (key, obj, recursion);
}

obj_t* obj_t::dictLookupNF (const char* key, obj_t* obj) {
    return as_dict ()->lookupNF (key, obj);
}

char* obj_t::dictGetKey (int i) {
    return as_dict ()->getKey (i);
}

obj_t* obj_t::dictGetVal (int i, obj_t* obj) {
    return as_dict ()->getVal (i, obj);
}

obj_t* obj_t::dictGetValNF (int i, obj_t* obj) {
    return as_dict ()->getValNF (i, obj);
}

//------------------------------------------------------------------------
// Stream accessors.
//------------------------------------------------------------------------

bool obj_t::streamIs (const char* dictType) const {
    return as_stream ()->as_dict ()->is (dictType);
}

bool obj_t::is_stream (const char* dictType) const {
    return is_stream () && streamIs (dictType);
}

void obj_t::streamReset () {
    as_stream ()->reset ();
}

void obj_t::streamClose () {
    as_stream ()->close ();
}

int obj_t::streamGetChar () {
    return as_stream ()->getChar ();
}

int obj_t::streamLookChar () {
    return as_stream ()->lookChar ();
}

int obj_t::streamGetBlock (char* blk, int size) {
    return as_stream ()->getBlock (blk, size);
}

char* obj_t::streamGetLine (char* buf, int size) {
    return as_stream ()->getLine (buf, size);
}

GFileOffset obj_t::streamGetPos () {
    return as_stream ()->getPos ();
}

void obj_t::streamSetPos (GFileOffset pos, int dir) {
    as_stream ()->setPos (pos, dir);
}

Dict* obj_t::streamGetDict () {
    return as_stream ()->as_dict ();
}

//
// More legacy stuff
//
const char* obj_t::getTypeName () const {
    static const char* arr [] = {
        "null", "eof", "error", "boolean", "integer", "real",
        "string", "name", "cmd", "ref", "array", "dictionary",
        "stream"
    };

    return arr [var_.index ()];
};

void obj_t::print (FILE*) {
#if 0
    Object obj;
    int i;

    switch (type) {
    case objBool: fprintf (f, "%s", booln ? "true" : "false"); break;
    case objInt: fprintf (f, "%d", intg); break;
    case objReal: fprintf (f, "%g", real); break;
    case objString:
        fprintf (f, "(");
        fwrite (string->c_str (), 1, string->getLength (), f);
        fprintf (f, ")");
        break;
    case objName: fprintf (f, "/%s", name); break;
    case objNull: fprintf (f, "null"); break;
    case objArray:
        fprintf (f, "[");
        for (i = 0; i < arrayGetLength (); ++i) {
            if (i > 0) fprintf (f, " ");
            arrayGetNF (i, &obj);
            obj.print (f);
        }
        fprintf (f, "]");
        break;
    case objDict:
        fprintf (f, "<<");
        for (i = 0; i < dictGetLength (); ++i) {
            fprintf (f, " /%s ", dictGetKey (i));
            dictGetValNF (i, &obj);
            obj.print (f);
        }
        fprintf (f, " >>");
        break;
    case objStream: fprintf (f, "<stream>"); break;
    case objRef: fprintf (f, "%d %d R", ref.num, ref.gen); break;
    case objCmd: fprintf (f, "%s", cmd); break;
    case objError: fprintf (f, "<error>"); break;
    case objEOF: fprintf (f, "<EOF>"); break;
    case objNone: fprintf (f, "<none>"); break;
    }
#endif // 0
}

} // namespace ast

ast::obj_t make_arr_obj (XRef* p) {
    return ast::obj_t (new Array (p));
}

ast::obj_t make_dict_obj (XRef* p) {
    return ast::obj_t (new Dict (p));
}

ast::obj_t make_dict_obj (Dict* p) {
    return ast::obj_t (p);
}

ast::obj_t make_stream_obj (Stream* p) {
    return ast::obj_t (p);
}

//
// Formerly obj_t::fetch
//
ast::obj_t fetch (ast::obj_t& obj, XRef& xref, int recursion /* = 0 */) {
    if (obj.is_ref ()) {
        auto& ref = obj.as_ref ();

        ast::obj_t result;
        xref.fetch (ref.num, ref.gen, &result, recursion);

        return result;
    }
    else {
        return obj;
    }
}

} // namespace xpdf

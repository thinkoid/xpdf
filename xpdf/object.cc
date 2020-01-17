// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#include <defs.hh>

#include <cstddef>
#include <cstdlib>

#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/object.hh>
#include <xpdf/Stream.hh>
#include <xpdf/XRef.hh>

namespace xpdf {

object_t make_array_object (XRef* p) {
    return object_t (new Array (p));
}

object_t make_dictionary_object (XRef* p) {
    return object_t (new Dict (p));
}

//
// Formerly object_t::fetch
//
object_t fetch (object_t& obj, XRef& xref, int recursion /* = 0 */) {
    if (obj.isRef ()) {
        auto& ref = obj.getRef ();

        Object result;
        xref.fetch (ref.num, ref.gen, &result, recursion);

        return result;
    }
    else {
        return obj;
    }
}

object_t::object_t (GString* p)
    : var_ (std::shared_ptr< GString > (p))
{ }

object_t::object_t (const std::string& s)
    : var_ (std::make_shared< GString > (s))
{ }

object_t::object_t (Array*  p) noexcept
    : var_ (std::shared_ptr<  Array > (p))
{ }

object_t::object_t (Dict*   p) noexcept
    : var_ (std::shared_ptr<   Dict > (p))
{ }

object_t::object_t (Stream* p) noexcept
    : var_ (std::shared_ptr< Stream > (p))
{ }

object_t* object_t::initArray (XRef* p) {
    var_ = std::make_shared< Array > (p);
    return this;
}

object_t* object_t::initDict (XRef* p) {
    var_ = std::make_shared< Dict > (p);
    return this;
}

object_t* object_t::initDict (Dict* p) {
    var_ = std::shared_ptr< Dict > (p);
    return this;
}

object_t* object_t::initStream (Stream* p) {
    var_ = std::shared_ptr< Stream > (p);
    return this;
}

object_t* object_t::copy (object_t* pobj) {
    return *pobj = *this, pobj;
}

void object_t::free () {
    initNull ();
}

object_t* object_t::fetch (XRef* xref, object_t* obj, int recursion) {
    return *obj = xpdf::fetch (*this, *xref, recursion), obj;
}

//------------------------------------------------------------------------
// Array accessors.
//------------------------------------------------------------------------

int object_t::arrayGetLength () {
    return getArray ()->getLength ();
}

void object_t::arrayAdd (object_t* pobj) {
    getArray ()->add (pobj);
}

object_t* object_t::arrayGet (int i, object_t* pobj) {
    return getArray ()->get (i, pobj);
}

object_t* object_t::arrayGetNF (int i, object_t* pobj) {
    return getArray ()->getNF (i, pobj);
}

//------------------------------------------------------------------------
// Dict accessors.
//------------------------------------------------------------------------

int object_t::dictGetLength () {
    return getDict ()->getLength ();
}

void object_t::dictAdd (const char* key, object_t* val) {
    getDict ()->add (key, val);
}

bool object_t::dictIs (const char* dictType) const {
    return getDict ()->is (dictType);
}

bool object_t::isDict (const char* dictType) const {
    return isDict () && dictIs (dictType);
}

object_t*
object_t::dictLookup (const char* key, object_t* obj, int recursion) {
    return getDict ()->lookup (key, obj, recursion);
}

object_t* object_t::dictLookupNF (const char* key, object_t* obj) {
    return getDict ()->lookupNF (key, obj);
}

char* object_t::dictGetKey (int i) {
    return getDict ()->getKey (i);
}

object_t* object_t::dictGetVal (int i, object_t* obj) {
    return getDict ()->getVal (i, obj);
}

object_t* object_t::dictGetValNF (int i, object_t* obj) {
    return getDict ()->getValNF (i, obj);
}

//------------------------------------------------------------------------
// Stream accessors.
//------------------------------------------------------------------------

bool object_t::streamIs (const char* dictType) const {
    return getStream ()->getDict ()->is (dictType);
}

bool object_t::isStream (const char* dictType) const {
    return isStream () && streamIs (dictType);
}

void object_t::streamReset () {
    getStream ()->reset ();
}

void object_t::streamClose () {
    getStream ()->close ();
}

int object_t::streamGetChar () {
    return getStream ()->getChar ();
}

int object_t::streamLookChar () {
    return getStream ()->lookChar ();
}

int object_t::streamGetBlock (char* blk, int size) {
    return getStream ()->getBlock (blk, size);
}

char* object_t::streamGetLine (char* buf, int size) {
    return getStream ()->getLine (buf, size);
}

GFileOffset object_t::streamGetPos () {
    return getStream ()->getPos ();
}

void object_t::streamSetPos (GFileOffset pos, int dir) {
    getStream ()->setPos (pos, dir);
}

Dict* object_t::streamGetDict () {
    return getStream ()->getDict ();
}

//
// More legacy stuff
//
const char* object_t::getTypeName () const {
    static const char* arr [] = {
        "null", "eof", "error", "boolean", "integer", "real",
        "string", "name", "cmd", "ref", "array", "dictionary",
        "stream"
    };

    return arr [var_.index ()];
};

void object_t::print (FILE*) {
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
            obj.free ();
        }
        fprintf (f, "]");
        break;
    case objDict:
        fprintf (f, "<<");
        for (i = 0; i < dictGetLength (); ++i) {
            fprintf (f, " /%s ", dictGetKey (i));
            dictGetValNF (i, &obj);
            obj.print (f);
            obj.free ();
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


} // namespace xpdf

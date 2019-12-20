//========================================================================
//
// Object.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <cstddef>
#include <cstdlib>

#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/Object.hh>
#include <xpdf/Stream.hh>
#include <xpdf/XRef.hh>

//------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------

const char* objTypeNames[numObjTypes] = {
    "boolean",    "integer", "real", "string", "name",  "null", "array",
    "dictionary", "stream",  "ref",  "cmd",    "error", "eof",  "none"
};

Object* Object::initArray (XRef* xref) {
    initObj (objArray);
    array = new Array (xref);
    return this;
}

Object* Object::initDict (XRef* xref) {
    initObj (objDict);
    dict = new Dict (xref);
    return this;
}

Object* Object::initDict (Dict* dictA) {
    initObj (objDict);
    dict = dictA;
    dict->incRef ();
    return this;
}

Object* Object::initStream (Stream* streamA) {
    initObj (objStream);
    stream = streamA;
    return this;
}

Object* Object::copy (Object* obj) {
    *obj = *this;
    switch (type) {
    case objString: obj->string = string->copy (); break;
    case objName: obj->name = strdup (name); break;
    case objArray: array->incRef (); break;
    case objDict: dict->incRef (); break;
    case objStream: stream->incRef (); break;
    case objCmd: obj->cmd = strdup (cmd); break;
    default: break;
    }
    return obj;
}

Object* Object::fetch (XRef* xref, Object* obj, int recursion) {
    return (type == objRef && xref)
               ? xref->fetch (ref.num, ref.gen, obj, recursion)
               : copy (obj);
}

void Object::free () {
    switch (type) {
    case objString: delete string; break;
    case objName: std::free (name); break;
    case objArray:
        if (!array->decRef ()) { delete array; }
        break;
    case objDict:
        if (!dict->decRef ()) { delete dict; }
        break;
    case objStream:
        if (!stream->decRef ()) { delete stream; }
        break;
    case objCmd: std::free (cmd); break;
    default: break;
    }
    type = objNone;
}

const char* Object::getTypeName () { return objTypeNames[type]; }

void Object::print (FILE* f) {
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
}

//------------------------------------------------------------------------
// Array accessors.
//------------------------------------------------------------------------

int Object::arrayGetLength () { return array->getLength (); }

void Object::arrayAdd (Object* elem) { array->add (elem); }

Object* Object::arrayGet (int i, Object* obj) {
    return array->get (i, obj);
}

Object* Object::arrayGetNF (int i, Object* obj) {
    return array->getNF (i, obj);
}

//------------------------------------------------------------------------
// Dict accessors.
//------------------------------------------------------------------------

int Object::dictGetLength () { return dict->getLength (); }

void Object::dictAdd (char* key, Object* val) { dict->add (key, val); }

bool Object::dictIs (const char* dictType) {
    return dict->is (dictType);
}

bool Object::isDict (const char* dictType) {
    return type == objDict && dictIs (dictType);
}

Object*
Object::dictLookup (const char* key, Object* obj, int recursion) {
    return dict->lookup (key, obj, recursion);
}

Object* Object::dictLookupNF (const char* key, Object* obj) {
    return dict->lookupNF (key, obj);
}

char* Object::dictGetKey (int i) { return dict->getKey (i); }

Object* Object::dictGetVal (int i, Object* obj) {
    return dict->getVal (i, obj);
}

Object* Object::dictGetValNF (int i, Object* obj) {
    return dict->getValNF (i, obj);
}

//------------------------------------------------------------------------
// Stream accessors.
//------------------------------------------------------------------------

bool Object::streamIs (char* dictType) {
    return stream->getDict ()->is (dictType);
}

bool Object::isStream (char* dictType) {
    return type == objStream && streamIs (dictType);
}

void Object::streamReset () { stream->reset (); }

void Object::streamClose () { stream->close (); }

int Object::streamGetChar () { return stream->getChar (); }

int Object::streamLookChar () { return stream->lookChar (); }

int Object::streamGetBlock (char* blk, int size) {
    return stream->getBlock (blk, size);
}

char* Object::streamGetLine (char* buf, int size) {
    return stream->getLine (buf, size);
}

GFileOffset Object::streamGetPos () { return stream->getPos (); }

void Object::streamSetPos (GFileOffset pos, int dir) {
    stream->setPos (pos, dir);
}

Dict* Object::streamGetDict () { return stream->getDict (); }

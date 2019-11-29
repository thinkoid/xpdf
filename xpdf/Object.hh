//========================================================================
//
// Object.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDF_XPDF_OBJECT_HH
#define XPDF_XPDF_OBJECT_HH

#include <defs.hh>

#include <cstdio>
#include <cstring>

#include <goo/gfile.hh>
#include <goo/GString.hh>

#include <xpdf/Dict.hh>

class XRef;
class Array;
class Stream;

//------------------------------------------------------------------------
// Ref
//------------------------------------------------------------------------

struct Ref {
    int num; // object number
    int gen; // generation number
};

//------------------------------------------------------------------------
// object types
//------------------------------------------------------------------------

enum ObjType {
    // simple objects
    objBool,   // boolean
    objInt,    // integer
    objReal,   // real
    objString, // string
    objName,   // name
    objNull,   // null

    // complex objects
    objArray,  // array
    objDict,   // dictionary
    objStream, // stream
    objRef,    // indirect reference

    // special objects
    objCmd,   // command name
    objError, // error return from Lexer
    objEOF,   // end of file return from Lexer
    objNone   // uninitialized object
};

#define numObjTypes 14 // total number of object types

//------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------

#define initObj(t) type = t

class Object {
public:
    // Default constructor.
    Object () : type (objNone) {}

    // Initialize an object.
    Object* initBool (bool boolnA) {
        initObj (objBool);
        booln = boolnA;
        return this;
    }
    Object* initInt (int intgA) {
        initObj (objInt);
        intg = intgA;
        return this;
    }
    Object* initReal (double realA) {
        initObj (objReal);
        real = realA;
        return this;
    }
    Object* initString (GString* stringA) {
        initObj (objString);
        string = stringA;
        return this;
    }
    Object* initName (const char* nameA) {
        initObj (objName);
        name = strdup (nameA);
        return this;
    }
    Object* initNull () {
        initObj (objNull);
        return this;
    }
    Object* initArray (XRef* xref);

    Object* initDict (XRef*);
    Object* initDict (Dict*);

    Object* initStream (Stream* streamA);
    Object* initRef (int numA, int genA) {
        initObj (objRef);
        ref.num = numA;
        ref.gen = genA;
        return this;
    }
    Object* initCmd (char* cmdA) {
        initObj (objCmd);
        cmd = strdup (cmdA);
        return this;
    }
    Object* initError () {
        initObj (objError);
        return this;
    }
    Object* initEOF () {
        initObj (objEOF);
        return this;
    }

    // Copy an object.
    Object* copy (Object* obj);

    // If object is a Ref, fetch and return the referenced object.
    // Otherwise, return a copy of the object.
    Object* fetch (XRef* xref, Object* obj, int recursion = 0);

    // Free object contents.
    void free ();

    // Type checking.
    ObjType getType () { return type; }
    bool isBool () { return type == objBool; }
    bool isInt () { return type == objInt; }
    bool isReal () { return type == objReal; }
    bool isNum () { return type == objInt || type == objReal; }
    bool isString () { return type == objString; }
    bool isName () { return type == objName; }
    bool isNull () { return type == objNull; }
    bool isArray () { return type == objArray; }
    bool isDict () { return type == objDict; }
    bool isStream () { return type == objStream; }
    bool isRef () { return type == objRef; }
    bool isCmd () { return type == objCmd; }
    bool isError () { return type == objError; }
    bool isEOF () { return type == objEOF; }
    bool isNone () { return type == objNone; }

    // Special type checking.
    bool isName (const char* nameA) const {
        return type == objName && 0 == strcmp (name, nameA);
    }

    bool isDict (const char* dictType);
    bool isStream (char* dictType);
    bool isCmd (const char* cmdA) {
        return type == objCmd && !strcmp (cmd, cmdA);
    }

    // Accessors.  NB: these assume object is of correct type.
    bool getBool () { return booln; }
    int getInt () { return intg; }
    double getReal () { return real; }
    double getNum () { return type == objInt ? (double)intg : real; }
    GString* getString () { return string; }
    char* getName () { return name; }
    Array* getArray () { return array; }
    Dict* getDict () { return dict; }
    Stream* getStream () { return stream; }
    Ref getRef () { return ref; }
    int getRefNum () { return ref.num; }
    int getRefGen () { return ref.gen; }
    char* getCmd () { return cmd; }

    // Array accessors.
    int arrayGetLength ();
    void arrayAdd (Object* elem);
    Object* arrayGet (int i, Object* obj);
    Object* arrayGetNF (int i, Object* obj);

    // Dict accessors.
    int dictGetLength ();
    void dictAdd (char* key, Object* val);
    bool dictIs (const char* dictType);
    Object* dictLookup (const char* key, Object* obj, int recursion = 0);
    Object* dictLookupNF (const char* key, Object* obj);
    const char* dictGetKey (int i);
    Object* dictGetVal (int i, Object* obj);
    Object* dictGetValNF (int i, Object* obj);

    // Stream accessors.
    bool streamIs (char* dictType);
    void streamReset ();
    void streamClose ();
    int streamGetChar ();
    int streamLookChar ();
    int streamGetBlock (char* blk, int size);
    char* streamGetLine (char* buf, int size);
    GFileOffset streamGetPos ();
    void streamSetPos (GFileOffset pos, int dir = 0);
    Dict* streamGetDict ();

    // Output.
    const char* getTypeName ();
    void print (FILE* f = stdout);

private:
    ObjType type;        // object type
    union {              // value for each type:
        bool booln;     //   boolean
        int intg;        //   integer
        double real;     //   real
        GString* string; //   string
        char* name;      //   name
        Array* array;    //   array
        Dict* dict;      //   dictionary
        Stream* stream;  //   stream
        Ref ref;         //   indirect reference
        char* cmd;       //   command
    };
};

//------------------------------------------------------------------------
// Array accessors.
//------------------------------------------------------------------------

#include <xpdf/Array.hh>

inline int Object::arrayGetLength () { return array->getLength (); }

inline void Object::arrayAdd (Object* elem) { array->add (elem); }

inline Object* Object::arrayGet (int i, Object* obj) {
    return array->get (i, obj);
}

inline Object* Object::arrayGetNF (int i, Object* obj) {
    return array->getNF (i, obj);
}

//------------------------------------------------------------------------
// Dict accessors.
//------------------------------------------------------------------------

#include <xpdf/Dict.hh>

inline int Object::dictGetLength () { return dict->size (); }

inline void Object::dictAdd (char* key, Object* val) { dict->insert (key, val); }

inline bool Object::dictIs (const char* dictType) {
    return dict->type_is (dictType);
}

inline bool Object::isDict (const char* dictType) {
    return type == objDict && dictIs (dictType);
}

inline Object*
Object::dictLookup (const char* key, Object* obj, int recursion) {
    return dict->lookup (key, obj, recursion);
}

inline Object* Object::dictLookupNF (const char* key, Object* obj) {
    return dict->lookupNF (key, obj);
}

inline const char* Object::dictGetKey (int n) {
    assert (n >= 0);
    return dict->key_at (n);
}

inline Object* Object::dictGetVal (int n, Object* pobj) {
    assert (n >= 0);
    return dict->fetch_at (n, pobj);
}

inline Object* Object::dictGetValNF (int n, Object* pobj) {
    assert (n >= 0);
    return dict->get_at (n, pobj);
}

//------------------------------------------------------------------------
// Stream accessors.
//------------------------------------------------------------------------

#include <xpdf/Stream.hh>

inline bool Object::streamIs (char* dictType) {
    return stream->getDict ()->type_is (dictType);
}

inline bool Object::isStream (char* dictType) {
    return type == objStream && streamIs (dictType);
}

inline void Object::streamReset () { stream->reset (); }

inline void Object::streamClose () { stream->close (); }

inline int Object::streamGetChar () { return stream->getChar (); }

inline int Object::streamLookChar () { return stream->lookChar (); }

inline int Object::streamGetBlock (char* blk, int size) {
    return stream->getBlock (blk, size);
}

inline char* Object::streamGetLine (char* buf, int size) {
    return stream->getLine (buf, size);
}

inline GFileOffset Object::streamGetPos () { return stream->getPos (); }

inline void Object::streamSetPos (GFileOffset pos, int dir) {
    stream->setPos (pos, dir);
}

inline Dict* Object::streamGetDict () { return stream->getDict (); }

#endif // XPDF_XPDF_OBJECT_HH

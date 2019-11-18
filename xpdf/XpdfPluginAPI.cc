//========================================================================
//
// XpdfPluginAPI.cc
//
// Copyright 2004 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#ifdef ENABLE_PLUGINS

#include <xpdf/GlobalParams.hh>
#include <xpdf/Object.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/XPDFCore.hh>
#include <xpdf/XpdfPluginAPI.hh>

//------------------------------------------------------------------------

//~ This should use a pool of Objects; change xpdfFreeObj to match.
static Object* allocObj () { return (Object*)malloc (sizeof (Object)); }

//------------------------------------------------------------------------
// Document access functions
//------------------------------------------------------------------------

XpdfObject _xpdfGetInfoDict (XpdfDoc doc) {
    Object* obj;

    obj = allocObj ();
    return (XpdfObject) ((PDFDoc*)doc)->getDocInfo (obj);
}

XpdfObject _xpdfGetCatalog (XpdfDoc doc) {
    Object* obj;

    obj = allocObj ();
    return (XpdfObject) ((PDFDoc*)doc)->getXRef ()->getCatalog (obj);
}

Widget _xpdfXGetWindow (XpdfDoc doc) {
    XPDFCore* core;

    if (!(core = (XPDFCore*)((PDFDoc*)doc)->getCore ())) { return NULL; }
    return core->getWidget ();
}

//------------------------------------------------------------------------
// Object access functions.
//------------------------------------------------------------------------

XpdfBool _xpdfObjIsBool (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isBool ();
}

XpdfBool _xpdfObjIsInt (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isInt ();
}

XpdfBool _xpdfObjIsReal (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isReal ();
}

XpdfBool _xpdfObjIsNumber (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isNum ();
}

XpdfBool _xpdfObjIsString (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isString ();
}

XpdfBool _xpdfObjIsName (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isName ();
}

XpdfBool _xpdfObjIsNull (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isNull ();
}

XpdfBool _xpdfObjIsArray (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isArray ();
}

XpdfBool _xpdfObjIsDict (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isDict ();
}

XpdfBool _xpdfObjIsStream (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isStream ();
}

XpdfBool _xpdfObjIsRef (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->isRef ();
}

XpdfBool _xpdfBoolValue (XpdfObject obj) {
    return (XpdfBool) ((Object*)obj)->getBool ();
}

int _xpdfIntValue (XpdfObject obj) {
    if (!((Object*)obj)->isInt ()) { return 0; }
    return ((Object*)obj)->getInt ();
}

double _xpdfRealValue (XpdfObject obj) {
    if (!((Object*)obj)->isReal ()) { return 0; }
    return ((Object*)obj)->getReal ();
}

double _xpdfNumberValue (XpdfObject obj) {
    if (!((Object*)obj)->isNum ()) { return 0; }
    return ((Object*)obj)->getNum ();
}

int _xpdfStringLength (XpdfObject obj) {
    if (!((Object*)obj)->isString ()) { return 0; }
    return ((Object*)obj)->getString ()->getLength ();
}

char* _xpdfStringValue (XpdfObject obj) {
    if (!((Object*)obj)->isString ()) { return 0; }
    return ((Object*)obj)->getString ()->c_str ();
}

char* _xpdfNameValue (XpdfObject obj) {
    if (!((Object*)obj)->isName ()) { return NULL; }
    return ((Object*)obj)->getName ();
}

int _xpdfArrayLength (XpdfObject obj) {
    if (!((Object*)obj)->isArray ()) { return 0; }
    return ((Object*)obj)->arrayGetLength ();
}

XpdfObject _xpdfArrayGet (XpdfObject obj, int idx) {
    Object* elem;

    elem = allocObj ();
    if (!((Object*)obj)->isArray ()) { return (XpdfObject)elem->initNull (); }
    return (XpdfObject) ((Object*)obj)->arrayGet (idx, elem);
}

XpdfObject _xpdfDictGet (XpdfObject obj, char* key) {
    Object* elem;

    elem = allocObj ();
    if (!((Object*)obj)->isDict ()) { return (XpdfObject)elem->initNull (); }
    return (XpdfObject) ((Object*)obj)->dictLookup (key, elem);
}

void _xpdfFreeObj (XpdfObject obj) {
    ((Object*)obj)->free ();
    free (obj);
}

//------------------------------------------------------------------------
// Memory allocation functions
//------------------------------------------------------------------------

void* _xpdfMalloc (int size) { return malloc (size); }

void* _xpdfRealloc (void* p, int size) { return realloc (p, size); }

void _xpdfFree (void* p) { free (p); }

//------------------------------------------------------------------------
// Security handlers
//------------------------------------------------------------------------

void _xpdfRegisterSecurityHandler (XpdfSecurityHandler* handler) {
    if (handler->version <= xpdfPluginAPIVersion) {
        globalParams->addSecurityHandler (handler);
    }
}

//------------------------------------------------------------------------

XpdfPluginVecTable xpdfPluginVecTable = {
    xpdfPluginAPIVersion, &_xpdfGetInfoDict,
    &_xpdfGetCatalog,     &_xpdfXGetWindow,
    &_xpdfObjIsBool,      &_xpdfObjIsInt,
    &_xpdfObjIsReal,      &_xpdfObjIsString,
    &_xpdfObjIsName,      &_xpdfObjIsNull,
    &_xpdfObjIsArray,     &_xpdfObjIsDict,
    &_xpdfObjIsStream,    &_xpdfObjIsRef,
    &_xpdfBoolValue,      &_xpdfIntValue,
    &_xpdfRealValue,      &_xpdfStringLength,
    &_xpdfStringValue,    &_xpdfNameValue,
    &_xpdfArrayLength,    &_xpdfArrayGet,
    &_xpdfDictGet,        &_xpdfFreeObj,
    &_xpdfMalloc,         &_xpdfRealloc,
    &_xpdfFree,           &_xpdfRegisterSecurityHandler,
};

#endif // ENABLE_PLUGINS

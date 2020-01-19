// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cctype>
#include <climits>

#include <iostream>

#include <goo/memory.hh>
#include <goo/gfile.hh>

#include <xpdf/object.hh>
#include <xpdf/Stream.hh>
#include <xpdf/lexer.hh>
#include <xpdf/Parser.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/ErrorCodes.hh>
#include <xpdf/XRef.hh>

//------------------------------------------------------------------------

#define xrefSearchSize 1024 // read this many bytes at end of file
//   to look for 'startxref'

//------------------------------------------------------------------------
// Permission bits
//------------------------------------------------------------------------

#define permPrint (1 << 2)
#define permChange (1 << 3)
#define permCopy (1 << 4)
#define permNotes (1 << 5)
#define defPermFlags 0xfffc

//------------------------------------------------------------------------
// XRefPosSet
//------------------------------------------------------------------------

class XRefPosSet {
public:
    XRefPosSet ();
    ~XRefPosSet ();
    void add (GFileOffset pos);
    bool check (GFileOffset pos);

private:
    int find (GFileOffset pos);

    GFileOffset* tab;
    int size;
    int len;
};

XRefPosSet::XRefPosSet () {
    size = 16;
    len = 0;
    tab = (GFileOffset*)calloc (size, sizeof (GFileOffset));
}

XRefPosSet::~XRefPosSet () { free (tab); }

void XRefPosSet::add (GFileOffset pos) {
    int i;

    i = find (pos);
    if (i < len && tab[i] == pos) { return; }
    if (len == size) {
        if (size > INT_MAX / 2) {
            std::cerr << "Integer overflow in XRefPosSet::add()" << std::endl;
            std::exit (1);
        }

        size *= 2;
        tab = (GFileOffset*)reallocarray (tab, size, sizeof (GFileOffset));
    }
    if (i < len) {
        memmove (&tab[i + 1], &tab[i], (len - i) * sizeof (GFileOffset));
    }
    tab[i] = pos;
    ++len;
}

bool XRefPosSet::check (GFileOffset pos) {
    int i;

    i = find (pos);
    return i < len && tab[i] == pos;
}

int XRefPosSet::find (GFileOffset pos) {
    int a, b, m;

    a = -1;
    b = len;
    // invariant: tab[a] < pos < tab[b]
    while (b - a > 1) {
        m = (a + b) / 2;
        if (tab[m] < pos) { a = m; }
        else if (tab[m] > pos) {
            b = m;
        }
        else {
            return m;
        }
    }
    return b;
}

//------------------------------------------------------------------------
// ObjectStream
//------------------------------------------------------------------------

class ObjectStream {
public:
    // Create an object stream, using object number <objStrNum>,
    // generation 0.
    ObjectStream (XRef* xref, int objStrNumA);

    bool isOk () { return ok; }

    ~ObjectStream ();

    // Return the object number of this object stream.
    int getObjStrNum () { return objStrNum; }

    // Get the <objIdx>th object from this stream, which should be
    // object number <objNum>, generation 0.
    Object* getObject (int objIdx, int objNum, Object* obj);

private:
    int objStrNum; // object number of the object stream
    int nObjects;  // number of objects in the stream
    Object* objs;  // the objects (length = nObjects)
    int* objNums;  // the object numbers (length = nObjects)
    bool ok;
};

ObjectStream::ObjectStream (XRef* xref, int objStrNumA) {
    Stream* str;
    int* offsets;
    Object objStr, obj1, obj2;
    int first, i;

    objStrNum = objStrNumA;
    nObjects = 0;
    objs = NULL;
    objNums = NULL;
    ok = false;

    if (!xref->fetch (objStrNum, 0, &objStr)->isStream ()) {
        return;
    }

    if (!objStr.streamGetDict ()->lookup ("N", &obj1)->isInt ()) {
        return;
    }

    nObjects = obj1.getInt ();

    if (nObjects <= 0) {
        return;
    }

    if (!objStr.streamGetDict ()->lookup ("First", &obj1)->isInt ()) {
        return;
    }

    first = obj1.getInt ();

    if (first < 0) {
        return;
    }

    // this is an arbitrary limit to avoid integer overflow problems
    // in the 'new Object[nObjects]' call (Acrobat apparently limits
    // object streams to 100-200 objects)
    if (nObjects > 1000000) {
        error (errSyntaxError, -1, "Too many objects in an object stream");
        return;
    }

    objs = new Object[nObjects];

    objNums = (int*)calloc (nObjects, sizeof (int));
    offsets = (int*)calloc (nObjects, sizeof (int));

    // parse the header: object numbers and offsets
    objStr.streamReset ();
    obj1.initNull ();

    str = new EmbedStream (objStr.getStream (), &obj1, true, first);

    Parser parser (xref, new xpdf::lexer_t (xref, str), false);

    for (i = 0; i < nObjects; ++i) {
        parser.getObj (&obj1, true);
        parser.getObj (&obj2, true);

        if (!obj1.isInt () || !obj2.isInt ()) {
            free (offsets);
            return;
        }

        objNums[i] = obj1.getInt ();
        offsets[i] = obj2.getInt ();

        if (objNums[i] < 0 || offsets[i] < 0 || (i > 0 && offsets[i] < offsets[i - 1])) {
            free (offsets);
            return;
        }
    }
    while (str->getChar () != EOF) ;

    // skip to the first object - this shouldn't be necessary because
    // the First key is supposed to be equal to offsets[0], but just in
    // case...
    if (i < offsets[0]) { objStr.getStream ()->discardChars (offsets[0] - i); }

    // parse the objects
    for (i = 0; i < nObjects; ++i) {
        obj1.initNull ();
        if (i == nObjects - 1) {
            str = new EmbedStream (objStr.getStream (), &obj1, false, 0);
        }
        else {
            str = new EmbedStream (
                objStr.getStream (), &obj1, true, offsets[i + 1] - offsets[i]);
        }

        Parser parser (xref, new xpdf::lexer_t (xref, str), false);
        parser.getObj (&objs[i]);

        while (str->getChar () != EOF) ;
    }

    free (offsets);
    ok = true;
}

ObjectStream::~ObjectStream () {
    if (objs) { delete[] objs; }
    free (objNums);
}

Object* ObjectStream::getObject (int objIdx, int objNum, Object* obj) {
    if (objIdx < 0 || objIdx >= nObjects || objNum != objNums[objIdx]) {
        return obj->initNull ();
    }

    return *obj = objs [objIdx], obj;
}

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

XRef::XRef (BaseStream* strA, bool repair) {
    GFileOffset pos;
    Object obj;
    int i;

    ok = true;
    errCode = errNone;
    size = 0;
    last = -1;
    entries = NULL;
    streamEnds = NULL;
    streamEndsLen = 0;
    for (i = 0; i < objStrCacheSize; ++i) { objStrs[i] = NULL; }

    encrypted = false;
    permFlags = defPermFlags;
    ownerPasswordOk = false;

    str = strA;
    start = str->getStart ();

    // if the 'repair' flag is set, try to reconstruct the xref table
    if (repair) {
        if (!(ok = constructXRef ())) {
            errCode = errDamaged;
            return;
        }

        // if the 'repair' flag is not set, read the xref table
    }
    else {
        // read the trailer
        pos = getStartXref ();
        if (pos == 0) {
            errCode = errDamaged;
            ok = false;
            return;
        }

        // read the xref table
        XRefPosSet posSet;
        while (readXRef (&pos, &posSet)) ;

        if (!ok) {
            errCode = errDamaged;
            return;
        }
    }

    // get the root dictionary (catalog) object
    trailerDict.dictLookupNF ("Root", &obj);

    if (obj.isRef ()) {
        rootNum = obj.getRefNum ();
        rootGen = obj.getRefGen ();
    }
    else {
        if (!(ok = constructXRef ())) {
            errCode = errDamaged;
            return;
        }
    }

    // now set the trailer dictionary's xref pointer so we can fetch
    // indirect objects from it
    trailerDict.getDict ()->setXRef (this);
}

XRef::~XRef () {
    free (entries);

    if (streamEnds) {
        free (streamEnds);
    }

    for (int i = 0; i < objStrCacheSize; ++i) {
        if (objStrs[i]) {
            delete objStrs[i];
        }
    }
}

// Read the 'startxref' position.
GFileOffset XRef::getStartXref () {
    char buf[xrefSearchSize + 1];
    char* p;
    int n, i;

    // read last xrefSearchSize bytes
    str->setPos (xrefSearchSize, -1);
    n = str->getBlock (buf, xrefSearchSize);
    buf[n] = '\0';

    // find startxref
    for (i = n - 9; i >= 0; --i) {
        if (!strncmp (&buf[i], "startxref", 9)) { break; }
    }
    if (i < 0) { return 0; }
    for (p = &buf[i + 9]; isspace (*p & 0xff); ++p)
        ;
    lastXRefPos = strToFileOffset (p);

    return lastXRefPos;
}

// Read one xref table section.  Also reads the associated trailer
// dictionary, and returns the prev pointer (if any).
bool XRef::readXRef (GFileOffset* pos, XRefPosSet* posSet) {
    Object obj;
    bool more;
    char buf[100];
    int n, i;

    // the xref data should either be "xref ..." (for an xref table) or
    // "nn gg obj << ... >> stream ..." (for an xref stream); possibly
    // preceded by whitespace
    str->setPos (start + *pos);
    n = str->getBlock (buf, 100);
    for (i = 0; i < n && xpdf::lexer_t::isSpace (buf[i]); ++i)
        ;

    // parse an old-style xref table
    if (i + 4 < n && buf[i] == 'x' && buf[i + 1] == 'r' && buf[i + 2] == 'e' &&
        buf[i + 3] == 'f' && xpdf::lexer_t::isSpace (buf[i + 4])) {
        more = readXRefTable (pos, i + 5, posSet);

        // parse an xref stream
    }
    else if (i < n && buf[i] >= '0' && buf[i] <= '9') {
        obj.initNull ();

        Parser parser (
            NULL,
            new xpdf::lexer_t (
                NULL, str->makeSubStream (start + *pos, false, 0, &obj)),
            true);

        if (!parser.getObj (&obj, true)->isInt ())      { goto err1; }
        if (!parser.getObj (&obj, true)->isInt ())      { goto err1; }
        if (!parser.getObj (&obj, true)->isCmd ("obj")) { goto err1; }
        if (!parser.getObj (&obj)->isStream ())         { goto err1; }

        more = readXRefStream (obj.getStream (), pos);
    }
    else {
        goto err1;
    }

    return more;

err1:
    ok = false;
    return false;
}

bool XRef::readXRefTable (GFileOffset* pos, int offset, XRefPosSet* posSet) {
    XRefEntry entry;
    Object obj, obj2;
    char buf[6];
    GFileOffset off, pos2;
    bool more;
    int first, n, newSize, gen, i, c;

    if (posSet->check (*pos)) {
        error (errSyntaxWarning, -1, "Infinite loop in xref table");
        return false;
    }
    posSet->add (*pos);

    str->setPos (start + *pos + offset);

    while (1) {
        do { c = str->getChar (); } while (xpdf::lexer_t::isSpace (c));
        if (c == 't') {
            if (str->getBlock (buf, 6) != 6 || memcmp (buf, "railer", 6)) {
                return ok = false;
            }
            break;
        }
        if (c < '0' || c > '9') { return ok = false; }
        first = 0;
        do {
            first = (first * 10) + (c - '0');
            c = str->getChar ();
        } while (c >= '0' && c <= '9');
        if (!xpdf::lexer_t::isSpace (c)) { return ok = false; }
        do { c = str->getChar (); } while (xpdf::lexer_t::isSpace (c));
        n = 0;
        do {
            n = (n * 10) + (c - '0');
            c = str->getChar ();
        } while (c >= '0' && c <= '9');
        if (!xpdf::lexer_t::isSpace (c)) { return ok = false; }
        if (first < 0 || n < 0 || first > INT_MAX - n) { return ok = false; }
        if (first + n > size) {
            for (newSize = size ? 2 * size : 1024;
                 first + n > newSize && newSize > 0; newSize <<= 1)
                ;
            if (newSize < 0) { return ok = false; }
            entries =
                (XRefEntry*)reallocarray (entries, newSize, sizeof (XRefEntry));
            for (i = size; i < newSize; ++i) {
                entries[i].offset = (GFileOffset)-1;
                entries[i].type = xrefEntryFree;
            }
            size = newSize;
        }
        for (i = first; i < first + n; ++i) {
            do { c = str->getChar (); } while (xpdf::lexer_t::isSpace (c));
            off = 0;
            do {
                off = (off * 10) + (c - '0');
                c = str->getChar ();
            } while (c >= '0' && c <= '9');
            if (!xpdf::lexer_t::isSpace (c)) { return ok = false; }
            entry.offset = off;
            do { c = str->getChar (); } while (xpdf::lexer_t::isSpace (c));
            gen = 0;
            do {
                gen = (gen * 10) + (c - '0');
                c = str->getChar ();
            } while (c >= '0' && c <= '9');
            if (!xpdf::lexer_t::isSpace (c)) { return ok = false; }
            entry.gen = gen;
            do { c = str->getChar (); } while (xpdf::lexer_t::isSpace (c));
            if (c == 'n') { entry.type = xrefEntryUncompressed; }
            else if (c == 'f') {
                entry.type = xrefEntryFree;
            }
            else {
                return ok = false;
            }
            c = str->getChar ();
            if (!xpdf::lexer_t::isSpace (c)) { return ok = false; }
            if (entries[i].offset == (GFileOffset)-1) {
                entries[i] = entry;
                // PDF files of patents from the IBM Intellectual Property
                // Network have a bug: the xref table claims to start at 1
                // instead of 0.
                if (i == 1 && first == 1 && entries[1].offset == 0 &&
                    entries[1].gen == 65535 &&
                    entries[1].type == xrefEntryFree) {
                    i = first = 0;
                    entries[0] = entries[1];
                    entries[1].offset = (GFileOffset)-1;
                }
                if (i > last) { last = i; }
            }
        }
    }

    // read the trailer dictionary
    obj.initNull ();

    Parser parser (
        NULL,
        new xpdf::lexer_t (NULL, str->makeSubStream (str->getPos (), false, 0, &obj)),
        true);

    parser.getObj (&obj);

    if (!obj.isDict ()) {
        return ok = false;
    }

    // get the 'Prev' pointer
    //~ this can be a 64-bit int (?)
    obj.getDict ()->lookupNF ("Prev", &obj2);
    if (obj2.isInt ()) {
        *pos = (GFileOffset) (unsigned)obj2.getInt ();
        more = true;
    }
    else if (obj2.isRef ()) {
        // certain buggy PDF generators generate "/Prev NNN 0 R" instead
        // of "/Prev NNN"
        *pos = (GFileOffset) (unsigned)obj2.getRefNum ();
        more = true;
    }
    else {
        more = false;
    }

    // save the first trailer dictionary
    if (trailerDict.isNone ()) {
        trailerDict = obj;
    }

    // check for an 'XRefStm' key
    //~ this can be a 64-bit int (?)
    if (obj.getDict ()->lookup ("XRefStm", &obj2)->isInt ()) {
        pos2 = (GFileOffset) (unsigned)obj2.getInt ();
        readXRef (&pos2, posSet);
        if (!ok) {
            return ok = false;
        }
    }

    return more;
}

bool XRef::readXRefStream (Stream* xrefStr, GFileOffset* pos) {
    Dict* dict;
    int w[3];
    bool more;
    Object obj, obj2, idx;
    int newSize, first, n, i;

    dict = xrefStr->getDict ();

    if (!dict->lookupNF ("Size", &obj)->isInt ()) { goto err1; }
    newSize = obj.getInt ();
    if (newSize < 0) { goto err1; }
    if (newSize > size) {
        entries = (XRefEntry*)reallocarray (entries, newSize, sizeof (XRefEntry));
        for (i = size; i < newSize; ++i) {
            entries[i].offset = (GFileOffset)-1;
            entries[i].type = xrefEntryFree;
        }
        size = newSize;
    }

    if (!dict->lookupNF ("W", &obj)->isArray () || obj.arrayGetLength () < 3) {
        goto err1;
    }
    for (i = 0; i < 3; ++i) {
        if (!obj.arrayGet (i, &obj2)->isInt ()) {
            goto err1;
        }
        w[i] = obj2.getInt ();
    }
    if (w[0] < 0 || w[0] > 4 || w[1] < 0 || w[1] > (int)sizeof (GFileOffset) ||
        w[2] < 0 || w[2] > 4) {
        goto err0;
    }

    xrefStr->reset ();
    dict->lookupNF ("Index", &idx);
    if (idx.isArray ()) {
        for (i = 0; i + 1 < idx.arrayGetLength (); i += 2) {
            if (!idx.arrayGet (i, &obj)->isInt ()) {
                goto err1;
            }
            first = obj.getInt ();
            if (!idx.arrayGet (i + 1, &obj)->isInt ()) {
                goto err1;
            }
            n = obj.getInt ();
            if (first < 0 || n < 0 ||
                !readXRefStreamSection (xrefStr, w, first, n)) {
                goto err0;
            }
        }
    }
    else {
        if (!readXRefStreamSection (xrefStr, w, 0, newSize)) {
            goto err0;
        }
    }

    //~ this can be a 64-bit int (?)
    dict->lookupNF ("Prev", &obj);
    if (obj.isInt ()) {
        *pos = (GFileOffset) (unsigned)obj.getInt ();
        more = true;
    }
    else {
        more = false;
    }

    if (trailerDict.isNone ()) {
        trailerDict.initDict (new Dict (*dict));
    }

    return more;

err1:
err0:
    ok = false;
    return false;
}

bool XRef::readXRefStreamSection (Stream* xrefStr, int* w, int first, int n) {
    GFileOffset offset;
    int type, gen, c, newSize, i, j;

    if (first + n < 0) { return false; }
    if (first + n > size) {
        for (newSize = size ? 2 * size : 1024;
             first + n > newSize && newSize > 0; newSize <<= 1)
            ;
        if (newSize < 0) { return false; }
        entries = (XRefEntry*)reallocarray (entries, newSize, sizeof (XRefEntry));
        for (i = size; i < newSize; ++i) {
            entries[i].offset = (GFileOffset)-1;
            entries[i].type = xrefEntryFree;
        }
        size = newSize;
    }
    for (i = first; i < first + n; ++i) {
        if (w[0] == 0) { type = 1; }
        else {
            for (type = 0, j = 0; j < w[0]; ++j) {
                if ((c = xrefStr->getChar ()) == EOF) { return false; }
                type = (type << 8) + c;
            }
        }
        for (offset = 0, j = 0; j < w[1]; ++j) {
            if ((c = xrefStr->getChar ()) == EOF) { return false; }
            offset = (offset << 8) + c;
        }
        for (gen = 0, j = 0; j < w[2]; ++j) {
            if ((c = xrefStr->getChar ()) == EOF) { return false; }
            gen = (gen << 8) + c;
        }
        if (entries[i].offset == (GFileOffset)-1) {
            switch (type) {
            case 0:
                entries[i].offset = offset;
                entries[i].gen = gen;
                entries[i].type = xrefEntryFree;
                break;
            case 1:
                entries[i].offset = offset;
                entries[i].gen = gen;
                entries[i].type = xrefEntryUncompressed;
                break;
            case 2:
                entries[i].offset = offset;
                entries[i].gen = gen;
                entries[i].type = xrefEntryCompressed;
                break;
            default: return false;
            }
            if (i > last) { last = i; }
        }
    }

    return true;
}

// Attempt to construct an xref table for a damaged file.
bool XRef::constructXRef () {
    Object newTrailerDict, obj;
    char buf[256];
    GFileOffset pos;
    int num, gen;
    int newSize;
    int streamEndsSize;
    char* p;
    int i;
    bool gotRoot;

    free (entries);
    size = 0;
    entries = NULL;

    gotRoot = false;
    streamEndsLen = streamEndsSize = 0;

    str->reset ();
    while (1) {
        pos = str->getPos ();
        if (!str->getLine (buf, 256)) { break; }
        p = buf;

        // skip whitespace
        while (*p && xpdf::lexer_t::isSpace (*p & 0xff)) ++p;

        // got trailer dictionary
        if (!strncmp (p, "trailer", 7)) {
            obj.initNull ();

            Parser parser (
                NULL,
                new xpdf::lexer_t (NULL, str->makeSubStream (pos + 7, false, 0, &obj)),
                false);

            parser.getObj (&newTrailerDict);

            if (newTrailerDict.isDict ()) {
                newTrailerDict.dictLookupNF ("Root", &obj);
                if (obj.isRef ()) {
                    rootNum = obj.getRefNum ();
                    rootGen = obj.getRefGen ();
                    trailerDict = newTrailerDict;
                    gotRoot = true;
                }
            }
        }
        else if (isdigit (*p & 0xff)) {
            // look for object
            num = atoi (p);
            if (num > 0) {
                do { ++p; } while (*p && isdigit (*p & 0xff));
                if (isspace (*p & 0xff)) {
                    do { ++p; } while (*p && isspace (*p & 0xff));
                    if (isdigit (*p & 0xff)) {
                        gen = atoi (p);
                        do { ++p; } while (*p && isdigit (*p & 0xff));
                        if (isspace (*p & 0xff)) {
                            do { ++p; } while (*p && isspace (*p & 0xff));
                            if (!strncmp (p, "obj", 3)) {
                                if (num >= size) {
                                    newSize = (num + 1 + 255) & ~255;
                                    if (newSize < 0) {
                                        error (
                                            errSyntaxError, -1,
                                            "Bad object number");
                                        return false;
                                    }
                                    entries = (XRefEntry*)reallocarray (
                                        entries, newSize, sizeof (XRefEntry));
                                    for (i = size; i < newSize; ++i) {
                                        entries[i].offset = (GFileOffset)-1;
                                        entries[i].type = xrefEntryFree;
                                    }
                                    size = newSize;
                                }
                                if (entries[num].type == xrefEntryFree ||
                                    gen >= entries[num].gen) {
                                    entries[num].offset = pos - start;
                                    entries[num].gen = gen;
                                    entries[num].type = xrefEntryUncompressed;
                                    if (num > last) { last = num; }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (!strncmp (p, "endstream", 9)) {
            if (streamEndsLen == streamEndsSize) {
                streamEndsSize += 64;
                streamEnds = (GFileOffset*)reallocarray (
                    streamEnds, streamEndsSize, sizeof (GFileOffset));
            }
            streamEnds[streamEndsLen++] = pos;
        }
    }

    if (gotRoot) { return true; }

    error (errSyntaxError, -1, "Couldn't find trailer dictionary");
    return false;
}

void XRef::setEncryption (
    int permFlagsA, bool ownerPasswordOkA, unsigned char* fileKeyA, int keyLengthA,
    int encVersionA, CryptAlgorithm encAlgorithmA) {
    int i;

    encrypted = true;
    permFlags = permFlagsA;
    ownerPasswordOk = ownerPasswordOkA;
    if (keyLengthA <= 32) { keyLength = keyLengthA; }
    else {
        keyLength = 32;
    }
    for (i = 0; i < keyLength; ++i) { fileKey[i] = fileKeyA[i]; }
    encVersion = encVersionA;
    encAlgorithm = encAlgorithmA;
}

bool XRef::okToPrint (bool ignoreOwnerPW) {
    return (!ignoreOwnerPW && ownerPasswordOk) || (permFlags & permPrint);
}

bool XRef::okToChange (bool ignoreOwnerPW) {
    return (!ignoreOwnerPW && ownerPasswordOk) || (permFlags & permChange);
}

bool XRef::okToCopy (bool ignoreOwnerPW) {
    return (!ignoreOwnerPW && ownerPasswordOk) || (permFlags & permCopy);
}

bool XRef::okToAddNotes (bool ignoreOwnerPW) {
    return (!ignoreOwnerPW && ownerPasswordOk) || (permFlags & permNotes);
}

Object* XRef::fetch (int num, int gen, Object* obj, int recursion) {
    XRefEntry* e;
    ObjectStream* objStr;
    Object obj1, obj2, obj3;

    // check for bogus ref - this can happen in corrupted PDF files
    if (num < 0 || num >= size) {
        goto err;
    }

    e = &entries[num];

    switch (e->type) {
    case xrefEntryUncompressed: {
        if (e->gen != gen) {
            goto err;
        }

        obj1.initNull ();

        Parser parser (
            this,
            new xpdf::lexer_t (
                this, str->makeSubStream (start + e->offset, false, 0, &obj1)),
            true);

        parser.getObj (&obj1, true);
        parser.getObj (&obj2, true);
        parser.getObj (&obj3, true);

        if (!obj1.isInt () || obj1.getInt () != num ||
            !obj2.isInt () || obj2.getInt () != gen ||
            !obj3.isCmd ("obj")) {
            goto err;
        }

        parser.getObj (
            obj, false, encrypted ? fileKey : (unsigned char*)NULL, encAlgorithm,
            keyLength, num, gen, recursion);
    }
        break;

    case xrefEntryCompressed:
        if (e->offset >= (GFileOffset)size ||
            entries[e->offset].type != xrefEntryUncompressed) {
            error (errSyntaxError, -1, "Invalid object stream");
            goto err;
        }

        if (!(objStr = getObjectStream ((int)e->offset))) {
            goto err;
        }

        objStr->getObject (e->gen, num, obj);
        break;

    default:
        goto err;
    }

    return obj;

err:
    return obj->initNull ();
}

ObjectStream* XRef::getObjectStream (int objStrNum) {
    ObjectStream* objStr;
    int i, j;

    // check the MRU entry in the cache
    if (objStrs[0] && objStrs[0]->getObjStrNum () == objStrNum) {
        return objStrs[0];
    }

    // check the rest of the cache
    for (i = 1; i < objStrCacheSize; ++i) {
        if (objStrs[i] && objStrs[i]->getObjStrNum () == objStrNum) {
            objStr = objStrs[i];
            for (j = i; j > 0; --j) { objStrs[j] = objStrs[j - 1]; }
            objStrs[0] = objStr;
            return objStr;
        }
    }

    // load a new ObjectStream
    objStr = new ObjectStream (this, objStrNum);
    if (!objStr->isOk ()) {
        delete objStr;
        return NULL;
    }
    if (objStrs[objStrCacheSize - 1]) { delete objStrs[objStrCacheSize - 1]; }
    for (j = objStrCacheSize - 1; j > 0; --j) { objStrs[j] = objStrs[j - 1]; }
    objStrs[0] = objStr;
    return objStr;
}

Object* XRef::getDocInfo (Object* obj) {
    return trailerDict.dictLookup ("Info", obj);
}

// Added for the pdftex project.
Object* XRef::getDocInfoNF (Object* obj) {
    return trailerDict.dictLookupNF ("Info", obj);
}

bool XRef::getStreamEnd (GFileOffset streamStart, GFileOffset* streamEnd) {
    int a, b, m;

    if (streamEndsLen == 0 || streamStart > streamEnds[streamEndsLen - 1]) {
        return false;
    }

    a = -1;
    b = streamEndsLen - 1;
    // invariant: streamEnds[a] < streamStart <= streamEnds[b]
    while (b - a > 1) {
        m = (a + b) / 2;
        if (streamStart <= streamEnds[m]) { b = m; }
        else {
            a = m;
        }
    }
    *streamEnd = streamEnds[b];
    return true;
}

GFileOffset XRef::strToFileOffset (char* s) {
    GFileOffset x, d;
    char* p;

    x = 0;
    for (p = s; *p && isdigit (*p & 0xff); ++p) {
        d = *p - '0';
        if (x > (GFILEOFFSET_MAX - d) / 10) { break; }
        x = 10 * x + d;
    }
    return x;
}

// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_XREF_HH
#define XPDF_XPDF_XREF_HH

#include <defs.hh>

#include <utils/path.hh>

#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>

class Parser;
class ObjectStream;
class XRefPosSet;

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

enum XRefEntryType { xrefEntryFree, xrefEntryUncompressed, xrefEntryCompressed };

struct XRefEntry
{
    off_t   offset;
    int           gen;
    XRefEntryType type;
};

struct XRefCacheEntry
{
    int    num;
    int    gen;
    Object obj;
};

#define objStrCacheSize 4

class XRef
{
public:
    // Constructor.  Read xref table from stream.
    XRef(BasicStream *strA, bool repair);

    // Destructor.
    ~XRef();

    // Is xref table valid?
    bool isOk() { return ok; }

    // Get the error code (if isOk() returns false).
    int getErrorCode() { return errCode; }

    // Set the encryption parameters.
    void setEncryption(int permFlagsA, bool ownerPasswordOkA,
                       unsigned char *fileKeyA, int keyLengthA, int encVersionA,
                       CryptAlgorithm encAlgorithmA);

    // Is the file encrypted?
    bool isEncrypted() { return encrypted; }

    // Check various permissions.
    bool okToPrint(bool ignoreOwnerPW = false);
    bool okToChange(bool ignoreOwnerPW = false);
    bool okToCopy(bool ignoreOwnerPW = false);
    bool okToAddNotes(bool ignoreOwnerPW = false);

    //
    // Get catalog object:
    //
    Object *getCatalog(Object *obj)
    {
        return *obj = fetch(rootNum, rootGen), obj;
    }

    //
    // Fetch an indirect reference:
    //
    Object *fetch(int num, int gen, Object *obj, int recursion = 0);

    //
    // Fetch a reference:
    //
    Object fetch(int num, int gen = 0, int recursion = 0)
    {
        Object obj;
        return fetch(num, gen, &obj, recursion), obj;
    }

    Object fetch(const Ref &ref, int recursion = 0)
    {
        return fetch(ref.num, ref.gen, recursion);
    }

    // Return the document's Info dictionary (if any).
    Object *getDocInfo(Object *obj);

    // Return the number of objects in the xref table.
    int getNumObjects() { return last + 1; }

    // Return the offset of the last xref table.
    off_t getLastXRefPos() { return lastXRefPos; }

    // Return the catalog object reference.
    int getRootNum() { return rootNum; }
    int getRootGen() { return rootGen; }

    // Get end position for a stream in a damaged file.
    // Returns false if unknown or file is not damaged.
    bool getStreamEnd(off_t streamStart, off_t *streamEnd);

    // Direct access.
    int getSize() { return size; }

    Object *getTrailerDict() { return &trailerDict; }

private:
    BasicStream *str; // input stream
    off_t start; // offset in file (to allow for garbage
        //   at beginning of file)
    XRefEntry *  entries; // xref entries
    int          size; // size of <entries> array
    int          last; // last used index in <entries>
    int          rootNum, rootGen; // catalog dict
    bool         ok; // true if xref table is valid
    int          errCode; // error code (if <ok> is false)
    Object       trailerDict; // trailer dictionary
    off_t  lastXRefPos; // offset of last xref table
    off_t *streamEnds; // 'endstream' positions - only used in
        //   damaged files
    int streamEndsLen; // number of valid entries in streamEnds
    ObjectStream * // cached object streams
                   objStrs[objStrCacheSize];
    bool           encrypted; // true if file is encrypted
    int            permFlags; // permission bits
    bool           ownerPasswordOk; // true if owner password is correct
    unsigned char  fileKey[32]; // file decryption key
    int            keyLength; // length of key, in bytes
    int            encVersion; // encryption version
    CryptAlgorithm encAlgorithm; // encryption algorithm

    off_t getStartXref();
    bool        readXRef(off_t *pos, XRefPosSet *posSet);
    bool        readXRefTable(off_t *pos, int offset, XRefPosSet *posSet);
    bool        readXRefStreamSection(StreamBase *xrefStr, int *w, int first, int n);
    bool        readXRefStream(StreamBase *xrefStr, off_t *pos);
    bool        constructXRef();
    ObjectStream *getObjectStream(int objStrNum);
    off_t   strToFileOffset(char *s);
};

#endif // XPDF_XPDF_XREF_HH

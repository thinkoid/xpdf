// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>

#include <goo/GString.hh>

#include <xpdf/Catalog.hh>
#include <xpdf/dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/ErrorCodes.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/Link.hh>
#include <xpdf/OptionalContent.hh>
#include <xpdf/Outline.hh>
#include <xpdf/OutputDev.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/Page.hh>
#include <xpdf/Parser.hh>
#include <xpdf/SecurityHandler.hh>
#include <xpdf/Stream.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Lexer.hh>

//------------------------------------------------------------------------

#define headerSearchSize 1024 // read this many bytes at beginning of
//   file to look for '%PDF'

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

PDFDoc::PDFDoc (
    GString* fileNameA, GString* ownerPassword, GString* userPassword,
    PDFCore* coreA) {
    Object obj;
    GString *fileName1, *fileName2;

    ok = false;
    errCode = errNone;

    core = coreA;

    file = NULL;
    str = NULL;
    xref = NULL;
    catalog = NULL;
    outline = NULL;
    optContent = NULL;

    fileName = fileNameA;

    fileName1 = fileName;

    // try to open file
    fileName2 = NULL;
    if (!(file = fopen (fileName1->c_str (), "rb"))) {
        fileName2 = fileName->copy ();
        fileName2->lowerCase ();
        if (!(file = fopen (fileName2->c_str (), "rb"))) {
            fileName2->upperCase ();
            if (!(file = fopen (fileName2->c_str (), "rb"))) {
                error (errIO, -1, "Couldn't open file '{0:t}'", fileName);
                delete fileName2;
                errCode = errOpenFile;
                return;
            }
        }
        delete fileName2;
    }

    // create stream
    obj = { };
    str = new FileStream (file, 0, false, 0, &obj);

    ok = setup (ownerPassword, userPassword);
}

PDFDoc::PDFDoc (
    BaseStream* strA, GString* ownerPassword, GString* userPassword,
    PDFCore* coreA) {
    ok = false;
    errCode = errNone;
    core = coreA;
    if (strA->getFileName ()) { fileName = strA->getFileName ()->copy (); }
    else {
        fileName = NULL;
    }
    file = NULL;
    str = strA;
    xref = NULL;
    catalog = NULL;
    outline = NULL;
    optContent = NULL;
    ok = setup (ownerPassword, userPassword);
}

bool PDFDoc::setup (GString* ownerPassword, GString* userPassword) {
    str->reset ();

    // check header
    checkHeader ();

    // read the xref and catalog
    if (!PDFDoc::setup2 (ownerPassword, userPassword, false)) {
        if (errCode == errDamaged || errCode == errBadCatalog) {
            // try repairing the xref table
            error (
                errSyntaxWarning, -1,
                "PDF file is damaged - attempting to reconstruct xref "
                "table...");
            if (!PDFDoc::setup2 (ownerPassword, userPassword, true)) {
                return false;
            }
        }
        else {
            return false;
        }
    }

    // read outline
    outline = new Outline (catalog->getOutline (), xref);

    // read the optional content info
    optContent = new OptionalContent (this);

    // done
    return true;
}

bool PDFDoc::setup2 (
    GString* ownerPassword, GString* userPassword, bool repairXRef) {
    // read xref table
    xref = new XRef (str, repairXRef);
    if (!xref->isOk ()) {
        error (errSyntaxError, -1, "Couldn't read xref table");
        errCode = xref->getErrorCode ();
        delete xref;
        xref = NULL;
        return false;
    }

    // check for encryption
    if (!checkEncryption (ownerPassword, userPassword)) {
        errCode = errEncrypted;
        delete xref;
        xref = NULL;
        return false;
    }

    // read catalog
    catalog = new Catalog (this);
    if (!catalog->isOk ()) {
        error (errSyntaxError, -1, "Couldn't read page catalog");
        errCode = errBadCatalog;
        delete catalog;
        catalog = NULL;
        delete xref;
        xref = NULL;
        return false;
    }

    return true;
}

PDFDoc::~PDFDoc () {
    if (optContent) { delete optContent; }
    if (outline) { delete outline; }
    if (catalog) { delete catalog; }
    if (xref) { delete xref; }
    if (str) { delete str; }
    if (file) { fclose (file); }
    if (fileName) { delete fileName; }
}

// Check for a PDF header on this stream.  Skip past some garbage
// if necessary.
void PDFDoc::checkHeader () {
    char hdrBuf[headerSearchSize + 1];
    char* p;
    int i;

    pdfVersion = 0;
    memset (hdrBuf, 0, headerSearchSize + 1);
    str->getBlock (hdrBuf, headerSearchSize);
    for (i = 0; i < headerSearchSize - 5; ++i) {
        if (!strncmp (&hdrBuf[i], "%PDF-", 5)) { break; }
    }
    if (i >= headerSearchSize - 5) {
        error (
            errSyntaxWarning, -1, "May not be a PDF file (continuing anyway)");
        return;
    }
    str->moveStart (i);
    if (!(p = strtok (&hdrBuf[i + 5], " \t\n\r"))) {
        error (
            errSyntaxWarning, -1, "May not be a PDF file (continuing anyway)");
        return;
    }
    pdfVersion = atof (p);
    if (!(hdrBuf[i + 5] >= '0' && hdrBuf[i + 5] <= '9') ||
        pdfVersion > XPDF_PDF_VERSION + 0.0001) {
        error (
            errSyntaxWarning, -1,
            "PDF version {0:s} -- xpdf supports version {1:s} (continuing "
            "anyway)",
            p, TO_S (XPDF_PDF_VERSION));
    }
}

bool PDFDoc::checkEncryption (GString* ownerPassword, GString* userPassword) {
    Object encrypt;
    bool encrypted;
    SecurityHandler* secHdlr;
    bool ret;

    encrypt = resolve (xref->getTrailerDict ()->as_dict ()["Encrypt"]);

    if ((encrypted = encrypt.is_dict ())) {
        if ((secHdlr = SecurityHandler::make (this, &encrypt))) {
            if (secHdlr->isUnencrypted ()) {
                // no encryption
                ret = true;
            }
            else if (secHdlr->checkEncryption (ownerPassword, userPassword)) {
                // authorization succeeded
                xref->setEncryption (
                    secHdlr->getPermissionFlags (),
                    secHdlr->getOwnerPasswordOk (), secHdlr->getFileKey (),
                    secHdlr->getFileKeyLength (), secHdlr->getEncVersion (),
                    secHdlr->getEncAlgorithm ());
                ret = true;
            }
            else {
                // authorization failed
                ret = false;
            }
            delete secHdlr;
        }
        else {
            // couldn't find the matching security handler
            ret = false;
        }
    }
    else {
        // document is not encrypted
        ret = true;
    }
    return ret;
}

void PDFDoc::displayPage (
    OutputDev* out, int page, double hDPI, double vDPI, int rotate,
    bool useMediaBox, bool crop, bool printing,
    bool (*abortCheckCbk) (void* data), void* abortCheckCbkData) {
    if (globalParams->getPrintCommands ()) {
        printf ("***** page %d *****\n", page);
    }
    catalog->getPage (page)->display (
        out, hDPI, vDPI, rotate, useMediaBox, crop, printing, abortCheckCbk,
        abortCheckCbkData);
}

void PDFDoc::displayPages (
    OutputDev* out, int firstPage, int lastPage, double hDPI, double vDPI,
    int rotate, bool useMediaBox, bool crop, bool printing,
    bool (*abortCheckCbk) (void* data), void* abortCheckCbkData) {
    int page;

    for (page = firstPage; page <= lastPage; ++page) {
        displayPage (
            out, page, hDPI, vDPI, rotate, useMediaBox, crop, printing,
            abortCheckCbk, abortCheckCbkData);
        catalog->doneWithPage (page);
    }
}

void PDFDoc::displayPageSlice (
    OutputDev* out, int page, double hDPI, double vDPI, int rotate,
    bool useMediaBox, bool crop, bool printing, int sliceX, int sliceY,
    int sliceW, int sliceH, bool (*abortCheckCbk) (void* data),
    void* abortCheckCbkData) {
    catalog->getPage (page)->displaySlice (
        out, hDPI, vDPI, rotate, useMediaBox, crop, sliceX, sliceY, sliceW,
        sliceH, printing, abortCheckCbk, abortCheckCbkData);
}

Links* PDFDoc::getLinks (int page) {
    return catalog->getPage (page)->getLinks ();
}

void PDFDoc::processLinks (OutputDev* out, int page) {
    catalog->getPage (page)->processLinks (out);
}

bool PDFDoc::isLinearized () {
    Parser* parser;
    Object obj1, obj2, obj3, obj4, obj5;
    bool lin;

    lin = false;
    obj1 = { };

    parser = new Parser (
        xref,
        new Lexer (
            str->makeSubStream (str->getStart (), false, 0, &obj1)),
        true);

    parser->getObj (&obj1);
    parser->getObj (&obj2);
    parser->getObj (&obj3);
    parser->getObj (&obj4);
    if (obj1.is_int () && obj2.is_int () && obj3.is_cmd ("obj") &&
        obj4.is_dict ()) {
        *&obj5 = resolve (obj4.as_dict ()["Linearized"]);
        if (obj5.is_num () && obj5.as_num () > 0) { lin = true; }
    }
    delete parser;
    return lin;
}

bool PDFDoc::saveAs (GString* name) {
    FILE* f;
    char buf[4096];
    int n;

    if (!(f = fopen (name->c_str (), "wb"))) {
        error (errIO, -1, "Couldn't open file '{0:t}'", name);
        return false;
    }
    str->reset ();
    while ((n = str->getBlock (buf, sizeof (buf))) > 0) {
        fwrite (buf, 1, n, f);
    }
    str->close ();
    fclose (f);
    return true;
}

bool PDFDoc::saveEmbeddedFile (int idx, char* path) {
    FILE* f;
    bool ret;

    if (!(f = fopen (path, "wb"))) { return false; }
    ret = saveEmbeddedFile2 (idx, f);
    fclose (f);
    return ret;
}

bool PDFDoc::saveEmbeddedFile2 (int idx, FILE* f) {
    Object strObj;
    char buf[4096];
    int n;

    if (!catalog->getEmbeddedFileStreamObj (idx, &strObj)) { return false; }
    strObj.streamReset ();
    while ((n = strObj.streamGetBlock (buf, sizeof (buf))) > 0) {
        fwrite (buf, 1, n, f);
    }
    strObj.streamClose ();
    return true;
}

char* PDFDoc::getEmbeddedFileMem (int idx, int* size) {
    Object strObj;
    char* buf;
    int bufSize, sizeInc, n;

    if (!catalog->getEmbeddedFileStreamObj (idx, &strObj)) { return NULL; }
    strObj.streamReset ();
    bufSize = 0;
    buf = NULL;
    do {
        sizeInc = bufSize ? bufSize : 1024;
        if (bufSize > INT_MAX - sizeInc) {
            error (errIO, -1, "embedded file is too large");
            *size = 0;
            return NULL;
        }
        buf = (char*)realloc (buf, bufSize + sizeInc);
        n = strObj.streamGetBlock (buf + bufSize, sizeInc);
        bufSize += n;
    } while (n == sizeInc);
    strObj.streamClose ();
    *size = bufSize;
    return buf;
}

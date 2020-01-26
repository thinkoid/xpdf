// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_PDFDOC_HH
#define XPDF_XPDF_PDFDOC_HH

#include <defs.hh>

#include <cstdio>
#include <xpdf/XRef.hh>
#include <xpdf/Catalog.hh>
#include <xpdf/Page.hh>

class GString;
class BaseStream;
class OutputDev;
class Links;
class LinkAction;
class LinkDest;
class Outline;
class OptionalContent;
class PDFCore;

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

class PDFDoc {
public:
    PDFDoc (
        GString* fileNameA, GString* ownerPassword = NULL,
        GString* userPassword = NULL, PDFCore* coreA = NULL);
    PDFDoc (
        BaseStream* strA, GString* ownerPassword = NULL,
        GString* userPassword = NULL, PDFCore* coreA = NULL);
    ~PDFDoc ();

    // Was PDF document successfully opened?
    bool isOk () { return ok; }

    // Get the error code (if isOk() returns false).
    int getErrorCode () { return errCode; }

    // Get file name.
    GString* getFileName () { return fileName; }

    // Get the xref table.
    XRef* getXRef () { return xref; }

    // Get catalog.
    Catalog* getCatalog () { return catalog; }

    // Get base stream.
    BaseStream* getBaseStream () { return str; }

    // Get page parameters.
    double getPageMediaWidth (int page) {
        return catalog->getPage (page)->getMediaWidth ();
    }
    double getPageMediaHeight (int page) {
        return catalog->getPage (page)->getMediaHeight ();
    }
    double getPageCropWidth (int page) {
        return catalog->getPage (page)->getCropWidth ();
    }
    double getPageCropHeight (int page) {
        return catalog->getPage (page)->getCropHeight ();
    }
    int getPageRotate (int page) {
        return catalog->getPage (page)->getRotate ();
    }

    // Get number of pages.
    int getNumPages () { return catalog->getNumPages (); }

    // Return the contents of the metadata stream, or NULL if there is
    // no metadata.
    GString* readMetadata () { return catalog->readMetadata (); }

    // Return the structure tree root object.
    Object* getStructTreeRoot () { return catalog->getStructTreeRoot (); }

    // Display a page.
    void displayPage (
        OutputDev* out, int page, double hDPI, double vDPI, int rotate,
        bool useMediaBox, bool crop, bool printing,
        bool (*abortCheckCbk) (void* data) = NULL,
        void* abortCheckCbkData = NULL);

    // Display a range of pages.
    void displayPages (
        OutputDev* out, int firstPage, int lastPage, double hDPI, double vDPI,
        int rotate, bool useMediaBox, bool crop, bool printing,
        bool (*abortCheckCbk) (void* data) = NULL,
        void* abortCheckCbkData = NULL);

    // Display part of a page.
    void displayPageSlice (
        OutputDev* out, int page, double hDPI, double vDPI, int rotate,
        bool useMediaBox, bool crop, bool printing, int sliceX, int sliceY,
        int sliceW, int sliceH, bool (*abortCheckCbk) (void* data) = NULL,
        void* abortCheckCbkData = NULL);

    // Find a page, given its object ID.  Returns page number, or 0 if
    // not found.
    int findPage (int num, int gen) { return catalog->findPage (num, gen); }

    // Returns the links for the current page, transferring ownership to
    // the caller.
    Links* getLinks (int page);

    // Find a named destination.  Returns the link destination, or
    // NULL if <name> is not a destination.
    LinkDest* findDest (GString* name) { return catalog->findDest (name); }

    // Process the links for a page.
    void processLinks (OutputDev* out, int page);

#ifndef DISABLE_OUTLINE
    // Return the outline object.
    Outline* getOutline () { return outline; }
#endif

    // Return the OptionalContent object.
    OptionalContent* getOptionalContent () { return optContent; }

    // Is the file encrypted?
    bool isEncrypted () { return xref->isEncrypted (); }

    // Check various permissions.
    bool okToPrint (bool ignoreOwnerPW = false) {
        return xref->okToPrint (ignoreOwnerPW);
    }
    bool okToChange (bool ignoreOwnerPW = false) {
        return xref->okToChange (ignoreOwnerPW);
    }
    bool okToCopy (bool ignoreOwnerPW = false) {
        return xref->okToCopy (ignoreOwnerPW);
    }
    bool okToAddNotes (bool ignoreOwnerPW = false) {
        return xref->okToAddNotes (ignoreOwnerPW);
    }

    // Is this document linearized?
    bool isLinearized ();

    // Return the document's Info dictionary (if any).
    Object* getDocInfo (Object* obj) { return xref->getDocInfo (obj); }
    Object* getDocInfoNF (Object* obj) { return xref->getDocInfoNF (obj); }

    // Return the PDF version specified by the file.
    double getPDFVersion () { return pdfVersion; }

    // Save this file with another name.
    bool saveAs (GString* name);

    // Return a pointer to the PDFCore object.
    PDFCore* getCore () { return core; }

    // Get the list of embedded files.
    int getNumEmbeddedFiles () { return catalog->getNumEmbeddedFiles (); }
    Unicode* getEmbeddedFileName (int idx) {
        return catalog->getEmbeddedFileName (idx);
    }
    int getEmbeddedFileNameLength (int idx) {
        return catalog->getEmbeddedFileNameLength (idx);
    }
    bool saveEmbeddedFile (int idx, char* path);
    char* getEmbeddedFileMem (int idx, int* size);

private:
    bool setup (GString* ownerPassword, GString* userPassword);
    bool
    setup2 (GString* ownerPassword, GString* userPassword, bool repairXRef);
    void checkHeader ();
    bool checkEncryption (GString* ownerPassword, GString* userPassword);
    bool saveEmbeddedFile2 (int idx, FILE* f);

    GString* fileName;
    FILE* file;
    BaseStream* str;
    PDFCore* core;
    double pdfVersion;
    XRef* xref;
    Catalog* catalog;
#ifndef DISABLE_OUTLINE
    Outline* outline;
#endif
    OptionalContent* optContent;

    bool ok;
    int errCode;
};

#endif // XPDF_XPDF_PDFDOC_HH

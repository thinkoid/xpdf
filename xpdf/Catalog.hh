// -*- mode: c++; -*-
// Copyright 1996-2007 Glyph & Cog, LLC

#ifndef XPDF_XPDF_CATALOG_HH
#define XPDF_XPDF_CATALOG_HH

#include <defs.hh>

#include <xpdf/CharTypes.hh>
#include <xpdf/obj.hh>

class GList;
class PDFDoc;
class XRef;
class Page;
class PageAttrs;
class LinkDest;
class PageTreeNode;
class Form;

//------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------

class Catalog
{
public:
    // Constructor.
    Catalog(PDFDoc *docA);

    // Destructor.
    ~Catalog();

    // Is catalog valid?
    bool isOk() { return ok; }

    // Get number of pages.
    int getNumPages() { return numPages; }

    // Get a page.
    Page *getPage(int i);

    // Get the reference for a page object.
    Ref *getPageRef(int i);

    // Remove a page from the catalog.  (It can be reloaded later by
    // calling getPage).
    void doneWithPage(int i);

    // Return base URI, or NULL if none.
    GString *getBaseURI() { return baseURI; }

    // Return the contents of the metadata stream, or NULL if there is
    // no metadata.
    GString *readMetadata();

    // Return the structure tree root object.
    Object *getStructTreeRoot() { return &structTreeRoot; }

    // Find a page, given its object ID.  Returns page number, or 0 if
    // not found.
    int findPage(int num, int gen);

    // Find a named destination.  Returns the link destination, or
    // NULL if <name> is not a destination.
    LinkDest *findDest(GString *name);

    Object *getDests() { return &dests; }

    Object *getNameTree() { return &nameTree; }

    Object *getOutline() { return &outline; }

    Object *getAcroForm() { return &acroForm; }

    Form *getForm() { return form; }

    Object *getOCProperties() { return &ocProperties; }

    // Get the list of embedded files.
    int      getNumEmbeddedFiles();
    Unicode *getEmbeddedFileName(int idx);
    int      getEmbeddedFileNameLength(int idx);
    Object * getEmbeddedFileStreamRef(int idx);
    Object * getEmbeddedFileStreamObj(int idx, Object *strObj);

private:
    PDFDoc *      doc;
    XRef *        xref; // the xref table for this PDF file
    PageTreeNode *pageTree; // the page tree
    Page **       pages; // array of pages
    Ref *         pageRefs; // object ID for each page
    int           numPages; // number of pages
    Object        dests; // named destination dictionary
    Object        nameTree; // name tree
    GString *     baseURI; // base URI for URI-type links
    Object        metadata; // metadata stream
    Object        structTreeRoot; // structure tree root dictionary
    Object        outline; // outline dictionary
    Object        acroForm; // AcroForm dictionary
    Form *        form; // parsed form
    Object        ocProperties; // OCProperties dictionary
    GList *       embeddedFiles; // embedded file list [EmbeddedFile]
    bool          ok; // true if catalog is valid

    Object *findDestInTree(Object *tree, GString *name, Object *obj);
    bool    readPageTree(Object *catDict);
    int     countPageTree(Object *pagesObj);
    void    loadPage(int pg);
    void    loadPage2(int pg, int relPg, PageTreeNode *node);
    void    readEmbeddedFileList(Dict *catDict);
    void    readEmbeddedFileTree(Object *node);
    void readFileAttachmentAnnots(const Object &pageNodeRef, char *touchedObjs);
    void readEmbeddedFile(const Object &fileSpec, const Object &name1);
};

#endif // XPDF_XPDF_CATALOG_HH

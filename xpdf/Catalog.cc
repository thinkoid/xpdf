//========================================================================
//
// Catalog.cc
//
// Copyright 1996-2013 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <cstring>
#include <cstddef>
#include <climits>

#include <goo/memory.hh>
#include <goo/gfile.hh>
#include <goo/GList.hh>

#include <xpdf/Object.hh>
#include <xpdf/CharTypes.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Page.hh>
#include <xpdf/Error.hh>
#include <xpdf/Link.hh>
#include <xpdf/Form.hh>
#include <xpdf/TextString.hh>
#include <xpdf/Catalog.hh>

//------------------------------------------------------------------------
// PageTreeNode
//------------------------------------------------------------------------

class PageTreeNode {
public:
    PageTreeNode (Ref refA, int countA, PageTreeNode* parentA);
    ~PageTreeNode ();

    Ref ref;
    int count;
    PageTreeNode* parent;
    GList* kids; // [PageTreeNode]
    PageAttrs* attrs;
};

PageTreeNode::PageTreeNode (Ref refA, int countA, PageTreeNode* parentA) {
    ref = refA;
    count = countA;
    parent = parentA;
    kids = NULL;
    attrs = NULL;
}

PageTreeNode::~PageTreeNode () {
    delete attrs;
    if (kids) { deleteGList (kids, PageTreeNode); }
}

//------------------------------------------------------------------------
// EmbeddedFile
//------------------------------------------------------------------------

class EmbeddedFile {
public:
    EmbeddedFile (TextString* nameA, Object* streamRefA);
    ~EmbeddedFile ();

    TextString* name;
    Object streamRef;
};

EmbeddedFile::EmbeddedFile (TextString* nameA, Object* streamRefA) {
    name = nameA;
    streamRefA->copy (&streamRef);
}

EmbeddedFile::~EmbeddedFile () {
    delete name;
    streamRef.free ();
}

//------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------

Catalog::Catalog (PDFDoc* docA) {
    Object catDict;
    Object obj, obj2;

    ok = true;
    doc = docA;
    xref = doc->getXRef ();
    pageTree = NULL;
    pages = NULL;
    pageRefs = NULL;
    numPages = 0;
    baseURI = NULL;
    form = NULL;
    embeddedFiles = NULL;

    xref->getCatalog (&catDict);
    if (!catDict.isDict ()) {
        error (
            errSyntaxError, -1, "Catalog object is wrong type ({0:s})",
            catDict.getTypeName ());
        goto err1;
    }

    // read page tree
    if (!readPageTree (&catDict)) { goto err1; }

    // read named destination dictionary
    catDict.dictLookup ("Dests", &dests);

    // read root of named destination tree
    if (catDict.dictLookup ("Names", &obj)->isDict ())
        obj.dictLookup ("Dests", &nameTree);
    else
        nameTree.initNull ();
    obj.free ();

    // read base URI
    if (catDict.dictLookup ("URI", &obj)->isDict ()) {
        if (obj.dictLookup ("Base", &obj2)->isString ()) {
            baseURI = obj2.getString ()->copy ();
        }
        obj2.free ();
    }
    obj.free ();
    if (!baseURI || baseURI->getLength () == 0) {
        if (baseURI) { delete baseURI; }
        if (doc->getFileName ()) {
            baseURI = makePathAbsolute (grabPath (doc->getFileName ()->c_str ()));
            if (baseURI->getChar (0) == '/') {
                baseURI->insert (0, "file://localhost");
            }
            else {
                baseURI->insert (0, "file://localhost/");
            }
        }
        else {
            baseURI = new GString ("file://localhost/");
        }
    }

    // get the metadata stream
    catDict.dictLookup ("Metadata", &metadata);

    // get the structure tree root
    catDict.dictLookup ("StructTreeRoot", &structTreeRoot);

    // get the outline dictionary
    catDict.dictLookup ("Outlines", &outline);

    // get the AcroForm dictionary
    catDict.dictLookup ("AcroForm", &acroForm);

    if (!acroForm.isNull ()) { form = Form::load (doc, this, &acroForm); }

    // get the OCProperties dictionary
    catDict.dictLookup ("OCProperties", &ocProperties);

    // get the list of embedded files
    readEmbeddedFileList (catDict.getDict ());

    catDict.free ();
    return;

err1:
    catDict.free ();
    dests.initNull ();
    nameTree.initNull ();
    ok = false;
}

Catalog::~Catalog () {
    int i;

    if (pageTree) { delete pageTree; }
    if (pages) {
        for (i = 0; i < numPages; ++i) {
            if (pages[i]) { delete pages[i]; }
        }
        free (pages);
        free (pageRefs);
    }
    dests.free ();
    nameTree.free ();
    if (baseURI) { delete baseURI; }
    metadata.free ();
    structTreeRoot.free ();
    outline.free ();
    acroForm.free ();
    if (form) { delete form; }
    ocProperties.free ();
    if (embeddedFiles) { deleteGList (embeddedFiles, EmbeddedFile); }
}

Page* Catalog::getPage (int i) {
    if (!pages[i - 1]) { loadPage (i); }
    return pages[i - 1];
}

Ref* Catalog::getPageRef (int i) {
    if (!pages[i - 1]) { loadPage (i); }
    return &pageRefs[i - 1];
}

void Catalog::doneWithPage (int i) {
    if (pages[i - 1]) {
        delete pages[i - 1];
        pages[i - 1] = NULL;
    }
}

GString* Catalog::readMetadata () {
    GString* s;
    Dict* dict;
    Object obj;
    char buf[4096];
    int n;

    if (!metadata.isStream ()) { return NULL; }
    dict = metadata.streamGetDict ();
    if (!dict->lookup ("Subtype", &obj)->isName ("XML")) {
        error (
            errSyntaxWarning, -1, "Unknown Metadata type: '{0:s}'",
            obj.isName () ? obj.getName () : "???");
    }
    obj.free ();
    s = new GString ();
    metadata.streamReset ();
    while ((n = metadata.streamGetBlock (buf, sizeof (buf))) > 0) {
        s->append (buf, n);
    }
    metadata.streamClose ();
    return s;
}

int Catalog::findPage (int num, int gen) {
    int i;

    for (i = 0; i < numPages; ++i) {
        if (!pages[i]) { loadPage (i + 1); }
        if (pageRefs[i].num == num && pageRefs[i].gen == gen) return i + 1;
    }
    return 0;
}

LinkDest* Catalog::findDest (GString* name) {
    LinkDest* dest;
    Object obj1, obj2;
    bool found;

    // try named destination dictionary then name tree
    found = false;
    if (dests.isDict ()) {
        if (!dests.dictLookup (name->c_str (), &obj1)->isNull ())
            found = true;
        else
            obj1.free ();
    }
    if (!found && nameTree.isDict ()) {
        if (!findDestInTree (&nameTree, name, &obj1)->isNull ())
            found = true;
        else
            obj1.free ();
    }
    if (!found) return NULL;

    // construct LinkDest
    dest = NULL;
    if (obj1.isArray ()) { dest = new LinkDest (obj1.getArray ()); }
    else if (obj1.isDict ()) {
        if (obj1.dictLookup ("D", &obj2)->isArray ())
            dest = new LinkDest (obj2.getArray ());
        else
            error (errSyntaxWarning, -1, "Bad named destination value");
        obj2.free ();
    }
    else {
        error (errSyntaxWarning, -1, "Bad named destination value");
    }
    obj1.free ();
    if (dest && !dest->isOk ()) {
        delete dest;
        dest = NULL;
    }

    return dest;
}

Object* Catalog::findDestInTree (Object* tree, GString* name, Object* obj) {
    Object names, name1;
    Object kids, kid, limits, low, high;
    bool done, found;
    int cmp, i;

    // leaf node
    if (tree->dictLookup ("Names", &names)->isArray ()) {
        done = found = false;
        for (i = 0; !done && i < names.arrayGetLength (); i += 2) {
            if (names.arrayGet (i, &name1)->isString ()) {
                cmp = name->cmp (name1.getString ());
                if (cmp == 0) {
                    names.arrayGet (i + 1, obj);
                    found = true;
                    done = true;
                }
                else if (cmp < 0) {
                    done = true;
                }
            }
            name1.free ();
        }
        names.free ();
        if (!found) obj->initNull ();
        return obj;
    }
    names.free ();

    // root or intermediate node
    done = false;
    if (tree->dictLookup ("Kids", &kids)->isArray ()) {
        for (i = 0; !done && i < kids.arrayGetLength (); ++i) {
            if (kids.arrayGet (i, &kid)->isDict ()) {
                if (kid.dictLookup ("Limits", &limits)->isArray ()) {
                    if (limits.arrayGet (0, &low)->isString () &&
                        name->cmp (low.getString ()) >= 0) {
                        if (limits.arrayGet (1, &high)->isString () &&
                            name->cmp (high.getString ()) <= 0) {
                            findDestInTree (&kid, name, obj);
                            done = true;
                        }
                        high.free ();
                    }
                    low.free ();
                }
                limits.free ();
            }
            kid.free ();
        }
    }
    kids.free ();

    // name was outside of ranges of all kids
    if (!done) obj->initNull ();

    return obj;
}

bool Catalog::readPageTree (Object* catDict) {
    Object topPagesRef, topPagesObj, countObj;
    int i;

    if (!catDict->dictLookupNF ("Pages", &topPagesRef)->isRef ()) {
        error (
            errSyntaxError, -1,
            "Top-level pages reference is wrong type ({0:s})",
            topPagesRef.getTypeName ());
        topPagesRef.free ();
        return false;
    }
    if (!topPagesRef.fetch (xref, &topPagesObj)->isDict ()) {
        error (
            errSyntaxError, -1, "Top-level pages object is wrong type ({0:s})",
            topPagesObj.getTypeName ());
        topPagesObj.free ();
        topPagesRef.free ();
        return false;
    }
    if (topPagesObj.dictLookup ("Count", &countObj)->isInt ()) {
        numPages = countObj.getInt ();
        if (numPages == 0) {
            // Acrobat apparently scans the page tree if it sees a zero count
            numPages = countPageTree (&topPagesObj);
        }
    }
    else {
        // assume we got a Page node instead of a Pages node
        numPages = 1;
    }
    countObj.free ();
    if (numPages < 0) {
        error (errSyntaxError, -1, "Invalid page count");
        topPagesObj.free ();
        topPagesRef.free ();
        numPages = 0;
        return false;
    }
    pageTree = new PageTreeNode (topPagesRef.getRef (), numPages, NULL);
    topPagesObj.free ();
    topPagesRef.free ();
    pages = (Page**)reallocarray (pages, numPages, sizeof (Page*));
    pageRefs = (Ref*)reallocarray (pageRefs, numPages, sizeof (Ref));
    for (i = 0; i < numPages; ++i) {
        pages[i] = NULL;
        pageRefs[i].num = -1;
        pageRefs[i].gen = -1;
    }
    return true;
}

int Catalog::countPageTree (Object* pagesObj) {
    Object kids, kid;
    int n, n2, i;

    if (!pagesObj->isDict ()) { return 0; }
    if (pagesObj->dictLookup ("Kids", &kids)->isArray ()) {
        n = 0;
        for (i = 0; i < kids.arrayGetLength (); ++i) {
            kids.arrayGet (i, &kid);
            n2 = countPageTree (&kid);
            if (n2 < INT_MAX - n) { n += n2; }
            else {
                error (errSyntaxError, -1, "Page tree contains too many pages");
                n = INT_MAX;
            }
            kid.free ();
        }
    }
    else {
        n = 1;
    }
    kids.free ();
    return n;
}

void Catalog::loadPage (int pg) { loadPage2 (pg, pg - 1, pageTree); }

void Catalog::loadPage2 (int pg, int relPg, PageTreeNode* node) {
    Object pageRefObj, pageObj, kidsObj, kidRefObj, kidObj, countObj;
    PageTreeNode *kidNode, *p;
    PageAttrs* attrs;
    int count, i;

    if (relPg >= node->count) {
        error (errSyntaxError, -1, "Internal error in page tree");
        pages[pg - 1] = new Page (doc, pg);
        return;
    }

    // if this node has not been filled in yet, it's either a leaf node
    // or an unread internal node
    if (!node->kids) {
        // check for a loop in the page tree
        for (p = node->parent; p; p = p->parent) {
            if (node->ref.num == p->ref.num && node->ref.gen == p->ref.gen) {
                error (errSyntaxError, -1, "Loop in Pages tree");
                pages[pg - 1] = new Page (doc, pg);
                return;
            }
        }

        // fetch the Page/Pages object
        pageRefObj.initRef (node->ref.num, node->ref.gen);
        if (!pageRefObj.fetch (xref, &pageObj)->isDict ()) {
            error (
                errSyntaxError, -1, "Page tree object is wrong type ({0:s})",
                pageObj.getTypeName ());
            pageObj.free ();
            pageRefObj.free ();
            pages[pg - 1] = new Page (doc, pg);
            return;
        }

        // merge the PageAttrs
        attrs = new PageAttrs (
            node->parent ? node->parent->attrs : (PageAttrs*)NULL,
            pageObj.getDict ());

        // if "Kids" exists, it's an internal node
        if (pageObj.dictLookup ("Kids", &kidsObj)->isArray ()) {
            // save the PageAttrs
            node->attrs = attrs;

            // read the kids
            node->kids = new GList ();
            for (i = 0; i < kidsObj.arrayGetLength (); ++i) {
                if (kidsObj.arrayGetNF (i, &kidRefObj)->isRef ()) {
                    if (kidRefObj.fetch (xref, &kidObj)->isDict ()) {
                        if (kidObj.dictLookup ("Count", &countObj)->isInt ()) {
                            count = countObj.getInt ();
                        }
                        else {
                            count = 1;
                        }
                        countObj.free ();
                        node->kids->append (new PageTreeNode (
                            kidRefObj.getRef (), count, node));
                    }
                    else {
                        error (
                            errSyntaxError, -1,
                            "Page tree object is wrong type ({0:s})",
                            kidObj.getTypeName ());
                    }
                    kidObj.free ();
                }
                else {
                    error (
                        errSyntaxError, -1,
                        "Page tree reference is wrong type ({0:s})",
                        kidRefObj.getTypeName ());
                }
                kidRefObj.free ();
            }
        }
        else {
            // create the Page object
            pageRefs[pg - 1] = node->ref;
            pages[pg - 1] = new Page (doc, pg, pageObj.getDict (), attrs);
            if (!pages[pg - 1]->isOk ()) {
                delete pages[pg - 1];
                pages[pg - 1] = new Page (doc, pg);
            }
        }

        kidsObj.free ();
        pageObj.free ();
        pageRefObj.free ();
    }

    // recursively descend the tree
    if (node->kids) {
        for (i = 0; i < node->kids->getLength (); ++i) {
            kidNode = (PageTreeNode*)node->kids->get (i);
            if (relPg < kidNode->count) {
                loadPage2 (pg, relPg, kidNode);
                break;
            }
            relPg -= kidNode->count;
        }

        // this will only happen if the page tree is invalid
        // (i.e., parent count > sum of children counts)
        if (i == node->kids->getLength ()) {
            error (errSyntaxError, -1, "Invalid page count in page tree");
            pages[pg - 1] = new Page (doc, pg);
        }
    }
}

void Catalog::readEmbeddedFileList (Dict* catDict) {
    Object obj1, obj2;
    char* touchedObjs;

    // read the embedded file name tree
    if (catDict->lookup ("Names", &obj1)->isDict ()) {
        if (obj1.dictLookup ("EmbeddedFiles", &obj2)->isDict ()) {
            readEmbeddedFileTree (&obj2);
        }
        obj2.free ();
    }
    obj1.free ();

    // look for file attachment annotations
    touchedObjs = (char*)malloc (xref->getNumObjects ());
    memset (touchedObjs, 0, xref->getNumObjects ());
    readFileAttachmentAnnots (catDict->lookupNF ("Pages", &obj1), touchedObjs);
    obj1.free ();
    free (touchedObjs);
}

void Catalog::readEmbeddedFileTree (Object* node) {
    Object kidsObj, kidObj;
    Object namesObj, nameObj, fileSpecObj;
    int i;

    if (node->dictLookup ("Kids", &kidsObj)->isArray ()) {
        for (i = 0; i < kidsObj.arrayGetLength (); ++i) {
            if (kidsObj.arrayGet (i, &kidObj)->isDict ()) {
                readEmbeddedFileTree (&kidObj);
            }
            kidObj.free ();
        }
    }
    else {
        if (node->dictLookup ("Names", &namesObj)->isArray ()) {
            for (i = 0; i + 1 < namesObj.arrayGetLength (); ++i) {
                namesObj.arrayGet (i, &nameObj);
                namesObj.arrayGet (i + 1, &fileSpecObj);
                readEmbeddedFile (&fileSpecObj, &nameObj);
                nameObj.free ();
                fileSpecObj.free ();
            }
        }
        namesObj.free ();
    }
    kidsObj.free ();
}

void Catalog::readFileAttachmentAnnots (
    Object* pageNodeRef, char* touchedObjs) {
    Object pageNode, kids, kid, annots, annot, subtype, fileSpec, contents;
    int i;

    // check for an invalid object reference (e.g., in a damaged PDF file)
    if (pageNodeRef->getRefNum () < 0 ||
        pageNodeRef->getRefNum () >= xref->getNumObjects ()) {
        return;
    }

    // check for a page tree loop
    if (pageNodeRef->isRef ()) {
        if (touchedObjs[pageNodeRef->getRefNum ()]) { return; }
        touchedObjs[pageNodeRef->getRefNum ()] = 1;
        xref->fetch (
            pageNodeRef->getRefNum (), pageNodeRef->getRefGen (), &pageNode);
    }
    else {
        pageNodeRef->copy (&pageNode);
    }

    if (pageNode.isDict ()) {
        if (pageNode.dictLookup ("Kids", &kids)->isArray ()) {
            for (i = 0; i < kids.arrayGetLength (); ++i) {
                readFileAttachmentAnnots (
                    kids.arrayGetNF (i, &kid), touchedObjs);
                kid.free ();
            }
        }
        else {
            if (pageNode.dictLookup ("Annots", &annots)->isArray ()) {
                for (i = 0; i < annots.arrayGetLength (); ++i) {
                    if (annots.arrayGet (i, &annot)->isDict ()) {
                        if (annot.dictLookup ("Subtype", &subtype)
                                ->isName ("FileAttachment")) {
                            if (annot.dictLookup ("FS", &fileSpec)) {
                                readEmbeddedFile (
                                    &fileSpec,
                                    annot.dictLookup ("Contents", &contents));
                                contents.free ();
                            }
                            fileSpec.free ();
                        }
                        subtype.free ();
                    }
                    annot.free ();
                }
            }
            annots.free ();
        }
        kids.free ();
    }

    pageNode.free ();
}

void Catalog::readEmbeddedFile (Object* fileSpec, Object* name1) {
    Object name2, efObj, streamObj;
    GString* s;
    TextString* name;

    if (fileSpec->isDict ()) {
        if (fileSpec->dictLookup ("UF", &name2)->isString ()) {
            name = new TextString (name2.getString ());
        }
        else {
            name2.free ();
            if (fileSpec->dictLookup ("F", &name2)->isString ()) {
                name = new TextString (name2.getString ());
            }
            else if (name1 && name1->isString ()) {
                name = new TextString (name1->getString ());
            }
            else {
                s = new GString ("?");
                name = new TextString (s);
                delete s;
            }
        }
        name2.free ();
        if (fileSpec->dictLookup ("EF", &efObj)->isDict ()) {
            if (efObj.dictLookupNF ("F", &streamObj)->isRef ()) {
                if (!embeddedFiles) { embeddedFiles = new GList (); }
                embeddedFiles->append (new EmbeddedFile (name, &streamObj));
            }
            else {
                delete name;
            }
            streamObj.free ();
        }
        else {
            delete name;
        }
        efObj.free ();
    }
}

int Catalog::getNumEmbeddedFiles () {
    return embeddedFiles ? embeddedFiles->getLength () : 0;
}

Unicode* Catalog::getEmbeddedFileName (int idx) {
    return ((EmbeddedFile*)embeddedFiles->get (idx))->name->getUnicode ();
}

int Catalog::getEmbeddedFileNameLength (int idx) {
    return ((EmbeddedFile*)embeddedFiles->get (idx))->name->getLength ();
}

Object* Catalog::getEmbeddedFileStreamRef (int idx) {
    return &((EmbeddedFile*)embeddedFiles->get (idx))->streamRef;
}

Object* Catalog::getEmbeddedFileStreamObj (int idx, Object* strObj) {
    ((EmbeddedFile*)embeddedFiles->get (idx))->streamRef.fetch (xref, strObj);
    if (!strObj->isStream ()) {
        strObj->free ();
        return NULL;
    }
    return strObj;
}

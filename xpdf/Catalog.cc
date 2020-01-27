// -*- mode: c++; -*-
// Copyright 1996-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <cstring>
#include <cstddef>
#include <climits>

#include <goo/memory.hh>
#include <goo/gfile.hh>
#include <goo/GList.hh>

#include <xpdf/obj.hh>
#include <xpdf/CharTypes.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/XRef.hh>
#include <xpdf/array.hh>
#include <xpdf/dict.hh>
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
    streamRef= *streamRefA;
}

EmbeddedFile::~EmbeddedFile () {
    delete name;
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
    if (!catDict.is_dict ()) {
        error (
            errSyntaxError, -1, "Catalog object is wrong type ({0:s})",
            catDict.getTypeName ());
        goto err1;
    }

    // read page tree
    if (!readPageTree (&catDict)) { goto err1; }

    // read named destination dictionary
    *&dests = resolve (catDict.as_dict ()["Dests"]);

    // read root of named destination tree
    if ((obj = resolve (catDict.as_dict ()["Names"])).is_dict ())
        *&nameTree = resolve (obj.as_dict ()["Dests"]);
    else
        nameTree = { };

    // read base URI
    if ((obj = resolve (catDict.as_dict ()["URI"])).is_dict ()) {
        if ((obj2 = resolve (obj.as_dict ()["Base"])).is_string ()) {
            baseURI = obj2.as_string ()->copy ();
        }
    }
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
    *&metadata = resolve (catDict.as_dict ()["Metadata"]);

    // get the structure tree root
    *&structTreeRoot = resolve (catDict.as_dict ()["StructTreeRoot"]);

    // get the outline dictionary
    *&outline = resolve (catDict.as_dict ()["Outlines"]);

    // get the AcroForm dictionary
    *&acroForm = resolve (catDict.as_dict ()["AcroForm"]);

    if (!acroForm.is_null ()) { form = Form::load (doc, this, &acroForm); }

    // get the OCProperties dictionary
    *&ocProperties = resolve (catDict.as_dict ()["OCProperties"]);

    // get the list of embedded files
    readEmbeddedFileList (catDict.as_dict_ptr ());

    return;

err1:
    dests = { };
    nameTree = { };
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
    if (baseURI) { delete baseURI; }
    if (form) { delete form; }
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

    if (!metadata.is_stream ()) { return NULL; }
    dict = metadata.streamGetDict ();
    if (!(obj = resolve ((*dict) ["Subtype"])).is_name ("XML")) {
        error (
            errSyntaxWarning, -1, "Unknown Metadata type: '{0:s}'",
            obj.is_name () ? obj.as_name () : "???");
    }
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
    if (dests.is_dict ()) {
        if (!(obj1 = resolve (dests.as_dict ()[name->c_str ()])).is_null ())
            found = true;
    }
    if (!found && nameTree.is_dict ()) {
        if (!findDestInTree (&nameTree, name, &obj1)->is_null ())
            found = true;
    }

    if (!found) return NULL;

    // construct LinkDest
    dest = NULL;
    if (obj1.is_array ()) { dest = new LinkDest (obj1.as_array ()); }
    else if (obj1.is_dict ()) {
        if ((obj2 = resolve (obj1.as_dict ()["D"])).is_array ())
            dest = new LinkDest (obj2.as_array ());
        else
            error (errSyntaxWarning, -1, "Bad named destination value");
    }
    else {
        error (errSyntaxWarning, -1, "Bad named destination value");
    }
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
    if ((names = resolve (tree->as_dict ()["Names"])).is_array ()) {
        done = found = false;
        for (i = 0; !done && i < names.as_array ().size (); i += 2) {
            if ((name1 = resolve (names [i])).is_string ()) {
                cmp = name->cmp (name1.as_string ());
                if (cmp == 0) {
                    *obj = resolve (names [i + 1]);
                    found = true;
                    done = true;
                }
                else if (cmp < 0) {
                    done = true;
                }
            }
        }
        if (!found) *obj = { };
        return obj;
    }

    // root or intermediate node
    done = false;
    if ((kids = resolve (tree->as_dict ()["Kids"])).is_array ()) {
        for (i = 0; !done && i < kids.as_array ().size (); ++i) {
            if ((kid = resolve (kids [i])).is_dict ()) {
                if ((limits = resolve (kid.as_dict ()["Limits"])).is_array ()) {
                    if ((low = resolve (limits [0UL])).is_string () &&
                        name->cmp (low.as_string ()) >= 0) {
                        if ((high = resolve (limits [1])).is_string () &&
                            name->cmp (high.as_string ()) <= 0) {
                            findDestInTree (&kid, name, obj);
                            done = true;
                        }
                    }
                }
            }
        }
    }

    // name was outside of ranges of all kids
    if (!done) *obj = { };

    return obj;
}

bool Catalog::readPageTree (Object* catDict) {
    Object topPagesRef, topPagesObj, countObj;
    int i;

    if (!(topPagesRef = (*catDict).as_dict ()["Pages"]).is_ref ()) {
        error (
            errSyntaxError, -1,
            "Top-level pages reference is wrong type ({0:s})",
            topPagesRef.getTypeName ());
        return false;
    }
    if (!(topPagesObj = resolve (topPagesRef)).is_dict ()) {
        error (
            errSyntaxError, -1, "Top-level pages object is wrong type ({0:s})",
            topPagesObj.getTypeName ());
        return false;
    }
    if ((countObj = resolve (topPagesObj.as_dict ()["Count"])).is_int ()) {
        numPages = countObj.as_int ();
        if (numPages == 0) {
            // Acrobat apparently scans the page tree if it sees a zero count
            numPages = countPageTree (&topPagesObj);
        }
    }
    else {
        // assume we got a Page node instead of a Pages node
        numPages = 1;
    }
    if (numPages < 0) {
        error (errSyntaxError, -1, "Invalid page count");
        numPages = 0;
        return false;
    }
    pageTree = new PageTreeNode (topPagesRef.as_ref (), numPages, NULL);
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

    if (!pagesObj->is_dict ()) { return 0; }
    if ((kids = resolve (pagesObj->as_dict ()["Kids"])).is_array ()) {
        n = 0;
        for (i = 0; i < kids.as_array ().size (); ++i) {
            kid = resolve (kids [i]);
            n2 = countPageTree (&kid);
            if (n2 < INT_MAX - n) { n += n2; }
            else {
                error (errSyntaxError, -1, "Page tree contains too many pages");
                n = INT_MAX;
            }
        }
    }
    else {
        n = 1;
    }
    return n;
}

void Catalog::loadPage (int pg) { loadPage2 (pg, pg - 1, pageTree); }

void Catalog::loadPage2 (int pg, int relPg, PageTreeNode* node) {
    Object pageObj, kidsObj, kidRefObj, kidObj, countObj;
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
        xref->fetch (node->ref.num, node->ref.gen, &pageObj);

        if (!pageObj.is_dict ()) {
            error (
                errSyntaxError, -1, "Page tree object is wrong type ({0:s})",
                pageObj.getTypeName ());
            pages[pg - 1] = new Page (doc, pg);
            return;
        }

        // merge the PageAttrs
        attrs = new PageAttrs (
            node->parent ? node->parent->attrs : (PageAttrs*)NULL,
            pageObj.as_dict_ptr ());

        // if "Kids" exists, it's an internal node
        if ((kidsObj = resolve (pageObj.as_dict ()["Kids"])).is_array ()) {
            // save the PageAttrs
            node->attrs = attrs;

            // read the kids
            node->kids = new GList ();
            for (i = 0; i < kidsObj.as_array ().size (); ++i) {
                kidRefObj = kidsObj [i];

                if (kidRefObj.is_ref ()) {
                    if ((kidObj = resolve (kidRefObj)).is_dict ()) {
                        if ((countObj = resolve (kidObj.as_dict ()["Count"])).is_int ()) {
                            count = countObj.as_int ();
                        }
                        else {
                            count = 1;
                        }
                        node->kids->append (new PageTreeNode (
                            kidRefObj.as_ref (), count, node));
                    }
                    else {
                        error (
                            errSyntaxError, -1,
                            "Page tree object is wrong type ({0:s})",
                            kidObj.getTypeName ());
                    }
                }
                else {
                    error (
                        errSyntaxError, -1,
                        "Page tree reference is wrong type ({0:s})",
                        kidRefObj.getTypeName ());
                }
            }
        }
        else {
            // create the Page object
            pageRefs[pg - 1] = node->ref;
            pages[pg - 1] = new Page (doc, pg, pageObj.as_dict_ptr (), attrs);
            if (!pages[pg - 1]->isOk ()) {
                delete pages[pg - 1];
                pages[pg - 1] = new Page (doc, pg);
            }
        }

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

    // read the embedded file name tree
    if ((obj1 = resolve ((*catDict) ["Names"])).is_dict ()) {
        if ((obj2 = resolve (obj1.as_dict ()["EmbeddedFiles"])).is_dict ()) {
            readEmbeddedFileTree (&obj2);
        }
    }

    // look for file attachment annotations
    auto touchedObjs = std::vector< char > (size_t (xref->getNumObjects ()), 0);
    readFileAttachmentAnnots ((*catDict) ["Pages"], touchedObjs.data ());
}

void Catalog::readEmbeddedFileTree (Object* node) {
    Object kidsObj, kidObj;
    Object namesObj, nameObj, fileSpecObj;
    int i;

    if ((kidsObj = resolve (node->as_dict ()["Kids"])).is_array ()) {
        for (i = 0; i < kidsObj.as_array ().size (); ++i) {
            if ((kidObj = resolve (kidsObj [i])).is_dict ()) {
                readEmbeddedFileTree (&kidObj);
            }
        }
    }
    else {
        if ((namesObj = resolve (node->as_dict ()["Names"])).is_array ()) {
            for (i = 0; i + 1 < namesObj.as_array ().size (); ++i) {
                nameObj = resolve (namesObj [i]);
                fileSpecObj = resolve (namesObj [i + 1]);
                readEmbeddedFile (fileSpecObj, nameObj);
            }
        }
    }
}

void Catalog::readFileAttachmentAnnots (
    const Object& pageNodeRef, char* touchedObjs) {
    Object pageNode, kids, kid, annots, annot, subtype, fileSpec, contents;
    int i;

    // check for an invalid object reference (e.g., in a damaged PDF file)
    if (pageNodeRef.getRefNum () < 0 ||
        pageNodeRef.getRefNum () >= xref->getNumObjects ()) {
        return;
    }

    // check for a page tree loop
    if (pageNodeRef.is_ref ()) {
        if (touchedObjs[pageNodeRef.getRefNum ()]) { return; }
        touchedObjs[pageNodeRef.getRefNum ()] = 1;
        pageNode = xref->fetch (pageNodeRef.as_ref ());
    }
    else {
        pageNode = pageNodeRef;
    }

    if (pageNode.is_dict ()) {
        if ((kids = resolve (pageNode.as_dict ()["Kids"])).is_array ()) {
            for (auto& kid : kids.as_array ()) {
                readFileAttachmentAnnots (kid, touchedObjs);
            }
        }
        else {
            if ((annots = resolve (pageNode.as_dict ()["Annots"])).is_array ()) {
                for (i = 0; i < annots.as_array ().size (); ++i) {
                    if ((annot = resolve (annots [i])).is_dict ()) {
                        auto subtype = resolve (annot.as_dict ()["Subtype"]);
                        if (subtype.is_name ("FileAttachment")) {
                            auto fileSpec = resolve (annot.as_dict () ["FS"]);

                            if (!fileSpec.is_null ()) {
                                readEmbeddedFile (
                                    fileSpec, resolve (annot.as_dict ()["Contents"]));
                            }
                        }
                    }
                }
            }
        }
    }

}

void Catalog::readEmbeddedFile (const Object& fileSpec, const Object& name1) {
    Object name2, efObj, streamObj;
    GString* s;
    TextString* name;

    if (fileSpec.is_dict ()) {
        if ((name2 = resolve (fileSpec.as_dict ()["UF"])).is_string ()) {
            name = new TextString (name2.as_string ());
        }
        else {
            if ((name2 = resolve (fileSpec.as_dict ()["F"])).is_string ()) {
                name = new TextString (name2.as_string ());
            }
            else if (name1.is_string ()) {
                name = new TextString (name1.as_string ());
            }
            else {
                s = new GString ("?");
                name = new TextString (s);
                delete s;
            }
        }
        if ((efObj = resolve (fileSpec.as_dict ()["EF"])).is_dict ()) {
            if ((streamObj = efObj.as_dict ()["F"]).is_ref ()) {
                if (!embeddedFiles) { embeddedFiles = new GList (); }
                embeddedFiles->append (new EmbeddedFile (name, &streamObj));
            }
            else {
                delete name;
            }
        }
        else {
            delete name;
        }
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
    auto& strref = ((EmbeddedFile*)embeddedFiles->get (idx))->streamRef;

    if (!(*strObj = resolve (strref)).is_stream ()) {
        *strObj = { };
        return 0;
    }

    return strObj;
}

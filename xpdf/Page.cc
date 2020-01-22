// -*- mode: c++; -*-
// Copyright 1996-2007 Glyph & Cog, LLC

#include <defs.hh>

#include <cstddef>
#include <xpdf/GlobalParams.hh>
#include <xpdf/obj.hh>
#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Link.hh>
#include <xpdf/OutputDev.hh>
#ifndef PDF_PARSER_ONLY
#include <xpdf/Gfx.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/Annot.hh>
#include <xpdf/Form.hh>
#endif
#include <xpdf/Error.hh>
#include <xpdf/Catalog.hh>
#include <xpdf/Page.hh>

//------------------------------------------------------------------------
// PDFRectangle
//------------------------------------------------------------------------

void PDFRectangle::clipTo (PDFRectangle* rect) {
    if (x1 < rect->x1) { x1 = rect->x1; }
    else if (x1 > rect->x2) {
        x1 = rect->x2;
    }
    if (x2 < rect->x1) { x2 = rect->x1; }
    else if (x2 > rect->x2) {
        x2 = rect->x2;
    }
    if (y1 < rect->y1) { y1 = rect->y1; }
    else if (y1 > rect->y2) {
        y1 = rect->y2;
    }
    if (y2 < rect->y1) { y2 = rect->y1; }
    else if (y2 > rect->y2) {
        y2 = rect->y2;
    }
}

//------------------------------------------------------------------------
// PageAttrs
//------------------------------------------------------------------------

PageAttrs::PageAttrs (PageAttrs* attrs, Dict* dict) {
    Object obj1;

    // get old/default values
    if (attrs) {
        mediaBox = attrs->mediaBox;
        cropBox = attrs->cropBox;
        haveCropBox = attrs->haveCropBox;
        rotate = attrs->rotate;
        resources = attrs->resources;
    }
    else {
        // set default MediaBox to 8.5" x 11" -- this shouldn't be necessary
        // but some (non-compliant) PDF files don't specify a MediaBox
        mediaBox.x1 = 0;
        mediaBox.y1 = 0;
        mediaBox.x2 = 612;
        mediaBox.y2 = 792;
        cropBox.x1 = cropBox.y1 = cropBox.x2 = cropBox.y2 = 0;
        haveCropBox = false;
        rotate = 0;
        resources = { };
    }

    // media box
    readBox (dict, "MediaBox", &mediaBox);

    // crop box
    if (readBox (dict, "CropBox", &cropBox)) { haveCropBox = true; }
    if (!haveCropBox) { cropBox = mediaBox; }

    // other boxes
    bleedBox = cropBox;
    readBox (dict, "BleedBox", &bleedBox);
    trimBox = cropBox;
    readBox (dict, "TrimBox", &trimBox);
    artBox = cropBox;
    readBox (dict, "ArtBox", &artBox);

    // rotate
    dict->lookup ("Rotate", &obj1);
    if (obj1.is_int ()) { rotate = obj1.as_int (); }
    while (rotate < 0) { rotate += 360; }
    while (rotate >= 360) { rotate -= 360; }

    // misc attributes
    dict->lookup ("LastModified", &lastModified);
    dict->lookup ("BoxColorInfo", &boxColorInfo);
    dict->lookup ("Group", &group);
    dict->lookup ("Metadata", &metadata);
    dict->lookup ("PieceInfo", &pieceInfo);
    dict->lookup ("SeparationInfo", &separationInfo);

    if (dict->lookup ("UserUnit", &obj1)->is_num ()) {
        userUnit = obj1.as_num ();
        if (userUnit < 1) { userUnit = 1; }
    }
    else {
        userUnit = 1;
    }

    // resource dictionary
    dict->lookup ("Resources", &obj1);

    if (obj1.is_dict ()) {
        resources = obj1;
    }
}

PageAttrs::PageAttrs () {
    mediaBox.x1 = mediaBox.y1 = 0;
    mediaBox.x2 = mediaBox.y2 = 50;
    cropBox = mediaBox;
    haveCropBox = false;
    bleedBox = cropBox;
    trimBox = cropBox;
    artBox = cropBox;
    rotate = 0;
    lastModified = { };
    boxColorInfo = { };
    group = { };
    metadata = { };
    pieceInfo = { };
    separationInfo = { };
    resources = { };
}

PageAttrs::~PageAttrs () {
}

void PageAttrs::clipBoxes () {
    cropBox.clipTo (&mediaBox);
    bleedBox.clipTo (&mediaBox);
    trimBox.clipTo (&mediaBox);
    artBox.clipTo (&mediaBox);
}

bool PageAttrs::readBox (Dict* dict, const char* key, PDFRectangle* box) {
    PDFRectangle tmp;
    double t;
    Object obj1, obj2;
    bool ok;

    dict->lookup (key, &obj1);
    if (obj1.is_array () && obj1.arrayGetLength () == 4) {
        ok = true;
        obj1.arrayGet (0, &obj2);
        if (obj2.is_num ()) { tmp.x1 = obj2.as_num (); }
        else {
            ok = false;
        }
        obj1.arrayGet (1, &obj2);
        if (obj2.is_num ()) { tmp.y1 = obj2.as_num (); }
        else {
            ok = false;
        }
        obj1.arrayGet (2, &obj2);
        if (obj2.is_num ()) { tmp.x2 = obj2.as_num (); }
        else {
            ok = false;
        }
        obj1.arrayGet (3, &obj2);
        if (obj2.is_num ()) { tmp.y2 = obj2.as_num (); }
        else {
            ok = false;
        }
        if (ok) {
            if (tmp.x1 > tmp.x2) {
                t = tmp.x1;
                tmp.x1 = tmp.x2;
                tmp.x2 = t;
            }
            if (tmp.y1 > tmp.y2) {
                t = tmp.y1;
                tmp.y1 = tmp.y2;
                tmp.y2 = t;
            }
            *box = tmp;
        }
    }
    else {
        ok = false;
    }
    return ok;
}

//------------------------------------------------------------------------
// Page
//------------------------------------------------------------------------

Page::Page (PDFDoc* docA, int numA, Dict* pageDict, PageAttrs* attrsA) {
    ok = true;
    doc = docA;
    xref = doc->getXRef ();
    num = numA;

    // get attributes
    attrs = attrsA;
    attrs->clipBoxes ();

    // annotations
    pageDict->lookupNF ("Annots", &annots);
    if (!(annots.is_ref () || annots.is_array () || annots.is_null ())) {
        error (
            errSyntaxError, -1,
            "Page annotations object (page {0:d}) is wrong type ({1:s})", num,
            annots.getTypeName ());
        goto err2;
    }

    // contents
    pageDict->lookupNF ("Contents", &contents);
    if (!(contents.is_ref () || contents.is_array () || contents.is_null ())) {
        error (
            errSyntaxError, -1,
            "Page contents object (page {0:d}) is wrong type ({1:s})", num,
            contents.getTypeName ());
        goto err1;
    }

    return;

err2:
    annots = { };
err1:
    contents = { };
    ok = false;
}

Page::Page (PDFDoc* docA, int numA) {
    doc = docA;
    xref = doc->getXRef ();
    num = numA;
    attrs = new PageAttrs ();
    annots = { };
    contents = { };
    ok = true;
}

Page::~Page () {
    delete attrs;
}

Links* Page::getLinks () {
    Links* links;
    Object obj;

    links = new Links (getAnnots (&obj), doc->getCatalog ()->getBaseURI ());
    return links;
}

void Page::display (
    OutputDev* out, double hDPI, double vDPI, int rotate, bool useMediaBox,
    bool crop, bool printing, bool (*abortCheckCbk) (void* data),
    void* abortCheckCbkData) {
    displaySlice (
        out, hDPI, vDPI, rotate, useMediaBox, crop, -1, -1, -1, -1, printing,
        abortCheckCbk, abortCheckCbkData);
}

void Page::displaySlice (
    OutputDev* out, double hDPI, double vDPI, int rotate, bool useMediaBox,
    bool crop, int sliceX, int sliceY, int sliceW, int sliceH, bool printing,
    bool (*abortCheckCbk) (void* data), void* abortCheckCbkData) {
#ifndef PDF_PARSER_ONLY
    PDFRectangle *mediaBox, *cropBox;
    PDFRectangle box;
    Gfx* gfx;
    Object obj;
    Annots* annotList;
    Form* form;
    int i;

    if (!out->checkPageSlice (
            this, hDPI, vDPI, rotate, useMediaBox, crop, sliceX, sliceY, sliceW,
            sliceH, printing, abortCheckCbk, abortCheckCbkData)) {
        return;
    }

    rotate += getRotate ();
    if (rotate >= 360) { rotate -= 360; }
    else if (rotate < 0) {
        rotate += 360;
    }

    makeBox (
        hDPI, vDPI, rotate, useMediaBox, out->upsideDown (), sliceX, sliceY,
        sliceW, sliceH, &box, &crop);
    cropBox = getCropBox ();

    if (globalParams->getPrintCommands ()) {
        mediaBox = getMediaBox ();
        printf (
            "***** MediaBox = ll:%g,%g ur:%g,%g\n", mediaBox->x1, mediaBox->y1,
            mediaBox->x2, mediaBox->y2);
        printf (
            "***** CropBox = ll:%g,%g ur:%g,%g\n", cropBox->x1, cropBox->y1,
            cropBox->x2, cropBox->y2);
        printf ("***** Rotate = %d\n", attrs->getRotate ());
    }

    gfx = new Gfx (
        doc, out, num, attrs->getResourceDict (), hDPI, vDPI, &box,
        crop ? cropBox : (PDFRectangle*)NULL, rotate, abortCheckCbk,
        abortCheckCbkData);
    contents.fetch (xref, &obj);
    if (!obj.is_null ()) {
        gfx->saveState ();
        gfx->display (&contents);
        while (gfx->getState ()->hasSaves ()) { gfx->restoreState (); }
    }
    else {
        // empty pages need to call dump to do any setup required by the
        // OutputDev
        out->dump ();
    }

    // draw (non-form) annotations
    if (globalParams->getDrawAnnotations ()) {
        annotList = new Annots (doc, getAnnots (&obj));
        annotList->generateAnnotAppearances ();
        if (annotList->getNumAnnots () > 0) {
            if (globalParams->getPrintCommands ()) {
                printf ("***** Annotations\n");
            }
            for (i = 0; i < annotList->getNumAnnots (); ++i) {
                annotList->getAnnot (i)->draw (gfx, printing);
            }
            out->dump ();
        }
        delete annotList;
    }

    // draw form fields
    if ((form = doc->getCatalog ()->getForm ())) {
        form->draw (num, gfx, printing);
        out->dump ();
    }

    delete gfx;
#endif
}

void Page::makeBox (
    double hDPI, double vDPI, int rotate, bool useMediaBox, bool upsideDown,
    double sliceX, double sliceY, double sliceW, double sliceH,
    PDFRectangle* box, bool* crop) {
    PDFRectangle *mediaBox, *cropBox, *baseBox;
    double kx, ky;

    mediaBox = getMediaBox ();
    cropBox = getCropBox ();
    if (sliceW >= 0 && sliceH >= 0) {
        baseBox = useMediaBox ? mediaBox : cropBox;
        kx = 72.0 / hDPI;
        ky = 72.0 / vDPI;
        if (rotate == 90) {
            if (upsideDown) {
                box->x1 = baseBox->x1 + ky * sliceY;
                box->x2 = baseBox->x1 + ky * (sliceY + sliceH);
            }
            else {
                box->x1 = baseBox->x2 - ky * (sliceY + sliceH);
                box->x2 = baseBox->x2 - ky * sliceY;
            }
            box->y1 = baseBox->y1 + kx * sliceX;
            box->y2 = baseBox->y1 + kx * (sliceX + sliceW);
        }
        else if (rotate == 180) {
            box->x1 = baseBox->x2 - kx * (sliceX + sliceW);
            box->x2 = baseBox->x2 - kx * sliceX;
            if (upsideDown) {
                box->y1 = baseBox->y1 + ky * sliceY;
                box->y2 = baseBox->y1 + ky * (sliceY + sliceH);
            }
            else {
                box->y1 = baseBox->y2 - ky * (sliceY + sliceH);
                box->y2 = baseBox->y2 - ky * sliceY;
            }
        }
        else if (rotate == 270) {
            if (upsideDown) {
                box->x1 = baseBox->x2 - ky * (sliceY + sliceH);
                box->x2 = baseBox->x2 - ky * sliceY;
            }
            else {
                box->x1 = baseBox->x1 + ky * sliceY;
                box->x2 = baseBox->x1 + ky * (sliceY + sliceH);
            }
            box->y1 = baseBox->y2 - kx * (sliceX + sliceW);
            box->y2 = baseBox->y2 - kx * sliceX;
        }
        else {
            box->x1 = baseBox->x1 + kx * sliceX;
            box->x2 = baseBox->x1 + kx * (sliceX + sliceW);
            if (upsideDown) {
                box->y1 = baseBox->y2 - ky * (sliceY + sliceH);
                box->y2 = baseBox->y2 - ky * sliceY;
            }
            else {
                box->y1 = baseBox->y1 + ky * sliceY;
                box->y2 = baseBox->y1 + ky * (sliceY + sliceH);
            }
        }
    }
    else if (useMediaBox) {
        *box = *mediaBox;
    }
    else {
        *box = *cropBox;
        *crop = false;
    }
}

void Page::processLinks (OutputDev* out) {
    Links* links;
    int i;

    links = getLinks ();
    for (i = 0; i < links->getNumLinks (); ++i) {
        out->processLink (links->getLink (i));
    }
    delete links;
}

#ifndef PDF_PARSER_ONLY
void Page::getDefaultCTM (
    double* ctm, double hDPI, double vDPI, int rotate, bool useMediaBox,
    bool upsideDown) {
    GfxState* state;
    int i;

    rotate += getRotate ();
    if (rotate >= 360) { rotate -= 360; }
    else if (rotate < 0) {
        rotate += 360;
    }
    state = new GfxState (
        hDPI, vDPI, useMediaBox ? getMediaBox () : getCropBox (), rotate,
        upsideDown);
    for (i = 0; i < 6; ++i) { ctm[i] = state->getCTM ()[i]; }
    delete state;
}
#endif

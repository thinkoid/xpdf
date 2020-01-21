// -*- mode: c++; -*-
// Copyright 2012 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdlib>
#include <cmath>

#include <goo/GString.hh>
#include <goo/GList.hh>

#include <xpdf/AcroForm.hh>
#include <xpdf/Annot.hh>
#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/Gfx.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/lexer.hh>
#include <xpdf/ast.hh>
#include <xpdf/OptionalContent.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/TextString.hh>

#include <fmt/format.h>

//------------------------------------------------------------------------

#define acroFormFlagReadOnly (1 << 0)           // all
#define acroFormFlagRequired (1 << 1)           // all
#define acroFormFlagNoExport (1 << 2)           // all
#define acroFormFlagMultiline (1 << 12)         // text
#define acroFormFlagPassword (1 << 13)          // text
#define acroFormFlagNoToggleToOff (1 << 14)     // button
#define acroFormFlagRadio (1 << 15)             // button
#define acroFormFlagPushbutton (1 << 16)        // button
#define acroFormFlagCombo (1 << 17)             // choice
#define acroFormFlagEdit (1 << 18)              // choice
#define acroFormFlagSort (1 << 19)              // choice
#define acroFormFlagFileSelect (1 << 20)        // text
#define acroFormFlagMultiSelect (1 << 21)       // choice
#define acroFormFlagDoNotSpellCheck (1 << 22)   // text, choice
#define acroFormFlagDoNotScroll (1 << 23)       // text
#define acroFormFlagComb (1 << 24)              // text
#define acroFormFlagRadiosInUnison (1 << 25)    // button
#define acroFormFlagRichText (1 << 25)          // text
#define acroFormFlagCommitOnSelChange (1 << 26) // choice

#define acroFormQuadLeft 0
#define acroFormQuadCenter 1
#define acroFormQuadRight 2

#define annotFlagHidden 0x0002
#define annotFlagPrint 0x0004
#define annotFlagNoView 0x0020

// distance of Bezier control point from center for circle approximation
// = (4 * (sqrt(2) - 1) / 3) * r
#define bezierCircle 0.55228475

//------------------------------------------------------------------------

// map an annotation ref to a page number
struct AcroFormAnnotPage {
    AcroFormAnnotPage (int a, int b, int c)
        : annotNum (a), annotGen (b), pageNum (c)
        { }

    int annotNum;
    int annotGen;
    int pageNum;
};

//------------------------------------------------------------------------
// AcroForm
//------------------------------------------------------------------------

AcroForm*
AcroForm::load (PDFDoc* docA, Catalog* catalog, Object* acroFormObjA) {
    AcroForm* acroForm;
    int i;

    acroForm = new AcroForm (docA, acroFormObjA);

    Object obj;
    acroFormObjA->dictLookup ("NeedAppearances", &obj);

    if (obj.is_bool ()) {
        acroForm->needAppearances = obj.as_bool ();
    }

    acroForm->buildAnnotPageList (catalog);
    acroFormObjA->dictLookup ("Fields", &obj);

    if (!obj.is_array ()) {
        if (!obj.is_null ()) {
            error (errSyntaxError, -1, "AcroForm Fields entry is wrong type");
        }

        delete acroForm;
        return 0;
    }

    for (i = 0; i < obj.arrayGetLength (); ++i) {
        Object tmp;
        obj.arrayGetNF (i, &tmp);
        acroForm->scanField (&tmp);
    }

    return acroForm;
}

AcroForm::AcroForm (PDFDoc* docA, Object* acroFormObjA) : Form (docA) {
    acroFormObj = *acroFormObjA;
    needAppearances = false;
}

AcroForm::~AcroForm () { }

void AcroForm::buildAnnotPageList (Catalog* catalog) {
    int pageNum, i;

    for (pageNum = 1; pageNum <= catalog->getNumPages (); ++pageNum) {
        Object annots;
        catalog->getPage (pageNum)->getAnnots (&annots);

        if (annots.is_array ()) {
            for (i = 0; i < annots.arrayGetLength (); ++i) {
                Object annot;
                annots.arrayGetNF (i, &annot);

                if (annot.is_ref ()) {
                    annotPages.push_back (
                        std::make_unique< AcroFormAnnotPage > (
                            annot.getRefNum (),
                            annot.getRefGen (),
                            pageNum));
                }
            }
        }
    }
}

int AcroForm::lookupAnnotPage (Object* annotRef) {
    if (!annotRef->is_ref ()) {
        return 0;
    }

    int num = annotRef->getRefNum ();
    int gen = annotRef->getRefGen ();

    auto iter = find_if (annotPages, [=](auto& p) {
        return p->annotNum == num && p->annotGen == gen;
    });

    return (iter == annotPages.end ()) ? 0 : (*iter)->pageNum;
}

void AcroForm::scanField (Object* fieldRef) {
    bool isTerminal;
    int i;

    Object fieldObj;
    fieldRef->fetch (doc->getXRef (), &fieldObj);

    if (!fieldObj.is_dict ()) {
        error (errSyntaxError, -1, "AcroForm field object is wrong type");
        return;
    }

    // if this field has a Kids array, and all of the kids have a Parent
    // reference (i.e., they're all form fields, not widget
    // annotations), then this is a non-terminal field, and we need to
    // scan the kids
    isTerminal = true;

    Object kidsObj;
    fieldObj.dictLookup ("Kids", &kidsObj);

    if (kidsObj.is_array ()) {
        isTerminal = false;

        for (i = 0; !isTerminal && i < kidsObj.arrayGetLength (); ++i) {
            Object kidObj;
            kidsObj.arrayGet (i, &kidObj);

            if (kidObj.is_dict ()) {
                Object subtypeObj;
                kidObj.dictLookup ("Parent", &subtypeObj);

                if (subtypeObj.is_null ()) {
                    isTerminal = true;
                }
            }
        }

        if (!isTerminal) {
            for (i = 0; !isTerminal && i < kidsObj.arrayGetLength (); ++i) {
                Object kidRef;
                kidsObj.arrayGetNF (i, &kidRef);
                scanField (&kidRef);
            }
        }
    }

    if (isTerminal) {
        if (auto p = AcroFormField::load (this, fieldRef)) {
            fields.push_back (std::unique_ptr< AcroFormField > (p));
        }
    }
}

void AcroForm::draw (int pageNum, Gfx* gfx, bool printing) {
    for (auto& p : fields) {
        p->draw (pageNum, gfx, printing);
    }
}

//------------------------------------------------------------------------
// AcroFormField
//------------------------------------------------------------------------

AcroFormField* AcroFormField::load (AcroForm* acroFormA, Object* fieldRefA) {
    GString* typeStr;
    TextString* nameA;
    unsigned flagsA;
    bool haveFlags;
    AcroFormFieldType typeA;
    AcroFormField* field;

    Object fieldObjA;
    fieldRefA->fetch (acroFormA->doc->getXRef (), &fieldObjA);

    //----- get field info

    Object obj;
    fieldObjA.dictLookup ("T", &obj);

    if (obj.is_string ()) {
        nameA = new TextString (obj.as_string ());
    }
    else {
        nameA = new TextString ();
    }

    fieldObjA.dictLookup ("FT", &obj);

    if (obj.is_name ()) {
        typeStr = new GString (obj.as_name ());
    }
    else {
        typeStr = NULL;
    }

    fieldObjA.dictLookup ("Ff", &obj);

    if (obj.is_int ()) {
        flagsA = (unsigned)obj.as_int ();
        haveFlags = true;
    }
    else {
        flagsA = 0;
        haveFlags = false;
    }

    //----- get info from parent non-terminal fields

    Object parentObj;
    fieldObjA.dictLookup ("Parent", &parentObj);

    while (parentObj.is_dict ()) {
        Object obj;
        parentObj.dictLookup ("T", &obj);

        if (obj.is_string ()) {
            if (nameA->getLength ()) {
                nameA->insert (0, (Unicode)'.');
            }

            nameA->insert (0, obj.as_string ());
        }

        if (!typeStr) {
            parentObj.dictLookup ("FT", &obj);

            if (obj.is_name ()) {
                typeStr = new GString (obj.as_name ());
            }
        }

        if (!haveFlags) {
            parentObj.dictLookup ("Ff", &obj);

            if (obj.is_int ()) {
                flagsA = (unsigned)obj.as_int ();
                haveFlags = true;
            }
        }

        Object tmp;
        parentObj.dictLookup ("Parent", &tmp);

        parentObj = tmp;
    }

    if (!typeStr) {
        error (errSyntaxError, -1, "Missing type in AcroForm field");
        goto err1;
    }
    else if (!typeStr->cmp ("Btn")) {
        if (flagsA & acroFormFlagPushbutton) {
            typeA = acroFormFieldPushbutton;
        }
        else if (flagsA & acroFormFlagRadio) {
            typeA = acroFormFieldRadioButton;
        }
        else {
            typeA = acroFormFieldCheckbox;
        }
    }
    else if (!typeStr->cmp ("Tx")) {
        if (flagsA & acroFormFlagFileSelect) {
            typeA = acroFormFieldFileSelect;
        }
        else if (flagsA & acroFormFlagMultiline) {
            typeA = acroFormFieldMultilineText;
        }
        else {
            typeA = acroFormFieldText;
        }
    }
    else if (!typeStr->cmp ("Ch")) {
        if (flagsA & acroFormFlagCombo) {
            typeA = acroFormFieldComboBox;
        }
        else {
            typeA = acroFormFieldListBox;
        }
    }
    else if (!typeStr->cmp ("Sig")) {
        typeA = acroFormFieldSignature;
    }
    else {
        error (errSyntaxError, -1, "Invalid type in AcroForm field");
        goto err1;
    }

    delete typeStr;

    field = new AcroFormField (
        acroFormA, fieldRefA, &fieldObjA, typeA, nameA, flagsA);

    return field;

err1:
    delete typeStr;
    delete nameA;

    return 0;
}

AcroFormField::AcroFormField (
    AcroForm* acroFormA, Object* fieldRefA, Object* fieldObjA,
    AcroFormFieldType typeA, TextString* nameA, unsigned flagsA) {
    acroForm = acroFormA;
    fieldRef = *fieldRefA;
    fieldObj = *fieldObjA;
    type = typeA;
    name = nameA;
    flags = flagsA;
}

AcroFormField::~AcroFormField () {
    delete name;
}

const char* AcroFormField::getType () {
    switch (type) {
    case acroFormFieldPushbutton: return "PushButton";
    case acroFormFieldRadioButton: return "RadioButton";
    case acroFormFieldCheckbox: return "Checkbox";
    case acroFormFieldFileSelect: return "FileSelect";
    case acroFormFieldMultilineText: return "MultilineText";
    case acroFormFieldText: return "Text";
    case acroFormFieldComboBox: return "ComboBox";
    case acroFormFieldListBox: return "ListBox";
    case acroFormFieldSignature: return "Signature";
    default: return NULL;
    }
}

Unicode* AcroFormField::as_name (int* length) {
    Unicode *u, *ret;
    int n;

    u = name->getUnicode ();
    n = name->getLength ();
    ret = (Unicode*)calloc (n, sizeof (Unicode));
    memcpy (ret, u, n * sizeof (Unicode));
    *length = n;
    return ret;
}

Unicode* AcroFormField::getValue (int* length) {
    Object obj1;
    Unicode* u;
    const char* s;
    TextString* ts;
    int n, i;

    fieldLookup ("V", &obj1);
    if (obj1.is_name ()) {
        s = obj1.as_name ();
        n = (int)strlen (s);
        u = (Unicode*)calloc (n, sizeof (Unicode));
        for (i = 0; i < n; ++i) { u[i] = s[i] & 0xff; }
        *length = n;
        return u;
    }
    else if (obj1.is_string ()) {
        ts = new TextString (obj1.as_string ());
        n = ts->getLength ();
        u = (Unicode*)calloc (n, sizeof (Unicode));
        memcpy (u, ts->getUnicode (), n * sizeof (Unicode));
        *length = n;
        delete ts;
        return u;
    }
    else {
        return NULL;
    }
}

void AcroFormField::draw (int pageNum, Gfx* gfx, bool printing) {
    // find the annotation object(s)
    Object kidsObj;
    fieldObj.dictLookup ("Kids", &kidsObj);
    if (kidsObj.is_array ()) {
        for (int i = 0; i < kidsObj.arrayGetLength (); ++i) {
            Object annotRef, annotObj;
            kidsObj.arrayGetNF (i, &annotRef);
            annotRef.fetch (acroForm->doc->getXRef (), &annotObj);
            drawAnnot (pageNum, gfx, printing, &annotRef, &annotObj);
        }
    }
    else {
        drawAnnot (pageNum, gfx, printing, &fieldRef, &fieldObj);
    }
}

void AcroFormField::drawAnnot (
    int pageNum, Gfx* gfx, bool printing, Object* annotRef, Object* annotObj) {
    double xMin, yMin, xMax, yMax, t;
    int annotFlags;
    bool oc;

    if (!annotObj->is_dict ()) { return; }

    //----- get the page number

    // the "P" (page) field in annotations is optional, so we can't
    // depend on it here
    if (acroForm->lookupAnnotPage (annotRef) != pageNum) { return; }

    //----- check annotation flags
    Object obj;
    annotObj->dictLookup ("F", &obj);

    if (obj.is_int ()) {
        annotFlags = obj.as_int ();
    }
    else {
        annotFlags = 0;
    }

    if ((annotFlags & annotFlagHidden) ||
        (printing && !(annotFlags & annotFlagPrint)) ||
        (!printing && (annotFlags & annotFlagNoView))) {
        return;
    }

    //----- check the optional content entry

    annotObj->dictLookupNF ("OC", &obj);

    if (acroForm->doc->getOptionalContent ()->evalOCObject (&obj, &oc) && !oc) {
        return;
    }

    //----- get the bounding box
    annotObj->dictLookup ("Rect", &obj);

    if (obj.is_array () && obj.arrayGetLength () == 4) {
        xMin = yMin = xMax = yMax = 0;

        Object number;

        if (obj.arrayGet (0, &number)->is_num ()) { xMin = number.as_num (); }
        if (obj.arrayGet (1, &number)->is_num ()) { yMin = number.as_num (); }
        if (obj.arrayGet (2, &number)->is_num ()) { xMax = number.as_num (); }
        if (obj.arrayGet (3, &number)->is_num ()) { yMax = number.as_num (); }

        if (xMin > xMax) {
            t = xMin;
            xMin = xMax;
            xMax = t;
        }

        if (yMin > yMax) {
            t = yMin;
            yMin = yMax;
            yMax = t;
        }
    }
    else {
        error (errSyntaxError, -1, "Bad bounding box for annotation");
        return;
    }

    //----- draw it

    if (acroForm->needAppearances) {
        drawNewAppearance (gfx, annotObj->as_dict (), xMin, yMin, xMax, yMax);
    }
    else {
        drawExistingAppearance (
            gfx, annotObj->as_dict (), xMin, yMin, xMax, yMax);
    }
}

// Draw the existing appearance stream for a single annotation
// attached to this field.
void AcroFormField::drawExistingAppearance (
    Gfx* gfx, Dict* annot, double xMin, double yMin, double xMax, double yMax) {

    //----- get the appearance stream
    Object appearance;

    Object apObj;
    annot->lookup ("AP", &apObj);

    if (apObj.is_dict ()) {
        Object obj1;
        apObj.dictLookup ("N", &obj1);

        if (obj1.is_dict ()) {
            Object asObj;
            annot->lookup ("AS", &asObj);

            if (asObj.is_name ()) {
                obj1.dictLookupNF (asObj.as_name (), &appearance);
            }
            else if (obj1.dictGetLength () == 1) {
                obj1.dictGetValNF (0, &appearance);
            }
            else {
                obj1.dictLookupNF ("Off", &appearance);
            }
        }
        else {
            apObj.dictLookupNF ("N", &appearance);
        }
    }

    //----- draw it

    if (!appearance.is_none ()) {
        gfx->drawAnnot (&appearance, NULL, xMin, yMin, xMax, yMax);
    }
}

// Regenerate the appearnce for this field, and draw it.
void AcroFormField::drawNewAppearance (
    Gfx* gfx, Dict* annot, double xMin, double yMin, double xMax, double yMax) {
    Object appearance, mkObj, ftObj, appearDict, drObj, apObj, asObj;
    Object obj1, obj2, obj3;
    Dict* mkDict;
    MemStream* appearStream;
    GfxFontDict* fontDict;
    bool hasCaption;
    double dx, dy, r;
    GString *caption, *da;
    GString** text;
    bool* selection;
    AnnotBorderType borderType;
    double borderWidth;
    double* borderDash;
    GString* appearanceState;
    int borderDashLength, rot, quadding, comb, nOptions, topIdx, i, j;

    // get the appearance characteristics (MK) dictionary
    if (annot->lookup ("MK", &mkObj)->is_dict ()) { mkDict = mkObj.as_dict (); }
    else {
        mkDict = NULL;
    }

    // draw the background
    if (mkDict) {
        if (mkDict->lookup ("BG", &obj1)->is_array () && obj1.arrayGetLength () > 0) {
            setColor (obj1.as_array (), true, 0);
            appearBuf += format (
                "0 0 {0:.4f} {1:.4f} re f\n", xMax - xMin, yMax - yMin);
        }
    }

    // get the field type
    fieldLookup ("FT", &ftObj);

    // draw the border
    borderType = annotBorderSolid;
    borderWidth = 1;
    borderDash = NULL;
    borderDashLength = 0;
    if (annot->lookup ("BS", &obj1)->is_dict ()) {
        if (obj1.dictLookup ("S", &obj2)->is_name ()) {
            if (obj2.is_name ("S")) { borderType = annotBorderSolid; }
            else if (obj2.is_name ("D")) {
                borderType = annotBorderDashed;
            }
            else if (obj2.is_name ("B")) {
                borderType = annotBorderBeveled;
            }
            else if (obj2.is_name ("I")) {
                borderType = annotBorderInset;
            }
            else if (obj2.is_name ("U")) {
                borderType = annotBorderUnderlined;
            }
        }
        if (obj1.dictLookup ("W", &obj2)->is_num ()) {
            borderWidth = obj2.as_num ();
        }
        if (obj1.dictLookup ("D", &obj2)->is_array ()) {
            borderDashLength = obj2.arrayGetLength ();
            borderDash = (double*)calloc (borderDashLength, sizeof (double));
            for (i = 0; i < borderDashLength; ++i) {
                if (obj2.arrayGet (i, &obj3)->is_num ()) {
                    borderDash[i] = obj3.as_num ();
                }
                else {
                    borderDash[i] = 1;
                }
            }
        }
    }
    else {
        if (annot->lookup ("Border", &obj1)->is_array ()) {
            if (obj1.arrayGetLength () >= 3) {
                if (obj1.arrayGet (2, &obj2)->is_num ()) {
                    borderWidth = obj2.as_num ();
                }

                if (obj1.arrayGetLength () >= 4) {
                    if (obj1.arrayGet (3, &obj2)->is_array ()) {
                        borderType = annotBorderDashed;
                        borderDashLength = obj2.arrayGetLength ();
                        borderDash = (double*)calloc (borderDashLength, sizeof (double));

                        for (i = 0; i < borderDashLength; ++i) {
                            if (obj2.arrayGet (i, &obj3)->is_num ()) {
                                borderDash[i] = obj3.as_num ();
                            }
                            else {
                                borderDash[i] = 1;
                            }
                        }
                    }
                    else {
                        // Adobe draws no border at all if the last element is of
                        // the wrong type.
                        borderWidth = 0;
                    }
                }
            }
        }
    }

    if (mkDict) {
        if (borderWidth > 0) {
            mkDict->lookup ("BC", &obj1);
            if (!(obj1.is_array () && obj1.arrayGetLength () > 0)) {
                mkDict->lookup ("BG", &obj1);
            }
            if (obj1.is_array () && obj1.arrayGetLength () > 0) {
                dx = xMax - xMin;
                dy = yMax - yMin;

                // radio buttons with no caption have a round border
                hasCaption = mkDict->lookup ("CA", &obj2)->is_string ();

                if (ftObj.is_name ("Btn") && (flags & acroFormFlagRadio) &&
                    !hasCaption) {
                    r = 0.5 * (dx < dy ? dx : dy);
                    switch (borderType) {
                    case annotBorderDashed:
                        appearBuf.append (1UL, '[');
                        for (i = 0; i < borderDashLength; ++i) {
                            appearBuf += format (" {0:.4f}", borderDash[i]);
                        }
                        appearBuf += "] 0 d\n";
                        // fall through to the solid case
                    case annotBorderSolid:
                    case annotBorderUnderlined:
                        appearBuf += format ("{0:.4f} w\n", borderWidth);
                        setColor (obj1.as_array (), false, 0);
                        drawCircle (
                            0.5 * dx, 0.5 * dy, r - 0.5 * borderWidth, "s");
                        break;
                    case annotBorderBeveled:
                    case annotBorderInset:
                        appearBuf += format ("{0:.4f} w\n", 0.5 * borderWidth);
                        setColor (obj1.as_array (), false, 0);
                        drawCircle (
                            0.5 * dx, 0.5 * dy, r - 0.25 * borderWidth, "s");
                        setColor (
                            obj1.as_array (), false,
                            borderType == annotBorderBeveled ? 1 : -1);
                        drawCircleTopLeft (
                            0.5 * dx, 0.5 * dy, r - 0.75 * borderWidth);
                        setColor (
                            obj1.as_array (), false,
                            borderType == annotBorderBeveled ? -1 : 1);
                        drawCircleBottomRight (
                            0.5 * dx, 0.5 * dy, r - 0.75 * borderWidth);
                        break;
                    }
                }
                else {
                    switch (borderType) {
                    case annotBorderDashed:
                        appearBuf.append (1UL, '[');
                        for (i = 0; i < borderDashLength; ++i) {
                            appearBuf += format (" {0:.4f}", borderDash[i]);
                        }
                        appearBuf += "] 0 d\n";
                        // fall through to the solid case
                    case annotBorderSolid:
                        appearBuf += format ("{0:.4f} w\n", borderWidth);
                        setColor (obj1.as_array (), false, 0);
                        appearBuf += format (
                            "{0:.4f} {0:.4f} {1:.4f} {2:.4f} re s\n",
                            0.5 * borderWidth, dx - borderWidth,
                            dy - borderWidth);
                        break;
                    case annotBorderBeveled:
                    case annotBorderInset:
                        setColor (
                            obj1.as_array (), true,
                            borderType == annotBorderBeveled ? 1 : -1);
                        appearBuf += "0 0 m\n";
                        appearBuf += format ("0 {0:.4f} l\n", dy);
                        appearBuf += format ("{0:.4f} {1:.4f} l\n", dx, dy);
                        appearBuf += format (
                            "{0:.4f} {1:.4f} l\n", dx - borderWidth,
                            dy - borderWidth);
                        appearBuf += format (
                            "{0:.4f} {1:.4f} l\n", borderWidth,
                            dy - borderWidth);
                        appearBuf += format ("{0:.4f} {0:.4f} l\n", borderWidth);
                        appearBuf += "f\n";
                        setColor (
                            obj1.as_array (), true,
                            borderType == annotBorderBeveled ? -1 : 1);
                        appearBuf += "0 0 m\n";
                        appearBuf += format ("{0:.4f} 0 l\n", dx);
                        appearBuf += format ("{0:.4f} {1:.4f} l\n", dx, dy);
                        appearBuf += format (
                            "{0:.4f} {1:.4f} l\n", dx - borderWidth,
                            dy - borderWidth);
                        appearBuf += format (
                            "{0:.4f} {1:.4f} l\n", dx - borderWidth,
                            borderWidth);
                        appearBuf += format ("{0:.4f} {0:.4f} l\n", borderWidth);
                        appearBuf += "f\n";
                        break;
                    case annotBorderUnderlined:
                        appearBuf += format ("{0:.4f} w\n", borderWidth);
                        setColor (obj1.as_array (), false, 0);
                        appearBuf += format ("0 0 m {0:.4f} 0 l s\n", dx);
                        break;
                    }

                    // clip to the inside of the border
                    appearBuf += format (
                        "{0:.4f} {0:.4f} {1:.4f} {2:.4f} re W n\n", borderWidth,
                        dx - 2 * borderWidth, dy - 2 * borderWidth);
                }
            }
        }
    }

    free (borderDash);

    // get the resource dictionary
    fieldLookup ("DR", &drObj);

    // build the font dictionary
    if (drObj.is_dict () && drObj.dictLookup ("Font", &obj1)->is_dict ()) {
        fontDict = new GfxFontDict (
            acroForm->doc->getXRef (), 0, obj1.as_dict ());
    }
    else {
        fontDict = NULL;
    }

    // get the default appearance string
    if (fieldLookup ("DA", &obj1)->is_string ()) {
        da = obj1.as_string ()->copy ();
    }
    else {
        da = NULL;
    }

    // get the rotation value
    rot = 0;

    if (mkDict) {
        if (mkDict->lookup ("R", &obj1)->is_int ()) {
            rot = obj1.as_int ();
        }
    }

    // get the appearance state
    annot->lookup ("AP", &apObj);
    annot->lookup ("AS", &asObj);

    appearanceState = 0;

    if (asObj.is_name ()) {
        appearanceState = new GString (asObj.as_name ());
    }
    else if (apObj.is_dict ()) {
        apObj.dictLookup ("N", &obj1);

        if (obj1.is_dict () && obj1.dictGetLength () == 1) {
            appearanceState = new GString (obj1.dictGetKey (0));
        }
    }

    if (!appearanceState) {
        appearanceState = new GString ("Off");
    }

    // draw the field contents
    if (ftObj.is_name ("Btn")) {
        caption = 0;

        if (mkDict) {
            if (mkDict->lookup ("CA", &obj1)->is_string ()) {
                caption = obj1.as_string ()->copy ();
            }
        }

        // radio button
        if (flags & acroFormFlagRadio) {
            //~ Acrobat doesn't draw a caption if there is no AP dict (?)
            if (fieldLookup ("V", &obj1)->is_name (appearanceState->c_str ())) {
                if (caption) {
                    drawText (
                        caption, da, fontDict, false, 0, acroFormQuadCenter,
                        false, true, rot, xMin, yMin, xMax, yMax,
                        borderWidth);
                }
                else {
                    if (mkDict) {
                        if (mkDict->lookup ("BC", &obj2)->is_array () &&
                            obj2.arrayGetLength () > 0) {
                            dx = xMax - xMin;
                            dy = yMax - yMin;
                            setColor (obj2.as_array (), true, 0);
                            drawCircle (
                                0.5 * dx, 0.5 * dy, 0.2 * (dx < dy ? dx : dy),
                                "f");
                        }
                    }
                }
            }
            // pushbutton
        }
        else if (flags & acroFormFlagPushbutton) {
            if (caption) {
                drawText (
                    caption, da, fontDict, false, 0, acroFormQuadCenter,
                    false, false, rot, xMin, yMin, xMax, yMax, borderWidth);
            }
            // checkbox
        }
        else {
            fieldLookup ("V", &obj1);
            if (obj1.is_name () && !obj1.is_name ("Off")) {
                if (!caption) {
                    caption = new GString ("3"); // ZapfDingbats checkmark
                }
                drawText (
                    caption, da, fontDict, false, 0, acroFormQuadCenter,
                    false, true, rot, xMin, yMin, xMax, yMax, borderWidth);
            }
        }
        if (caption) { delete caption; }
    }
    else if (ftObj.is_name ("Tx")) {
        //~ value strings can be Unicode
        if (!fieldLookup ("V", &obj1)->is_string ()) {
            fieldLookup ("DV", &obj1);
        }
        if (obj1.is_string ()) {
            if (fieldLookup ("Q", &obj2)->is_int ()) {
                quadding = obj2.as_int ();
            }
            else {
                quadding = acroFormQuadLeft;
            }
            comb = 0;
            if (flags & acroFormFlagComb) {
                if (fieldLookup ("MaxLen", &obj2)->is_int ()) {
                    comb = obj2.as_int ();
                }
            }
            drawText (
                obj1.as_string (), da, fontDict, flags & acroFormFlagMultiline,
                comb, quadding, true, false, rot, xMin, yMin, xMax, yMax,
                borderWidth);
        }
    }
    else if (ftObj.is_name ("Ch")) {
        //~ value/option strings can be Unicode
        if (fieldLookup ("Q", &obj1)->is_int ()) { quadding = obj1.as_int (); }
        else {
            quadding = acroFormQuadLeft;
        }
        // combo box
        if (flags & acroFormFlagCombo) {
            if (fieldLookup ("V", &obj1)->is_string ()) {
                drawText (
                    obj1.as_string (), da, fontDict, false, 0, quadding, true,
                    false, rot, xMin, yMin, xMax, yMax, borderWidth);
                //~ Acrobat draws a popup icon on the right side
            }
            // list box
        }
        else {
            if (fieldObj.dictLookup ("Opt", &obj1)->is_array ()) {
                nOptions = obj1.arrayGetLength ();
                // get the option text
                text = (GString**)calloc (nOptions, sizeof (GString*));
                for (i = 0; i < nOptions; ++i) {
                    text[i] = NULL;
                    obj1.arrayGet (i, &obj2);
                    if (obj2.is_string ()) {
                        text[i] = obj2.as_string ()->copy ();
                    }
                    else if (obj2.is_array () && obj2.arrayGetLength () == 2) {
                        if (obj2.arrayGet (1, &obj3)->is_string ()) {
                            text[i] = obj3.as_string ()->copy ();
                        }
                    }
                    if (!text[i]) { text[i] = new GString (); }
                }
                // get the selected option(s)
                selection = (bool*)calloc (nOptions, sizeof (bool));
                //~ need to use the I field in addition to the V field
                fieldLookup ("V", &obj2);
                for (i = 0; i < nOptions; ++i) {
                    selection[i] = false;
                    if (obj2.is_string ()) {
                        if (!obj2.as_string ()->cmp (text[i])) {
                            selection[i] = true;
                        }
                    }
                    else if (obj2.is_array ()) {
                        for (j = 0; j < obj2.arrayGetLength (); ++j) {
                            if (obj2.arrayGet (j, &obj3)->is_string () &&
                                !obj3.as_string ()->cmp (text[i])) {
                                selection[i] = true;
                            }
                        }
                    }
                }
                // get the top index
                if (fieldObj.dictLookup ("TI", &obj2)->is_int ()) {
                    topIdx = obj2.as_int ();
                }
                else {
                    topIdx = 0;
                }
                // draw the text
                drawListBox (
                    text, selection, nOptions, topIdx, da, fontDict, quadding,
                    xMin, yMin, xMax, yMax, borderWidth);
                for (i = 0; i < nOptions; ++i) { delete text[i]; }
                free (text);
                free (selection);
            }
        }
    }
    else if (ftObj.is_name ("Sig")) {
        //~unimp
    }
    else {
        error (errSyntaxError, -1, "Unknown field type");
    }

    delete appearanceState;
    if (da) { delete da; }

    // build the appearance stream dictionary
    appearDict = xpdf::make_dict_obj (acroForm->doc->getXRef ());

    appearDict.dictAdd ("Length",  xpdf::make_int_obj (appearBuf.size ()));
    appearDict.dictAdd ("Subtype", xpdf::make_name_obj ("Form"));

    obj1 = xpdf::make_arr_obj (acroForm->doc->getXRef ());

    obj1.arrayAdd (xpdf::make_real_obj (0));
    obj1.arrayAdd (xpdf::make_real_obj (0));
    obj1.arrayAdd (xpdf::make_real_obj (xMax - xMin));
    obj1.arrayAdd (xpdf::make_real_obj (yMax - yMin));

    appearDict.dictAdd ("BBox", &obj1);

    // set the resource dictionary
    if (drObj.is_dict ()) {
        appearDict.dictAdd ("Resources", &drObj);
    }

    // build the appearance stream
    appearStream = new MemStream (
        appearBuf.c_str (), 0, appearBuf.size (), &appearDict);
    appearance = xpdf::make_stream_obj (appearStream);

    // draw it
    gfx->drawAnnot (&appearance, NULL, xMin, yMin, xMax, yMax);

    if (fontDict) {
        delete fontDict;
    }

}

// Set the current fill or stroke color, based on <a> (which should
// have 1, 3, or 4 elements).  If <adjust> is +1, color is brightened;
// if <adjust> is -1, color is darkened; otherwise color is not
// modified.
void AcroFormField::setColor (Array& arr, bool fill, int adjust) {
    Object obj1;
    double color[4];
    int nComps, i;

    nComps = arr.size ();
    if (nComps > 4) { nComps = 4; }
    for (i = 0; i < nComps && i < 4; ++i) {
        if (arr.get (i, &obj1)->is_num ()) { color[i] = obj1.as_num (); }
        else {
            color[i] = 0;
        }
    }
    if (nComps == 4) { adjust = -adjust; }
    if (adjust > 0) {
        for (i = 0; i < nComps; ++i) { color[i] = 0.5 * color[i] + 0.5; }
    }
    else if (adjust < 0) {
        for (i = 0; i < nComps; ++i) { color[i] = 0.5 * color[i]; }
    }
    if (nComps == 4) {
        appearBuf += format (
            "{0:.2f} {1:.2f} {2:.2f} {3:.2f} {4:c}\n", color[0], color[1],
            color[2], color[3], fill ? 'k' : 'K');
    }
    else if (nComps == 3) {
        appearBuf += format (
            "{0:.2f} {1:.2f} {2:.2f} {3:s}\n", color[0], color[1], color[2],
            fill ? "rg" : "RG");
    }
    else {
        appearBuf += format ("{0:.2f} {1:c}\n", color[0], fill ? 'g' : 'G');
    }
}

// Draw the variable text or caption for a field.
void AcroFormField::drawText (
    GString* text, GString* da, GfxFontDict* fontDict, bool multiline,
    int comb, int quadding, bool txField, bool forceZapfDingbats, int rot,
    double xMin, double yMin, double xMax, double yMax, double border) {
    GString* text2;
    GList* daToks;
    GString* tok;
    GfxFont* font;
    double dx, dy;
    double fontSize, fontSize2, x, xPrev, y, w, w2, wMax;
    int tfPos, tmPos, i, j, k, c;

    //~ if there is no MK entry, this should use the existing content stream,
    //~ and only replace the marked content portion of it
    //~ (this is only relevant for Tx fields)

    // check for a Unicode string
    //~ this currently drops all non-Latin1 characters
    if (text->getLength () >= 2 && text->getChar (0) == '\xfe' &&
        text->getChar (1) == '\xff') {
        text2 = new GString ();
        for (i = 2; i + 1 < text->getLength (); i += 2) {
            c = ((text->getChar (i) & 0xff) << 8) +
                (text->getChar (i + 1) & 0xff);
            if (c <= 0xff) { text2->append ((char)c); }
            else {
                text2->append ('?');
            }
        }
    }
    else {
        text2 = text;
    }

    // parse the default appearance string
    tfPos = tmPos = -1;
    if (da) {
        daToks = new GList ();
        i = 0;
        while (i < da->getLength ()) {
            while (i < da->getLength () && xpdf::lexer_t::isSpace (da->getChar (i))) {
                ++i;
            }
            if (i < da->getLength ()) {
                for (j = i + 1;
                     j < da->getLength () && !xpdf::lexer_t::isSpace (da->getChar (j));
                     ++j)
                    ;
                daToks->append (new GString (da, i, j - i));
                i = j;
            }
        }
        for (i = 2; i < daToks->getLength (); ++i) {
            if (i >= 2 && !((GString*)daToks->get (i))->cmp ("Tf")) {
                tfPos = i - 2;
            }
            else if (i >= 6 && !((GString*)daToks->get (i))->cmp ("Tm")) {
                tmPos = i - 6;
            }
        }
    }
    else {
        daToks = NULL;
    }

    // force ZapfDingbats
    //~ this should create the font if needed (?)
    if (forceZapfDingbats) {
        if (tfPos >= 0) {
            tok = (GString*)daToks->get (tfPos);
            if (tok->cmp ("/ZaDb")) {
                tok->clear ();
                tok->append ("/ZaDb");
            }
        }
    }

    // get the font and font size
    font = NULL;
    fontSize = 0;
    if (tfPos >= 0) {
        tok = (GString*)daToks->get (tfPos);
        if (tok->getLength () >= 1 && tok->getChar (0) == '/') {
            if (!fontDict ||
                !(font = fontDict->lookup (tok->c_str () + 1))) {
                error (errSyntaxError, -1, "Unknown font in field's DA string");
            }
        }
        else {
            error (
                errSyntaxError, -1,
                "Invalid font name in 'Tf' operator in field's DA string");
        }
        tok = (GString*)daToks->get (tfPos + 1);
        fontSize = atof (tok->c_str ());
    }
    else {
        error (
            errSyntaxError, -1, "Missing 'Tf' operator in field's DA string");
    }

    // setup
    if (txField) { appearBuf += "/Tx BMC\n"; }
    appearBuf += "q\n";
    if (rot == 90) {
        appearBuf += format ("0 1 -1 0 {0:.4f} 0 cm\n", xMax - xMin);
        dx = yMax - yMin;
        dy = xMax - xMin;
    }
    else if (rot == 180) {
        appearBuf += format (
            "-1 0 0 -1 {0:.4f} {1:.4f} cm\n", xMax - xMin, yMax - yMin);
        dx = xMax - yMax;
        dy = yMax - yMin;
    }
    else if (rot == 270) {
        appearBuf += format ("0 -1 1 0 0 {0:.4f} cm\n", yMax - yMin);
        dx = yMax - yMin;
        dy = xMax - xMin;
    }
    else { // assume rot == 0
        dx = xMax - xMin;
        dy = yMax - yMin;
    }
    appearBuf += "BT\n";

    // multi-line text
    if (multiline) {
        // note: the comb flag is ignored in multiline mode

        wMax = dx - 2 * border - 4;

        // compute font autosize
        if (fontSize == 0) {
            for (fontSize = 20; fontSize > 1; --fontSize) {
                y = dy - 3;
                w2 = 0;
                i = 0;
                while (i < text2->getLength ()) {
                    getNextLine (text2, i, font, fontSize, wMax, &j, &w, &k);
                    if (w > w2) { w2 = w; }
                    i = k;
                    y -= fontSize;
                }
                // approximate the descender for the last line
                if (y >= 0.33 * fontSize) { break; }
            }
            if (tfPos >= 0) {
                tok = (GString*)daToks->get (tfPos + 1);
                tok->clear ();
                tok->appendf ("{0:.2f}", fontSize);
            }
        }

        // starting y coordinate
        // (note: each line of text starts with a Td operator that moves
        // down a line)
        y = dy - 3;

        // set the font matrix
        if (tmPos >= 0) {
            tok = (GString*)daToks->get (tmPos + 4);
            tok->clear ();
            tok->append ('0');
            tok = (GString*)daToks->get (tmPos + 5);
            tok->clear ();
            tok->appendf ("{0:.4f}", y);
        }

        // write the DA string
        if (daToks) {
            for (i = 0; i < daToks->getLength (); ++i) {
                auto& gstr = *(GString*)daToks->get (i);
                appearBuf += std::string (gstr.c_str ());
                appearBuf.append (1UL, ' ');
            }
        }

        // write the font matrix (if not part of the DA string)
        if (tmPos < 0) { appearBuf += format ("1 0 0 1 0 {0:.4f} Tm\n", y); }

        // write a series of lines of text
        i = 0;
        xPrev = 0;
        while (i < text2->getLength ()) {
            getNextLine (text2, i, font, fontSize, wMax, &j, &w, &k);

            // compute text start position
            switch (quadding) {
            case acroFormQuadLeft:
            default: x = border + 2; break;
            case acroFormQuadCenter: x = (dx - w) / 2; break;
            case acroFormQuadRight: x = dx - border - 2 - w; break;
            }

            // draw the line
            appearBuf += format ("{0:.4f} {1:.4f} Td\n", x - xPrev, -fontSize);
            appearBuf.append (1UL, '(');
            for (; i < j; ++i) {
                c = text2->getChar (i) & 0xff;
                if (c == '(' || c == ')' || c == '\\') {
                    appearBuf += '\\';
                    appearBuf += c;
                }
                else if (c < 0x20 || c >= 0x80) {
                    appearBuf += format ("\\{0:03o}", c);
                }
                else {
                    appearBuf += c;
                }
            }
            appearBuf += ") Tj\n";

            // next line
            i = k;
            xPrev = x;
        }

        // single-line text
    }
    else {
        //~ replace newlines with spaces? - what does Acrobat do?

        // comb formatting
        if (comb > 0) {
            // compute comb spacing
            w = (dx - 2 * border) / comb;

            // compute font autosize
            if (fontSize == 0) {
                fontSize = dy - 2 * border;
                if (w < fontSize) { fontSize = w; }
                fontSize = floor (fontSize);
                if (tfPos >= 0) {
                    tok = (GString*)daToks->get (tfPos + 1);
                    tok->clear ();
                    tok->appendf ("{0:.4f}", fontSize);
                }
            }

            // compute text start position
            switch (quadding) {
            case acroFormQuadLeft:
            default: x = border + 2; break;
            case acroFormQuadCenter:
                x = border + 2 + 0.5 * (comb - text2->getLength ()) * w;
                break;
            case acroFormQuadRight:
                x = border + 2 + (comb - text2->getLength ()) * w;
                break;
            }
            y = 0.5 * dy - 0.4 * fontSize;

            // set the font matrix
            if (tmPos >= 0) {
                tok = (GString*)daToks->get (tmPos + 4);
                tok->clear ();
                tok->appendf ("{0:.4f}", x);
                tok = (GString*)daToks->get (tmPos + 5);
                tok->clear ();
                tok->appendf ("{0:.4f}", y);
            }

            // write the DA string
            if (daToks) {
                for (i = 0; i < daToks->getLength (); ++i) {
                    auto& gstr = *(GString*)daToks->get (i);
                    appearBuf += std::string (gstr.c_str ());
                    appearBuf.append (1UL, ' ');
                }
            }

            // write the font matrix (if not part of the DA string)
            if (tmPos < 0) {
                appearBuf += format ("1 0 0 1 {0:.4f} {1:.4f} Tm\n", x, y);
            }

            // write the text string
            //~ this should center (instead of left-justify) each character within
            //~     its comb cell
            for (i = 0; i < text2->getLength (); ++i) {
                if (i > 0) { appearBuf += format ("{0:.4f} 0 Td\n", w); }
                appearBuf.append (1UL, '(');
                c = text2->getChar (i) & 0xff;
                if (c == '(' || c == ')' || c == '\\') {
                    appearBuf += '\\';
                    appearBuf += c;
                }
                else if (c < 0x20 || c >= 0x80) {
                    appearBuf += format ("{0:.4f} 0 Td\n", w);
                }
                else {
                    appearBuf += c;
                }
                appearBuf += ") Tj\n";
            }

            // regular (non-comb) formatting
        }
        else {
            // compute string width
            if (font && !font->isCIDFont ()) {
                w = 0;
                for (i = 0; i < text2->getLength (); ++i) {
                    w += ((Gfx8BitFont*)font)->getWidth (text2->getChar (i));
                }
            }
            else {
                // otherwise, make a crude estimate
                w = text2->getLength () * 0.5;
            }

            // compute font autosize
            if (fontSize == 0) {
                fontSize = dy - 2 * border;
                fontSize2 = (dx - 4 - 2 * border) / w;
                if (fontSize2 < fontSize) { fontSize = fontSize2; }
                fontSize = floor (fontSize);
                if (tfPos >= 0) {
                    tok = (GString*)daToks->get (tfPos + 1);
                    tok->clear ();
                    tok->appendf ("{0:.4f}", fontSize);
                }
            }

            // compute text start position
            w *= fontSize;
            switch (quadding) {
            case acroFormQuadLeft:
            default: x = border + 2; break;
            case acroFormQuadCenter: x = (dx - w) / 2; break;
            case acroFormQuadRight: x = dx - border - 2 - w; break;
            }
            y = 0.5 * dy - 0.4 * fontSize;

            // set the font matrix
            if (tmPos >= 0) {
                tok = (GString*)daToks->get (tmPos + 4);
                tok->clear ();
                tok->appendf ("{0:.4f}", x);
                tok = (GString*)daToks->get (tmPos + 5);
                tok->clear ();
                tok->appendf ("{0:.4f}", y);
            }

            // write the DA string
            if (daToks) {
                for (i = 0; i < daToks->getLength (); ++i) {
                    auto& gstr = *(GString*)daToks->get (i);
                    appearBuf += std::string (gstr.c_str ());
                    appearBuf.append (1UL, ' ');
                }
            }

            // write the font matrix (if not part of the DA string)
            if (tmPos < 0) {
                appearBuf += format ("1 0 0 1 {0:.4f} {1:.4f} Tm\n", x, y);
            }

            // write the text string
            appearBuf.append (1UL, '(');
            for (i = 0; i < text2->getLength (); ++i) {
                c = text2->getChar (i) & 0xff;
                if (c == '(' || c == ')' || c == '\\') {
                    appearBuf += '\\';
                    appearBuf += c;
                }
                else if (c < 0x20 || c >= 0x80) {
                    appearBuf += format ("\\{0:03o}", c);
                }
                else {
                    appearBuf += c;
                }
            }
            appearBuf += ") Tj\n";
        }
    }

    // cleanup
    appearBuf += "ET\n";
    appearBuf += "Q\n";
    if (txField) { appearBuf += "EMC\n"; }

    if (daToks) { deleteGList (daToks, GString); }
    if (text2 != text) { delete text2; }
}

// Draw the variable text or caption for a field.
void AcroFormField::drawListBox (
    GString** text, bool* selection, int nOptions, int topIdx, GString* da,
    GfxFontDict* fontDict, int quadding, double xMin, double yMin,
    double xMax, double yMax, double border) {
    GList* daToks;
    GString* tok;
    GfxFont* font;
    double fontSize, fontSize2, x, y, w, wMax;
    int tfPos, tmPos, i, j, c;

    //~ if there is no MK entry, this should use the existing content stream,
    //~ and only replace the marked content portion of it
    //~ (this is only relevant for Tx fields)

    // parse the default appearance string
    tfPos = tmPos = -1;
    if (da) {
        daToks = new GList ();
        i = 0;
        while (i < da->getLength ()) {
            while (i < da->getLength () && xpdf::lexer_t::isSpace (da->getChar (i))) {
                ++i;
            }
            if (i < da->getLength ()) {
                for (j = i + 1;
                     j < da->getLength () && !xpdf::lexer_t::isSpace (da->getChar (j));
                     ++j)
                    ;
                daToks->append (new GString (da, i, j - i));
                i = j;
            }
        }
        for (i = 2; i < daToks->getLength (); ++i) {
            if (i >= 2 && !((GString*)daToks->get (i))->cmp ("Tf")) {
                tfPos = i - 2;
            }
            else if (i >= 6 && !((GString*)daToks->get (i))->cmp ("Tm")) {
                tmPos = i - 6;
            }
        }
    }
    else {
        daToks = NULL;
    }

    // get the font and font size
    font = NULL;
    fontSize = 0;
    if (tfPos >= 0) {
        tok = (GString*)daToks->get (tfPos);
        if (tok->getLength () >= 1 && tok->getChar (0) == '/') {
            if (!fontDict ||
                !(font = fontDict->lookup (tok->c_str () + 1))) {
                error (errSyntaxError, -1, "Unknown font in field's DA string");
            }
        }
        else {
            error (
                errSyntaxError, -1,
                "Invalid font name in 'Tf' operator in field's DA string");
        }
        tok = (GString*)daToks->get (tfPos + 1);
        fontSize = atof (tok->c_str ());
    }
    else {
        error (
            errSyntaxError, -1, "Missing 'Tf' operator in field's DA string");
    }

    // compute font autosize
    if (fontSize == 0) {
        wMax = 0;
        for (i = 0; i < nOptions; ++i) {
            if (font && !font->isCIDFont ()) {
                w = 0;
                for (j = 0; j < text[i]->getLength (); ++j) {
                    w += ((Gfx8BitFont*)font)->getWidth (text[i]->getChar (j));
                }
            }
            else {
                // otherwise, make a crude estimate
                w = text[i]->getLength () * 0.5;
            }
            if (w > wMax) { wMax = w; }
        }
        fontSize = yMax - yMin - 2 * border;
        fontSize2 = (xMax - xMin - 4 - 2 * border) / wMax;
        if (fontSize2 < fontSize) { fontSize = fontSize2; }
        fontSize = floor (fontSize);
        if (tfPos >= 0) {
            tok = (GString*)daToks->get (tfPos + 1);
            tok->clear ();
            tok->appendf ("{0:.4f}", fontSize);
        }
    }

    // draw the text
    y = yMax - yMin - 1.1 * fontSize;
    for (i = topIdx; i < nOptions; ++i) {
        // setup
        appearBuf += "q\n";

        // draw the background if selected
        if (selection[i]) {
            appearBuf += "0 g f\n";
            appearBuf += format (
                "{0:.4f} {1:.4f} {2:.4f} {3:.4f} re f\n", border,
                y - 0.2 * fontSize, xMax - xMin - 2 * border, 1.1 * fontSize);
        }

        // setup
        appearBuf += "BT\n";

        // compute string width
        if (font && !font->isCIDFont ()) {
            w = 0;
            for (j = 0; j < text[i]->getLength (); ++j) {
                w += ((Gfx8BitFont*)font)->getWidth (text[i]->getChar (j));
            }
        }
        else {
            // otherwise, make a crude estimate
            w = text[i]->getLength () * 0.5;
        }

        // compute text start position
        w *= fontSize;
        switch (quadding) {
        case acroFormQuadLeft:
        default: x = border + 2; break;
        case acroFormQuadCenter: x = (xMax - xMin - w) / 2; break;
        case acroFormQuadRight: x = xMax - xMin - border - 2 - w; break;
        }

        // set the font matrix
        if (tmPos >= 0) {
            tok = (GString*)daToks->get (tmPos + 4);
            tok->clear ();
            tok->appendf ("{0:.4f}", x);
            tok = (GString*)daToks->get (tmPos + 5);
            tok->clear ();
            tok->appendf ("{0:.4f}", y);
        }

        // write the DA string
        if (daToks) {
            for (j = 0; j < daToks->getLength (); ++j) {
                auto& gstr = *(GString*)daToks->get (i);
                appearBuf += std::string (gstr.c_str ());
                appearBuf.append (1UL, ' ');
            }
        }

        // write the font matrix (if not part of the DA string)
        if (tmPos < 0) {
            appearBuf += format ("1 0 0 1 {0:.4f} {1:.4f} Tm\n", x, y);
        }

        // change the text color if selected
        if (selection[i]) { appearBuf += "1 g\n"; }

        // write the text string
        appearBuf.append (1UL, '(');
        for (j = 0; j < text[i]->getLength (); ++j) {
            c = text[i]->getChar (j) & 0xff;
            if (c == '(' || c == ')' || c == '\\') {
                appearBuf += '\\';
                appearBuf += c;
            }
            else if (c < 0x20 || c >= 0x80) {
                appearBuf += format ("\\{0:03o}", c);
            }
            else {
                appearBuf += c;
            }
        }
        appearBuf += ") Tj\n";

        // cleanup
        appearBuf += "ET\nQ\n";

        // next line
        y -= 1.1 * fontSize;
    }

    if (daToks) { deleteGList (daToks, GString); }
}

// Figure out how much text will fit on the next line.  Returns:
// *end = one past the last character to be included
// *width = width of the characters start .. end-1
// *next = index of first character on the following line
void AcroFormField::getNextLine (
    GString* text, int start, GfxFont* font, double fontSize, double wMax,
    int* end, double* width, int* next) {
    double w, dw;
    int j, k, c;

    // figure out how much text will fit on the line
    //~ what does Adobe do with tabs?
    w = 0;
    for (j = start; j < text->getLength () && w <= wMax; ++j) {
        c = text->getChar (j) & 0xff;
        if (c == 0x0a || c == 0x0d) { break; }
        if (font && !font->isCIDFont ()) {
            dw = ((Gfx8BitFont*)font)->getWidth (c) * fontSize;
        }
        else {
            // otherwise, make a crude estimate
            dw = 0.5 * fontSize;
        }
        w += dw;
    }
    if (w > wMax) {
        for (k = j; k > start && text->getChar (k - 1) != ' '; --k)
            ;
        for (; k > start && text->getChar (k - 1) == ' '; --k)
            ;
        if (k > start) { j = k; }
        if (j == start) {
            // handle the pathological case where the first character is
            // too wide to fit on the line all by itself
            j = start + 1;
        }
    }
    *end = j;

    // compute the width
    w = 0;
    for (k = start; k < j; ++k) {
        if (font && !font->isCIDFont ()) {
            dw = ((Gfx8BitFont*)font)->getWidth (text->getChar (k)) * fontSize;
        }
        else {
            // otherwise, make a crude estimate
            dw = 0.5 * fontSize;
        }
        w += dw;
    }
    *width = w;

    // next line
    while (j < text->getLength () && text->getChar (j) == ' ') { ++j; }
    if (j < text->getLength () && text->getChar (j) == 0x0d) { ++j; }
    if (j < text->getLength () && text->getChar (j) == 0x0a) { ++j; }
    *next = j;
}

// Draw an (approximate) circle of radius <r> centered at (<cx>, <cy>).
// <cmd> is used to draw the circle ("f", "s", or "b").
void AcroFormField::drawCircle (
    double cx, double cy, double r, const char* cmd) {
    appearBuf += format ("{0:.4f} {1:.4f} m\n", cx + r, cy);
    appearBuf += format (
        "{0:.4f} {1:.4f} {2:.4f} {3:.4f} {4:.4f} {5:.4f} c\n", cx + r,
        cy + bezierCircle * r, cx + bezierCircle * r, cy + r, cx, cy + r);
    appearBuf += format (
        "{0:.4f} {1:.4f} {2:.4f} {3:.4f} {4:.4f} {5:.4f} c\n",
        cx - bezierCircle * r, cy + r, cx - r, cy + bezierCircle * r, cx - r,
        cy);
    appearBuf += format (
        "{0:.4f} {1:.4f} {2:.4f} {3:.4f} {4:.4f} {5:.4f} c\n", cx - r,
        cy - bezierCircle * r, cx - bezierCircle * r, cy - r, cx, cy - r);
    appearBuf += format (
        "{0:.4f} {1:.4f} {2:.4f} {3:.4f} {4:.4f} {5:.4f} c\n",
        cx + bezierCircle * r, cy - r, cx + r, cy - bezierCircle * r, cx + r,
        cy);
    appearBuf += format ("{0:s}\n", cmd);
}

// Draw the top-left half of an (approximate) circle of radius <r>
// centered at (<cx>, <cy>).
void AcroFormField::drawCircleTopLeft (double cx, double cy, double r) {
    double r2;

    r2 = r / sqrt (2.0);
    appearBuf += format ("{0:.4f} {1:.4f} m\n", cx + r2, cy + r2);
    appearBuf += format (
        "{0:.4f} {1:.4f} {2:.4f} {3:.4f} {4:.4f} {5:.4f} c\n",
        cx + (1 - bezierCircle) * r2, cy + (1 + bezierCircle) * r2,
        cx - (1 - bezierCircle) * r2, cy + (1 + bezierCircle) * r2, cx - r2,
        cy + r2);
    appearBuf += format (
        "{0:.4f} {1:.4f} {2:.4f} {3:.4f} {4:.4f} {5:.4f} c\n",
        cx - (1 + bezierCircle) * r2, cy + (1 - bezierCircle) * r2,
        cx - (1 + bezierCircle) * r2, cy - (1 - bezierCircle) * r2, cx - r2,
        cy - r2);
    appearBuf += "S\n";
}

// Draw the bottom-right half of an (approximate) circle of radius <r>
// centered at (<cx>, <cy>).
void AcroFormField::drawCircleBottomRight (double cx, double cy, double r) {
    double r2;

    r2 = r / sqrt (2.0);
    appearBuf += format ("{0:.4f} {1:.4f} m\n", cx - r2, cy - r2);
    appearBuf += format (
        "{0:.4f} {1:.4f} {2:.4f} {3:.4f} {4:.4f} {5:.4f} c\n",
        cx - (1 - bezierCircle) * r2, cy - (1 + bezierCircle) * r2,
        cx + (1 - bezierCircle) * r2, cy - (1 + bezierCircle) * r2, cx + r2,
        cy - r2);
    appearBuf += format (
        "{0:.4f} {1:.4f} {2:.4f} {3:.4f} {4:.4f} {5:.4f} c\n",
        cx + (1 + bezierCircle) * r2, cy - (1 - bezierCircle) * r2,
        cx + (1 + bezierCircle) * r2, cy + (1 - bezierCircle) * r2, cx + r2,
        cy + r2);
    appearBuf += "S\n";
}

Object* AcroFormField::getResources (Object* res) {
    Object kidsObj, annotObj, obj1;
    int i;

    if (acroForm->needAppearances) { fieldLookup ("DR", res); }
    else {
        *res = xpdf::make_arr_obj (acroForm->doc->getXRef ());
        // find the annotation object(s)
        if (fieldObj.dictLookup ("Kids", &kidsObj)->is_array ()) {
            for (i = 0; i < kidsObj.arrayGetLength (); ++i) {
                kidsObj.arrayGet (i, &annotObj);
                if (annotObj.is_dict ()) {
                    if (getAnnotResources (annotObj.as_dict (), &obj1)
                            ->is_dict ()) {
                        res->arrayAdd (&obj1);
                    }
                    else {
                    }
                }
            }
        }
        else {
            if (getAnnotResources (fieldObj.as_dict (), &obj1)->is_dict ()) {
                res->arrayAdd (&obj1);
            }
            else {
            }
        }
    }

    return res;
}

Object* AcroFormField::getAnnotResources (Dict* annot, Object* res) {
    Object apObj, appearance;

    // get the appearance stream
    if (annot->lookup ("AP", &apObj)->is_dict ()) {
        Object obj1;

        apObj.dictLookup ("N", &obj1);

        if (obj1.is_dict ()) {
            Object asObj;

            if (annot->lookup ("AS", &asObj)->is_name ()) {
                obj1.dictLookup (asObj.as_name (), &appearance);
            }
            else if (obj1.dictGetLength () == 1) {
                obj1.dictGetVal (0, &appearance);
            }
            else {
                obj1.dictLookup ("Off", &appearance);
            }
        }
        else {
            appearance = obj1;
        }
    }


    if (appearance.is_stream ()) {
        appearance.streamGetDict ()->lookup ("Resources", res);
    }
    else {
        *res = { };
    }

    return res;
}

// Look up an inheritable field dictionary entry.
Object* AcroFormField::fieldLookup (const char* key, Object* obj) {
    return fieldLookup (fieldObj.as_dict (), key, obj);
}

Object* AcroFormField::fieldLookup (Dict* dict, const char* key, Object* obj) {
    dict->lookup (key, obj);

    if (!obj->is_null ()) {
        return obj;
    }

    Object parent;
    dict->lookup ("Parent", &parent);

    if (parent.is_dict ()) {
        fieldLookup (parent.as_dict (), key, obj);
    }
    else {
        // some fields don't specify a parent, so we check the AcroForm
        // dictionary just in case
        acroForm->acroFormObj.dictLookup (key, obj);
    }

    return obj;
}

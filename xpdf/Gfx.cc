// -*- mode: c++; -*-
// Copyright 1996-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdlib>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <utils/string.hh>
#include <utils/GList.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/CharTypes.hh>
#include <xpdf/obj.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/array.hh>
#include <xpdf/dict.hh>
#include <xpdf/Stream.hh>
#include <xpdf/Lexer.hh>
#include <xpdf/Parser.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/OutputDev.hh>
#include <xpdf/Page.hh>
#include <xpdf/Annot.hh>
#include <xpdf/OptionalContent.hh>
#include <xpdf/Error.hh>
#include <xpdf/TextString.hh>
#include <xpdf/Gfx.hh>

#include <range/v3/algorithm/fill.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
using namespace ranges;

// the MSVC math.h doesn't define this
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//------------------------------------------------------------------------
// constants
//------------------------------------------------------------------------

// Max recursive depth for a function shading fill.
#define functionMaxDepth 6

// Max delta allowed in any color component for a function shading fill.
#define functionColorDelta (xpdf::color_t{ 256 })

// Max number of splits along the t axis for an axial shading fill.
#define axialMaxSplits 256

// Max delta allowed in any color component for an axial shading fill.
#define axialColorDelta (xpdf::color_t{ 256 })

// Max number of splits along the t axis for a radial shading fill.
#define radialMaxSplits 256

// Max delta allowed in any color component for a radial shading fill.
#define radialColorDelta (xpdf::color_t{ 256 })

// Max recursive depth for a Gouraud triangle shading fill.
#define gouraudMaxDepth 6

// Max delta allowed in any color component for a Gouraud triangle
// shading fill.
#define gouraudColorDelta (xpdf::color_t{ 256 })

// Max recursive depth for a patch mesh shading fill.
#define patchMaxDepth 6

// Max delta allowed in any color component for a patch mesh shading
// fill.
#define patchColorDelta (xpdf::color_t{ 256 })

// Max errors (undefined operator, wrong number of args) allowed before
// giving up on a content stream.
#define contentStreamErrorLimit 500

//------------------------------------------------------------------------
// Operator table
//------------------------------------------------------------------------

/* static */ Gfx::Operator Gfx::opTab [] = {
    { "\"", 3, { typeCheckNum, typeCheckNum, typeCheckString }, &Gfx::opMoveSetShowText },
    { "'", 1, { typeCheckString }, &Gfx::opMoveShowText },
    { "B", 0, { typeCheckNone }, &Gfx::opFillStroke },
    { "B*", 0, { typeCheckNone }, &Gfx::opEOFillStroke },
    { "BDC", 2, { typeCheckName, typeCheckProps }, &Gfx::opBeginMarkedContent },
    { "BI", 0, { typeCheckNone }, &Gfx::opBeginImage },
    { "BMC", 1, { typeCheckName }, &Gfx::opBeginMarkedContent },
    { "BT", 0, { typeCheckNone }, &Gfx::opBeginText },
    { "BX", 0, { typeCheckNone }, &Gfx::opBeginIgnoreUndef },
    { "CS", 1, { typeCheckName }, &Gfx::opSetStrokeColorSpace },
    { "DP", 2, { typeCheckName, typeCheckProps }, &Gfx::opMarkPoint },
    { "Do", 1, { typeCheckName }, &Gfx::opXObject },
    { "EI", 0, { typeCheckNone }, &Gfx::opEndImage },
    { "EMC", 0, { typeCheckNone }, &Gfx::opEndMarkedContent },
    { "ET", 0, { typeCheckNone }, &Gfx::opEndText },
    { "EX", 0, { typeCheckNone }, &Gfx::opEndIgnoreUndef },
    { "F", 0, { typeCheckNone }, &Gfx::opFill },
    { "G", 1, { typeCheckNum }, &Gfx::opSetStrokeGray },
    { "ID", 0, { typeCheckNone }, &Gfx::opImageData },
    { "J", 1, { typeCheckInt }, &Gfx::opSetLineCap },
    { "K",
      4,
      { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum },
      &Gfx::opSetStrokeCMYKColor },
    { "M", 1, { typeCheckNum }, &Gfx::opSetMiterLimit },
    { "MP", 1, { typeCheckName }, &Gfx::opMarkPoint },
    { "Q", 0, { typeCheckNone }, &Gfx::opRestore },
    { "RG", 3, { typeCheckNum, typeCheckNum, typeCheckNum }, &Gfx::opSetStrokeRGBColor },
    { "S", 0, { typeCheckNone }, &Gfx::opStroke },
    { "SC",
      -4,
      { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum },
      &Gfx::opSetStrokeColor },
    { "SCN",
      -33,
      { typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN,
        typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN,
        typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN,
        typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN,
        typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN },
      &Gfx::opSetStrokeColorN },
    { "T*", 0, { typeCheckNone }, &Gfx::opTextNextLine },
    { "TD", 2, { typeCheckNum, typeCheckNum }, &Gfx::opTextMoveSet },
    { "TJ", 1, { typeCheckArray }, &Gfx::opShowSpaceText },
    { "TL", 1, { typeCheckNum }, &Gfx::opSetTextLeading },
    { "Tc", 1, { typeCheckNum }, &Gfx::opSetCharSpacing },
    { "Td", 2, { typeCheckNum, typeCheckNum }, &Gfx::opTextMove },
    { "Tf", 2, { typeCheckName, typeCheckNum }, &Gfx::opSetFont },
    { "Tj", 1, { typeCheckString }, &Gfx::opShowText },
    { "Tm",
      6,
      { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum },
      &Gfx::opSetTextMatrix },
    { "Tr", 1, { typeCheckInt }, &Gfx::opSetTextRender },
    { "Ts", 1, { typeCheckNum }, &Gfx::opSetTextRise },
    { "Tw", 1, { typeCheckNum }, &Gfx::opSetWordSpacing },
    { "Tz", 1, { typeCheckNum }, &Gfx::opSetHorizScaling },
    { "W", 0, { typeCheckNone }, &Gfx::opClip },
    { "W*", 0, { typeCheckNone }, &Gfx::opEOClip },
    { "b", 0, { typeCheckNone }, &Gfx::opCloseFillStroke },
    { "b*", 0, { typeCheckNone }, &Gfx::opCloseEOFillStroke },
    { "c",
      6,
      { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum },
      &Gfx::opCurveTo },
    { "cm",
      6,
      { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum },
      &Gfx::opConcat },
    { "cs", 1, { typeCheckName }, &Gfx::opSetFillColorSpace },
    { "d", 2, { typeCheckArray, typeCheckNum }, &Gfx::opSetDash },
    { "d0", 2, { typeCheckNum, typeCheckNum }, &Gfx::opSetCharWidth },
    { "d1",
      6,
      { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum },
      &Gfx::opSetCacheDevice },
    { "f", 0, { typeCheckNone }, &Gfx::opFill },
    { "f*", 0, { typeCheckNone }, &Gfx::opEOFill },
    { "g", 1, { typeCheckNum }, &Gfx::opSetFillGray },
    { "gs", 1, { typeCheckName }, &Gfx::opSetExtGState },
    { "h", 0, { typeCheckNone }, &Gfx::opClosePath },
    { "i", 1, { typeCheckNum }, &Gfx::opSetFlat },
    { "j", 1, { typeCheckInt }, &Gfx::opSetLineJoin },
    { "k",
      4,
      { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum },
      &Gfx::opSetFillCMYKColor },
    { "l", 2, { typeCheckNum, typeCheckNum }, &Gfx::opLineTo },
    { "m", 2, { typeCheckNum, typeCheckNum }, &Gfx::opMoveTo },
    { "n", 0, { typeCheckNone }, &Gfx::opEndPath },
    { "q", 0, { typeCheckNone }, &Gfx::opSave },
    { "re", 4, { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum }, &Gfx::opRectangle },
    { "rg", 3, { typeCheckNum, typeCheckNum, typeCheckNum }, &Gfx::opSetFillRGBColor },
    { "ri", 1, { typeCheckName }, &Gfx::opSetRenderingIntent },
    { "s", 0, { typeCheckNone }, &Gfx::opCloseStroke },
    { "sc", -4, { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum }, &Gfx::opSetFillColor },
    { "scn",
      -33,
      { typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN,
        typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN,
        typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN,
        typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN,
        typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN, typeCheckSCN },
      &Gfx::opSetFillColorN },
    { "sh", 1, { typeCheckName }, &Gfx::opShFill },
    { "v", 4, { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum }, &Gfx::opCurveTo1 },
    { "w", 1, { typeCheckNum }, &Gfx::opSetLineWidth },
    { "y", 4, { typeCheckNum, typeCheckNum, typeCheckNum, typeCheckNum }, &Gfx::opCurveTo2 },
};

#define numOps (sizeof (opTab) / sizeof (Operator))

//------------------------------------------------------------------------
// GfxResources
//------------------------------------------------------------------------

GfxResources::GfxResources (XRef* xref, Dict* resDict, GfxResources* nextA) {
    Object obj1, obj2;
    Ref r;

    if (resDict) {
        // build font dictionary
        fonts = NULL;
        obj1 = (*resDict) ["Font"];
        if (obj1.is_ref ()) {
            obj2 = resolve (obj1);
            if (obj2.is_dict ()) {
                r = obj1.as_ref ();
                fonts = new GfxFontDict (xref, &r, &obj2.as_dict ());
            }
        }
        else if (obj1.is_dict ()) {
            fonts = new GfxFontDict (xref, NULL, &obj1.as_dict ());
        }

        // get XObject dictionary
        xObjDict = resolve ((*resDict) ["XObject"]);

        // get color space dictionary
        colorSpaceDict = resolve ((*resDict) ["ColorSpace"]);

        // get pattern dictionary
        patternDict = resolve ((*resDict) ["Pattern"]);

        // get shading dictionary
        shadingDict = resolve ((*resDict) ["Shading"]);

        // get graphics state parameter dictionary
        gStateDict = resolve ((*resDict) ["ExtGState"]);

        // get properties dictionary
        propsDict = resolve ((*resDict) ["Properties"]);
    }
    else {
        fonts = NULL;
        xObjDict = { };
        colorSpaceDict = { };
        patternDict = { };
        shadingDict = { };
        gStateDict = { };
        propsDict = { };
    }

    next = nextA;
}

GfxResources::~GfxResources () {
    if (fonts) { delete fonts; }
}

GfxFont* GfxResources::lookupFont (const char* name) {
    GfxFont* font;
    GfxResources* resPtr;

    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->fonts) {
            if ((font = resPtr->fonts->lookup (name))) { return font; }
        }
    }
    error (errSyntaxError, -1, "Unknown font tag '{0:s}'", name);
    return NULL;
}

GfxFont* GfxResources::lookupFontByRef (Ref ref) {
    GfxFont* font;
    GfxResources* resPtr;

    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->fonts) {
            if ((font = resPtr->fonts->lookupByRef (ref))) { return font; }
        }
    }
    error (
        errSyntaxError, -1, "Unknown font ref {0:d}.{1:d}", ref.num, ref.gen);
    return NULL;
}

bool GfxResources::lookupXObject (const char* name, Object* obj) {
    GfxResources* resPtr;

    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->xObjDict.is_dict ()) {
            *obj = resolve (resPtr->xObjDict.as_dict ()[name]);

            if (!obj->is_null ()) {
                return true;
            }

            *obj = { };
        }
    }

    error (errSyntaxError, -1, "XObject '{0:s}' is unknown", name);
    return false;
}

bool GfxResources::lookupXObjectNF (const char* name, Object* obj) {
    GfxResources* resPtr;

    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->xObjDict.is_dict ()) {
            *obj = resPtr->xObjDict.as_dict ()[name];

            if (!obj->is_null ()) {
                return true;
            }

            *obj = { };
        }
    }
    error (errSyntaxError, -1, "XObject '{0:s}' is unknown", name);
    return false;
}

void GfxResources::lookupColorSpace (const char* name, Object* obj) {
    GfxResources* resPtr;

    //~ should also test for G, RGB, and CMYK - but only in inline images (?)
    if (!strcmp (name, "DeviceGray") || !strcmp (name, "DeviceRGB") ||
        !strcmp (name, "DeviceCMYK")) {
        *obj = { };
        return;
    }
    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->colorSpaceDict.is_dict ()) {
            *obj = resolve (resPtr->colorSpaceDict.as_dict ()[name]);

            if (!obj->is_null ()) {
                return;
            }

            *obj = { };
        }
    }
    *obj = { };
}

GfxPattern* GfxResources::lookupPattern (const char* name) {
    GfxResources* resPtr;
    GfxPattern* pattern;
    Object objRef, obj;

    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->patternDict.is_dict ()) {
            if (!(obj = resolve (resPtr->patternDict.as_dict ()[name])).is_null ()) {
                objRef = resPtr->patternDict.as_dict ()[name];
                pattern = GfxPattern::parse (&objRef, &obj);
                return pattern;
            }
        }
    }
    error (errSyntaxError, -1, "Unknown pattern '{0:s}'", name);
    return NULL;
}

GfxShading* GfxResources::lookupShading (const char* name) {
    GfxResources* resPtr;
    GfxShading* shading;
    Object obj;

    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->shadingDict.is_dict ()) {
            if (!(obj = resolve (resPtr->shadingDict.as_dict ()[name])).is_null ()) {
                shading = GfxShading::parse (&obj);
                return shading;
            }
        }
    }
    error (errSyntaxError, -1, "Unknown shading '{0:s}'", name);
    return NULL;
}

bool GfxResources::lookupGState (const char* name, Object* obj) {
    GfxResources* resPtr;

    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->gStateDict.is_dict ()) {
            *obj = resolve (resPtr->gStateDict.as_dict ()[name]);

            if (!obj->is_null ()) {
                return true;
            }

            *obj = { };
        }
    }
    error (errSyntaxError, -1, "ExtGState '{0:s}' is unknown", name);
    return false;
}

bool GfxResources::lookupPropertiesNF (const char* name, Object* obj) {
    GfxResources* resPtr;

    for (resPtr = this; resPtr; resPtr = resPtr->next) {
        if (resPtr->propsDict.is_dict ()) {
            *obj = resPtr->propsDict.as_dict ()[name];

            if (!obj->is_null ()) {
                return true;
            }

            *obj = { };
        }
    }

    error (errSyntaxError, -1, "Properties '{0:s}' is unknown", name);
    return false;
}

//------------------------------------------------------------------------
// Gfx
//------------------------------------------------------------------------

Gfx::Gfx (
    PDFDoc* docA, OutputDev* outA, int pageNum, Dict* resDict, double hDPI,
    double vDPI, PDFRectangle* box, PDFRectangle* cropBox, int rotate,
    bool (*abortCheckCbkA) (void* data), void* abortCheckCbkDataA) {
    int i;

    doc = docA;
    xref = doc->getXRef ();
    subPage = false;
    printCommands = globalParams->getPrintCommands ();

    // start the resource stack
    res = new GfxResources (xref, resDict, NULL);

    // initialize
    out = outA;
    state = new GfxState (hDPI, vDPI, box, rotate, out->upsideDown ());
    fontChanged = false;
    clip = clipNone;
    ignoreUndef = 0;
    out->startPage (pageNum, state);
    out->setDefaultCTM (state->getCTM ());
    out->updateAll (state);
    for (i = 0; i < 6; ++i) { baseMatrix[i] = state->getCTM ()[i]; }
    formDepth = 0;
    textClipBBoxEmpty = true;
    markedContentStack = new GList ();
    ocState = true;
    parser = NULL;
    abortCheckCbk = abortCheckCbkA;
    abortCheckCbkData = abortCheckCbkDataA;

    // set crop box
    if (cropBox) {
        state->moveTo (cropBox->x1, cropBox->y1);
        state->lineTo (cropBox->x2, cropBox->y1);
        state->lineTo (cropBox->x2, cropBox->y2);
        state->lineTo (cropBox->x1, cropBox->y2);
        state->closePath ();
        state->clip ();
        out->clip (state);
        state->clearPath ();
    }
}

Gfx::Gfx (
    PDFDoc* docA, OutputDev* outA, Dict* resDict, PDFRectangle* box,
    PDFRectangle* cropBox, bool (*abortCheckCbkA) (void* data),
    void* abortCheckCbkDataA) {
    int i;

    doc = docA;
    xref = doc->getXRef ();
    subPage = true;
    printCommands = globalParams->getPrintCommands ();

    // start the resource stack
    res = new GfxResources (xref, resDict, NULL);

    // initialize
    out = outA;
    state = new GfxState (72, 72, box, 0, false);
    fontChanged = false;
    clip = clipNone;
    ignoreUndef = 0;
    for (i = 0; i < 6; ++i) { baseMatrix[i] = state->getCTM ()[i]; }
    formDepth = 0;
    textClipBBoxEmpty = true;
    markedContentStack = new GList ();
    ocState = true;
    parser = NULL;
    abortCheckCbk = abortCheckCbkA;
    abortCheckCbkData = abortCheckCbkDataA;

    // set crop box
    if (cropBox) {
        state->moveTo (cropBox->x1, cropBox->y1);
        state->lineTo (cropBox->x2, cropBox->y1);
        state->lineTo (cropBox->x2, cropBox->y2);
        state->lineTo (cropBox->x1, cropBox->y2);
        state->closePath ();
        state->clip ();
        out->clip (state);
        state->clearPath ();
    }
}

Gfx::~Gfx () {
    if (!subPage) { out->endPage (); }
    while (state->hasSaves ()) { restoreState (); }
    delete state;
    while (res) { popResources (); }
    deleteGList (markedContentStack, GfxMarkedContent);
}

void Gfx::display (Object* objRef, bool topLevel) {
    Object obj = xpdf::resolve (*objRef);

    if (obj.is_array ()) {
        auto& arr = obj.as_array ();

        auto iter = find_if (arr, [this](auto& x) {
            return checkForContentStreamLoop (&x); });

        if (iter != arr.end ()) {
            return;
        }

        for_each (arr, [](auto& x) {
            if (!resolve (x).is_stream ()) {
                throw std::runtime_error ("not a content stream");
            }});

        contentStreamStack.push_back (obj);
    }
    else if (obj.is_stream ()) {
        if (checkForContentStreamLoop (objRef)) {
            return;
        }

        contentStreamStack.push_back (*objRef);
    }
    else {
        throw std::runtime_error ("not a content stream");
    }

    parser = new Parser (xref, new Lexer (&obj), false);
    go (topLevel);

    delete parser;
    parser = 0;

    contentStreamStack.pop_back ();
}

// If <ref> is already on contentStreamStack, i.e., if there is a loop
// in the content streams, report an error, and return true.
bool Gfx::checkForContentStreamLoop (Object* ref) {
    if (ref->is_ref ()) {
        for (size_t i = 0; i < contentStreamStack.size (); ++i) {
            auto& obj = contentStreamStack [i];

            if (obj.is_ref ()) {
                if (ref->getRefNum () == obj.getRefNum () &&
                    ref->getRefGen () == obj.getRefGen ()) {
                    error (errSyntaxError, -1, "Loop in content streams");
                    return true;
                }
            }
            else if (obj.is_array ()) {
                auto& arr = obj.as_array ();

                for (size_t j = 0; j < arr.size (); ++j) {
                    auto& obj1 = arr [j];

                    if (obj1.is_ref ()) {
                        if (ref->getRefNum () == obj1.getRefNum () &&
                            ref->getRefGen () == obj1.getRefGen ()) {
                            error (errSyntaxError, -1, "Loop in content streams");
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

void Gfx::go (bool topLevel) {
    Object obj;
    Object args[maxArgs];
    int numArgs, i;
    int lastAbortCheck, errCount;

    // scan a sequence of objects
    updateLevel = 1; // make sure even empty pages trigger a call to dump()
    lastAbortCheck = 0;
    errCount = 0;
    numArgs = 0;
    parser->getObj (&obj);
    while (!obj.is_eof ()) {
        // got a command - execute it
        if (obj.is_cmd ()) {
            if (printCommands) {
                obj.print (stdout);
                for (i = 0; i < numArgs; ++i) {
                    printf (" ");
                    args[i].print (stdout);
                }
                printf ("\n");
                fflush (stdout);
            }
            if (!execOp (&obj, args, numArgs)) { ++errCount; }

            fill (args, args + numArgs, xpdf::obj_t{ });
            numArgs = 0;

            // periodically update display
            if (++updateLevel >= 20000) {
                out->dump ();
                updateLevel = 0;
            }

            // check for an abort
            if (abortCheckCbk) {
                if (updateLevel - lastAbortCheck > 10) {
                    if ((*abortCheckCbk) (abortCheckCbkData)) { break; }
                    lastAbortCheck = updateLevel;
                }
            }

            // check for too many errors
            if (errCount > contentStreamErrorLimit) {
                error (
                    errSyntaxError, -1,
                    "Too many errors - giving up on this content stream");
                break;
            }

            // got an argument - save it
        }
        else if (numArgs < maxArgs) {
            args[numArgs++] = obj;

            // too many arguments - something is wrong
        }
        else {
            error (
                errSyntaxError, getPos (), "Too many args in content stream");
            if (printCommands) {
                printf ("throwing away arg: ");
                obj.print (stdout);
                printf ("\n");
                fflush (stdout);
            }
        }

        // grab the next object
        parser->getObj (&obj);
    }

    // args at end with no command
    if (numArgs > 0) {
        error (errSyntaxError, getPos (), "Leftover args in content stream");
        if (printCommands) {
            printf ("%d leftovers:", numArgs);
            for (i = 0; i < numArgs; ++i) {
                printf (" ");
                args[i].print (stdout);
            }
            printf ("\n");
            fflush (stdout);
        }

        fill (args, args + numArgs, xpdf::obj_t{ });
    }

    // update display
    if (topLevel && updateLevel > 0) { out->dump (); }
}

// Returns true if successful, false on error.
bool Gfx::execOp (Object* cmd, Object args[], int numArgs) {
    Operator* op;
    const char* name;
    Object* argPtr;
    int i;

    // find operator
    name = cmd->as_cmd ();
    if (!(op = findOp (name))) {
        if (ignoreUndef > 0) { return true; }
        error (errSyntaxError, getPos (), "Unknown operator '{0:s}'", name);
        return false;
    }

    // type check args
    argPtr = args;
    if (op->numArgs >= 0) {
        if (numArgs < op->numArgs) {
            error (
                errSyntaxError, getPos (),
                "Too few ({0:d}) args to '{1:s}' operator", numArgs, name);
            return false;
        }
        if (numArgs > op->numArgs) {
#if 0
      error(errSyntaxWarning, getPos(),
        "Too many ({0:d}) args to '{1:s}' operator", numArgs, name);
#endif
            argPtr += numArgs - op->numArgs;
            numArgs = op->numArgs;
        }
    }
    else {
        if (numArgs > -op->numArgs) {
            error (
                errSyntaxError, getPos (),
                "Too many ({0:d}) args to '{1:s}' operator", numArgs, name);
            return false;
        }
    }
    for (i = 0; i < numArgs; ++i) {
        if (!checkArg (&argPtr[i], op->tchk[i])) {
            error (
                errSyntaxError, getPos (),
                "Arg #{0:d} to '{1:s}' operator is wrong type ({2:s})", i, name,
                argPtr[i].getTypeName ());
            return false;
        }
    }

    // do it
    (this->*op->func) (argPtr, numArgs);

    return true;
}

Gfx::Operator* Gfx::findOp (const char* name) {
    int a, b, m, cmp;

    a = -1;
    b = numOps;
    cmp = 0; // make gcc happy
    // invariant: opTab[a] < name < opTab[b]
    while (b - a > 1) {
        m = (a + b) / 2;
        cmp = strcmp (opTab[m].name, name);
        if (cmp < 0)
            a = m;
        else if (cmp > 0)
            b = m;
        else
            a = b = m;
    }
    if (cmp != 0) return NULL;
    return &opTab[a];
}

bool Gfx::checkArg (Object* arg, typeCheckType type) {
    switch (type) {
    case typeCheckBool: return arg->is_bool ();
    case typeCheckInt: return arg->is_int ();
    case typeCheckNum: return arg->is_num ();
    case typeCheckString: return arg->is_string ();
    case typeCheckName: return arg->is_name ();
    case typeCheckArray: return arg->is_array ();
    case typeCheckProps: return arg->is_dict () || arg->is_name ();
    case typeCheckSCN: return arg->is_num () || arg->is_name ();
    case typeCheckNone: return false;
    }
    return false;
}

GFileOffset Gfx::getPos () { return parser ? parser->getPos () : -1; }

//------------------------------------------------------------------------
// graphics state operators
//------------------------------------------------------------------------

void Gfx::opSave (Object args[], int numArgs) { saveState (); }

void Gfx::opRestore (Object args[], int numArgs) { restoreState (); }

void Gfx::opConcat (Object args[], int numArgs) {
    state->concatCTM (
        args[0].as_num (), args[1].as_num (), args[2].as_num (),
        args[3].as_num (), args[4].as_num (), args[5].as_num ());
    out->updateCTM (
        state, args[0].as_num (), args[1].as_num (), args[2].as_num (),
        args[3].as_num (), args[4].as_num (), args[5].as_num ());
    fontChanged = true;
}

void Gfx::opSetDash (Object args[], int numArgs) {
    int length;
    Object obj;
    double* dash;
    int i;

    Array& arr = args[0].as_array ();
    length = arr.size ();

    if (length == 0) {
        dash = NULL;
    }
    else {
        dash = (double*)calloc (length, sizeof (double));

        for (i = 0; i < length; ++i) {
            dash[i] = resolve (arr [i]).as_num ();
        }
    }

    state->setLineDash (dash, length, args[1].as_num ());
    out->updateLineDash (state);
}

void Gfx::opSetFlat (Object args[], int numArgs) {
    state->setFlatness ((int)args[0].as_num ());
    out->updateFlatness (state);
}

void Gfx::opSetLineJoin (Object args[], int numArgs) {
    state->setLineJoin (args[0].as_int ());
    out->updateLineJoin (state);
}

void Gfx::opSetLineCap (Object args[], int numArgs) {
    state->setLineCap (args[0].as_int ());
    out->updateLineCap (state);
}

void Gfx::opSetMiterLimit (Object args[], int numArgs) {
    state->setMiterLimit (args[0].as_num ());
    out->updateMiterLimit (state);
}

void Gfx::opSetLineWidth (Object args[], int numArgs) {
    state->setLineWidth (args[0].as_num ());
    out->updateLineWidth (state);
}

void Gfx::opSetExtGState (Object args[], int numArgs) {
    Object obj1, obj2, obj3, objRef3, obj4, obj5;
    Object args2[2];
    GfxBlendMode mode;
    bool haveFillOP;
    Function funcs[4];
    GfxColor backdropColor;
    bool haveBackdropColor;
    GfxColorSpace* blendingColorSpace;
    bool alpha, isolated, knockout;
    double opac;
    int i;

    if (!res->lookupGState (args[0].as_name (), &obj1)) { return; }
    if (!obj1.is_dict ()) {
        error (
            errSyntaxError, getPos (), "ExtGState '{0:s}' is wrong type",
            args[0].as_name ());
        return;
    }
    if (printCommands) {
        printf ("  gfx state dict: ");
        obj1.print ();
        printf ("\n");
    }

    // parameters that are also set by individual PDF operators
    if ((obj2 = resolve (obj1.as_dict ()["LW"])).is_num ()) { opSetLineWidth (&obj2, 1); }
    if ((obj2 = resolve (obj1.as_dict ()["LC"])).is_int ()) { opSetLineCap (&obj2, 1); }
    if ((obj2 = resolve (obj1.as_dict ()["LJ"])).is_int ()) { opSetLineJoin (&obj2, 1); }
    if ((obj2 = resolve (obj1.as_dict ()["ML"])).is_num ()) { opSetMiterLimit (&obj2, 1); }

    if ((obj2 = resolve (obj1.as_dict ()["D"])).is_array () && obj2.as_array ().size () == 2) {
        args2[0] = resolve (obj2 [0UL]);
        args2[1] = resolve (obj2 [1]);

        if (args2[0].is_array () && args2[1].is_num ()) {
            opSetDash (args2, 2);
        }

        args2 [0] = { };
        args2 [1] = { };
    }

    if ((obj2 = resolve (obj1.as_dict ()["FL"])).is_num ()) { opSetFlat (&obj2, 1); }

    // font
    if ((obj2 = resolve (obj1.as_dict ()["Font"])).is_array () &&
        obj2.as_array ().size () == 2) {
        obj3 = obj2 [0UL];
        obj4 = obj2 [2];
        if (obj3.is_ref () && obj4.is_num ()) {
            doSetFont (res->lookupFontByRef (obj3.as_ref ()), obj4.as_num ());
        }
    }

    // transparency support: blend mode, fill/stroke opacity
    if (!(obj2 = resolve (obj1.as_dict ()["BM"])).is_null ()) {
        if (state->parseBlendMode (&obj2, &mode)) {
            state->setBlendMode (mode);
            out->updateBlendMode (state);
        }
        else {
            error (
                errSyntaxError, getPos (), "Invalid blend mode in ExtGState");
        }
    }
    if ((obj2 = resolve (obj1.as_dict ()["ca"])).is_num ()) {
        opac = obj2.as_num ();
        state->setFillOpacity (opac < 0 ? 0 : opac > 1 ? 1 : opac);
        out->updateFillOpacity (state);
    }
    if ((obj2 = resolve (obj1.as_dict ()["CA"])).is_num ()) {
        opac = obj2.as_num ();
        state->setStrokeOpacity (opac < 0 ? 0 : opac > 1 ? 1 : opac);
        out->updateStrokeOpacity (state);
    }

    // fill/stroke overprint, overprint mode
    if ((haveFillOP = ((obj2 = resolve (obj1.as_dict ()["op"])).is_bool ()))) {
        state->setFillOverprint (obj2.as_bool ());
        out->updateFillOverprint (state);
    }
    if ((obj2 = resolve (obj1.as_dict ()["OP"])).is_bool ()) {
        state->setStrokeOverprint (obj2.as_bool ());
        out->updateStrokeOverprint (state);
        if (!haveFillOP) {
            state->setFillOverprint (obj2.as_bool ());
            out->updateFillOverprint (state);
        }
    }
    if ((obj2 = resolve (obj1.as_dict ()["OPM"])).is_int ()) {
        state->setOverprintMode (obj2.as_int ());
        out->updateOverprintMode (state);
    }

    // stroke adjust
    if ((obj2 = resolve (obj1.as_dict ()["SA"])).is_bool ()) {
        state->setStrokeAdjust (obj2.as_bool ());
        out->updateStrokeAdjust (state);
    }

    // transfer function
    if ((obj2 = resolve (obj1.as_dict ()["TR2"])).is_null ()) {
        *&obj2 = resolve (obj1.as_dict ()["TR"]);
    }
    if (obj2.is_name ("Default") || obj2.is_name ("Identity")) {
        state->setTransfer (funcs);
        out->updateTransfer (state);
    }
    else if (obj2.is_array () && obj2.as_array ().size () == 4) {
        for (i = 0; i < 4; ++i) {
            obj3 = resolve (obj2 [i]);
            funcs[i] = xpdf::make_function (obj3);
            if (!funcs[i]) { break; }
        }
        if (i == 4) {
            state->setTransfer (funcs);
            out->updateTransfer (state);
        }
    }
    else if (obj2.is_name () || obj2.is_dict () || obj2.is_stream ()) {
        if ((funcs[0] = xpdf::make_function (obj2))) {
            state->setTransfer (funcs);
            out->updateTransfer (state);
        }
    }
    else if (!obj2.is_null ()) {
        error (
            errSyntaxError, getPos (),
            "Invalid transfer function in ExtGState");
    }

    // soft mask
    if (!(obj2 = resolve (obj1.as_dict ()["SMask"])).is_null ()) {
        if (obj2.is_name ("None")) { out->clearSoftMask (state); }
        else if (obj2.is_dict ()) {
            if ((obj3 = resolve (obj2.as_dict ()["S"])).is_name ("Alpha")) {
                alpha = true;
            }
            else { // "Luminosity"
                alpha = false;
            }
            fill (funcs, funcs + 4, Function{ });
            if (!(obj3 = resolve (obj2.as_dict ()["TR"])).is_null ()) {
                if (obj3.is_name ("Default") || obj3.is_name ("Identity")) {
                    ; // funcs[0] = { };
                }
                else {
                    auto is_unary = [](const auto& f) {
                        return 1 == f.arity () && 1 == f.coarity (); };

                    funcs[0] = xpdf::make_function (obj3);

                    if (!is_unary (funcs [0])) {
                        error (
                            errSyntaxError, getPos (),
                            "Invalid transfer function in soft mask in "
                            "ExtGState");
                        funcs[0] = { };
                    }
                }
            }
            if ((haveBackdropColor =
                     (obj3 = resolve (obj2.as_dict ()["BC"])).is_array ())) {
                for (i = 0; i < gfxColorMaxComps; ++i) {
                    backdropColor.c[i] = 0;
                }
                for (i = 0; i < obj3.as_array ().size () && i < gfxColorMaxComps;
                     ++i) {
                    obj4 = resolve (obj3 [i]);
                    if (obj4.is_num ()) {
                        backdropColor.c[i] = xpdf::to_color (obj4.as_num ());
                    }
                }
            }
            if ((obj3 = resolve (obj2.as_dict ()["G"])).is_stream ()) {
                if ((obj4 = resolve ((*obj3.streamGetDict ()) ["Group"])).is_dict ()) {
                    blendingColorSpace = NULL;
                    isolated = knockout = false;
                    if (!(obj5 = resolve (obj4.as_dict ()["CS"])).is_null ()) {
                        blendingColorSpace = GfxColorSpace::parse (&obj5);
                    }
                    if ((obj5 = resolve (obj4.as_dict ()["I"])).is_bool ()) {
                        isolated = obj5.as_bool ();
                    }
                    if ((obj5 = resolve (obj4.as_dict ()["K"])).is_bool ()) {
                        knockout = obj5.as_bool ();
                    }
                    if (!haveBackdropColor) {
                        if (blendingColorSpace) {
                            blendingColorSpace->getDefaultColor (
                                &backdropColor);
                        }
                        else {
                            //~ need to get the parent or default color space (?)
                            for (i = 0; i < gfxColorMaxComps; ++i) {
                                backdropColor.c[i] = 0;
                            }
                        }
                    }
                    objRef3 = obj2.as_dict ()["G"];
                    doSoftMask (
                        &obj3, &objRef3, alpha, blendingColorSpace, isolated,
                        knockout, funcs [0], &backdropColor);

                    funcs [0] = { };
                }
                else {
                    error (
                        errSyntaxError, getPos (),
                        "Invalid soft mask in ExtGState - missing group");
                }
            }
            else {
                error (
                    errSyntaxError, getPos (),
                    "Invalid soft mask in ExtGState - missing group");
            }
        }
        else if (!obj2.is_null ()) {
            error (errSyntaxError, getPos (), "Invalid soft mask in ExtGState");
        }
    }

}

void Gfx::doSoftMask (
    Object* str, Object* strRef, bool alpha, GfxColorSpace* blendingColorSpace,
    bool isolated, bool knockout, const Function& transferFunc,
    GfxColor* backdropColor) {
    Dict *dict, *resDict;
    double m[6], bbox[4];
    Object obj1, obj2;
    int i;

    // check for excessive recursion
    if (formDepth > 20) { return; }

    // get stream dict
    dict = str->streamGetDict ();

    // check form type
    obj1 = resolve ((*dict) ["FormType"]);
    if (!(obj1.is_null () || (obj1.is_int () && obj1.as_int () == 1))) {
        error (errSyntaxError, getPos (), "Unknown form type");
    }

    // get bounding box
    obj1 = resolve ((*dict) ["BBox"]);
    if (!obj1.is_array ()) {
        error (errSyntaxError, getPos (), "Bad form bounding box");
        return;
    }
    for (i = 0; i < 4; ++i) {
        obj2 = resolve (obj1 [i]);
        bbox[i] = obj2.as_num ();
    }

    // get matrix
    obj1 = resolve ((*dict) ["Matrix"]);
    if (obj1.is_array ()) {
        for (i = 0; i < 6; ++i) {
            obj2 = resolve (obj1 [i]);
            m[i] = obj2.as_num ();
        }
    }
    else {
        m[0] = 1;
        m[1] = 0;
        m[2] = 0;
        m[3] = 1;
        m[4] = 0;
        m[5] = 0;
    }

    // get resources
    obj1 = resolve ((*dict) ["Resources"]);
    resDict = obj1.is_dict () ? &obj1.as_dict () : (Dict*)NULL;

    // draw it
    ++formDepth;
    drawForm (
        strRef, resDict, m, bbox, true, true, blendingColorSpace, isolated,
        knockout, alpha, transferFunc, backdropColor);
    --formDepth;

    if (blendingColorSpace) { delete blendingColorSpace; }
}

void Gfx::opSetRenderingIntent (Object args[], int numArgs) {}

//------------------------------------------------------------------------
// color operators
//------------------------------------------------------------------------

void Gfx::opSetFillGray (Object args[], int numArgs) {
    GfxColor color;

    state->setFillPattern (NULL);
    state->setFillColorSpace (GfxColorSpace::create (csDeviceGray));
    out->updateFillColorSpace (state);
    color.c[0] = xpdf::to_color (args[0].as_num ());
    state->setFillColor (&color);
    out->updateFillColor (state);
}

void Gfx::opSetStrokeGray (Object args[], int numArgs) {
    GfxColor color;

    state->setStrokePattern (NULL);
    state->setStrokeColorSpace (GfxColorSpace::create (csDeviceGray));
    out->updateStrokeColorSpace (state);
    color.c[0] = xpdf::to_color (args[0].as_num ());
    state->setStrokeColor (&color);
    out->updateStrokeColor (state);
}

void Gfx::opSetFillCMYKColor (Object args[], int numArgs) {
    GfxColor color;
    int i;

    state->setFillPattern (NULL);
    state->setFillColorSpace (GfxColorSpace::create (csDeviceCMYK));
    out->updateFillColorSpace (state);
    for (i = 0; i < 4; ++i) { color.c[i] = xpdf::to_color (args[i].as_num ()); }
    state->setFillColor (&color);
    out->updateFillColor (state);
}

void Gfx::opSetStrokeCMYKColor (Object args[], int numArgs) {
    GfxColor color;
    int i;

    state->setStrokePattern (NULL);
    state->setStrokeColorSpace (GfxColorSpace::create (csDeviceCMYK));
    out->updateStrokeColorSpace (state);
    for (i = 0; i < 4; ++i) { color.c[i] = xpdf::to_color (args[i].as_num ()); }
    state->setStrokeColor (&color);
    out->updateStrokeColor (state);
}

void Gfx::opSetFillRGBColor (Object args[], int numArgs) {
    GfxColor color;
    int i;

    state->setFillPattern (NULL);
    state->setFillColorSpace (GfxColorSpace::create (csDeviceRGB));
    out->updateFillColorSpace (state);
    for (i = 0; i < 3; ++i) { color.c[i] = xpdf::to_color (args[i].as_num ()); }
    state->setFillColor (&color);
    out->updateFillColor (state);
}

void Gfx::opSetStrokeRGBColor (Object args[], int numArgs) {
    GfxColor color;
    int i;

    state->setStrokePattern (NULL);
    state->setStrokeColorSpace (GfxColorSpace::create (csDeviceRGB));
    out->updateStrokeColorSpace (state);
    for (i = 0; i < 3; ++i) { color.c[i] = xpdf::to_color (args[i].as_num ()); }
    state->setStrokeColor (&color);
    out->updateStrokeColor (state);
}

void Gfx::opSetFillColorSpace (Object args[], int numArgs) {
    Object obj;
    GfxColorSpace* colorSpace;
    GfxColor color;

    state->setFillPattern (NULL);
    res->lookupColorSpace (args[0].as_name (), &obj);
    if (obj.is_null ()) { colorSpace = GfxColorSpace::parse (&args[0]); }
    else {
        colorSpace = GfxColorSpace::parse (&obj);
    }
    if (colorSpace) {
        state->setFillColorSpace (colorSpace);
        out->updateFillColorSpace (state);
        colorSpace->getDefaultColor (&color);
        state->setFillColor (&color);
        out->updateFillColor (state);
    }
    else {
        error (errSyntaxError, getPos (), "Bad color space (fill)");
    }
}

void Gfx::opSetStrokeColorSpace (Object args[], int numArgs) {
    Object obj;
    GfxColorSpace* colorSpace;
    GfxColor color;

    state->setStrokePattern (NULL);
    res->lookupColorSpace (args[0].as_name (), &obj);
    if (obj.is_null ()) { colorSpace = GfxColorSpace::parse (&args[0]); }
    else {
        colorSpace = GfxColorSpace::parse (&obj);
    }
    if (colorSpace) {
        state->setStrokeColorSpace (colorSpace);
        out->updateStrokeColorSpace (state);
        colorSpace->getDefaultColor (&color);
        state->setStrokeColor (&color);
        out->updateStrokeColor (state);
    }
    else {
        error (errSyntaxError, getPos (), "Bad color space (stroke)");
    }
}

void Gfx::opSetFillColor (Object args[], int numArgs) {
    GfxColor color;
    int i;

    if (numArgs != state->getFillColorSpace ()->getNComps ()) {
        error (
            errSyntaxError, getPos (),
            "Incorrect number of arguments in 'sc' command");
        return;
    }
    state->setFillPattern (NULL);
    for (i = 0; i < numArgs; ++i) { color.c[i] = xpdf::to_color (args[i].as_num ()); }
    state->setFillColor (&color);
    out->updateFillColor (state);
}

void Gfx::opSetStrokeColor (Object args[], int numArgs) {
    GfxColor color;
    int i;

    if (numArgs != state->getStrokeColorSpace ()->getNComps ()) {
        error (
            errSyntaxError, getPos (),
            "Incorrect number of arguments in 'SC' command");
        return;
    }
    state->setStrokePattern (NULL);
    for (i = 0; i < numArgs; ++i) { color.c[i] = xpdf::to_color (args[i].as_num ()); }
    state->setStrokeColor (&color);
    out->updateStrokeColor (state);
}

void Gfx::opSetFillColorN (Object args[], int numArgs) {
    GfxColor color;
    GfxPattern* pattern;
    int i;

    if (state->getFillColorSpace ()->getMode () == csPattern) {
        if (numArgs > 1) {
            if (!((GfxPatternColorSpace*)state->getFillColorSpace ())
                     ->getUnder () ||
                numArgs - 1 !=
                    ((GfxPatternColorSpace*)state->getFillColorSpace ())
                        ->getUnder ()
                        ->getNComps ()) {
                error (
                    errSyntaxError, getPos (),
                    "Incorrect number of arguments in 'scn' command");
                return;
            }
            for (i = 0; i < numArgs - 1 && i < gfxColorMaxComps; ++i) {
                if (args[i].is_num ()) {
                    color.c[i] = xpdf::to_color (args[i].as_num ());
                }
            }
            state->setFillColor (&color);
            out->updateFillColor (state);
        }
        if (args[numArgs - 1].is_name () &&
            (pattern = res->lookupPattern (args[numArgs - 1].as_name ()))) {
            state->setFillPattern (pattern);
        }
    }
    else {
        if (numArgs != state->getFillColorSpace ()->getNComps ()) {
            error (
                errSyntaxError, getPos (),
                "Incorrect number of arguments in 'scn' command");
            return;
        }
        state->setFillPattern (NULL);
        for (i = 0; i < numArgs && i < gfxColorMaxComps; ++i) {
            if (args[i].is_num ()) { color.c[i] = xpdf::to_color (args[i].as_num ()); }
        }
        state->setFillColor (&color);
        out->updateFillColor (state);
    }
}

void Gfx::opSetStrokeColorN (Object args[], int numArgs) {
    GfxColor color;
    GfxPattern* pattern;
    int i;

    if (state->getStrokeColorSpace ()->getMode () == csPattern) {
        if (numArgs > 1) {
            if (!((GfxPatternColorSpace*)state->getStrokeColorSpace ())
                     ->getUnder () ||
                numArgs - 1 !=
                    ((GfxPatternColorSpace*)state->getStrokeColorSpace ())
                        ->getUnder ()
                        ->getNComps ()) {
                error (
                    errSyntaxError, getPos (),
                    "Incorrect number of arguments in 'SCN' command");
                return;
            }
            for (i = 0; i < numArgs - 1 && i < gfxColorMaxComps; ++i) {
                if (args[i].is_num ()) {
                    color.c[i] = xpdf::to_color (args[i].as_num ());
                }
            }
            state->setStrokeColor (&color);
            out->updateStrokeColor (state);
        }
        if (args[numArgs - 1].is_name () &&
            (pattern = res->lookupPattern (args[numArgs - 1].as_name ()))) {
            state->setStrokePattern (pattern);
        }
    }
    else {
        if (numArgs != state->getStrokeColorSpace ()->getNComps ()) {
            error (
                errSyntaxError, getPos (),
                "Incorrect number of arguments in 'SCN' command");
            return;
        }
        state->setStrokePattern (NULL);
        for (i = 0; i < numArgs && i < gfxColorMaxComps; ++i) {
            if (args[i].is_num ()) { color.c[i] = xpdf::to_color (args[i].as_num ()); }
        }
        state->setStrokeColor (&color);
        out->updateStrokeColor (state);
    }
}

//------------------------------------------------------------------------
// path segment operators
//------------------------------------------------------------------------

void Gfx::opMoveTo (Object args[], int numArgs) {
    state->moveTo (args[0].as_num (), args[1].as_num ());
}

void Gfx::opLineTo (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        error (errSyntaxError, getPos (), "No current point in lineto");
        return;
    }
    state->lineTo (args[0].as_num (), args[1].as_num ());
}

void Gfx::opCurveTo (Object args[], int numArgs) {
    double x1, y1, x2, y2, x3, y3;

    if (!state->isCurPt ()) {
        error (errSyntaxError, getPos (), "No current point in curveto");
        return;
    }
    x1 = args[0].as_num ();
    y1 = args[1].as_num ();
    x2 = args[2].as_num ();
    y2 = args[3].as_num ();
    x3 = args[4].as_num ();
    y3 = args[5].as_num ();
    state->curveTo (x1, y1, x2, y2, x3, y3);
}

void Gfx::opCurveTo1 (Object args[], int numArgs) {
    double x1, y1, x2, y2, x3, y3;

    if (!state->isCurPt ()) {
        error (errSyntaxError, getPos (), "No current point in curveto1");
        return;
    }
    x1 = state->getCurX ();
    y1 = state->getCurY ();
    x2 = args[0].as_num ();
    y2 = args[1].as_num ();
    x3 = args[2].as_num ();
    y3 = args[3].as_num ();
    state->curveTo (x1, y1, x2, y2, x3, y3);
}

void Gfx::opCurveTo2 (Object args[], int numArgs) {
    double x1, y1, x2, y2, x3, y3;

    if (!state->isCurPt ()) {
        error (errSyntaxError, getPos (), "No current point in curveto2");
        return;
    }
    x1 = args[0].as_num ();
    y1 = args[1].as_num ();
    x2 = args[2].as_num ();
    y2 = args[3].as_num ();
    x3 = x2;
    y3 = y2;
    state->curveTo (x1, y1, x2, y2, x3, y3);
}

void Gfx::opRectangle (Object args[], int numArgs) {
    double x, y, w, h;

    x = args[0].as_num ();
    y = args[1].as_num ();
    w = args[2].as_num ();
    h = args[3].as_num ();
    state->moveTo (x, y);
    state->lineTo (x + w, y);
    state->lineTo (x + w, y + h);
    state->lineTo (x, y + h);
    state->closePath ();
}

void Gfx::opClosePath (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        error (errSyntaxError, getPos (), "No current point in closepath");
        return;
    }
    state->closePath ();
}

//------------------------------------------------------------------------
// path painting operators
//------------------------------------------------------------------------

void Gfx::opEndPath (Object args[], int numArgs) { doEndPath (); }

void Gfx::opStroke (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        //error(errSyntaxError, getPos(), "No path in stroke");
        return;
    }
    if (state->isPath ()) {
        if (ocState) {
            if (state->getStrokeColorSpace ()->getMode () == csPattern) {
                doPatternStroke ();
            }
            else {
                out->stroke (state);
            }
        }
    }
    doEndPath ();
}

void Gfx::opCloseStroke (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        //error(errSyntaxError, getPos(), "No path in closepath/stroke");
        return;
    }
    if (state->isPath ()) {
        state->closePath ();
        if (ocState) {
            if (state->getStrokeColorSpace ()->getMode () == csPattern) {
                doPatternStroke ();
            }
            else {
                out->stroke (state);
            }
        }
    }
    doEndPath ();
}

void Gfx::opFill (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        //error(errSyntaxError, getPos(), "No path in fill");
        return;
    }
    if (state->isPath ()) {
        if (ocState) {
            if (state->getFillColorSpace ()->getMode () == csPattern) {
                doPatternFill (false);
            }
            else {
                out->fill (state);
            }
        }
    }
    doEndPath ();
}

void Gfx::opEOFill (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        //error(errSyntaxError, getPos(), "No path in eofill");
        return;
    }
    if (state->isPath ()) {
        if (ocState) {
            if (state->getFillColorSpace ()->getMode () == csPattern) {
                doPatternFill (true);
            }
            else {
                out->eoFill (state);
            }
        }
    }
    doEndPath ();
}

void Gfx::opFillStroke (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        //error(errSyntaxError, getPos(), "No path in fill/stroke");
        return;
    }
    if (state->isPath ()) {
        if (ocState) {
            if (state->getFillColorSpace ()->getMode () == csPattern) {
                doPatternFill (false);
            }
            else {
                out->fill (state);
            }
            if (state->getStrokeColorSpace ()->getMode () == csPattern) {
                doPatternStroke ();
            }
            else {
                out->stroke (state);
            }
        }
    }
    doEndPath ();
}

void Gfx::opCloseFillStroke (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        //error(errSyntaxError, getPos(), "No path in closepath/fill/stroke");
        return;
    }
    if (state->isPath ()) {
        state->closePath ();
        if (ocState) {
            if (state->getFillColorSpace ()->getMode () == csPattern) {
                doPatternFill (false);
            }
            else {
                out->fill (state);
            }
            if (state->getStrokeColorSpace ()->getMode () == csPattern) {
                doPatternStroke ();
            }
            else {
                out->stroke (state);
            }
        }
    }
    doEndPath ();
}

void Gfx::opEOFillStroke (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        //error(errSyntaxError, getPos(), "No path in eofill/stroke");
        return;
    }
    if (state->isPath ()) {
        if (ocState) {
            if (state->getFillColorSpace ()->getMode () == csPattern) {
                doPatternFill (true);
            }
            else {
                out->eoFill (state);
            }
            if (state->getStrokeColorSpace ()->getMode () == csPattern) {
                doPatternStroke ();
            }
            else {
                out->stroke (state);
            }
        }
    }
    doEndPath ();
}

void Gfx::opCloseEOFillStroke (Object args[], int numArgs) {
    if (!state->isCurPt ()) {
        //error(errSyntaxError, getPos(), "No path in closepath/eofill/stroke");
        return;
    }
    if (state->isPath ()) {
        state->closePath ();
        if (ocState) {
            if (state->getFillColorSpace ()->getMode () == csPattern) {
                doPatternFill (true);
            }
            else {
                out->eoFill (state);
            }
            if (state->getStrokeColorSpace ()->getMode () == csPattern) {
                doPatternStroke ();
            }
            else {
                out->stroke (state);
            }
        }
    }
    doEndPath ();
}

void Gfx::doPatternFill (bool eoFill) {
    GfxPattern* pattern;

    // this is a bit of a kludge -- patterns can be really slow, so we
    // skip them if we're only doing text extraction, since they almost
    // certainly don't contain any text
    if (!out->needNonText ()) { return; }

    if (!(pattern = state->getFillPattern ())) { return; }
    switch (pattern->getType ()) {
    case 1:
        doTilingPatternFill (
            (GfxTilingPattern*)pattern, false, eoFill, false);
        break;
    case 2:
        doShadingPatternFill (
            (GfxShadingPattern*)pattern, false, eoFill, false);
        break;
    default:
        error (
            errSyntaxError, getPos (), "Unknown pattern type ({0:d}) in fill",
            pattern->getType ());
        break;
    }
}

void Gfx::doPatternStroke () {
    GfxPattern* pattern;

    // this is a bit of a kludge -- patterns can be really slow, so we
    // skip them if we're only doing text extraction, since they almost
    // certainly don't contain any text
    if (!out->needNonText ()) { return; }

    if (!(pattern = state->getStrokePattern ())) { return; }
    switch (pattern->getType ()) {
    case 1:
        doTilingPatternFill ((GfxTilingPattern*)pattern, true, false, false);
        break;
    case 2:
        doShadingPatternFill (
            (GfxShadingPattern*)pattern, true, false, false);
        break;
    default:
        error (
            errSyntaxError, getPos (), "Unknown pattern type ({0:d}) in stroke",
            pattern->getType ());
        break;
    }
}

void Gfx::doPatternText () {
    GfxPattern* pattern;

    // this is a bit of a kludge -- patterns can be really slow, so we
    // skip them if we're only doing text extraction, since they almost
    // certainly don't contain any text
    if (!out->needNonText ()) { return; }

    if (!(pattern = state->getFillPattern ())) { return; }
    switch (pattern->getType ()) {
    case 1:
        doTilingPatternFill ((GfxTilingPattern*)pattern, false, false, true);
        break;
    case 2:
        doShadingPatternFill (
            (GfxShadingPattern*)pattern, false, false, true);
        break;
    default:
        error (
            errSyntaxError, getPos (), "Unknown pattern type ({0:d}) in fill",
            pattern->getType ());
        break;
    }
}

void Gfx::doPatternImageMask (
    Object* ref, Stream* str, int width, int height, bool invert,
    bool inlineImg, bool interpolate) {
    saveState ();

    out->setSoftMaskFromImageMask (
        state, ref, str, width, height, invert, inlineImg, interpolate);

    state->clearPath ();
    state->moveTo (0, 0);
    state->lineTo (1, 0);
    state->lineTo (1, 1);
    state->lineTo (0, 1);
    state->closePath ();
    doPatternFill (true);

    restoreState ();
}

void Gfx::doTilingPatternFill (
    GfxTilingPattern* tPat, bool stroke, bool eoFill, bool text) {
    GfxPatternColorSpace* patCS;
    GfxColorSpace* cs;
    GfxState* savedState;
    double xMin, yMin, xMax, yMax, x, y, x1, y1, t;
    double cxMin, cyMin, cxMax, cyMax;
    int xi0, yi0, xi1, yi1, xi, yi;
    double *ctm, *btm, *ptm;
    double bbox[4], m[6], ictm[6], m1[6], imb[6];
    double det;
    double xstep, ystep;
    int i;

    // get color space
    patCS =
        (GfxPatternColorSpace*)(stroke ? state->getStrokeColorSpace () : state->getFillColorSpace ());

    // construct a (pattern space) -> (current space) transform matrix
    ctm = state->getCTM ();
    btm = baseMatrix;
    ptm = tPat->getMatrix ();
    // iCTM = invert CTM
    det = ctm[0] * ctm[3] - ctm[1] * ctm[2];
    if (fabs (det) < 0.000001) {
        error (
            errSyntaxError, getPos (),
            "Singular matrix in tiling pattern fill");
        return;
    }
    det = 1 / det;
    ictm[0] = ctm[3] * det;
    ictm[1] = -ctm[1] * det;
    ictm[2] = -ctm[2] * det;
    ictm[3] = ctm[0] * det;
    ictm[4] = (ctm[2] * ctm[5] - ctm[3] * ctm[4]) * det;
    ictm[5] = (ctm[1] * ctm[4] - ctm[0] * ctm[5]) * det;
    // m1 = PTM * BTM = PTM * base transform matrix
    m1[0] = ptm[0] * btm[0] + ptm[1] * btm[2];
    m1[1] = ptm[0] * btm[1] + ptm[1] * btm[3];
    m1[2] = ptm[2] * btm[0] + ptm[3] * btm[2];
    m1[3] = ptm[2] * btm[1] + ptm[3] * btm[3];
    m1[4] = ptm[4] * btm[0] + ptm[5] * btm[2] + btm[4];
    m1[5] = ptm[4] * btm[1] + ptm[5] * btm[3] + btm[5];
    // m = m1 * iCTM = (PTM * BTM) * (iCTM)
    m[0] = m1[0] * ictm[0] + m1[1] * ictm[2];
    m[1] = m1[0] * ictm[1] + m1[1] * ictm[3];
    m[2] = m1[2] * ictm[0] + m1[3] * ictm[2];
    m[3] = m1[2] * ictm[1] + m1[3] * ictm[3];
    m[4] = m1[4] * ictm[0] + m1[5] * ictm[2] + ictm[4];
    m[5] = m1[4] * ictm[1] + m1[5] * ictm[3] + ictm[5];

    // construct a (device space) -> (pattern space) transform matrix
    det = m1[0] * m1[3] - m1[1] * m1[2];
    if (fabs (det) < 0.000001) {
        error (
            errSyntaxError, getPos (),
            "Singular matrix in tiling pattern fill");
        return;
    }
    det = 1 / det;
    imb[0] = m1[3] * det;
    imb[1] = -m1[1] * det;
    imb[2] = -m1[2] * det;
    imb[3] = m1[0] * det;
    imb[4] = (m1[2] * m1[5] - m1[3] * m1[4]) * det;
    imb[5] = (m1[1] * m1[4] - m1[0] * m1[5]) * det;

    // save current graphics state
    savedState = saveStateStack ();

    // set underlying color space (for uncolored tiling patterns); set
    // various other parameters (stroke color, line width) to match
    // Adobe's behavior
    state->setFillPattern (NULL);
    state->setStrokePattern (NULL);
    if (tPat->getPaintType () == 2 && (cs = patCS->getUnder ())) {
        state->setFillColorSpace (cs->copy ());
        out->updateFillColorSpace (state);
        state->setStrokeColorSpace (cs->copy ());
        out->updateStrokeColorSpace (state);
        state->setStrokeColor (state->getFillColor ());
        out->updateFillColor (state);
        out->updateStrokeColor (state);
    }
    else {
        state->setFillColorSpace (GfxColorSpace::create (csDeviceGray));
        out->updateFillColorSpace (state);
        state->setStrokeColorSpace (GfxColorSpace::create (csDeviceGray));
        out->updateStrokeColorSpace (state);
    }
    if (!stroke) {
        state->setLineWidth (0);
        out->updateLineWidth (state);
    }

    // clip to current path
    if (stroke) {
        state->clipToStrokePath ();
        out->clipToStrokePath (state);
    }
    else if (!text) {
        state->clip ();
        if (eoFill) { out->eoClip (state); }
        else {
            out->clip (state);
        }
    }
    state->clearPath ();

    // get the clip region, check for empty
    state->getClipBBox (&cxMin, &cyMin, &cxMax, &cyMax);
    if (cxMin > cxMax || cyMin > cyMax) { goto err; }

    // transform clip region bbox to pattern space
    xMin = xMax = cxMin * imb[0] + cyMin * imb[2] + imb[4];
    yMin = yMax = cxMin * imb[1] + cyMin * imb[3] + imb[5];
    x1 = cxMin * imb[0] + cyMax * imb[2] + imb[4];
    y1 = cxMin * imb[1] + cyMax * imb[3] + imb[5];
    if (x1 < xMin) { xMin = x1; }
    else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) { yMin = y1; }
    else if (y1 > yMax) {
        yMax = y1;
    }
    x1 = cxMax * imb[0] + cyMin * imb[2] + imb[4];
    y1 = cxMax * imb[1] + cyMin * imb[3] + imb[5];
    if (x1 < xMin) { xMin = x1; }
    else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) { yMin = y1; }
    else if (y1 > yMax) {
        yMax = y1;
    }
    x1 = cxMax * imb[0] + cyMax * imb[2] + imb[4];
    y1 = cxMax * imb[1] + cyMax * imb[3] + imb[5];
    if (x1 < xMin) { xMin = x1; }
    else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) { yMin = y1; }
    else if (y1 > yMax) {
        yMax = y1;
    }

    // draw the pattern
    //~ this should treat negative steps differently -- start at right/top
    //~ edge instead of left/bottom (?)
    bbox[0] = tPat->getBBox ()[0];
    bbox[1] = tPat->getBBox ()[1];
    bbox[2] = tPat->getBBox ()[2];
    bbox[3] = tPat->getBBox ()[3];
    if (bbox[0] > bbox[2]) {
        t = bbox[0];
        bbox[0] = bbox[2];
        bbox[2] = t;
    }
    if (bbox[1] > bbox[3]) {
        t = bbox[1];
        bbox[1] = bbox[3];
        bbox[3] = t;
    }
    xstep = fabs (tPat->getXStep ());
    ystep = fabs (tPat->getYStep ());
    xi0 = (int)ceil ((xMin - bbox[2]) / xstep);
    xi1 = (int)floor ((xMax - bbox[0]) / xstep) + 1;
    yi0 = (int)ceil ((yMin - bbox[3]) / ystep);
    yi1 = (int)floor ((yMax - bbox[1]) / ystep) + 1;
    for (i = 0; i < 4; ++i) { m1[i] = m[i]; }
    if (out->useTilingPatternFill ()) {
        m1[4] = m[4];
        m1[5] = m[5];
        out->tilingPatternFill (
            state, this, tPat->getContentStreamRef (), tPat->getPaintType (),
            tPat->getResDict (), m1, bbox, xi0, yi0, xi1, yi1, xstep, ystep);
    }
    else {
        for (yi = yi0; yi < yi1; ++yi) {
            for (xi = xi0; xi < xi1; ++xi) {
                x = xi * xstep;
                y = yi * ystep;
                m1[4] = x * m[0] + y * m[2] + m[4];
                m1[5] = x * m[1] + y * m[3] + m[5];
                drawForm (
                    tPat->getContentStreamRef (), tPat->getResDict (), m1,
                    bbox);
            }
        }
    }

    // restore graphics state
err:
    restoreStateStack (savedState);
}

void Gfx::doShadingPatternFill (
    GfxShadingPattern* sPat, bool stroke, bool eoFill, bool text) {
    GfxShading* shading;
    GfxState* savedState;
    double *ctm, *btm, *ptm;
    double m[6], ictm[6], m1[6];
    double xMin, yMin, xMax, yMax;
    double det;

    shading = sPat->getShading ();

    // save current graphics state
    savedState = saveStateStack ();

    // clip to current path
    if (stroke) {
        state->clipToStrokePath ();
        out->clipToStrokePath (state);
    }
    else if (!text) {
        state->clip ();
        if (eoFill) { out->eoClip (state); }
        else {
            out->clip (state);
        }
    }
    state->clearPath ();

    // construct a (pattern space) -> (current space) transform matrix
    ctm = state->getCTM ();
    btm = baseMatrix;
    ptm = sPat->getMatrix ();
    // iCTM = invert CTM
    det = ctm[0] * ctm[3] - ctm[1] * ctm[2];
    if (fabs (det) < 0.000001) {
        error (
            errSyntaxError, getPos (),
            "Singular matrix in shading pattern fill");
        return;
    }
    det = 1 / det;
    ictm[0] = ctm[3] * det;
    ictm[1] = -ctm[1] * det;
    ictm[2] = -ctm[2] * det;
    ictm[3] = ctm[0] * det;
    ictm[4] = (ctm[2] * ctm[5] - ctm[3] * ctm[4]) * det;
    ictm[5] = (ctm[1] * ctm[4] - ctm[0] * ctm[5]) * det;
    // m1 = PTM * BTM = PTM * base transform matrix
    m1[0] = ptm[0] * btm[0] + ptm[1] * btm[2];
    m1[1] = ptm[0] * btm[1] + ptm[1] * btm[3];
    m1[2] = ptm[2] * btm[0] + ptm[3] * btm[2];
    m1[3] = ptm[2] * btm[1] + ptm[3] * btm[3];
    m1[4] = ptm[4] * btm[0] + ptm[5] * btm[2] + btm[4];
    m1[5] = ptm[4] * btm[1] + ptm[5] * btm[3] + btm[5];
    // m = m1 * iCTM = (PTM * BTM) * (iCTM)
    m[0] = m1[0] * ictm[0] + m1[1] * ictm[2];
    m[1] = m1[0] * ictm[1] + m1[1] * ictm[3];
    m[2] = m1[2] * ictm[0] + m1[3] * ictm[2];
    m[3] = m1[2] * ictm[1] + m1[3] * ictm[3];
    m[4] = m1[4] * ictm[0] + m1[5] * ictm[2] + ictm[4];
    m[5] = m1[4] * ictm[1] + m1[5] * ictm[3] + ictm[5];

    // set the new matrix
    state->concatCTM (m[0], m[1], m[2], m[3], m[4], m[5]);
    out->updateCTM (state, m[0], m[1], m[2], m[3], m[4], m[5]);

    // clip to bbox
    if (shading->getHasBBox ()) {
        shading->getBBox (&xMin, &yMin, &xMax, &yMax);
        state->moveTo (xMin, yMin);
        state->lineTo (xMax, yMin);
        state->lineTo (xMax, yMax);
        state->lineTo (xMin, yMax);
        state->closePath ();
        state->clip ();
        out->clip (state);
        state->clearPath ();
    }

    // set the color space
    state->setFillColorSpace (shading->getColorSpace ()->copy ());
    out->updateFillColorSpace (state);

    // background color fill
    if (shading->getHasBackground ()) {
        state->setFillColor (shading->getBackground ());
        out->updateFillColor (state);
        state->getUserClipBBox (&xMin, &yMin, &xMax, &yMax);
        state->moveTo (xMin, yMin);
        state->lineTo (xMax, yMin);
        state->lineTo (xMax, yMax);
        state->lineTo (xMin, yMax);
        state->closePath ();
        out->fill (state);
        state->clearPath ();
    }

#if 1 //~tmp: turn off anti-aliasing temporarily
    out->setInShading (true);
#endif

    // do shading type-specific operations
    switch (shading->getType ()) {
    case 1: doFunctionShFill ((GfxFunctionShading*)shading); break;
    case 2: doAxialShFill ((GfxAxialShading*)shading); break;
    case 3: doRadialShFill ((GfxRadialShading*)shading); break;
    case 4:
    case 5:
        doGouraudTriangleShFill ((GfxGouraudTriangleShading*)shading);
        break;
    case 6:
    case 7: doPatchMeshShFill ((GfxPatchMeshShading*)shading); break;
    }

#if 1 //~tmp: turn off anti-aliasing temporarily
    out->setInShading (false);
#endif

    // restore graphics state
    restoreStateStack (savedState);
}

void Gfx::opShFill (Object args[], int numArgs) {
    GfxShading* shading;
    GfxState* savedState;
    double xMin, yMin, xMax, yMax;

    if (!out->needNonText ()) { return; }

    if (!ocState) { return; }

    if (!(shading = res->lookupShading (args[0].as_name ()))) { return; }

    // save current graphics state
    savedState = saveStateStack ();

    // clip to bbox
    if (shading->getHasBBox ()) {
        shading->getBBox (&xMin, &yMin, &xMax, &yMax);
        state->moveTo (xMin, yMin);
        state->lineTo (xMax, yMin);
        state->lineTo (xMax, yMax);
        state->lineTo (xMin, yMax);
        state->closePath ();
        state->clip ();
        out->clip (state);
        state->clearPath ();
    }

    // set the color space
    state->setFillColorSpace (shading->getColorSpace ()->copy ());
    out->updateFillColorSpace (state);

#if 1 //~tmp: turn off anti-aliasing temporarily
    out->setInShading (true);
#endif

    // do shading type-specific operations
    switch (shading->getType ()) {
    case 1: doFunctionShFill ((GfxFunctionShading*)shading); break;
    case 2: doAxialShFill ((GfxAxialShading*)shading); break;
    case 3: doRadialShFill ((GfxRadialShading*)shading); break;
    case 4:
    case 5:
        doGouraudTriangleShFill ((GfxGouraudTriangleShading*)shading);
        break;
    case 6:
    case 7: doPatchMeshShFill ((GfxPatchMeshShading*)shading); break;
    }

#if 1 //~tmp: turn off anti-aliasing temporarily
    out->setInShading (false);
#endif

    // restore graphics state
    restoreStateStack (savedState);

    delete shading;
}

void Gfx::doFunctionShFill (GfxFunctionShading* shading) {
    double x0, y0, x1, y1;
    GfxColor colors[4];

    if (out->useShadedFills () && out->functionShadedFill (state, shading)) {
        return;
    }

    shading->getDomain (&x0, &y0, &x1, &y1);
    shading->getColor (x0, y0, &colors[0]);
    shading->getColor (x0, y1, &colors[1]);
    shading->getColor (x1, y0, &colors[2]);
    shading->getColor (x1, y1, &colors[3]);
    doFunctionShFill1 (shading, x0, y0, x1, y1, colors, 0);
}

void Gfx::doFunctionShFill1 (
    GfxFunctionShading* shading, double x0, double y0, double x1, double y1,
    GfxColor* colors, int depth) {
    GfxColor fillColor;
    GfxColor color0M, color1M, colorM0, colorM1, colorMM;
    GfxColor colors2[4];
    double* matrix;
    double xM, yM;
    int nComps, i, j;

    nComps = shading->getColorSpace ()->getNComps ();
    matrix = shading->getMatrix ();

    // compare the four corner colors
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < nComps; ++j) {
            if (abs (colors[i].c[j] - colors[(i + 1) & 3].c[j]) >
                functionColorDelta) {
                break;
            }
        }
        if (j < nComps) { break; }
    }

    // center of the rectangle
    xM = 0.5 * (x0 + x1);
    yM = 0.5 * (y0 + y1);

    // the four corner colors are close (or we hit the recursive limit)
    // -- fill the rectangle; but require at least one subdivision
    // (depth==0) to avoid problems when the four outer corners of the
    // shaded region are the same color
    if ((i == 4 && depth > 0) || depth == functionMaxDepth) {
        // use the center color
        shading->getColor (xM, yM, &fillColor);
        state->setFillColor (&fillColor);
        out->updateFillColor (state);

        // fill the rectangle
        state->moveTo (
            x0 * matrix[0] + y0 * matrix[2] + matrix[4],
            x0 * matrix[1] + y0 * matrix[3] + matrix[5]);
        state->lineTo (
            x1 * matrix[0] + y0 * matrix[2] + matrix[4],
            x1 * matrix[1] + y0 * matrix[3] + matrix[5]);
        state->lineTo (
            x1 * matrix[0] + y1 * matrix[2] + matrix[4],
            x1 * matrix[1] + y1 * matrix[3] + matrix[5]);
        state->lineTo (
            x0 * matrix[0] + y1 * matrix[2] + matrix[4],
            x0 * matrix[1] + y1 * matrix[3] + matrix[5]);
        state->closePath ();
        out->fill (state);
        state->clearPath ();

        // the four corner colors are not close enough -- subdivide the
        // rectangle
    }
    else {
        // colors[0]       colorM0       colors[2]
        //   (x0,y0)       (xM,y0)       (x1,y0)
        //         +----------+----------+
        //         |          |          |
        //         |    UL    |    UR    |
        // color0M |       colorMM       | color1M
        // (x0,yM) +----------+----------+ (x1,yM)
        //         |       (xM,yM)       |
        //         |    LL    |    LR    |
        //         |          |          |
        //         +----------+----------+
        // colors[1]       colorM1       colors[3]
        //   (x0,y1)       (xM,y1)       (x1,y1)

        shading->getColor (x0, yM, &color0M);
        shading->getColor (x1, yM, &color1M);
        shading->getColor (xM, y0, &colorM0);
        shading->getColor (xM, y1, &colorM1);
        shading->getColor (xM, yM, &colorMM);

        // upper-left sub-rectangle
        colors2[0] = colors[0];
        colors2[1] = color0M;
        colors2[2] = colorM0;
        colors2[3] = colorMM;
        doFunctionShFill1 (shading, x0, y0, xM, yM, colors2, depth + 1);

        // lower-left sub-rectangle
        colors2[0] = color0M;
        colors2[1] = colors[1];
        colors2[2] = colorMM;
        colors2[3] = colorM1;
        doFunctionShFill1 (shading, x0, yM, xM, y1, colors2, depth + 1);

        // upper-right sub-rectangle
        colors2[0] = colorM0;
        colors2[1] = colorMM;
        colors2[2] = colors[2];
        colors2[3] = color1M;
        doFunctionShFill1 (shading, xM, y0, x1, yM, colors2, depth + 1);

        // lower-right sub-rectangle
        colors2[0] = colorMM;
        colors2[1] = colorM1;
        colors2[2] = color1M;
        colors2[3] = colors[3];
        doFunctionShFill1 (shading, xM, yM, x1, y1, colors2, depth + 1);
    }
}

void Gfx::doAxialShFill (GfxAxialShading* shading) {
    double xMin, yMin, xMax, yMax;
    double x0, y0, x1, y1;
    double dx, dy, mul;
    bool dxdyZero, horiz;
    double tMin, tMax, t, tx, ty;
    double sMin, sMax, tmp;
    double ux0, uy0, ux1, uy1, vx0, vy0, vx1, vy1;
    double t0, t1, tt;
    double ta[axialMaxSplits + 1];
    int next[axialMaxSplits + 1];
    GfxColor color0, color1;
    int nComps;
    int i, j, k;

    if (out->useShadedFills () && out->axialShadedFill (state, shading)) {
        return;
    }

    // get the clip region bbox
    state->getUserClipBBox (&xMin, &yMin, &xMax, &yMax);

    // compute min and max t values, based on the four corners of the
    // clip region bbox
    shading->getCoords (&x0, &y0, &x1, &y1);
    dx = x1 - x0;
    dy = y1 - y0;
    dxdyZero = fabs (dx) < 0.01 && fabs (dy) < 0.01;
    horiz = fabs (dy) < fabs (dx);
    if (dxdyZero) { tMin = tMax = 0; }
    else {
        mul = 1 / (dx * dx + dy * dy);
        tMin = tMax = ((xMin - x0) * dx + (yMin - y0) * dy) * mul;
        t = ((xMin - x0) * dx + (yMax - y0) * dy) * mul;
        if (t < tMin) { tMin = t; }
        else if (t > tMax) {
            tMax = t;
        }
        t = ((xMax - x0) * dx + (yMin - y0) * dy) * mul;
        if (t < tMin) { tMin = t; }
        else if (t > tMax) {
            tMax = t;
        }
        t = ((xMax - x0) * dx + (yMax - y0) * dy) * mul;
        if (t < tMin) { tMin = t; }
        else if (t > tMax) {
            tMax = t;
        }
        if (tMin < 0 && !shading->getExtend0 ()) { tMin = 0; }
        if (tMax > 1 && !shading->getExtend1 ()) { tMax = 1; }
    }

    // get the function domain
    t0 = shading->getDomain0 ();
    t1 = shading->getDomain1 ();

    // Traverse the t axis and do the shading.
    //
    // For each point (tx, ty) on the t axis, consider a line through
    // that point perpendicular to the t axis:
    //
    //     x(s) = tx + s * -dy   -->   s = (x - tx) / -dy
    //     y(s) = ty + s * dx    -->   s = (y - ty) / dx
    //
    // Then look at the intersection of this line with the bounding box
    // (xMin, yMin, xMax, yMax).  For -1 < |dy/dx| < 1, look at the
    // intersection with yMin, yMax:
    //
    //     s0 = (yMin - ty) / dx
    //     s1 = (yMax - ty) / dx
    //
    // else look at the intersection with xMin, xMax:
    //
    //     s0 = (xMin - tx) / -dy
    //     s1 = (xMax - tx) / -dy
    //
    // Each filled polygon is bounded by two of these line segments
    // perpdendicular to the t axis.
    //
    // The t axis is bisected into smaller regions until the color
    // difference across a region is small enough, and then the region
    // is painted with a single color.

    // set up
    nComps = shading->getColorSpace ()->getNComps ();
    ta[0] = tMin;
    next[0] = axialMaxSplits;
    ta[axialMaxSplits] = tMax;

    // compute the color at t = tMin
    if (tMin < 0) { tt = t0; }
    else if (tMin > 1) {
        tt = t1;
    }
    else {
        tt = t0 + (t1 - t0) * tMin;
    }
    shading->getColor (tt, &color0);

    // compute the coordinates of the point on the t axis at t = tMin;
    // then compute the intersection of the perpendicular line with the
    // bounding box
    tx = x0 + tMin * dx;
    ty = y0 + tMin * dy;
    if (dxdyZero) { sMin = sMax = 0; }
    else {
        if (horiz) {
            sMin = (yMin - ty) / dx;
            sMax = (yMax - ty) / dx;
        }
        else {
            sMin = (xMin - tx) / -dy;
            sMax = (xMax - tx) / -dy;
        }
        if (sMin > sMax) {
            tmp = sMin;
            sMin = sMax;
            sMax = tmp;
        }
    }
    ux0 = tx - sMin * dy;
    uy0 = ty + sMin * dx;
    vx0 = tx - sMax * dy;
    vy0 = ty + sMax * dx;

    i = 0;
    while (i < axialMaxSplits) {
        // bisect until color difference is small enough or we hit the
        // bisection limit
        j = next[i];
        while (j > i + 1) {
            if (ta[j] < 0) { tt = t0; }
            else if (ta[j] > 1) {
                tt = t1;
            }
            else {
                tt = t0 + (t1 - t0) * ta[j];
            }
            // require at least two splits (to avoid problems where the
            // color doesn't change smoothly along the t axis)
            if (j - i <= axialMaxSplits / 4) {
                shading->getColor (tt, &color1);
                for (k = 0; k < nComps; ++k) {
                    if (abs (color1.c[k] - color0.c[k]) > axialColorDelta) {
                        break;
                    }
                }
                if (k == nComps) { break; }
            }
            k = (i + j) / 2;
            ta[k] = 0.5 * (ta[i] + ta[j]);
            next[i] = k;
            next[k] = j;
            j = k;
        }

        // use the average of the colors of the two sides of the region
        for (k = 0; k < nComps; ++k) {
            color0.c[k] = (color0.c[k] + color1.c[k]) / 2;
        }

        // compute the coordinates of the point on the t axis; then
        // compute the intersection of the perpendicular line with the
        // bounding box
        tx = x0 + ta[j] * dx;
        ty = y0 + ta[j] * dy;
        if (dxdyZero) { sMin = sMax = 0; }
        else {
            if (horiz) {
                sMin = (yMin - ty) / dx;
                sMax = (yMax - ty) / dx;
            }
            else {
                sMin = (xMin - tx) / -dy;
                sMax = (xMax - tx) / -dy;
            }
            if (sMin > sMax) {
                tmp = sMin;
                sMin = sMax;
                sMax = tmp;
            }
        }
        ux1 = tx - sMin * dy;
        uy1 = ty + sMin * dx;
        vx1 = tx - sMax * dy;
        vy1 = ty + sMax * dx;

        // set the color
        state->setFillColor (&color0);
        out->updateFillColor (state);

        // fill the region
        state->moveTo (ux0, uy0);
        state->lineTo (vx0, vy0);
        state->lineTo (vx1, vy1);
        state->lineTo (ux1, uy1);
        state->closePath ();
        out->fill (state);
        state->clearPath ();

        // set up for next region
        ux0 = ux1;
        uy0 = uy1;
        vx0 = vx1;
        vy0 = vy1;
        color0 = color1;
        i = next[i];
    }
}

void Gfx::doRadialShFill (GfxRadialShading* shading) {
    double xMin, yMin, xMax, yMax;
    double x0, y0, r0, x1, y1, r1, t0, t1;
    int nComps;
    GfxColor colorA, colorB;
    double xa, ya, xb, yb, ra, rb;
    double ta, tb, sa, sb;
    double sMin, sMax, h;
    double sLeft, sRight, sTop, sBottom, sZero, sDiag;
    bool haveSLeft, haveSRight, haveSTop, haveSBottom, haveSZero;
    bool haveSMin, haveSMax;
    bool enclosed;
    int ia, ib, k, n;
    double* ctm;
    double theta, alpha, angle, t;

    if (out->useShadedFills () && out->radialShadedFill (state, shading)) {
        return;
    }

    // get the shading info
    shading->getCoords (&x0, &y0, &r0, &x1, &y1, &r1);
    t0 = shading->getDomain0 ();
    t1 = shading->getDomain1 ();
    nComps = shading->getColorSpace ()->getNComps ();

    // Compute the point at which r(s) = 0; check for the enclosed
    // circles case; and compute the angles for the tangent lines.
    h = sqrt ((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    if (h == 0) {
        enclosed = true;
        theta = 0; // make gcc happy
    }
    else if (r1 - r0 == 0) {
        enclosed = false;
        theta = 0;
    }
    else if (fabs (r1 - r0) >= h - 0.0001) {
        enclosed = true;
        theta = 0; // make gcc happy
    }
    else {
        enclosed = false;
        theta = asin ((r1 - r0) / h);
    }
    if (enclosed) { alpha = 0; }
    else {
        alpha = atan2 (y1 - y0, x1 - x0);
    }

    // compute the (possibly extended) s range
    state->getUserClipBBox (&xMin, &yMin, &xMax, &yMax);
    if (enclosed) {
        sMin = 0;
        sMax = 1;
    }
    else {
        // solve x(sLeft) + r(sLeft) = xMin
        if ((haveSLeft = fabs ((x1 + r1) - (x0 + r0)) > 0.000001)) {
            sLeft = (xMin - (x0 + r0)) / ((x1 + r1) - (x0 + r0));
        }
        else {
            sLeft = 0; // make gcc happy
        }
        // solve x(sRight) - r(sRight) = xMax
        if ((haveSRight = fabs ((x1 - r1) - (x0 - r0)) > 0.000001)) {
            sRight = (xMax - (x0 - r0)) / ((x1 - r1) - (x0 - r0));
        }
        else {
            sRight = 0; // make gcc happy
        }
        // solve y(sBottom) + r(sBottom) = yMin
        if ((haveSBottom = fabs ((y1 + r1) - (y0 + r0)) > 0.000001)) {
            sBottom = (yMin - (y0 + r0)) / ((y1 + r1) - (y0 + r0));
        }
        else {
            sBottom = 0; // make gcc happy
        }
        // solve y(sTop) - r(sTop) = yMax
        if ((haveSTop = fabs ((y1 - r1) - (y0 - r0)) > 0.000001)) {
            sTop = (yMax - (y0 - r0)) / ((y1 - r1) - (y0 - r0));
        }
        else {
            sTop = 0; // make gcc happy
        }
        // solve r(sZero) = 0
        if ((haveSZero = fabs (r1 - r0) > 0.000001)) {
            sZero = -r0 / (r1 - r0);
        }
        else {
            sZero = 0; // make gcc happy
        }
        // solve r(sDiag) = sqrt((xMax-xMin)^2 + (yMax-yMin)^2)
        if (haveSZero) {
            sDiag = (sqrt (
                         (xMax - xMin) * (xMax - xMin) +
                         (yMax - yMin) * (yMax - yMin)) -
                     r0) /
                    (r1 - r0);
        }
        else {
            sDiag = 0; // make gcc happy
        }
        // compute sMin
        if (shading->getExtend0 ()) {
            sMin = 0;
            haveSMin = false;
            if (x0 < x1 && haveSLeft && sLeft < 0) {
                sMin = sLeft;
                haveSMin = true;
            }
            else if (x0 > x1 && haveSRight && sRight < 0) {
                sMin = sRight;
                haveSMin = true;
            }
            if (y0 < y1 && haveSBottom && sBottom < 0) {
                if (!haveSMin || sBottom > sMin) {
                    sMin = sBottom;
                    haveSMin = true;
                }
            }
            else if (y0 > y1 && haveSTop && sTop < 0) {
                if (!haveSMin || sTop > sMin) {
                    sMin = sTop;
                    haveSMin = true;
                }
            }
            if (haveSZero && sZero <= 0) {
                if (!haveSMin || sZero > sMin) { sMin = sZero; }
            }
        }
        else {
            sMin = 0;
        }
        // compute sMax
        if (shading->getExtend1 ()) {
            sMax = 1;
            haveSMax = false;
            if (x1 < x0 && haveSLeft && sLeft > 1) {
                sMax = sLeft;
                haveSMax = true;
            }
            else if (x1 > x0 && haveSRight && sRight > 1) {
                sMax = sRight;
                haveSMax = true;
            }
            if (y1 < y0 && haveSBottom && sBottom > 1) {
                if (!haveSMax || sBottom < sMax) {
                    sMax = sBottom;
                    haveSMax = true;
                }
            }
            else if (y1 > y0 && haveSTop && sTop > 1) {
                if (!haveSMax || sTop < sMax) {
                    sMax = sTop;
                    haveSMax = true;
                }
            }
            if (haveSZero && sDiag > 1) {
                if (!haveSMax || sDiag < sMax) { sMax = sDiag; }
            }
        }
        else {
            sMax = 1;
        }
    }

    // compute the number of steps into which circles must be divided to
    // achieve a curve flatness of 0.1 pixel in device space for the
    // largest circle (note that "device space" is 72 dpi when generating
    // PostScript, hence the relatively small 0.1 pixel accuracy)
    ctm = state->getCTM ();
    t = fabs (ctm[0]);
    if (fabs (ctm[1]) > t) { t = fabs (ctm[1]); }
    if (fabs (ctm[2]) > t) { t = fabs (ctm[2]); }
    if (fabs (ctm[3]) > t) { t = fabs (ctm[3]); }
    if (r0 > r1) { t *= r0; }
    else {
        t *= r1;
    }
    if (t < 1) { n = 3; }
    else {
        n = (int)(M_PI / acos (1 - 0.1 / t));
        if (n < 3) { n = 3; }
        else if (n > 200) {
            n = 200;
        }
    }

    // setup for the start circle
    ia = 0;
    sa = sMin;
    ta = t0 + sa * (t1 - t0);
    xa = x0 + sa * (x1 - x0);
    ya = y0 + sa * (y1 - y0);
    ra = r0 + sa * (r1 - r0);
    if (ta < t0) { shading->getColor (t0, &colorA); }
    else if (ta > t1) {
        shading->getColor (t1, &colorA);
    }
    else {
        shading->getColor (ta, &colorA);
    }

    // fill the circles
    while (ia < radialMaxSplits) {
        // go as far along the t axis (toward t1) as we can, such that the
        // color difference is within the tolerance (radialColorDelta) --
        // this uses bisection (between the current value, t, and t1),
        // limited to radialMaxSplits points along the t axis; require at
        // least one split to avoid problems when the innermost and
        // outermost colors are the same
        ib = radialMaxSplits;
        sb = sMax;
        tb = t0 + sb * (t1 - t0);
        if (tb < t0) { shading->getColor (t0, &colorB); }
        else if (tb > t1) {
            shading->getColor (t1, &colorB);
        }
        else {
            shading->getColor (tb, &colorB);
        }
        while (ib - ia > 1) {
            for (k = 0; k < nComps; ++k) {
                if (abs (colorB.c[k] - colorA.c[k]) > radialColorDelta) {
                    break;
                }
            }
            if (k == nComps && ib < radialMaxSplits) { break; }
            ib = (ia + ib) / 2;
            sb = sMin + ((double)ib / (double)radialMaxSplits) * (sMax - sMin);
            tb = t0 + sb * (t1 - t0);
            if (tb < t0) { shading->getColor (t0, &colorB); }
            else if (tb > t1) {
                shading->getColor (t1, &colorB);
            }
            else {
                shading->getColor (tb, &colorB);
            }
        }

        // compute center and radius of the circle
        xb = x0 + sb * (x1 - x0);
        yb = y0 + sb * (y1 - y0);
        rb = r0 + sb * (r1 - r0);

        // use the average of the colors at the two circles
        for (k = 0; k < nComps; ++k) {
            colorA.c[k] = (colorA.c[k] + colorB.c[k]) / 2;
        }
        state->setFillColor (&colorA);
        out->updateFillColor (state);

        if (enclosed) {
            // construct path for first circle (counterclockwise)
            state->moveTo (xa + ra, ya);
            for (k = 1; k < n; ++k) {
                angle = ((double)k / (double)n) * 2 * M_PI;
                state->lineTo (xa + ra * cos (angle), ya + ra * sin (angle));
            }
            state->closePath ();

            // construct and append path for second circle (clockwise)
            state->moveTo (xb + rb, yb);
            for (k = 1; k < n; ++k) {
                angle = -((double)k / (double)n) * 2 * M_PI;
                state->lineTo (xb + rb * cos (angle), yb + rb * sin (angle));
            }
            state->closePath ();
        }
        else {
            // construct the first subpath (clockwise)
            state->moveTo (
                xa + ra * cos (alpha + theta + 0.5 * M_PI),
                ya + ra * sin (alpha + theta + 0.5 * M_PI));
            for (k = 0; k < n; ++k) {
                angle = alpha + theta + 0.5 * M_PI -
                        ((double)k / (double)n) * (2 * theta + M_PI);
                state->lineTo (xb + rb * cos (angle), yb + rb * sin (angle));
            }
            for (k = 0; k < n; ++k) {
                angle = alpha - theta - 0.5 * M_PI +
                        ((double)k / (double)n) * (2 * theta - M_PI);
                state->lineTo (xa + ra * cos (angle), ya + ra * sin (angle));
            }
            state->closePath ();

            // construct the second subpath (counterclockwise)
            state->moveTo (
                xa + ra * cos (alpha + theta + 0.5 * M_PI),
                ya + ra * sin (alpha + theta + 0.5 * M_PI));
            for (k = 0; k < n; ++k) {
                angle = alpha + theta + 0.5 * M_PI +
                        ((double)k / (double)n) * (-2 * theta + M_PI);
                state->lineTo (xb + rb * cos (angle), yb + rb * sin (angle));
            }
            for (k = 0; k < n; ++k) {
                angle = alpha - theta - 0.5 * M_PI +
                        ((double)k / (double)n) * (2 * theta + M_PI);
                state->lineTo (xa + ra * cos (angle), ya + ra * sin (angle));
            }
            state->closePath ();
        }

        // fill the path
        out->fill (state);
        state->clearPath ();

        // step to the next value of t
        ia = ib;
        sa = sb;
        ta = tb;
        xa = xb;
        ya = yb;
        ra = rb;
        colorA = colorB;
    }

    if (enclosed) {
        // extend the smaller circle
        if ((shading->getExtend0 () && r0 <= r1) ||
            (shading->getExtend1 () && r1 < r0)) {
            if (r0 <= r1) {
                ta = t0;
                ra = r0;
                xa = x0;
                ya = y0;
            }
            else {
                ta = t1;
                ra = r1;
                xa = x1;
                ya = y1;
            }
            shading->getColor (ta, &colorA);
            state->setFillColor (&colorA);
            out->updateFillColor (state);
            state->moveTo (xa + ra, ya);
            for (k = 1; k < n; ++k) {
                angle = ((double)k / (double)n) * 2 * M_PI;
                state->lineTo (xa + ra * cos (angle), ya + ra * sin (angle));
            }
            state->closePath ();
            out->fill (state);
            state->clearPath ();
        }

        // extend the larger circle
        if ((shading->getExtend0 () && r0 > r1) ||
            (shading->getExtend1 () && r1 >= r0)) {
            if (r0 > r1) {
                ta = t0;
                ra = r0;
                xa = x0;
                ya = y0;
            }
            else {
                ta = t1;
                ra = r1;
                xa = x1;
                ya = y1;
            }
            shading->getColor (ta, &colorA);
            state->setFillColor (&colorA);
            out->updateFillColor (state);
            state->moveTo (xMin, yMin);
            state->lineTo (xMin, yMax);
            state->lineTo (xMax, yMax);
            state->lineTo (xMax, yMin);
            state->closePath ();
            state->moveTo (xa + ra, ya);
            for (k = 1; k < n; ++k) {
                angle = ((double)k / (double)n) * 2 * M_PI;
                state->lineTo (xa + ra * cos (angle), ya + ra * sin (angle));
            }
            state->closePath ();
            out->fill (state);
            state->clearPath ();
        }
    }
}

void Gfx::doGouraudTriangleShFill (GfxGouraudTriangleShading* shading) {
    double x0, y0, x1, y1, x2, y2;
    double color0[gfxColorMaxComps];
    double color1[gfxColorMaxComps];
    double color2[gfxColorMaxComps];
    int i;

    for (i = 0; i < shading->getNTriangles (); ++i) {
        shading->getTriangle (
            i, &x0, &y0, color0, &x1, &y1, color1, &x2, &y2, color2);
        gouraudFillTriangle (
            x0, y0, color0, x1, y1, color1, x2, y2, color2, shading, 0);
    }
}

void Gfx::gouraudFillTriangle (
    double x0, double y0, double* color0, double x1, double y1, double* color1,
    double x2, double y2, double* color2, GfxGouraudTriangleShading* shading,
    int depth) {
    double dx0, dy0, dx1, dy1, dx2, dy2;
    double x01, y01, x12, y12, x20, y20;
    double color01[gfxColorMaxComps];
    double color12[gfxColorMaxComps];
    double color20[gfxColorMaxComps];
    GfxColor c0, c1, c2;
    int nComps, i;

    // recursion ends when:
    // (1) color difference is smaller than gouraudColorDelta; or
    // (2) triangles are smaller than 0.5 pixel (note that "device
    //     space" is 72dpi when generating PostScript); or
    // (3) max recursion depth (gouraudMaxDepth) is hit.
    nComps = shading->getColorSpace ()->getNComps ();
    shading->getColor (color0, color0 + nComps, &c0);
    shading->getColor (color1, color1 + nComps, &c1);
    shading->getColor (color2, color2 + nComps, &c2);
    for (i = 0; i < nComps; ++i) {
        if (abs (c0.c[i] - c1.c[i]) > gouraudColorDelta ||
            abs (c1.c[i] - c2.c[i]) > gouraudColorDelta) {
            break;
        }
    }
    state->transformDelta (x1 - x0, y1 - y0, &dx0, &dy0);
    state->transformDelta (x2 - x1, y2 - y1, &dx1, &dy1);
    state->transformDelta (x0 - x2, y0 - y2, &dx2, &dy2);
    if (i == nComps || depth == gouraudMaxDepth ||
        (fabs (dx0) < 0.5 && fabs (dy0) < 0.5 && fabs (dx1) < 0.5 &&
         fabs (dy1) < 0.5 && fabs (dx2) < 0.5 && fabs (dy2) < 0.5)) {
        state->setFillColor (&c0);
        out->updateFillColor (state);
        state->moveTo (x0, y0);
        state->lineTo (x1, y1);
        state->lineTo (x2, y2);
        state->closePath ();
        out->fill (state);
        state->clearPath ();
    }
    else {
        x01 = 0.5 * (x0 + x1);
        y01 = 0.5 * (y0 + y1);
        x12 = 0.5 * (x1 + x2);
        y12 = 0.5 * (y1 + y2);
        x20 = 0.5 * (x2 + x0);
        y20 = 0.5 * (y2 + y0);
        for (i = 0; i < shading->getNComps (); ++i) {
            color01[i] = 0.5 * (color0[i] + color1[i]);
            color12[i] = 0.5 * (color1[i] + color2[i]);
            color20[i] = 0.5 * (color2[i] + color0[i]);
        }
        gouraudFillTriangle (
            x0, y0, color0, x01, y01, color01, x20, y20, color20, shading,
            depth + 1);
        gouraudFillTriangle (
            x01, y01, color01, x1, y1, color1, x12, y12, color12, shading,
            depth + 1);
        gouraudFillTriangle (
            x01, y01, color01, x12, y12, color12, x20, y20, color20, shading,
            depth + 1);
        gouraudFillTriangle (
            x20, y20, color20, x12, y12, color12, x2, y2, color2, shading,
            depth + 1);
    }
}

void Gfx::doPatchMeshShFill (GfxPatchMeshShading* shading) {
    int start, i;

    if (shading->getNPatches () > 128) { start = 3; }
    else if (shading->getNPatches () > 64) {
        start = 2;
    }
    else if (shading->getNPatches () > 16) {
        start = 1;
    }
    else {
        start = 0;
    }
    for (i = 0; i < shading->getNPatches (); ++i) {
        fillPatch (shading->getPatch (i), shading, start);
    }
}

void Gfx::fillPatch (GfxPatch* patch, GfxPatchMeshShading* shading, int depth) {
    GfxPatch patch00, patch01, patch10, patch11;
    GfxColor c00, c01, c10, c11;
    double xx[4][8], yy[4][8];
    double xxm, yym;
    int nComps, i;

    nComps = shading->getColorSpace ()->getNComps ();
    shading->getColor (patch->color[0][0], patch->color[0][0] + nComps, &c00);
    shading->getColor (patch->color[0][1], patch->color[0][1] + nComps, &c01);
    shading->getColor (patch->color[1][0], patch->color[1][0] + nComps, &c10);
    shading->getColor (patch->color[1][1], patch->color[1][1] + nComps, &c11);
    for (i = 0; i < nComps; ++i) {
        if (abs (c00.c[i] - c01.c[i]) > patchColorDelta ||
            abs (c01.c[i] - c11.c[i]) > patchColorDelta ||
            abs (c11.c[i] - c10.c[i]) > patchColorDelta ||
            abs (c10.c[i] - c00.c[i]) > patchColorDelta) {
            break;
        }
    }
    if (i == nComps || depth == patchMaxDepth) {
        state->setFillColor (&c00);
        out->updateFillColor (state);
        state->moveTo (patch->x[0][0], patch->y[0][0]);
        state->curveTo (
            patch->x[0][1], patch->y[0][1], patch->x[0][2], patch->y[0][2],
            patch->x[0][3], patch->y[0][3]);
        state->curveTo (
            patch->x[1][3], patch->y[1][3], patch->x[2][3], patch->y[2][3],
            patch->x[3][3], patch->y[3][3]);
        state->curveTo (
            patch->x[3][2], patch->y[3][2], patch->x[3][1], patch->y[3][1],
            patch->x[3][0], patch->y[3][0]);
        state->curveTo (
            patch->x[2][0], patch->y[2][0], patch->x[1][0], patch->y[1][0],
            patch->x[0][0], patch->y[0][0]);
        state->closePath ();
        out->fill (state);
        state->clearPath ();
    }
    else {
        for (i = 0; i < 4; ++i) {
            xx[i][0] = patch->x[i][0];
            yy[i][0] = patch->y[i][0];
            xx[i][1] = 0.5 * (patch->x[i][0] + patch->x[i][1]);
            yy[i][1] = 0.5 * (patch->y[i][0] + patch->y[i][1]);
            xxm = 0.5 * (patch->x[i][1] + patch->x[i][2]);
            yym = 0.5 * (patch->y[i][1] + patch->y[i][2]);
            xx[i][6] = 0.5 * (patch->x[i][2] + patch->x[i][3]);
            yy[i][6] = 0.5 * (patch->y[i][2] + patch->y[i][3]);
            xx[i][2] = 0.5 * (xx[i][1] + xxm);
            yy[i][2] = 0.5 * (yy[i][1] + yym);
            xx[i][5] = 0.5 * (xxm + xx[i][6]);
            yy[i][5] = 0.5 * (yym + yy[i][6]);
            xx[i][3] = xx[i][4] = 0.5 * (xx[i][2] + xx[i][5]);
            yy[i][3] = yy[i][4] = 0.5 * (yy[i][2] + yy[i][5]);
            xx[i][7] = patch->x[i][3];
            yy[i][7] = patch->y[i][3];
        }
        for (i = 0; i < 4; ++i) {
            patch00.x[0][i] = xx[0][i];
            patch00.y[0][i] = yy[0][i];
            patch00.x[1][i] = 0.5 * (xx[0][i] + xx[1][i]);
            patch00.y[1][i] = 0.5 * (yy[0][i] + yy[1][i]);
            xxm = 0.5 * (xx[1][i] + xx[2][i]);
            yym = 0.5 * (yy[1][i] + yy[2][i]);
            patch10.x[2][i] = 0.5 * (xx[2][i] + xx[3][i]);
            patch10.y[2][i] = 0.5 * (yy[2][i] + yy[3][i]);
            patch00.x[2][i] = 0.5 * (patch00.x[1][i] + xxm);
            patch00.y[2][i] = 0.5 * (patch00.y[1][i] + yym);
            patch10.x[1][i] = 0.5 * (xxm + patch10.x[2][i]);
            patch10.y[1][i] = 0.5 * (yym + patch10.y[2][i]);
            patch00.x[3][i] = 0.5 * (patch00.x[2][i] + patch10.x[1][i]);
            patch00.y[3][i] = 0.5 * (patch00.y[2][i] + patch10.y[1][i]);
            patch10.x[0][i] = patch00.x[3][i];
            patch10.y[0][i] = patch00.y[3][i];
            patch10.x[3][i] = xx[3][i];
            patch10.y[3][i] = yy[3][i];
        }
        for (i = 4; i < 8; ++i) {
            patch01.x[0][i - 4] = xx[0][i];
            patch01.y[0][i - 4] = yy[0][i];
            patch01.x[1][i - 4] = 0.5 * (xx[0][i] + xx[1][i]);
            patch01.y[1][i - 4] = 0.5 * (yy[0][i] + yy[1][i]);
            xxm = 0.5 * (xx[1][i] + xx[2][i]);
            yym = 0.5 * (yy[1][i] + yy[2][i]);
            patch11.x[2][i - 4] = 0.5 * (xx[2][i] + xx[3][i]);
            patch11.y[2][i - 4] = 0.5 * (yy[2][i] + yy[3][i]);
            patch01.x[2][i - 4] = 0.5 * (patch01.x[1][i - 4] + xxm);
            patch01.y[2][i - 4] = 0.5 * (patch01.y[1][i - 4] + yym);
            patch11.x[1][i - 4] = 0.5 * (xxm + patch11.x[2][i - 4]);
            patch11.y[1][i - 4] = 0.5 * (yym + patch11.y[2][i - 4]);
            patch01.x[3][i - 4] =
                0.5 * (patch01.x[2][i - 4] + patch11.x[1][i - 4]);
            patch01.y[3][i - 4] =
                0.5 * (patch01.y[2][i - 4] + patch11.y[1][i - 4]);
            patch11.x[0][i - 4] = patch01.x[3][i - 4];
            patch11.y[0][i - 4] = patch01.y[3][i - 4];
            patch11.x[3][i - 4] = xx[3][i];
            patch11.y[3][i - 4] = yy[3][i];
        }
        for (i = 0; i < shading->getNComps (); ++i) {
            patch00.color[0][0][i] = patch->color[0][0][i];
            patch00.color[0][1][i] =
                0.5 * (patch->color[0][0][i] + patch->color[0][1][i]);
            patch01.color[0][0][i] = patch00.color[0][1][i];
            patch01.color[0][1][i] = patch->color[0][1][i];
            patch01.color[1][1][i] =
                0.5 * (patch->color[0][1][i] + patch->color[1][1][i]);
            patch11.color[0][1][i] = patch01.color[1][1][i];
            patch11.color[1][1][i] = patch->color[1][1][i];
            patch11.color[1][0][i] =
                0.5 * (patch->color[1][1][i] + patch->color[1][0][i]);
            patch10.color[1][1][i] = patch11.color[1][0][i];
            patch10.color[1][0][i] = patch->color[1][0][i];
            patch10.color[0][0][i] =
                0.5 * (patch->color[1][0][i] + patch->color[0][0][i]);
            patch00.color[1][0][i] = patch10.color[0][0][i];
            patch00.color[1][1][i] =
                0.5 * (patch00.color[1][0][i] + patch01.color[1][1][i]);
            patch01.color[1][0][i] = patch00.color[1][1][i];
            patch11.color[0][0][i] = patch00.color[1][1][i];
            patch10.color[0][1][i] = patch00.color[1][1][i];
        }
        fillPatch (&patch00, shading, depth + 1);
        fillPatch (&patch10, shading, depth + 1);
        fillPatch (&patch01, shading, depth + 1);
        fillPatch (&patch11, shading, depth + 1);
    }
}

void Gfx::doEndPath () {
    if (state->isCurPt () && clip != clipNone) {
        state->clip ();
        if (clip == clipNormal) { out->clip (state); }
        else {
            out->eoClip (state);
        }
    }
    clip = clipNone;
    state->clearPath ();
}

//------------------------------------------------------------------------
// path clipping operators
//------------------------------------------------------------------------

void Gfx::opClip (Object args[], int numArgs) { clip = clipNormal; }

void Gfx::opEOClip (Object args[], int numArgs) { clip = clipEO; }

//------------------------------------------------------------------------
// text object operators
//------------------------------------------------------------------------

void Gfx::opBeginText (Object args[], int numArgs) {
    state->setTextMat (1, 0, 0, 1, 0, 0);
    state->textMoveTo (0, 0);
    out->updateTextMat (state);
    out->updateTextPos (state);
    fontChanged = true;
    textClipBBoxEmpty = true;
}

void Gfx::opEndText (Object args[], int numArgs) { out->endTextObject (state); }

//------------------------------------------------------------------------
// text state operators
//------------------------------------------------------------------------

void Gfx::opSetCharSpacing (Object args[], int numArgs) {
    state->setCharSpace (args[0].as_num ());
    out->updateCharSpace (state);
}

void Gfx::opSetFont (Object args[], int numArgs) {
    doSetFont (res->lookupFont (args[0].as_name ()), args[1].as_num ());
}

void Gfx::doSetFont (GfxFont* font, double size) {
    if (!font) {
        state->setFont (NULL, 0);
        return;
    }
    if (printCommands) {
        printf (
            "  font: tag=%s name='%s' %g\n", font->getTag ()->c_str (),
            font->as_name () ? font->as_name ()->c_str () : "???", size);
        fflush (stdout);
    }
    state->setFont (font, size);
    fontChanged = true;
}

void Gfx::opSetTextLeading (Object args[], int numArgs) {
    state->setLeading (args[0].as_num ());
}

void Gfx::opSetTextRender (Object args[], int numArgs) {
    state->setRender (args[0].as_int ());
    out->updateRender (state);
}

void Gfx::opSetTextRise (Object args[], int numArgs) {
    state->setRise (args[0].as_num ());
    out->updateRise (state);
}

void Gfx::opSetWordSpacing (Object args[], int numArgs) {
    state->setWordSpace (args[0].as_num ());
    out->updateWordSpace (state);
}

void Gfx::opSetHorizScaling (Object args[], int numArgs) {
    state->setHorizScaling (args[0].as_num ());
    out->updateHorizScaling (state);
    fontChanged = true;
}

//------------------------------------------------------------------------
// text positioning operators
//------------------------------------------------------------------------

void Gfx::opTextMove (Object args[], int numArgs) {
    double tx, ty;

    tx = state->getLineX () + args[0].as_num ();
    ty = state->getLineY () + args[1].as_num ();
    state->textMoveTo (tx, ty);
    out->updateTextPos (state);
}

void Gfx::opTextMoveSet (Object args[], int numArgs) {
    double tx, ty;

    tx = state->getLineX () + args[0].as_num ();
    ty = args[1].as_num ();
    state->setLeading (-ty);
    ty += state->getLineY ();
    state->textMoveTo (tx, ty);
    out->updateTextPos (state);
}

void Gfx::opSetTextMatrix (Object args[], int numArgs) {
    state->setTextMat (
        args[0].as_num (), args[1].as_num (), args[2].as_num (),
        args[3].as_num (), args[4].as_num (), args[5].as_num ());
    state->textMoveTo (0, 0);
    out->updateTextMat (state);
    out->updateTextPos (state);
    fontChanged = true;
}

void Gfx::opTextNextLine (Object args[], int numArgs) {
    double tx, ty;

    tx = state->getLineX ();
    ty = state->getLineY () - state->getLeading ();
    state->textMoveTo (tx, ty);
    out->updateTextPos (state);
}

//------------------------------------------------------------------------
// text string operators
//------------------------------------------------------------------------

void Gfx::opShowText (Object args[], int numArgs) {
    if (!state->getFont ()) {
        error (errSyntaxError, getPos (), "No font in show");
        return;
    }
    if (fontChanged) {
        out->updateFont (state);
        fontChanged = false;
    }
    if (ocState) {
        out->beginStringOp (state);
        doShowText (args[0].as_string ());
        out->endStringOp (state);
    }
    else {
        doIncCharCount (args[0].as_string ());
    }
}

void Gfx::opMoveShowText (Object args[], int numArgs) {
    double tx, ty;

    if (!state->getFont ()) {
        error (errSyntaxError, getPos (), "No font in move/show");
        return;
    }
    if (fontChanged) {
        out->updateFont (state);
        fontChanged = false;
    }
    tx = state->getLineX ();
    ty = state->getLineY () - state->getLeading ();
    state->textMoveTo (tx, ty);
    out->updateTextPos (state);
    if (ocState) {
        out->beginStringOp (state);
        doShowText (args[0].as_string ());
        out->endStringOp (state);
    }
    else {
        doIncCharCount (args[0].as_string ());
    }
}

void Gfx::opMoveSetShowText (Object args[], int numArgs) {
    double tx, ty;

    if (!state->getFont ()) {
        error (errSyntaxError, getPos (), "No font in move/set/show");
        return;
    }
    if (fontChanged) {
        out->updateFont (state);
        fontChanged = false;
    }
    state->setWordSpace (args[0].as_num ());
    state->setCharSpace (args[1].as_num ());
    tx = state->getLineX ();
    ty = state->getLineY () - state->getLeading ();
    state->textMoveTo (tx, ty);
    out->updateWordSpace (state);
    out->updateCharSpace (state);
    out->updateTextPos (state);
    if (ocState) {
        out->beginStringOp (state);
        doShowText (args[2].as_string ());
        out->endStringOp (state);
    }
    else {
        doIncCharCount (args[2].as_string ());
    }
}

void Gfx::opShowSpaceText (Object args[], int numArgs) {
    Object obj;
    int wMode;
    int i;

    if (!state->getFont ()) {
        error (errSyntaxError, getPos (), "No font in show/space");
        return;
    }
    if (fontChanged) {
        out->updateFont (state);
        fontChanged = false;
    }
    if (ocState) {
        out->beginStringOp (state);
        wMode = state->getFont ()->getWMode ();
        Array& a = args[0].as_array ();
        for (i = 0; i < a.size (); ++i) {
            obj = resolve (a [i]);
            if (obj.is_num ()) {
                if (wMode) {
                    state->textShift (
                        0, -obj.as_num () * 0.001 * state->getFontSize ());
                }
                else {
                    state->textShift (
                        -obj.as_num () * 0.001 * state->getFontSize () *
                            state->getHorizScaling (),
                        0);
                }
                out->updateTextShift (state, obj.as_num ());
            }
            else if (obj.is_string ()) {
                doShowText (obj.as_string ());
            }
            else {
                error (
                    errSyntaxError, getPos (),
                    "Element of show/space array must be number or string");
            }
        }
        out->endStringOp (state);
    }
    else {
        Array& a = args[0].as_array ();
        for (i = 0; i < a.size (); ++i) {
            obj = resolve (a [i]);
            if (obj.is_string ()) { doIncCharCount (obj.as_string ()); }
        }
    }
}

void Gfx::doShowText (GString* s) {
    GfxFont* font;
    int wMode;
    double riseX, riseY;
    CharCode code;
    Unicode u[8];
    double x, y, dx, dy, dx2, dy2, curX, curY, tdx, tdy, ddx, ddy;
    double originX, originY, tOriginX, tOriginY;
    double x0, y0, x1, y1;
    double oldCTM[6], newCTM[6];
    double* mat;
    Object charProcRef, charProc;
    Dict* resDict;
    Parser* oldParser;
    GfxState* savedState;
    int render;
    bool patternFill;
    int len, n, uLen, nChars, nSpaces, i;

    font = state->getFont ();
    wMode = font->getWMode ();

    if (out->useDrawChar ()) { out->beginString (state, s); }

    // if we're doing a pattern fill, set up clipping
    render = state->getRender ();
    if (!(render & 1) && state->getFillColorSpace ()->getMode () == csPattern) {
        patternFill = true;
        saveState ();
        // disable fill, enable clipping, leave stroke unchanged
        if ((render ^ (render >> 1)) & 1) { render = 5; }
        else {
            render = 7;
        }
        state->setRender (render);
        out->updateRender (state);
    }
    else {
        patternFill = false;
    }

    state->textTransformDelta (0, state->getRise (), &riseX, &riseY);
    x0 = state->getCurX () + riseX;
    y0 = state->getCurY () + riseY;

    // handle a Type 3 char
    if (font->getType () == fontType3 && out->interpretType3Chars ()) {
        mat = state->getCTM ();
        for (i = 0; i < 6; ++i) { oldCTM[i] = mat[i]; }
        mat = state->getTextMat ();
        newCTM[0] = mat[0] * oldCTM[0] + mat[1] * oldCTM[2];
        newCTM[1] = mat[0] * oldCTM[1] + mat[1] * oldCTM[3];
        newCTM[2] = mat[2] * oldCTM[0] + mat[3] * oldCTM[2];
        newCTM[3] = mat[2] * oldCTM[1] + mat[3] * oldCTM[3];
        mat = font->getFontMatrix ();
        newCTM[0] = mat[0] * newCTM[0] + mat[1] * newCTM[2];
        newCTM[1] = mat[0] * newCTM[1] + mat[1] * newCTM[3];
        newCTM[2] = mat[2] * newCTM[0] + mat[3] * newCTM[2];
        newCTM[3] = mat[2] * newCTM[1] + mat[3] * newCTM[3];
        newCTM[0] *= state->getFontSize ();
        newCTM[1] *= state->getFontSize ();
        newCTM[2] *= state->getFontSize ();
        newCTM[3] *= state->getFontSize ();
        newCTM[0] *= state->getHorizScaling ();
        newCTM[2] *= state->getHorizScaling ();
        curX = state->getCurX ();
        curY = state->getCurY ();
        oldParser = parser;
        const char* p = s->c_str ();
        len = s->getLength ();
        while (len > 0) {
            n = font->getNextChar (
                p, len, &code, u, (int)(sizeof (u) / sizeof (Unicode)), &uLen,
                &dx, &dy, &originX, &originY);
            dx = dx * state->getFontSize () + state->getCharSpace ();
            if (n == 1 && *p == ' ') { dx += state->getWordSpace (); }
            dx *= state->getHorizScaling ();
            dy *= state->getFontSize ();
            state->textTransformDelta (dx, dy, &tdx, &tdy);
            state->transform (curX + riseX, curY + riseY, &x, &y);
            savedState = saveStateStack ();
            state->setCTM (newCTM[0], newCTM[1], newCTM[2], newCTM[3], x, y);
            //~ the CTM concat values here are wrong (but never used)
            out->updateCTM (state, 1, 0, 0, 1, 0, 0);
            state->transformDelta (dx, dy, &ddx, &ddy);
            if (!out->beginType3Char (
                    state, curX + riseX, curY + riseY, ddx, ddy, code, u,
                    uLen)) {
                ((Gfx8BitFont*)font)->getCharProcNF (code, &charProcRef);
                charProc = resolve (charProcRef);
                if ((resDict = ((Gfx8BitFont*)font)->getResources ())) {
                    pushResources (resDict);
                }
                if (charProc.is_stream ()) { display (&charProcRef, false); }
                else {
                    error (
                        errSyntaxError, getPos (),
                        "Missing or bad Type3 CharProc entry");
                }
                out->endType3Char (state);
                if (resDict) { popResources (); }
            }
            restoreStateStack (savedState);
            curX += tdx;
            curY += tdy;
            state->moveTo (curX, curY);
            p += n;
            len -= n;
        }
        parser = oldParser;
    }
    else if (out->useDrawChar ()) {
        const char* p = s->c_str ();
        len = s->getLength ();
        while (len > 0) {
            n = font->getNextChar (
                p, len, &code, u, (int)(sizeof (u) / sizeof (Unicode)), &uLen,
                &dx, &dy, &originX, &originY);
            if (wMode) {
                dx *= state->getFontSize ();
                dy = dy * state->getFontSize () + state->getCharSpace ();
                if (n == 1 && *p == ' ') { dy += state->getWordSpace (); }
            }
            else {
                dx = dx * state->getFontSize () + state->getCharSpace ();
                if (n == 1 && *p == ' ') { dx += state->getWordSpace (); }
                dx *= state->getHorizScaling ();
                dy *= state->getFontSize ();
            }
            state->textTransformDelta (dx, dy, &tdx, &tdy);
            originX *= state->getFontSize ();
            originY *= state->getFontSize ();
            state->textTransformDelta (originX, originY, &tOriginX, &tOriginY);
            out->drawChar (
                state, state->getCurX () + riseX, state->getCurY () + riseY,
                tdx, tdy, tOriginX, tOriginY, code, n, u, uLen);
            state->shift (tdx, tdy);
            p += n;
            len -= n;
        }
    }
    else {
        dx = dy = 0;
        const char* p = s->c_str ();
        len = s->getLength ();
        nChars = nSpaces = 0;
        while (len > 0) {
            n = font->getNextChar (
                p, len, &code, u, (int)(sizeof (u) / sizeof (Unicode)), &uLen,
                &dx2, &dy2, &originX, &originY);
            dx += dx2;
            dy += dy2;
            if (n == 1 && *p == ' ') { ++nSpaces; }
            ++nChars;
            p += n;
            len -= n;
        }
        if (wMode) {
            dx *= state->getFontSize ();
            dy = dy * state->getFontSize () + nChars * state->getCharSpace () +
                 nSpaces * state->getWordSpace ();
        }
        else {
            dx = dx * state->getFontSize () + nChars * state->getCharSpace () +
                 nSpaces * state->getWordSpace ();
            dx *= state->getHorizScaling ();
            dy *= state->getFontSize ();
        }
        state->textTransformDelta (dx, dy, &tdx, &tdy);
        out->drawString (state, s);
        state->shift (tdx, tdy);
    }

    if (out->useDrawChar ()) { out->endString (state); }

    if (patternFill) {
        out->saveTextPos (state);
        // tell the OutputDev to do the clipping
        out->endTextObject (state);
        // set up a clipping bbox so doPatternText will work -- assume
        // that the text bounding box does not extend past the baseline in
        // any direction by more than twice the font size
        x1 = state->getCurX () + riseX;
        y1 = state->getCurY () + riseY;
        if (x0 > x1) {
            x = x0;
            x0 = x1;
            x1 = x;
        }
        if (y0 > y1) {
            y = y0;
            y0 = y1;
            y1 = y;
        }
        state->textTransformDelta (0, state->getFontSize (), &dx, &dy);
        state->textTransformDelta (state->getFontSize (), 0, &dx2, &dy2);
        dx = fabs (dx);
        dx2 = fabs (dx2);
        if (dx2 > dx) { dx = dx2; }
        dy = fabs (dy);
        dy2 = fabs (dy2);
        if (dy2 > dy) { dy = dy2; }
        state->clipToRect (x0 - 2 * dx, y0 - 2 * dy, x1 + 2 * dx, y1 + 2 * dy);
        // set render mode to fill-only
        state->setRender (0);
        out->updateRender (state);
        doPatternText ();
        restoreState ();
        out->restoreTextPos (state);
    }

    updateLevel += 10 * s->getLength ();
}

// NB: this is only called when ocState is false.
void Gfx::doIncCharCount (GString* s) {
    if (out->needCharCount ()) { out->incCharCount (s->getLength ()); }
}

//------------------------------------------------------------------------
// XObject operators
//------------------------------------------------------------------------

void Gfx::opXObject (Object args[], int numArgs) {
    const char* name;
    Object obj1, obj2, obj3, refObj;
#if OPI_SUPPORT
    Object opiDict;
#endif

    if (!ocState && !out->needCharCount ()) { return; }
    name = args[0].as_name ();
    if (!res->lookupXObject (name, &obj1)) { return; }
    if (!obj1.is_stream ()) {
        error (
            errSyntaxError, getPos (), "XObject '{0:s}' is wrong type", name);
        return;
    }
#if OPI_SUPPORT
    opiDict = resolve ((*obj1.streamGetDict ()) ["OPI"]));
    if (opiDict.is_dict ()) { out->opiBegin (state, &opiDict.as_dict ()); }
#endif
    obj2 = resolve ((*obj1.streamGetDict ()) ["Subtype"]);
    if (obj2.is_name ("Image")) {
        if (out->needNonText ()) {
            res->lookupXObjectNF (name, &refObj);
            doImage (&refObj, obj1.as_stream (), false);
        }
    }
    else if (obj2.is_name ("Form")) {
        res->lookupXObjectNF (name, &refObj);
        if (out->useDrawForm () && refObj.is_ref ()) {
            out->drawForm (refObj.as_ref ());
        }
        else {
            doForm (&refObj, &obj1);
        }
    }
    else if (obj2.is_name ("PS")) {
        obj3 = resolve ((*obj1.streamGetDict ()) ["Level1"]);
        out->psXObject (
            obj1.as_stream (),
            obj3.is_stream () ? obj3.as_stream () : (Stream*)NULL);
    }
    else if (obj2.is_name ()) {
        error (
            errSyntaxError, getPos (), "Unknown XObject subtype '{0:s}'",
            obj2.as_name ());
    }
    else {
        error (
            errSyntaxError, getPos (),
            "XObject subtype is missing or wrong type");
    }
#if OPI_SUPPORT
    if (opiDict.is_dict ()) { out->opiEnd (state, &opiDict.as_dict ()); }
#endif
}

void Gfx::doImage (Object* ref, Stream* str, bool inlineImg) {
    Dict *dict, *maskDict;
    int width, height;
    int bits, maskBits;
    StreamColorSpaceMode csMode;
    bool mask;
    bool invert;
    GfxColorSpace *colorSpace, *maskColorSpace;
    GfxImageColorMap *colorMap, *maskColorMap;
    Object maskObj, smaskObj;
    bool haveColorKeyMask, haveExplicitMask, haveSoftMask;
    int maskColors[2 * gfxColorMaxComps];
    int maskWidth, maskHeight;
    bool maskInvert;
    Stream* maskStr;
    bool interpolate;
    Object obj1, obj2;
    int i, n;

    // get info from the stream
    bits = 0;
    csMode = streamCSNone;
    str->getImageParams (&bits, &csMode);

    // get stream dict
    dict = &str->as_dict ();

    // get size
    obj1 = resolve ((*dict) ["Width"]);
    if (obj1.is_null ()) {
        obj1 = resolve ((*dict) ["W"]);
    }
    if (!obj1.is_int ()) { goto err2; }
    width = obj1.as_int ();
    if (width <= 0) { goto err1; }
    obj1 = resolve ((*dict) ["Height"]);
    if (obj1.is_null ()) {
        obj1 = resolve ((*dict) ["H"]);
    }
    if (!obj1.is_int ()) { goto err2; }
    height = obj1.as_int ();
    if (height <= 0) { goto err1; }

    // image or mask?
    obj1 = resolve ((*dict) ["ImageMask"]);
    if (obj1.is_null ()) {
        obj1 = resolve ((*dict) ["IM"]);
    }
    mask = false;
    if (obj1.is_bool ())
        mask = obj1.as_bool ();
    else if (!obj1.is_null ())
        goto err2;

    // bit depth
    if (bits == 0) {
        obj1 = resolve ((*dict) ["BitsPerComponent"]);
        if (obj1.is_null ()) {
            obj1 = resolve ((*dict) ["BPC"]);
        }
        if (obj1.is_int ()) {
            bits = obj1.as_int ();
            if (bits < 1 || bits > 16) { goto err2; }
        }
        else if (mask) {
            bits = 1;
        }
        else {
            goto err2;
        }
    }

    // interpolate flag
    obj1 = resolve ((*dict) ["Interpolate"]);
    if (obj1.is_null ()) {
        obj1 = resolve ((*dict) ["I"]);
    }
    interpolate = obj1.is_bool () && obj1.as_bool ();

    // display a mask
    if (mask) {
        // check for inverted mask
        if (bits != 1) goto err1;
        invert = false;
        obj1 = resolve ((*dict) ["Decode"]);
        if (obj1.is_null ()) {
            obj1 = resolve ((*dict) ["D"]);
        }
        if (obj1.is_array ()) {
            obj2 = resolve (obj1 [0UL]);
            invert = obj2.is_num () && obj2.as_num () == 1;
        }
        else if (!obj1.is_null ()) {
            goto err2;
        }

        // if drawing is disabled, skip over inline image data
        if (!ocState) {
            str->reset ();
            n = height * ((width + 7) / 8);
            for (i = 0; i < n; ++i) { str->get (); }
            str->close ();

            // draw it
        }
        else {
            if (state->getFillColorSpace ()->getMode () == csPattern) {
                doPatternImageMask (
                    ref, str, width, height, invert, inlineImg, interpolate);
            }
            else {
                out->drawImageMask (
                    state, ref, str, width, height, invert, inlineImg,
                    interpolate);
            }
        }
    }
    else {
        // get color space and color map
        obj1 = resolve ((*dict) ["ColorSpace"]);
        if (obj1.is_null ()) {
            obj1 = resolve ((*dict) ["CS"]);
        }
        if (obj1.is_name ()) {
            res->lookupColorSpace (obj1.as_name (), &obj2);
            if (!obj2.is_null ()) {
                obj1 = obj2;
            }
            else {
            }
        }
        if (!obj1.is_null ()) { colorSpace = GfxColorSpace::parse (&obj1); }
        else if (csMode == streamCSDeviceGray) {
            colorSpace = GfxColorSpace::create (csDeviceGray);
        }
        else if (csMode == streamCSDeviceRGB) {
            colorSpace = GfxColorSpace::create (csDeviceRGB);
        }
        else if (csMode == streamCSDeviceCMYK) {
            colorSpace = GfxColorSpace::create (csDeviceCMYK);
        }
        else {
            colorSpace = NULL;
        }
        if (!colorSpace) { goto err1; }
        obj1 = resolve ((*dict) ["Decode"]);
        if (obj1.is_null ()) {
            obj1 = resolve ((*dict) ["D"]);
        }
        colorMap = new GfxImageColorMap (bits, &obj1, colorSpace);
        if (!colorMap->isOk ()) {
            delete colorMap;
            goto err1;
        }

        // get the mask
        haveColorKeyMask = haveExplicitMask = haveSoftMask = false;
        maskStr = NULL;             // make gcc happy
        maskWidth = maskHeight = 0; // make gcc happy
        maskInvert = false;        // make gcc happy
        maskColorMap = NULL;        // make gcc happy
        maskObj = resolve ((*dict) ["Mask"]);
        smaskObj = resolve ((*dict) ["SMask"]);
        if (smaskObj.is_stream ()) {
            // soft mask
            if (inlineImg) { goto err1; }
            maskStr = smaskObj.as_stream ();
            maskDict = smaskObj.streamGetDict ();
            obj1 = resolve ((*maskDict) ["Width"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["W"]);
            }
            if (!obj1.is_int ()) { goto err2; }
            maskWidth = obj1.as_int ();
            obj1 = resolve ((*maskDict) ["Height"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["H"]);
            }
            if (!obj1.is_int ()) { goto err2; }
            maskHeight = obj1.as_int ();
            obj1 = resolve ((*maskDict) ["BitsPerComponent"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["BPC"]);
            }
            if (!obj1.is_int ()) { goto err2; }
            maskBits = obj1.as_int ();
            obj1 = resolve ((*maskDict) ["ColorSpace"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["CS"]);
            }
            if (obj1.is_name ()) {
                res->lookupColorSpace (obj1.as_name (), &obj2);
                if (!obj2.is_null ()) {
                    obj1 = obj2;
                }
                else {
                }
            }
            maskColorSpace = GfxColorSpace::parse (&obj1);
            if (!maskColorSpace || maskColorSpace->getMode () != csDeviceGray) {
                goto err1;
            }
            obj1 = resolve ((*maskDict) ["Decode"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["D"]);
            }
            maskColorMap =
                new GfxImageColorMap (maskBits, &obj1, maskColorSpace);
            if (!maskColorMap->isOk ()) {
                delete maskColorMap;
                goto err1;
            }
            //~ handle the Matte entry
            haveSoftMask = true;
        }
        else if (maskObj.is_array ()) {
            // color key mask
            haveColorKeyMask = true;
            for (i = 0; i + 1 < maskObj.as_array ().size () &&
                        i + 1 < 2 * gfxColorMaxComps;
                 i += 2) {
                obj1 = resolve (maskObj [i]);
                if (!obj1.is_int ()) {
                    haveColorKeyMask = false;
                    break;
                }
                maskColors[i] = obj1.as_int ();
                if (maskColors[i] < 0 || maskColors[i] >= (1 << bits)) {
                    haveColorKeyMask = false;
                    break;
                }
                obj1 = resolve (maskObj [i + 1]);
                if (!obj1.is_int ()) {
                    haveColorKeyMask = false;
                    break;
                }
                maskColors[i + 1] = obj1.as_int ();
                if (maskColors[i + 1] < 0 || maskColors[i + 1] >= (1 << bits) ||
                    maskColors[i] > maskColors[i + 1]) {
                    haveColorKeyMask = false;
                    break;
                }
            }
        }
        else if (maskObj.is_stream ()) {
            // explicit mask
            if (inlineImg) { goto err1; }
            maskStr = maskObj.as_stream ();
            maskDict = maskObj.streamGetDict ();
            obj1 = resolve ((*maskDict) ["Width"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["W"]);
            }
            if (!obj1.is_int ()) { goto err2; }
            maskWidth = obj1.as_int ();
            obj1 = resolve ((*maskDict) ["Height"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["H"]);
            }
            if (!obj1.is_int ()) { goto err2; }
            maskHeight = obj1.as_int ();
            obj1 = resolve ((*maskDict) ["ImageMask"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["IM"]);
            }
            if (!obj1.is_bool () || !obj1.as_bool ()) { goto err2; }
            maskInvert = false;
            obj1 = resolve ((*maskDict) ["Decode"]);
            if (obj1.is_null ()) {
                obj1 = resolve ((*maskDict) ["D"]);
            }
            if (obj1.is_array ()) {
                obj2 = resolve (obj1 [0UL]);
                maskInvert = obj2.is_num () && obj2.as_num () == 1;
            }
            else if (!obj1.is_null ()) {
                goto err2;
            }
            haveExplicitMask = true;
        }

        // if drawing is disabled, skip over inline image data
        if (!ocState) {
            str->reset ();
            n = height *
                ((width * colorMap->getNumPixelComps () * colorMap->getBits () +
                  7) /
                 8);
            for (i = 0; i < n; ++i) { str->get (); }
            str->close ();

            // draw it
        }
        else {
            if (haveSoftMask) {
                out->drawSoftMaskedImage (
                    state, ref, str, width, height, colorMap, maskStr,
                    maskWidth, maskHeight, maskColorMap, interpolate);
                delete maskColorMap;
            }
            else if (haveExplicitMask) {
                out->drawMaskedImage (
                    state, ref, str, width, height, colorMap, maskStr,
                    maskWidth, maskHeight, maskInvert, interpolate);
            }
            else {
                out->drawImage (
                    state, ref, str, width, height, colorMap,
                    haveColorKeyMask ? maskColors : (int*)NULL, inlineImg,
                    interpolate);
            }
        }

        delete colorMap;
    }

    if ((i = width * height) > 1000) { i = 1000; }
    updateLevel += i;

    return;

err2:
err1:
    error (errSyntaxError, getPos (), "Bad image parameters");
}

void Gfx::doForm (Object* strRef, Object* str) {
    Dict* dict;
    bool transpGroup, isolated, knockout;
    GfxColorSpace* blendingColorSpace;
    Object matrixObj, bboxObj;
    double m[6], bbox[4];
    Object resObj;
    Dict* resDict;
    bool oc, ocSaved;
    Object obj1, obj2, obj3;
    int i;

    // check for excessive recursion
    if (formDepth > 100) { return; }

    // get stream dict
    dict = str->streamGetDict ();

    // check form type
    obj1 = resolve ((*dict) ["FormType"]);
    if (!(obj1.is_null () || (obj1.is_int () && obj1.as_int () == 1))) {
        error (errSyntaxError, getPos (), "Unknown form type");
    }

    // check for optional content key
    ocSaved = ocState;
    obj1 = (*dict) ["OC"];
    if (doc->getOptionalContent ()->evalOCObject (&obj1, &oc) && !oc) {
        if (out->needCharCount ()) { ocState = false; }
        else {
            return;
        }
    }

    // get bounding box
    bboxObj = resolve ((*dict) ["BBox"]);
    if (!bboxObj.is_array ()) {
        error (errSyntaxError, getPos (), "Bad form bounding box");
        ocState = ocSaved;
        return;
    }
    for (i = 0; i < 4; ++i) {
        obj1 = resolve (bboxObj [i]);
        bbox[i] = obj1.as_num ();
    }

    // get matrix
    matrixObj = resolve ((*dict) ["Matrix"]);
    if (matrixObj.is_array ()) {
        for (i = 0; i < 6; ++i) {
            obj1 = resolve (matrixObj [i]);
            m[i] = obj1.as_num ();
        }
    }
    else {
        m[0] = 1;
        m[1] = 0;
        m[2] = 0;
        m[3] = 1;
        m[4] = 0;
        m[5] = 0;
    }

    // get resources
    resObj = resolve ((*dict) ["Resources"]);
    resDict = resObj.is_dict () ? &resObj.as_dict () : (Dict*)NULL;

    // check for a transparency group
    transpGroup = isolated = knockout = false;
    blendingColorSpace = NULL;
    if ((obj1 = resolve ((*dict) ["Group"])).is_dict ()) {
        if ((obj2 = resolve (obj1.as_dict ()["S"])).is_name ("Transparency")) {
            transpGroup = true;
            if (!(obj3 = resolve (obj1.as_dict ()["CS"])).is_null ()) {
                blendingColorSpace = GfxColorSpace::parse (&obj3);
            }
            if ((obj3 = resolve (obj1.as_dict ()["I"])).is_bool ()) {
                isolated = obj3.as_bool ();
            }
            if ((obj3 = resolve (obj1.as_dict ()["K"])).is_bool ()) {
                knockout = obj3.as_bool ();
            }
        }
    }

    // draw it
    ++formDepth;
    drawForm (
        strRef, resDict, m, bbox, transpGroup, false, blendingColorSpace,
        isolated, knockout);
    --formDepth;

    if (blendingColorSpace) { delete blendingColorSpace; }

    ocState = ocSaved;
}

void Gfx::drawForm (
    Object* strRef, Dict* resDict, double* matrix, double* bbox,
    bool transpGroup, bool softMask, GfxColorSpace* blendingColorSpace,
    bool isolated, bool knockout, bool alpha, const Function& transferFunc,
    GfxColor* backdropColor) {
    Parser* oldParser;
    GfxState* savedState;
    double oldBaseMatrix[6];
    int i;

    // push new resources on stack
    pushResources (resDict);

    // save current graphics state
    savedState = saveStateStack ();

    // kill any pre-existing path
    state->clearPath ();

    // save current parser
    oldParser = parser;

    // set form transformation matrix
    state->concatCTM (
        matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]);
    out->updateCTM (
        state, matrix[0], matrix[1], matrix[2], matrix[3], matrix[4],
        matrix[5]);

    // set form bounding box
    state->moveTo (bbox[0], bbox[1]);
    state->lineTo (bbox[2], bbox[1]);
    state->lineTo (bbox[2], bbox[3]);
    state->lineTo (bbox[0], bbox[3]);
    state->closePath ();
    state->clip ();
    out->clip (state);
    state->clearPath ();

    if (softMask || transpGroup) {
        if (state->getBlendMode () != gfxBlendNormal) {
            state->setBlendMode (gfxBlendNormal);
            out->updateBlendMode (state);
        }
        if (state->getFillOpacity () != 1) {
            state->setFillOpacity (1);
            out->updateFillOpacity (state);
        }
        if (state->getStrokeOpacity () != 1) {
            state->setStrokeOpacity (1);
            out->updateStrokeOpacity (state);
        }
        out->clearSoftMask (state);
        out->beginTransparencyGroup (
            state, bbox, blendingColorSpace, isolated, knockout, softMask);
    }

    // set new base matrix
    for (i = 0; i < 6; ++i) {
        oldBaseMatrix[i] = baseMatrix[i];
        baseMatrix[i] = state->getCTM ()[i];
    }

    // draw the form
    display (strRef, false);

    if (softMask || transpGroup) { out->endTransparencyGroup (state); }

    // restore base matrix
    for (i = 0; i < 6; ++i) { baseMatrix[i] = oldBaseMatrix[i]; }

    // restore parser
    parser = oldParser;

    // restore graphics state
    restoreStateStack (savedState);

    // pop resource stack
    popResources ();

    if (softMask) {
        out->setSoftMask (state, bbox, alpha, transferFunc, backdropColor);
    }
    else if (transpGroup) {
        out->paintTransparencyGroup (state, bbox);
    }

    return;
}

void Gfx::takeContentStreamStack (Gfx* oldGfx) {
    auto& dst = contentStreamStack;
    auto& src = oldGfx->contentStreamStack;
    dst.insert (dst.end (), src.begin (), src.end ());
    src.clear ();
}

//------------------------------------------------------------------------
// in-line image operators
//------------------------------------------------------------------------

void Gfx::opBeginImage (Object args[], int numArgs) {
    Stream* str;
    int c1, c2, c3;

    // NB: this function is run even if ocState is false -- doImage() is
    // responsible for skipping over the inline image data

    // build dict/stream
    str = buildImageStream ();

    // display the image
    if (str) {
        doImage (NULL, str, true);

        // skip 'EI' tag
        c1 = str->getUndecodedStream ()->get ();
        c2 = str->getUndecodedStream ()->get ();
        c3 = str->getUndecodedStream ()->peek ();
        while (!(c1 == 'E' && c2 == 'I' && Lexer::isSpace (c3)) && c3 != EOF) {
            c1 = c2;
            c2 = str->getUndecodedStream ()->get ();
            c3 = str->getUndecodedStream ()->peek ();
        }
        delete str;
    }
}

Stream* Gfx::buildImageStream () {
    Object dict;
    Object obj;
    Stream* str;

    // build dictionary
    dict = xpdf::make_dict_obj ();
    parser->getObj (&obj);
    while (!obj.is_cmd ("ID") && !obj.is_eof ()) {
        if (!obj.is_name ()) {
            error (
                errSyntaxError, getPos (),
                "Inline image dictionary key must be a name object");
        }
        else {
            std::string key (obj.as_name ());

            parser->getObj (&obj);

            if (obj.is_eof () || obj.is_err ()) {
                break;
            }

            dict.emplace (key, std::move (obj));
        }

        parser->getObj (&obj);
    }
    if (obj.is_eof ()) {
        error (errSyntaxError, getPos (), "End of file in inline image");
        return NULL;
    }

    // make stream
    if (!(str = parser->as_stream ())) {
        error (errSyntaxError, getPos (), "Invalid inline image data");
        return NULL;
    }
    str = new EmbedStream (str, &dict, false, 0);
    str = str->addFilters (&dict);

    return str;
}

void Gfx::opImageData (Object args[], int numArgs) {
    error (errInternal, getPos (), "Got 'ID' operator");
}

void Gfx::opEndImage (Object args[], int numArgs) {
    error (errInternal, getPos (), "Got 'EI' operator");
}

//------------------------------------------------------------------------
// type 3 font operators
//------------------------------------------------------------------------

void Gfx::opSetCharWidth (Object args[], int numArgs) {
    out->type3D0 (state, args[0].as_num (), args[1].as_num ());
}

void Gfx::opSetCacheDevice (Object args[], int numArgs) {
    out->type3D1 (
        state, args[0].as_num (), args[1].as_num (), args[2].as_num (),
        args[3].as_num (), args[4].as_num (), args[5].as_num ());
}

//------------------------------------------------------------------------
// compatibility operators
//------------------------------------------------------------------------

void Gfx::opBeginIgnoreUndef (Object args[], int numArgs) { ++ignoreUndef; }

void Gfx::opEndIgnoreUndef (Object args[], int numArgs) {
    if (ignoreUndef > 0) --ignoreUndef;
}

//------------------------------------------------------------------------
// marked content operators
//------------------------------------------------------------------------

void Gfx::opBeginMarkedContent (Object args[], int numArgs) {
    GfxMarkedContent* mc;
    Object obj;
    bool ocStateNew;
    GfxMarkedContentKind mcKind;

    if (printCommands) {
        printf ("  marked content: %s ", args[0].as_name ());
        if (numArgs == 2) { args[1].print (stdout); }
        printf ("\n");
        fflush (stdout);
    }
    mcKind = gfxMCOther;
    if (args[0].is_name ("OC") && numArgs == 2 && args[1].is_name () &&
        res->lookupPropertiesNF (args[1].as_name (), &obj)) {
        if (doc->getOptionalContent ()->evalOCObject (&obj, &ocStateNew)) {
            ocState = ocStateNew;
        }
        mcKind = gfxMCOptionalContent;
    }
    else if (args[0].is_name ("Span") && numArgs == 2 && args[1].is_dict ()) {
        if ((obj = args[1].as_dict ()["ActualText"]).is_string ()) {
            TextString s (obj.as_string ());
            out->beginActualText (state, s.getUnicode (), s.getLength ());
            mcKind = gfxMCActualText;
        }
    }
    mc = new GfxMarkedContent (mcKind, ocState);
    markedContentStack->append (mc);
}

void Gfx::opEndMarkedContent (Object args[], int numArgs) {
    GfxMarkedContent* mc;
    GfxMarkedContentKind mcKind;

    if (markedContentStack->getLength () > 0) {
        mc = (GfxMarkedContent*)markedContentStack->del (
            markedContentStack->getLength () - 1);
        mcKind = mc->kind;
        delete mc;
        if (mcKind == gfxMCOptionalContent) {
            if (markedContentStack->getLength () > 0) {
                mc = (GfxMarkedContent*)markedContentStack->get (
                    markedContentStack->getLength () - 1);
                ocState = mc->ocState;
            }
            else {
                ocState = true;
            }
        }
        else if (mcKind == gfxMCActualText) {
            out->endActualText (state);
        }
    }
    else {
        error (errSyntaxWarning, getPos (), "Mismatched EMC operator");
    }
}

void Gfx::opMarkPoint (Object args[], int numArgs) {
    if (printCommands) {
        printf ("  mark point: %s ", args[0].as_name ());
        if (numArgs == 2) args[1].print (stdout);
        printf ("\n");
        fflush (stdout);
    }
}

//------------------------------------------------------------------------
// misc
//------------------------------------------------------------------------

void Gfx::drawAnnot (
    Object* strRef, AnnotBorderStyle* borderStyle, double xMin, double yMin,
    double xMax, double yMax) {
    Dict *dict, *resDict;
    Object str, matrixObj, bboxObj, resObj, obj1;
    double formXMin, formYMin, formXMax, formYMax;
    double x, y, sx, sy, tx, ty;
    double m[6], bbox[4];
    double* borderColor;
    GfxColor color;
    double *dash, *dash2;
    int dashLength;
    int i;

    // this function assumes that we are in the default user space,
    // i.e., baseMatrix = ctm

    // if the bounding box has zero width or height, don't draw anything
    // at all
    if (xMin == xMax || yMin == yMax) { return; }

    // draw the appearance stream (if there is one)
    if ((str = resolve (*strRef)).is_stream ()) {
        // get stream dict
        dict = str.streamGetDict ();

        // get the form bounding box
        bboxObj = resolve ((*dict) ["BBox"]);
        if (!bboxObj.is_array ()) {
            error (errSyntaxError, getPos (), "Bad form bounding box");
            return;
        }
        for (i = 0; i < 4; ++i) {
            obj1 = resolve (bboxObj [i]);
            bbox[i] = obj1.as_num ();
        }

        // get the form matrix
        matrixObj = resolve ((*dict) ["Matrix"]);
        if (matrixObj.is_array ()) {
            for (i = 0; i < 6; ++i) {
                obj1 = resolve (matrixObj [i]);
                m[i] = obj1.as_num ();
            }
        }
        else {
            m[0] = 1;
            m[1] = 0;
            m[2] = 0;
            m[3] = 1;
            m[4] = 0;
            m[5] = 0;
        }

        // transform the four corners of the form bbox to default user
        // space, and construct the transformed bbox
        x = bbox[0] * m[0] + bbox[1] * m[2] + m[4];
        y = bbox[0] * m[1] + bbox[1] * m[3] + m[5];
        formXMin = formXMax = x;
        formYMin = formYMax = y;
        x = bbox[0] * m[0] + bbox[3] * m[2] + m[4];
        y = bbox[0] * m[1] + bbox[3] * m[3] + m[5];
        if (x < formXMin) { formXMin = x; }
        else if (x > formXMax) {
            formXMax = x;
        }
        if (y < formYMin) { formYMin = y; }
        else if (y > formYMax) {
            formYMax = y;
        }
        x = bbox[2] * m[0] + bbox[1] * m[2] + m[4];
        y = bbox[2] * m[1] + bbox[1] * m[3] + m[5];
        if (x < formXMin) { formXMin = x; }
        else if (x > formXMax) {
            formXMax = x;
        }
        if (y < formYMin) { formYMin = y; }
        else if (y > formYMax) {
            formYMax = y;
        }
        x = bbox[2] * m[0] + bbox[3] * m[2] + m[4];
        y = bbox[2] * m[1] + bbox[3] * m[3] + m[5];
        if (x < formXMin) { formXMin = x; }
        else if (x > formXMax) {
            formXMax = x;
        }
        if (y < formYMin) { formYMin = y; }
        else if (y > formYMax) {
            formYMax = y;
        }

        // construct a mapping matrix, [sx 0  0], which maps the transformed
        //                             [0  sy 0]
        //                             [tx ty 1]
        // bbox to the annotation rectangle
        if (formXMin == formXMax) {
            // this shouldn't happen
            sx = 1;
        }
        else {
            sx = (xMax - xMin) / (formXMax - formXMin);
        }
        if (formYMin == formYMax) {
            // this shouldn't happen
            sy = 1;
        }
        else {
            sy = (yMax - yMin) / (formYMax - formYMin);
        }
        tx = -formXMin * sx + xMin;
        ty = -formYMin * sy + yMin;

        // the final transform matrix is (form matrix) * (mapping matrix)
        m[0] *= sx;
        m[1] *= sy;
        m[2] *= sx;
        m[3] *= sy;
        m[4] = m[4] * sx + tx;
        m[5] = m[5] * sy + ty;

        // get the resources
        resObj = resolve ((*dict) ["Resources"]);
        resDict = resObj.is_dict () ? &resObj.as_dict () : (Dict*)NULL;

        // draw it
        drawForm (strRef, resDict, m, bbox);

    }

    // draw the border
    if (borderStyle && borderStyle->getWidth () > 0 &&
        borderStyle->getNumColorComps () > 0) {
        borderColor = borderStyle->getColor ();
        switch (borderStyle->getNumColorComps ()) {
        case 1:
            if (state->getStrokeColorSpace ()->getMode () != csDeviceGray) {
                state->setStrokePattern (NULL);
                state->setStrokeColorSpace (
                    GfxColorSpace::create (csDeviceGray));
                out->updateStrokeColorSpace (state);
            }
            break;
        case 3:
            if (state->getStrokeColorSpace ()->getMode () != csDeviceRGB) {
                state->setStrokePattern (NULL);
                state->setStrokeColorSpace (
                    GfxColorSpace::create (csDeviceRGB));
                out->updateStrokeColorSpace (state);
            }
            break;
        case 4:
            if (state->getStrokeColorSpace ()->getMode () != csDeviceCMYK) {
                state->setStrokePattern (NULL);
                state->setStrokeColorSpace (
                    GfxColorSpace::create (csDeviceCMYK));
                out->updateStrokeColorSpace (state);
            }
            break;
        }
        color.c[0] = xpdf::to_color (borderColor[0]);
        color.c[1] = xpdf::to_color (borderColor[1]);
        color.c[2] = xpdf::to_color (borderColor[2]);
        color.c[3] = xpdf::to_color (borderColor[3]);
        state->setStrokeColor (&color);
        out->updateStrokeColor (state);
        state->setLineWidth (borderStyle->getWidth ());
        out->updateLineWidth (state);
        borderStyle->getDash (&dash, &dashLength);
        if (borderStyle->getType () == annotBorderDashed && dashLength > 0) {
            dash2 = (double*)calloc (dashLength, sizeof (double));
            memcpy (dash2, dash, dashLength * sizeof (double));
            state->setLineDash (dash2, dashLength, 0);
            out->updateLineDash (state);
        }
        //~ this doesn't currently handle the beveled and engraved styles
        state->clearPath ();
        state->moveTo (xMin, yMin);
        state->lineTo (xMax, yMin);
        if (borderStyle->getType () != annotBorderUnderlined) {
            state->lineTo (xMax, yMax);
            state->lineTo (xMin, yMax);
            state->closePath ();
        }
        out->stroke (state);
    }
}

void Gfx::saveState () {
    out->saveState (state);
    state = state->save ();
}

void Gfx::restoreState () {
    state = state->restore ();
    out->restoreState (state);
}

// Create a new state stack, and initialize it with a copy of the
// current state.
GfxState* Gfx::saveStateStack () {
    GfxState* oldState;

    out->saveState (state);
    oldState = state;
    state = state->copy (true);
    return oldState;
}

// Switch back to the previous state stack.
void Gfx::restoreStateStack (GfxState* oldState) {
    while (state->hasSaves ()) { restoreState (); }
    delete state;
    state = oldState;
    out->restoreState (state);
}

void Gfx::pushResources (Dict* resDict) {
    res = new GfxResources (xref, resDict, res);
}

void Gfx::popResources () {
    GfxResources* resPtr;

    resPtr = res->getNext ();
    delete res;
    res = resPtr;
}

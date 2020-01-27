// -*- mode: c++; -*-
// Copyright 2001-2007 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <climits>

#include <goo/memory.hh>
#include <goo/parseargs.hh>
#include <goo/GString.hh>

#include <xpdf/GlobalParams.hh>
#include <xpdf/Error.hh>
#include <xpdf/obj.hh>
#include <xpdf/dict.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/Annot.hh>
#include <xpdf/Form.hh>
#include <xpdf/PDFDoc.hh>

// NB: this must match the definition of GfxFontType in GfxFont.h.
static const char* fontTypeNames[] = {
    "unknown",     "Type 1",           "Type 1C",       "Type 1C (OT)",
    "Type 3",      "TrueType",         "TrueType (OT)", "CID Type 0",
    "CID Type 0C", "CID Type 0C (OT)", "CID TrueType",  "CID TrueType (OT)"
};

static void scanFonts (Object* obj, PDFDoc* doc);
static void scanFonts (Dict* resDict, PDFDoc* doc);
static void scanFont (GfxFont* font, PDFDoc* doc);

static int firstPage = 1;
static int lastPage = 0;
static bool showFontLoc = false;
static bool showFontLocPS = false;
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static char cfgFileName[256] = "";
static bool printVersion = false;
static bool printHelp = false;

static ArgDesc argDesc[] = {
    { "-f", argInt, &firstPage, 0, "first page to examine" },
    { "-l", argInt, &lastPage, 0, "last page to examine" },
    { "-loc", argFlag, &showFontLoc, 0,
      "print extended info on font location" },
    { "-locPS", argFlag, &showFontLocPS, 0,
      "print extended info on font location for PostScript conversion" },
    { "-opw", argString, ownerPassword, sizeof (ownerPassword),
      "owner password (for encrypted files)" },
    { "-upw", argString, userPassword, sizeof (userPassword),
      "user password (for encrypted files)" },
    { "-cfg", argString, cfgFileName, sizeof (cfgFileName),
      "configuration file to use in place of .xpdfrc" },
    { "-v", argFlag, &printVersion, 0, "print copyright and version info" },
    { "-h", argFlag, &printHelp, 0, "print usage information" },
    { "-help", argFlag, &printHelp, 0, "print usage information" },
    { "--help", argFlag, &printHelp, 0, "print usage information" },
    { "-?", argFlag, &printHelp, 0, "print usage information" },
    { }
};

static Ref* fonts;
static int fontsLen;
static int fontsSize;

static Ref* seenObjs;
static int seenObjsLen;
static int seenObjsSize;

int main (int argc, char* argv[]) {
    PDFDoc* doc;
    GString* fileName;
    GString *ownerPW, *userPW;
    bool ok;
    Page* page;
    Dict* resDict;
    Annots* annots;
    Form* form;
    Object obj1, obj2;
    int pg, i, j;
    int exitCode;

    exitCode = 99;

    // parse args
    ok = parseArgs (argDesc, &argc, argv);
    if (!ok || argc != 2 || printVersion || printHelp) {
        fprintf (stderr, "pdffonts version %s\n", PACKAGE_VERSION);
        fprintf (stderr, "%s\n", XPDF_COPYRIGHT);
        if (!printVersion) { printUsage ("pdffonts", "<PDF-file>", argDesc); }
        goto err0;
    }
    fileName = new GString (argv[1]);

    // read config file
    globalParams = new GlobalParams (cfgFileName);
    globalParams->setupBaseFonts (NULL);

    // open PDF file
    if (ownerPassword[0] != '\001') { ownerPW = new GString (ownerPassword); }
    else {
        ownerPW = NULL;
    }
    if (userPassword[0] != '\001') { userPW = new GString (userPassword); }
    else {
        userPW = NULL;
    }
    doc = new PDFDoc (fileName, ownerPW, userPW);
    if (userPW) { delete userPW; }
    if (ownerPW) { delete ownerPW; }
    if (!doc->isOk ()) {
        exitCode = 1;
        goto err1;
    }

    // get page range
    if (firstPage < 1) { firstPage = 1; }
    if (lastPage < 1 || lastPage > doc->getNumPages ()) {
        lastPage = doc->getNumPages ();
    }

    // scan the fonts
    if (showFontLoc || showFontLocPS) {
        printf (
            "name                                 type              emb sub "
            "uni object ID location\n");
        printf (
            "------------------------------------ ----------------- --- --- "
            "--- --------- --------\n");
    }
    else {
        printf (
            "name                                 type              emb sub "
            "uni object ID\n");
        printf (
            "------------------------------------ ----------------- --- --- "
            "--- ---------\n");
    }
    fonts = NULL;
    fontsLen = fontsSize = 0;
    seenObjs = NULL;
    seenObjsLen = seenObjsSize = 0;
    for (pg = firstPage; pg <= lastPage; ++pg) {
        page = doc->getCatalog ()->getPage (pg);
        if ((resDict = page->getResourceDict ())) { scanFonts (resDict, doc); }
        annots = new Annots (doc, page->getAnnots (&obj1));
        for (i = 0; i < annots->getNumAnnots (); ++i) {
            if (annots->getAnnot (i)->getAppearance (&obj1)->is_stream ()) {
                obj2 = (*obj1.streamGetDict ()) ["Resources"];
                scanFonts (&obj2, doc);
            }
        }
        delete annots;
    }
    if ((form = doc->getCatalog ()->getForm ())) {
        for (i = 0; i < form->getNumFields (); ++i) {
            form->getField (i)->getResources (&obj1);
            if (obj1.is_array ()) {
                for (j = 0; j < obj1.as_array ().size (); ++j) {
                    obj2 = obj1 [j];
                    scanFonts (&obj2, doc);
                }
            }
            else if (obj1.is_dict ()) {
                scanFonts (obj1.as_dict_ptr (), doc);
            }
        }
    }

    exitCode = 0;

    // clean up
    free (fonts);
    free (seenObjs);
err1:
    delete doc;
    delete globalParams;
err0:

    return exitCode;
}

static void scanFonts (Object* obj, PDFDoc* doc) {
    Object obj2;
    int i;

    if (obj->is_ref ()) {
        for (i = 0; i < seenObjsLen; ++i) {
            if (obj->getRefNum () == seenObjs[i].num &&
                obj->getRefGen () == seenObjs[i].gen) {
                return;
            }
        }
        if (seenObjsLen == seenObjsSize) {
            if (seenObjsSize <= INT_MAX - 32) { seenObjsSize += 32; }
            else {
                // let reallocarray throw an exception
                seenObjsSize = -1;
            }
            seenObjs = (Ref*)reallocarray (seenObjs, seenObjsSize, sizeof (Ref));
        }
        seenObjs[seenObjsLen++] = obj->as_ref ();
    }
    if ((obj2 = resolve (*obj)).is_dict ()) {
        scanFonts (obj2.as_dict_ptr (), doc);
    }
}

static void scanFonts (Dict* resDict, PDFDoc* doc) {
    Object obj1, obj2, xObjDict;
    Object patternDict, resObj;
    Ref r;
    GfxFontDict* gfxFontDict;
    GfxFont* font;
    int i;

    // scan the fonts in this resource dictionary
    gfxFontDict = NULL;
    obj1 = (*resDict) ["Font"];
    if (obj1.is_ref ()) {
        obj2 = resolve (obj1);
        if (obj2.is_dict ()) {
            r = obj1.as_ref ();
            gfxFontDict =
                new GfxFontDict (doc->getXRef (), &r, obj2.as_dict_ptr ());
        }
    }
    else if (obj1.is_dict ()) {
        gfxFontDict = new GfxFontDict (doc->getXRef (), NULL, obj1.as_dict_ptr ());
    }
    if (gfxFontDict) {
        for (i = 0; i < gfxFontDict->getNumFonts (); ++i) {
            if ((font = gfxFontDict->getFont (i))) { scanFont (font, doc); }
        }
        delete gfxFontDict;
    }

    // recursively scan any resource dictionaries in XObjects in this
    // resource dictionary
    xObjDict = resolve ((*resDict) ["XObject"]);
    if (xObjDict.is_dict ()) {
        for (i = 0; i < xObjDict.as_dict ().size (); ++i) {
            auto& xObj = xObjDict.val_at (i);
            if (xObj.is_stream ()) {
                resObj = (*xObj.streamGetDict ()) ["Resources"];
                scanFonts (&resObj, doc);
            }
        }
    }

    // recursively scan any resource dictionaries in Patterns in this
    // resource dictionary
    patternDict = resolve ((*resDict) ["Pattern"]);
    if (patternDict.is_dict ()) {
        for (i = 0; i < patternDict.as_dict ().size (); ++i) {
            auto& pattern = patternDict.val_at (i);
            if (pattern.is_stream ()) {
                resObj = (*pattern.streamGetDict ()) ["Resources"];
                scanFonts (&resObj, doc);
            }
        }
    }

    // recursively scan any resource dictionaries in ExtGStates in this
    // resource dictionary
    Object gsDict;
    gsDict = resolve ((*resDict) ["ExtGState"]);

    if (gsDict.is_dict ()) {
        for (i = 0; i < gsDict.as_dict ().size (); ++i) {
            auto& gs = gsDict.val_at (i);

            if (gs.is_dict ()) {
                Object smask;

                if ((smask = resolve (gs.as_dict ()["SMask"])).is_dict ()) {
                    Object smaskGroup;

                    if ((smaskGroup = resolve (smask.as_dict ()["G"])).is_stream ()) {
                        resObj = (*smaskGroup.streamGetDict ()) ["Resources"];
                        scanFonts (&resObj, doc);
                    }
                }
            }
        }
    }
}

static void scanFont (GfxFont* font, PDFDoc* doc) {
    Ref fontRef, embRef;
    GString* name;
    bool emb, subset, hasToUnicode;
    GfxFontLoc* loc;
    int i;

    fontRef = *font->getID ();

    // check for an already-seen font
    for (i = 0; i < fontsLen; ++i) {
        if (fontRef.num == fonts[i].num && fontRef.gen == fonts[i].gen) {
            return;
        }
    }

    // font name
    name = font->as_name ();

    // check for an embedded font
    if (font->getType () == fontType3) { emb = true; }
    else {
        emb = font->getEmbeddedFontID (&embRef);
    }

    // look for a ToUnicode map
    hasToUnicode = false;

    Object fontObj;
    doc->getXRef ()->fetch (fontRef, &fontObj);

    if (fontObj.is_dict ()) {
        Object toUnicodeObj;
        hasToUnicode = resolve (fontObj.as_dict ()["ToUnicode"]).is_stream ();
    }

    // check for a font subset name: capital letters followed by a '+'
    // sign
    subset = false;
    if (name) {
        for (i = 0; i < name->getLength (); ++i) {
            if (name->getChar (i) < 'A' || name->getChar (i) > 'Z') { break; }
        }
        subset = i > 0 && i < name->getLength () && name->getChar (i) == '+';
    }

    // print the font info
    printf (
        "%-36s %-17s %-3s %-3s %-3s", name ? name->c_str () : "[none]",
        fontTypeNames[font->getType ()], emb ? "yes" : "no",
        subset ? "yes" : "no", hasToUnicode ? "yes" : "no");
    if (fontRef.gen >= 100000) { printf (" [none]"); }
    else {
        printf (" %6d %2d", fontRef.num, fontRef.gen);
    }
    if (showFontLoc || showFontLocPS) {
        if (font->getType () == fontType3) { printf (" embedded"); }
        else {
            loc = font->locateFont (doc->getXRef (), showFontLocPS);
            if (loc) {
                if (loc->locType == gfxFontLocEmbedded) {
                    printf (" embedded");
                }
                else if (loc->locType == gfxFontLocExternal) {
                    if (loc->path) {
                        printf (" external: %s", loc->path->c_str ());
                    }
                    else {
                        printf (" unavailable");
                    }
                }
                else if (loc->locType == gfxFontLocResident) {
                    if (loc->path) {
                        printf (" resident: %s", loc->path->c_str ());
                    }
                    else {
                        printf (" unavailable");
                    }
                }
            }
            else {
                printf (" unknown");
            }
            delete loc;
        }
    }
    printf ("\n");

    // add this font to the list
    if (fontsLen == fontsSize) {
        if (fontsSize <= INT_MAX - 32) { fontsSize += 32; }
        else {
            // let reallocarray throw an exception
            fontsSize = -1;
        }
        fonts = (Ref*)reallocarray (fonts, fontsSize, sizeof (Ref));
    }
    fonts[fontsLen++] = *font->getID ();
}

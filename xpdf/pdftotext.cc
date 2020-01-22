// -*- mode: c++; -*-
// Copyright 1997-2013 Glyph & Cog, LLC

#include <defs.hh>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#ifdef DEBUG_FP_LINUX
#include <fenv.h>
#include <fpu_control.h>
#endif
#include <goo/parseargs.hh>
#include <goo/GString.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>
#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Catalog.hh>
#include <xpdf/Page.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/TextOutputDev.hh>
#include <xpdf/CharTypes.hh>
#include <xpdf/UnicodeMap.hh>
#include <xpdf/TextString.hh>
#include <xpdf/Error.hh>
#include <defs.hh>

static int firstPage = 1;
static int lastPage = 0;
static bool physLayout = false;
static bool tableLayout = false;
static bool linePrinter = false;
static bool rawOrder = false;
static double fixedPitch = 0;
static double fixedLineSpacing = 0;
static bool clipText = false;
static char textEncName[128] = "";
static char textEOL[16] = "";
static bool noPageBreaks = false;
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static bool quiet = false;
static char cfgFileName[256] = "";
static bool printVersion = false;
static bool printHelp = false;

static ArgDesc argDesc[] = {
    { "-f", argInt, &firstPage, 0, "first page to convert" },
    { "-l", argInt, &lastPage, 0, "last page to convert" },
    { "-layout", argFlag, &physLayout, 0, "maintain original physical layout" },
    { "-table", argFlag, &tableLayout, 0,
      "similar to -layout, but optimized for tables" },
    { "-lineprinter", argFlag, &linePrinter, 0,
      "use strict fixed-pitch/height layout" },
    { "-raw", argFlag, &rawOrder, 0, "keep strings in content stream order" },
    { "-fixed", argFP, &fixedPitch, 0, "assume fixed-pitch (or tabular) text" },
    { "-linespacing", argFP, &fixedLineSpacing, 0,
      "fixed line spacing for LinePrinter mode" },
    { "-clip", argFlag, &clipText, 0, "separate clipped text" },
    { "-enc", argString, textEncName, sizeof (textEncName),
      "output text encoding name" },
    { "-eol", argString, textEOL, sizeof (textEOL),
      "output end-of-line convention (unix, dos, or mac)" },
    { "-nopgbrk", argFlag, &noPageBreaks, 0,
      "don't insert page breaks between pages" },
    { "-opw", argString, ownerPassword, sizeof (ownerPassword),
      "owner password (for encrypted files)" },
    { "-upw", argString, userPassword, sizeof (userPassword),
      "user password (for encrypted files)" },
    { "-q", argFlag, &quiet, 0, "don't print any messages or errors" },
    { "-cfg", argString, cfgFileName, sizeof (cfgFileName),
      "configuration file to use in place of .xpdfrc" },
    { "-v", argFlag, &printVersion, 0, "print copyright and version info" },
    { "-h", argFlag, &printHelp, 0, "print usage information" },
    { "-help", argFlag, &printHelp, 0, "print usage information" },
    { "--help", argFlag, &printHelp, 0, "print usage information" },
    { "-?", argFlag, &printHelp, 0, "print usage information" },
    { }
};

int main (int argc, char* argv[]) {
    PDFDoc* doc;
    GString* fileName;
    GString* textFileName;
    GString *ownerPW, *userPW;
    TextOutputControl textOutControl;
    TextOutputDev* textOut;
    UnicodeMap* uMap;
    Object info;
    bool ok;
    const char* p;
    int exitCode;

#ifdef DEBUG_FP_LINUX
    // enable exceptions on floating point div-by-zero
    feenableexcept (FE_DIVBYZERO);
    // force 64-bit rounding: this avoids changes in output when minor
    // code changes result in spills of x87 registers; it also avoids
    // differences in output with valgrind's 64-bit floating point
    // emulation (yes, this is a kludge; but it's pretty much
    // unavoidable given the x87 instruction set; see gcc bug 323 for
    // more info)
    fpu_control_t cw;
    _FPU_GETCW (cw);
    cw = (cw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
    _FPU_SETCW (cw);
#endif

    exitCode = 99;

    // parse args
    ok = parseArgs (argDesc, &argc, argv);
    if (!ok || argc < 2 || argc > 3 || printVersion || printHelp) {
        fprintf (stderr, "pdftotext version %s\n", PACKAGE_VERSION);
        fprintf (stderr, "%s\n", XPDF_COPYRIGHT);
        if (!printVersion) {
            printUsage ("pdftotext", "<PDF-file> [<text-file>]", argDesc);
        }
        goto err0;
    }
    fileName = new GString (argv[1]);

    // read config file
    globalParams = new GlobalParams (cfgFileName);
    if (textEncName[0]) { globalParams->setTextEncoding (textEncName); }
    if (textEOL[0]) {
        if (!globalParams->setTextEOL (textEOL)) {
            fprintf (stderr, "Bad '-eol' value on command line\n");
        }
    }
    if (noPageBreaks) { globalParams->setTextPageBreaks (false); }
    if (quiet) { globalParams->setErrQuiet (quiet); }

    // get mapping to output encoding
    if (!(uMap = globalParams->getTextEncoding ())) {
        error (errConfig, -1, "Couldn't get text encoding");
        delete fileName;
        goto err1;
    }

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
        goto err2;
    }

    // check for copy permission
    if (!doc->okToCopy ()) {
        error (
            errNotAllowed, -1,
            "Copying of text from this document is not allowed.");
        exitCode = 3;
        goto err2;
    }

    // construct text file name
    if (argc == 3) { textFileName = new GString (argv[2]); }
    else {
        p = fileName->c_str () + fileName->getLength () - 4;
        if (!strcmp (p, ".pdf") || !strcmp (p, ".PDF")) {
            textFileName = new GString (
                fileName->c_str (), fileName->getLength () - 4);
        }
        else {
            textFileName = fileName->copy ();
        }
        textFileName->append (".txt");
    }

    // get page range
    if (firstPage < 1) { firstPage = 1; }
    if (lastPage < 1 || lastPage > doc->getNumPages ()) {
        lastPage = doc->getNumPages ();
    }

    // write text file
    if (tableLayout) {
        textOutControl.mode = textOutTableLayout;
        textOutControl.fixedPitch = fixedPitch;
    }
    else if (physLayout) {
        textOutControl.mode = textOutPhysLayout;
        textOutControl.fixedPitch = fixedPitch;
    }
    else if (linePrinter) {
        textOutControl.mode = textOutLinePrinter;
        textOutControl.fixedPitch = fixedPitch;
        textOutControl.fixedLineSpacing = fixedLineSpacing;
    }
    else if (rawOrder) {
        textOutControl.mode = textOutRawOrder;
    }
    else {
        textOutControl.mode = textOutReadingOrder;
    }
    textOutControl.clipText = clipText;
    textOut = new TextOutputDev (
        textFileName->c_str (), &textOutControl, false);
    if (textOut->isOk ()) {
        doc->displayPages (
            textOut, firstPage, lastPage, 72, 72, 0, false, true, false);
    }
    else {
        delete textOut;
        exitCode = 2;
        goto err3;
    }
    delete textOut;

    exitCode = 0;

    // clean up
err3:
    delete textFileName;
err2:
    delete doc;
    uMap->decRefCnt ();
err1:
    delete globalParams;
err0:

    return exitCode;
}

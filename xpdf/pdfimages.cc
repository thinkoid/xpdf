//========================================================================
//
// pdfimages.cc
//
// Copyright 1998-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <goo/parseargs.hh>
#include <goo/GString.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/object.hh>
#include <xpdf/Stream.hh>
#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Catalog.hh>
#include <xpdf/Page.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/ImageOutputDev.hh>
#include <xpdf/Error.hh>
#include <defs.hh>

static int firstPage = 1;
static int lastPage = 0;
static bool dumpJPEG = false;
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static bool quiet = false;
static char cfgFileName[256] = "";
static bool printVersion = false;
static bool printHelp = false;

static ArgDesc argDesc[] = {
    { "-f", argInt, &firstPage, 0, "first page to convert" },
    { "-l", argInt, &lastPage, 0, "last page to convert" },
    { "-j", argFlag, &dumpJPEG, 0, "write JPEG images as JPEG files" },
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
    char* imgRoot;
    GString *ownerPW, *userPW;
    ImageOutputDev* imgOut;
    bool ok;
    int exitCode;

    exitCode = 99;

    // parse args
    ok = parseArgs (argDesc, &argc, argv);
    if (!ok || argc != 3 || printVersion || printHelp) {
        fprintf (stderr, "pdfimages version %s\n", PACKAGE_VERSION);
        fprintf (stderr, "%s\n", XPDF_COPYRIGHT);
        if (!printVersion) {
            printUsage ("pdfimages", "<PDF-file> <image-root>", argDesc);
        }
        goto err0;
    }
    fileName = new GString (argv[1]);
    imgRoot = argv[2];

    // read config file
    globalParams = new GlobalParams (cfgFileName);
    if (quiet) { globalParams->setErrQuiet (quiet); }

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

    // check for copy permission
    if (!doc->okToCopy ()) {
        error (
            errNotAllowed, -1,
            "Copying of images from this document is not allowed.");
        exitCode = 3;
        goto err1;
    }

    // get page range
    if (firstPage < 1) firstPage = 1;
    if (lastPage < 1 || lastPage > doc->getNumPages ())
        lastPage = doc->getNumPages ();

    // write image files
    imgOut = new ImageOutputDev (imgRoot, dumpJPEG);
    if (imgOut->isOk ()) {
        doc->displayPages (
            imgOut, firstPage, lastPage, 72, 72, 0, false, true, false);
    }
    delete imgOut;

    exitCode = 0;

    // clean up
err1:
    delete doc;
    delete globalParams;
err0:

    return exitCode;
}

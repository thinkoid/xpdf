// -*- mode: c++; -*-
// Copyright 1998-2013 Glyph & Cog, LLC

#include <defs.hh>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <cmath>
#include <goo/parseargs.hh>
#include <goo/GString.hh>
#include <goo/gfile.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>
#include <xpdf/array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/XRef.hh>
#include <xpdf/Catalog.hh>
#include <xpdf/Page.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/CharTypes.hh>
#include <xpdf/UnicodeMap.hh>
#include <xpdf/TextString.hh>
#include <xpdf/Error.hh>
#include <defs.hh>

static void printInfoString (
    Dict* infoDict, const char* key, const char* text, UnicodeMap* uMap);
static void printInfoDate (Dict* infoDict, const char* key, const char* text);
static void printBox (const char* text, PDFRectangle* box);

static int firstPage = 1;
static int lastPage = 0;
static bool printBoxes = false;
static bool printMetadata = false;
static bool rawDates = false;
static char textEncName[128] = "";
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static char cfgFileName[256] = "";
static bool printVersion = false;
static bool printHelp = false;

static ArgDesc argDesc[] = {
    { "-f", argInt, &firstPage, 0, "first page to convert" },
    { "-l", argInt, &lastPage, 0, "last page to convert" },
    { "-box", argFlag, &printBoxes, 0, "print the page bounding boxes" },
    { "-meta", argFlag, &printMetadata, 0,
      "print the document metadata (XML)" },
    { "-rawdates", argFlag, &rawDates, 0,
      "print the undecoded date strings directly from the PDF file" },
    { "-enc", argString, textEncName, sizeof (textEncName),
      "output text encoding name" },
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

int main (int argc, char* argv[]) {
    PDFDoc* doc;
    GString* fileName;
    GString *ownerPW, *userPW;
    UnicodeMap* uMap;
    Page* page;
    Object* acroForm;
    char buf[256];
    double w, h, wISO, hISO;
    FILE* f;
    GString* metadata;
    bool ok;
    int exitCode;
    int pg, i;
    bool multiPage;

    exitCode = 99;

    // parse args
    ok = parseArgs (argDesc, &argc, argv);
    if (!ok || argc != 2 || printVersion || printHelp) {
        fprintf (stderr, "pdfinfo version %s\n", PACKAGE_VERSION);
        fprintf (stderr, "%s\n", XPDF_COPYRIGHT);

        if (!printVersion) {
            printUsage ("pdfinfo", "<PDF-file>", argDesc);
        }

        return exitCode;
    }

    fileName = new GString (argv[1]);

    // read config file
    globalParams = new GlobalParams (cfgFileName);
    if (textEncName[0]) { globalParams->setTextEncoding (textEncName); }

    // get mapping to output encoding
    if (!(uMap = globalParams->getTextEncoding ())) {
        error (errConfig, -1, "Couldn't get text encoding");

        delete fileName;
        delete globalParams;

        return exitCode;
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
        uMap->decRefCnt ();

        delete doc;
        delete globalParams;

        return 1;
    }

    // get page range
    if (firstPage < 1) { firstPage = 1; }
    if (lastPage == 0) {
        multiPage = false;
        lastPage = 1;
    }
    else {
        multiPage = true;
    }
    if (lastPage < 1 || lastPage > doc->getNumPages ()) {
        lastPage = doc->getNumPages ();
    }

    // print doc info
    Object info;
    doc->getDocInfo (&info);

    if (info.is_dict ()) {
        printInfoString (info.as_dict_ptr (), "Title", "Title:          ", uMap);
        printInfoString (info.as_dict_ptr (), "Subject", "Subject:        ", uMap);
        printInfoString (info.as_dict_ptr (), "Keywords", "Keywords:       ", uMap);
        printInfoString (info.as_dict_ptr (), "Author", "Author:         ", uMap);
        printInfoString (info.as_dict_ptr (), "Creator", "Creator:        ", uMap);
        printInfoString (info.as_dict_ptr (), "Producer", "Producer:       ", uMap);
        if (rawDates) {
            printInfoString (
                info.as_dict_ptr (), "CreationDate", "CreationDate:   ", uMap);
            printInfoString (
                info.as_dict_ptr (), "ModDate", "ModDate:        ", uMap);
        }
        else {
            printInfoDate (info.as_dict_ptr (), "CreationDate", "CreationDate:   ");
            printInfoDate (info.as_dict_ptr (), "ModDate", "ModDate:        ");
        }
    }

    // print tagging info
    printf (
        "Tagged:         %s\n",
        doc->getStructTreeRoot ()->is_dict () ? "yes" : "no");

    // print form info
    if ((acroForm = doc->getCatalog ()->getAcroForm ())->is_dict ()) {
        Object xfa;
        acroForm->dictLookup ("XFA", &xfa);

        if (xfa.is_stream () || xfa.is_array ()) {
            printf ("Form:           XFA\n");
        }
        else {
            printf ("Form:           AcroForm\n");
        }
    }
    else {
        printf ("Form:           none\n");
    }

    // print page count
    printf ("Pages:          %d\n", doc->getNumPages ());

    // print encryption info
    printf ("Encrypted:      ");
    if (doc->isEncrypted ()) {
        printf (
            "yes (print:%s copy:%s change:%s addNotes:%s)\n",
            doc->okToPrint (true) ? "yes" : "no",
            doc->okToCopy (true) ? "yes" : "no",
            doc->okToChange (true) ? "yes" : "no",
            doc->okToAddNotes (true) ? "yes" : "no");
    }
    else {
        printf ("no\n");
    }

    // print page size
    for (pg = firstPage; pg <= lastPage; ++pg) {
        w = doc->getPageCropWidth (pg);
        h = doc->getPageCropHeight (pg);
        if (multiPage) { printf ("Page %4d size: %g x %g pts", pg, w, h); }
        else {
            printf ("Page size:      %g x %g pts", w, h);
        }
        if ((fabs (w - 612) < 0.1 && fabs (h - 792) < 0.1) ||
            (fabs (w - 792) < 0.1 && fabs (h - 612) < 0.1)) {
            printf (" (letter)");
        }
        else {
            hISO = sqrt (sqrt (2.0)) * 7200 / 2.54;
            wISO = hISO / sqrt (2.0);
            for (i = 0; i <= 6; ++i) {
                if ((fabs (w - wISO) < 1 && fabs (h - hISO) < 1) ||
                    (fabs (w - hISO) < 1 && fabs (h - wISO) < 1)) {
                    printf (" (A%d)", i);
                    break;
                }
                hISO = wISO;
                wISO /= sqrt (2.0);
            }
        }
        printf (" (rotated %d degrees)", doc->getPageRotate (pg));
        printf ("\n");
    }

    // print the boxes
    if (printBoxes) {
        if (multiPage) {
            for (pg = firstPage; pg <= lastPage; ++pg) {
                page = doc->getCatalog ()->getPage (pg);
                sprintf (buf, "Page %4d MediaBox: ", pg);
                printBox (buf, page->getMediaBox ());
                sprintf (buf, "Page %4d CropBox:  ", pg);
                printBox (buf, page->getCropBox ());
                sprintf (buf, "Page %4d BleedBox: ", pg);
                printBox (buf, page->getBleedBox ());
                sprintf (buf, "Page %4d TrimBox:  ", pg);
                printBox (buf, page->getTrimBox ());
                sprintf (buf, "Page %4d ArtBox:   ", pg);
                printBox (buf, page->getArtBox ());
            }
        }
        else {
            page = doc->getCatalog ()->getPage (firstPage);
            printBox ("MediaBox:       ", page->getMediaBox ());
            printBox ("CropBox:        ", page->getCropBox ());
            printBox ("BleedBox:       ", page->getBleedBox ());
            printBox ("TrimBox:        ", page->getTrimBox ());
            printBox ("ArtBox:         ", page->getArtBox ());
        }
    }

    // print file size
    f = fopen (fileName->c_str (), "rb");
    if (f) {
        gfseek (f, 0, SEEK_END);
        printf ("File size:      %u bytes\n", (unsigned)gftell (f));
        fclose (f);
    }

    // print linearization info
    printf ("Optimized:      %s\n", doc->isLinearized () ? "yes" : "no");

    // print PDF version
    printf ("PDF version:    %.1f\n", doc->getPDFVersion ());

    // print the metadata
    if (printMetadata && (metadata = doc->readMetadata ())) {
        fputs ("Metadata:\n", stdout);
        fputs (metadata->c_str (), stdout);
        fputc ('\n', stdout);
        delete metadata;
    }

    exitCode = 0;

    // clean up
    uMap->decRefCnt ();
    delete doc;

    delete globalParams;

    return exitCode;
}

static void printInfoString (
    Dict* infoDict, const char* key, const char* text, UnicodeMap* uMap) {
    TextString* s;
    Unicode* u;
    char buf[8];
    int i, n;

    Object obj;

    if (infoDict->lookup (key, &obj)->is_string ()) {
        fputs (text, stdout);
        s = new TextString (obj.as_string ());
        u = s->getUnicode ();
        for (i = 0; i < s->getLength (); ++i) {
            n = uMap->mapUnicode (u[i], buf, sizeof (buf));
            fwrite (buf, 1, n, stdout);
        }
        fputc ('\n', stdout);
        delete s;
    }
}

static void printInfoDate (Dict* infoDict, const char* key, const char* text) {
    const char* s;
    int year, mon, day, hour, min, sec, n;
    struct tm tmStruct;
    char buf[256];

    Object obj;

    if (infoDict->lookup (key, &obj)->is_string ()) {
        fputs (text, stdout);
        s = obj.as_string ()->c_str ();
        if (s[0] == 'D' && s[1] == ':') { s += 2; }
        if ((n = sscanf (
                 s, "%4d%2d%2d%2d%2d%2d", &year, &mon, &day, &hour, &min,
                 &sec)) >= 1) {
            switch (n) {
            case 1: mon = 1;
            case 2: day = 1;
            case 3: hour = 0;
            case 4: min = 0;
            case 5: sec = 0;
            }
            tmStruct.tm_year = year - 1900;
            tmStruct.tm_mon = mon - 1;
            tmStruct.tm_mday = day;
            tmStruct.tm_hour = hour;
            tmStruct.tm_min = min;
            tmStruct.tm_sec = sec;
            tmStruct.tm_wday = -1;
            tmStruct.tm_yday = -1;
            tmStruct.tm_isdst = -1;
            // compute the tm_wday and tm_yday fields
            if (mktime (&tmStruct) != (time_t)-1 &&
                strftime (buf, sizeof (buf), "%c", &tmStruct)) {
                fputs (buf, stdout);
            }
            else {
                fputs (s, stdout);
            }
        }
        else {
            fputs (s, stdout);
        }
        fputc ('\n', stdout);
    }
}

static void printBox (const char* text, PDFRectangle* box) {
    printf (
        "%s%8.2f %8.2f %8.2f %8.2f\n", text, box->x1, box->y1, box->x2,
        box->y2);
}

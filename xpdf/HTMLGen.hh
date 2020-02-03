// -*- mode: c++; -*-
// Copyright 2010 Glyph & Cog, LLC

#ifndef XPDF_XPDF_HTMLGEN_HH
#define XPDF_XPDF_HTMLGEN_HH

#include <defs.hh>

class PDFDoc;
class TextOutputDev;
struct TextFontInfo;
class SplashOutputDev;

//------------------------------------------------------------------------

class HTMLGen {
public:
    HTMLGen (double backgroundResolutionA);
    ~HTMLGen ();

    bool isOk () { return ok; }

    double getBackgroundResolution () { return backgroundResolution; }
    void setBackgroundResolution (double backgroundResolutionA) {
        backgroundResolution = backgroundResolutionA;
    }

    bool getDrawInvisibleText () { return drawInvisibleText; }
    void setDrawInvisibleText (bool drawInvisibleTextA) {
        drawInvisibleText = drawInvisibleTextA;
    }

    void startDoc (PDFDoc* docA);
    int convertPage (
        int pg, const char* pngURL,
        int (*writeHTML) (void* stream, const char* data, int size),
        void* htmlStream,
        int (*writePNG) (void* stream, const char* data, int size),
        void* pngStream);

private:
    GString* getFontDefn (TextFontInfo* font, double* scale);

    double backgroundResolution;
    bool drawInvisibleText;

    PDFDoc* doc;
    TextOutputDev* textOut;
    SplashOutputDev* splashOut;

    bool ok;
};

#endif // XPDF_XPDF_HTMLGEN_HH

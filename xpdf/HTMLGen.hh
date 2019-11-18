//========================================================================
//
// HTMLGen.h
//
// Copyright 2010 Glyph & Cog, LLC
//
//========================================================================

#ifndef HTMLGEN_H
#define HTMLGEN_H

#include <defs.hh>

class GString;
class PDFDoc;
class TextOutputDev;
class TextFontInfo;
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

#endif

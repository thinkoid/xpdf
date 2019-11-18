//========================================================================
//
// XFAForm.h
//
// Copyright 2012 Glyph & Cog, LLC
//
//========================================================================

#ifndef XFAFORM_H
#define XFAFORM_H

#include <defs.hh>

#include <xpdf/Form.hh>

class ZxDoc;
class ZxElement;
class ZxAttr;

//------------------------------------------------------------------------

enum XFAHorizAlign { xfaHAlignLeft, xfaHAlignCenter, xfaHAlignRight };

enum XFAVertAlign { xfaVAlignTop, xfaVAlignBottom, xfaVAlignMiddle };

//------------------------------------------------------------------------

class XFAForm : public Form {
public:
    static XFAForm* load (PDFDoc* docA, Object* acroFormObj, Object* xfaObj);

    virtual ~XFAForm ();

    virtual const char* getType () { return "XFA"; }

    virtual void draw (int pageNum, Gfx* gfx, bool printing);

    virtual int getNumFields ();
    virtual FormField* getField (int idx);

private:
    XFAForm (PDFDoc* docA, ZxDoc* xmlA, Object* resourceDictA, bool fullXFAA);
    void scanFields (ZxElement* elem, GString* name, GString* dataName);

    ZxDoc* xml;
    GList* fields; // [XFAFormField]
    Object resourceDict;
    bool fullXFA;     // true for "Full XFA", false for
                       //   "XFA Foreground"
    int curPageNum;    // current page number - used by scanFields()
    double curXOffset, // current x,y offset - used by scanFields()
        curYOffset;

    friend class XFAFormField;
};

//------------------------------------------------------------------------

class XFAFormField : public FormField {
public:
    XFAFormField (
        XFAForm* xfaFormA, ZxElement* xmlA, GString* nameA, GString* dataNameA,
        int pageNumA, double xOffsetA, double yOffsetA);

    virtual ~XFAFormField ();

    virtual const char* getType ();
    virtual Unicode* getName (int* length);
    virtual Unicode* getValue (int* length);

    virtual Object* getResources (Object* res);

private:
    Unicode* utf8ToUnicode (GString* s, int* length);
    void draw (int pageNumA, Gfx* gfx, bool printing, GfxFontDict* fontDict);
    void drawTextEdit (
        GfxFontDict* fontDict, double w, double h, int rot, GString* appearBuf);
    void drawBarCode (
        GfxFontDict* fontDict, double w, double h, int rot, GString* appearBuf);
    static double getMeasurement (ZxAttr* attr, double defaultVal);
    GString* getFieldValue (const char* valueChildType);
    ZxElement* findFieldData (ZxElement* elem, const char* partName);
    void transform (
        int rot, double w, double h, double* wNew, double* hNew,
        GString* appearBuf);
    void drawText (
        GString* text, bool multiLine, int combCells, GString* fontName,
        bool bold, bool italic, double fontSize, XFAHorizAlign hAlign,
        XFAVertAlign vAlign, double x, double y, double w, double h,
        bool whiteBackground, GfxFontDict* fontDict, GString* appearBuf);
    GfxFont* findFont (
        GfxFontDict* fontDict, GString* fontName, bool bold, bool italic);
    void getNextLine (
        GString* text, int start, GfxFont* font, double fontSize, double wMax,
        int* end, double* width, int* next);

    XFAForm* xfaForm;
    ZxElement* xml;
    GString* name;
    GString* dataName;
    int pageNum;
    double xOffset, yOffset;

    friend class XFAForm;
};

#endif

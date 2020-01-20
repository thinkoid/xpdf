// -*- mode: c++; -*-
// Copyright 2012 Glyph & Cog, LLC

#ifndef XPDF_XPDF_XFAFORM_HH
#define XPDF_XPDF_XFAFORM_HH

#include <defs.hh>

#include <memory>
#include <vector>

#include <xpdf/Form.hh>

class ZxDoc;
class ZxElement;
class ZxAttr;

//------------------------------------------------------------------------

enum XFAHorizAlign { xfaHAlignLeft, xfaHAlignCenter, xfaHAlignRight };

enum XFAVertAlign { xfaVAlignTop, xfaVAlignBottom, xfaVAlignMiddle };

//------------------------------------------------------------------------

class XFAForm;

class XFAFormField : public FormField {
public:
    XFAFormField (
        XFAForm* xfaFormA, ZxElement* xmlA, GString* nameA, GString* dataNameA,
        int pageNumA, double xOffsetA, double yOffsetA);

    virtual ~XFAFormField ();

    virtual const char* getType ();
    virtual Unicode* as_name (int* length);
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

//------------------------------------------------------------------------

class XFAFormField;

class XFAForm : public Form {
public:
    static XFAForm* load (PDFDoc* docA, Object* acroFormObj, Object* xfaObj);

    ~XFAForm ();

    const char* getType () { return "XFA"; }

    void draw (int pageNum, Gfx* gfx, bool printing);

    size_t getNumFields () const {
        return fields.size ();
    }

    FormField* getField (size_t i) const {
        ASSERT (i < fields.size ());
        return fields [i].get ();
    }

private:
    XFAForm (PDFDoc* docA, ZxDoc* xmlA, Object* resourceDictA, bool fullXFAA);
    void scanFields (ZxElement* elem, GString* name, GString* dataName);

    ZxDoc* xml;
    std::vector< std::unique_ptr< XFAFormField > > fields;
    Object resourceDict;

    //
    // current x,y offset - used by scanFields ()
    //
    double curXOffset, curYOffset;

    //
    // current page number - used by scanFields()
    //
    int curPageNum;

    //
    // true for "Full XFA", false for "XFA Foreground"
    //
    bool fullXFA;

private:
    friend class XFAFormField;
};

#endif // XPDF_XPDF_XFAFORM_HH

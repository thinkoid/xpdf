//========================================================================
//
// AcroForm.h
//
// Copyright 2012 Glyph & Cog, LLC
//
//========================================================================

#ifndef ACROFORM_H
#define ACROFORM_H

#include <defs.hh>

#include <memory>
#include <vector>

#include <xpdf/Form.hh>
#include <xpdf/object.hh>

class AcroForm;
class TextString;
class GfxFont;
class GfxFontDict;

////////////////////////////////////////////////////////////////////////

enum AcroFormFieldType {
    acroFormFieldPushbutton,
    acroFormFieldRadioButton,
    acroFormFieldCheckbox,
    acroFormFieldFileSelect,
    acroFormFieldMultilineText,
    acroFormFieldText,
    acroFormFieldComboBox,
    acroFormFieldListBox,
    acroFormFieldSignature
};

class AcroFormField : public FormField {
public:
    static AcroFormField* load (AcroForm* acroFormA, Object* fieldRefA);

    virtual ~AcroFormField ();

    virtual const char* getType ();

    virtual Unicode* getName (int* length);
    virtual Unicode* getValue (int* length);

    virtual Object* getResources (Object* res);

private:
    AcroFormField (
        AcroForm* acroFormA, Object* fieldRefA, Object* fieldObjA,
        AcroFormFieldType typeA, TextString* nameA, unsigned flagsA);
    void draw (int pageNum, Gfx* gfx, bool printing);
    void drawAnnot (
        int pageNum, Gfx* gfx, bool printing, Object* annotRef,
        Object* annotObj);
    void drawExistingAppearance (
        Gfx* gfx, Dict* annot, double xMin, double yMin, double xMax,
        double yMax);
    void drawNewAppearance (
        Gfx* gfx, Dict* annot, double xMin, double yMin, double xMax,
        double yMax);
    void setColor (Array* a, bool fill, int adjust);
    void drawText (
        GString* text, GString* da, GfxFontDict* fontDict, bool multiline,
        int comb, int quadding, bool txField, bool forceZapfDingbats, int rot,
        double xMin, double yMin, double xMax, double yMax, double border);
    void drawListBox (
        GString** text, bool* selection, int nOptions, int topIdx, GString* da,
        GfxFontDict* fontDict, int quadding, double xMin, double yMin,
        double xMax, double yMax, double border);
    void getNextLine (
        GString* text, int start, GfxFont* font, double fontSize, double wMax,
        int* end, double* width, int* next);
    void drawCircle (double cx, double cy, double r, const char* cmd);
    void drawCircleTopLeft (double cx, double cy, double r);
    void drawCircleBottomRight (double cx, double cy, double r);
    Object* getAnnotResources (Dict* annot, Object* res);
    Object* fieldLookup (const char* key, Object* obj);
    Object* fieldLookup (Dict* dict, const char* key, Object* obj);

    AcroForm* acroForm;
    Object fieldRef;
    Object fieldObj;
    AcroFormFieldType type;
    TextString* name;
    unsigned flags;
    std::string appearBuf;

    friend class AcroForm;
};

//------------------------------------------------------------------------

class AcroFormAnnotPage;

class AcroForm : public Form {
public:
    virtual ~AcroForm ();

    const char* getType () { return "AcroForm"; }

    void draw (int pageNum, Gfx* gfx, bool printing);

    size_t getNumFields () const { return fields.size (); }

    FormField* getField (size_t i) const {
        ASSERT (i < fields.size ());
        return fields [i].get ();
    }

public:
    static AcroForm* load (PDFDoc*, Catalog*, Object*);

private:
    friend class AcroFormField;

    AcroForm (PDFDoc*, Object*);

    void buildAnnotPageList (Catalog* catalog);
    int lookupAnnotPage (Object* annotRef);
    void scanField (Object* fieldRef);

private:
    Object acroFormObj;
    bool needAppearances;

    std::vector< std::unique_ptr< AcroFormAnnotPage > > annotPages;
    std::vector< std::unique_ptr< AcroFormField > > fields;
};

//------------------------------------------------------------------------

#endif

// -*- mode: c++; -*-
// Copyright 2012 Glyph & Cog, LLC

#ifndef XPDF_XPDF_FORM_HH
#define XPDF_XPDF_FORM_HH

#include <defs.hh>

#include <xpdf/CharTypes.hh>
#include <xpdf/obj_fwd.hh>

class Catalog;
class FormField;
class Gfx;
class PDFDoc;

//------------------------------------------------------------------------

class Form
{
public:
    static Form *load(PDFDoc *docA, Catalog *catalog, Object *acroFormObj);

    virtual ~Form();

    virtual const char *getType() = 0;

    virtual void draw(int pageNum, Gfx *gfx, bool printing) = 0;

    virtual size_t     getNumFields() const = 0;
    virtual FormField *getField(size_t) const = 0;

protected:
    Form(PDFDoc *docA);
    PDFDoc *doc;
};

//------------------------------------------------------------------------

class FormField
{
public:
    FormField();
    virtual ~FormField();

    virtual const char *getType() = 0;
    virtual Unicode *   as_name(int *length) = 0;
    virtual Unicode *   getValue(int *length) = 0;

    // Return the resource dictionaries used to draw this field.  The
    // returned object must be either a dictionary or an array of
    // dictonaries.
    virtual Object *getResources(Object *res) = 0;
};

#endif // XPDF_XPDF_FORM_HH

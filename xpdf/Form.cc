// -*- mode: c++; -*-
// Copyright 2012 Glyph & Cog, LLC

#include <defs.hh>

#include <xpdf/GlobalParams.hh>
#include <xpdf/Error.hh>
#include <xpdf/ast.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/AcroForm.hh>
#include <xpdf/XFAForm.hh>
#include <xpdf/Form.hh>

//------------------------------------------------------------------------
// Form
//------------------------------------------------------------------------

Form* Form::load (PDFDoc* docA, Catalog* catalog, Object* acroFormObj) {
    Form* form;
    Object xfaObj, catDict, needsRenderingObj;

    if (!acroFormObj->is_dict ()) {
        error (errSyntaxError, -1, "AcroForm object is wrong type");
        return NULL;
    }
    //~ temporary: create an XFAForm only for XFAF, not for dynamic XFA
    acroFormObj->dictLookup ("XFA", &xfaObj);
    docA->getXRef ()->getCatalog (&catDict);
    catDict.dictLookup ("NeedsRendering", &needsRenderingObj);
    if (globalParams->getEnableXFA () && !xfaObj.is_null () &&
        !(needsRenderingObj.is_bool () && needsRenderingObj.as_bool ())) {
        form = XFAForm::load (docA, acroFormObj, &xfaObj);
    }
    else {
        form = AcroForm::load (docA, catalog, acroFormObj);
    }
    return form;
}

Form::Form (PDFDoc* docA) { doc = docA; }

Form::~Form () {}

//------------------------------------------------------------------------
// FormField
//------------------------------------------------------------------------

FormField::FormField () {}

FormField::~FormField () {}

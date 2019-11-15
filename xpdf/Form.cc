//========================================================================
//
// Form.cc
//
// Copyright 2012 Glyph & Cog, LLC
//
//========================================================================

#include <config.hh>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <xpdf/GlobalParams.hh>
#include <xpdf/Error.hh>
#include <xpdf/Object.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/AcroForm.hh>
#include <xpdf/XFAForm.hh>
#include <xpdf/Form.hh>

//------------------------------------------------------------------------
// Form
//------------------------------------------------------------------------

Form *Form::load(PDFDoc *docA, Catalog *catalog, Object *acroFormObj) {
  Form *form;
  Object xfaObj, catDict, needsRenderingObj;

  if (!acroFormObj->isDict()) {
    error(errSyntaxError, -1, "AcroForm object is wrong type");
    return NULL;
  }
  //~ temporary: create an XFAForm only for XFAF, not for dynamic XFA
  acroFormObj->dictLookup("XFA", &xfaObj);
  docA->getXRef()->getCatalog(&catDict);
  catDict.dictLookup("NeedsRendering", &needsRenderingObj);
  catDict.free();
  if (globalParams->getEnableXFA() &&
      !xfaObj.isNull() &&
      !(needsRenderingObj.isBool() && needsRenderingObj.getBool())) {
    form = XFAForm::load(docA, acroFormObj, &xfaObj);
  } else {
    form = AcroForm::load(docA, catalog, acroFormObj);
  }
  xfaObj.free();
  needsRenderingObj.free();
  return form;
}

Form::Form(PDFDoc *docA) {
  doc = docA;
}

Form::~Form() {
}

//------------------------------------------------------------------------
// FormField
//------------------------------------------------------------------------

FormField::FormField() {
}

FormField::~FormField() {
}

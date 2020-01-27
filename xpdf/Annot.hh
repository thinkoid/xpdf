// -*- mode: c++; -*-
// Copyright 2000-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_ANNOT_HH
#define XPDF_XPDF_ANNOT_HH

#include <defs.hh>

#include <xpdf/obj_fwd.hh>
#include <xpdf/XRef.hh>

class Catalog;
class Gfx;
class GfxFontDict;
class PDFDoc;
class GString;

//------------------------------------------------------------------------
// AnnotBorderStyle
//------------------------------------------------------------------------

enum AnnotBorderType {
    annotBorderSolid,
    annotBorderDashed,
    annotBorderBeveled,
    annotBorderInset,
    annotBorderUnderlined
};

class AnnotBorderStyle {
public:
    AnnotBorderStyle (
        AnnotBorderType typeA, double widthA, double* dashA, int dashLengthA,
        double* colorA, int nColorCompsA);
    ~AnnotBorderStyle ();

    AnnotBorderType getType () { return type; }
    double getWidth () { return width; }
    void getDash (double** dashA, int* dashLengthA) {
        *dashA = dash;
        *dashLengthA = dashLength;
    }
    int getNumColorComps () { return nColorComps; }
    double* getColor () { return color; }

private:
    AnnotBorderType type;
    double width;
    double* dash;
    int dashLength;
    double color[4];
    int nColorComps;
};

//------------------------------------------------------------------------

enum AnnotLineEndType {
    annotLineEndNone,
    annotLineEndSquare,
    annotLineEndCircle,
    annotLineEndDiamond,
    annotLineEndOpenArrow,
    annotLineEndClosedArrow,
    annotLineEndButt,
    annotLineEndROpenArrow,
    annotLineEndRClosedArrow,
    annotLineEndSlash
};

//------------------------------------------------------------------------
// Annot
//------------------------------------------------------------------------

class Annot {
public:
    Annot (PDFDoc* docA, Dict* dict, Ref* refA);
    ~Annot ();
    bool isOk () { return ok; }

    void draw (Gfx* gfx, bool printing);

    GString* getType () { return type; }
    double getXMin () { return xMin; }
    double getYMin () { return yMin; }
    double getXMax () { return xMax; }
    double getYMax () { return yMax; }
    Object* getObject (Object* obj);

    // Get appearance object.
    Object getAppearance () { return resolve (appearance); }

    AnnotBorderStyle* getBorderStyle () { return borderStyle; }

    bool match (Ref* refA) {
        return ref.num == refA->num && ref.gen == refA->gen;
    }

    void generateAnnotAppearance ();

private:
    void generateLineAppearance ();
    void generatePolyLineAppearance ();
    void generatePolygonAppearance ();
    void setLineStyle (AnnotBorderStyle* bs, double* lineWidth);
    void setStrokeColor (double* color, int nComps);
    bool setFillColor (Object* colorObj);
    AnnotLineEndType parseLineEndType (const Object& obj);
    void adjustLineEndpoint (
        AnnotLineEndType lineEnd, double x, double y, double dx, double dy,
        double w, double* tx, double* ty);
    void drawLineArrow (
        AnnotLineEndType lineEnd, double x, double y, double dx, double dy,
        double w, bool fill);
    void drawCircle (double cx, double cy, double r, const char* cmd);
    void drawCircleTopLeft (double cx, double cy, double r);
    void drawCircleBottomRight (double cx, double cy, double r);

    PDFDoc* doc;
    XRef* xref;               // the xref table for this PDF file
    Ref ref;                  // object ref identifying this annotation
    GString* type;            // annotation type
    GString* appearanceState; // appearance state name
    Object appearance;        // a reference to the Form XObject stream
                              //   for the normal appearance
    GString* appearBuf;
    double xMin, yMin, // annotation rectangle
        xMax, yMax;
    unsigned flags;
    AnnotBorderStyle* borderStyle;
    Object ocObj; // optional content entry
    bool ok;
};

//------------------------------------------------------------------------
// Annots
//------------------------------------------------------------------------

class Annots {
public:
    // Build a list of Annot objects.
    Annots (PDFDoc*, const Object&);

    ~Annots ();

    // Iterate through list of annotations.
    int getNumAnnots () { return nAnnots; }
    Annot* getAnnot (int i) { return annots[i]; }

    // Generate an appearance stream for any non-form-field annotation
    // that is missing it.
    void generateAnnotAppearances ();

private:
    void
    scanFieldAppearances (Dict* node, Ref* ref, Dict* parent, Dict* acroForm);
    Annot* findAnnot (Ref* ref);

    PDFDoc* doc;
    Annot** annots;
    int nAnnots;
};

#endif // XPDF_XPDF_ANNOT_HH

// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_GFX_HH
#define XPDF_XPDF_GFX_HH

#include <defs.hh>

#include <vector>

#include <utils/path.hh>

#include <xpdf/array_fwd.hh>
#include <xpdf/function.hh>
#include <xpdf/obj.hh>

class AnnotBorderStyle;
class GList;
class Gfx;
class GfxAxialShading;
class GfxColorSpace;
class GfxFont;
class GfxFontDict;
class GfxFunctionShading;
class GfxGouraudTriangleShading;
class GfxPatchMeshShading;
class GfxPattern;
class GfxRadialShading;
class GfxShading;
class GfxShadingPattern;
class GfxState;
class GfxTilingPattern;
class OutputDev;
class PDFDoc;
class PDFRectangle;
class Parser;
class StreamBase;
class XRef;

struct GfxColor;
struct GfxPatch;

//------------------------------------------------------------------------

enum GfxClipType { clipNone, clipNormal, clipEO };

#define maxArgs 33

//------------------------------------------------------------------------

class GfxResources
{
public:
    GfxResources(XRef *xref, Dict *resDict, GfxResources *nextA);
    ~GfxResources();

    GfxFont *   lookupFont(const char *name);
    GfxFont *   lookupFontByRef(Ref ref);
    bool        lookupXObject(const char *name, Object *obj);
    bool        lookupXObjectNF(const char *name, Object *obj);
    void        lookupColorSpace(const char *name, Object *obj);
    GfxPattern *lookupPattern(const char *name);
    GfxShading *lookupShading(const char *name);
    bool        lookupGState(const char *name, Object *obj);
    bool        lookupPropertiesNF(const char *name, Object *obj);

    GfxResources *getNext() { return next; }

private:
    GfxFontDict * fonts;
    Object        xObjDict;
    Object        colorSpaceDict;
    Object        patternDict;
    Object        shadingDict;
    Object        gStateDict;
    Object        propsDict;
    GfxResources *next;
};

//------------------------------------------------------------------------
// GfxMarkedContent
//------------------------------------------------------------------------

enum GfxMarkedContentKind { gfxMCOptionalContent, gfxMCActualText, gfxMCOther };

class GfxMarkedContent
{
public:
    GfxMarkedContent(GfxMarkedContentKind kindA, bool ocStateA)
    {
        kind = kindA;
        ocState = ocStateA;
    }
    ~GfxMarkedContent() { }

    GfxMarkedContentKind kind;
    bool                 ocState; // true if drawing is enabled, false if
        //   disabled
};

//------------------------------------------------------------------------
// Gfx
//------------------------------------------------------------------------

class Gfx
{
public:
    // Constructor for regular output.
    Gfx(PDFDoc *docA, OutputDev *outA, int pageNum, Dict *resDict, double hDPI,
        double vDPI, PDFRectangle *box, PDFRectangle *cropBox, int rotate,
        bool (*abortCheckCbkA)(void *data) = NULL,
        void *abortCheckCbkDataA = NULL);

    // Constructor for a sub-page object.
    Gfx(PDFDoc *docA, OutputDev *outA, Dict *resDict, PDFRectangle *box,
        PDFRectangle *cropBox, bool (*abortCheckCbkA)(void *data) = NULL,
        void *        abortCheckCbkDataA = NULL);

    ~Gfx();

    // Interpret a stream or array of streams.  <objRef> should be a
    // reference wherever possible (for loop-checking).
    // TODO: sanitize interface
    void display(Object *objRef, bool topLevel = true);

    // Display an annotation, given its appearance (a Form XObject),
    // border style, and bounding box (in default user space).
    void drawAnnot(Object *strRef, AnnotBorderStyle *borderStyle, double xMin,
                   double yMin, double xMax, double yMax);

    // Save graphics state.
    void saveState();

    // Restore graphics state.
    void restoreState();

    // Get the current graphics state object.
    GfxState *getState() { return state; }

    void drawForm(Object *strRef, Dict *resDict, double *matrix, double *bbox,
                  bool transpGroup = false, bool softMask = false,
                  GfxColorSpace *blendingColorSpace = NULL, bool isolated = false,
                  bool knockout = false, bool alpha = false,
                  const Function &transferFunc = {},
                  GfxColor *      backdropColor = NULL);

    // Take all of the content stream stack entries from <oldGfx>.  This
    // is useful when creating a new Gfx object to handle a pattern,
    // etc., where it's useful to check for loops that span both Gfx
    // objects.  This function should be called immediately after the
    // Gfx constructor, i.e., before processing any content streams with
    // the new Gfx object.
    void takeContentStreamStack(Gfx *oldGfx);

private:
    PDFDoc *      doc;
    XRef *        xref; // the xref table for this PDF file
    OutputDev *   out; // output device
    bool          subPage; // is this a sub-page object?
    bool          printCommands; // print the drawing commands (for debugging)
    GfxResources *res; // resource stack
    int           updateLevel;

    GfxState *  state; // current graphics state
    bool        fontChanged; // set if font or text matrix has changed
    GfxClipType clip; // do a clip?
    int         ignoreUndef; // current BX/EX nesting level
    double      baseMatrix[6]; // default matrix for most recent
        //   page/form/pattern
    int  formDepth;
    bool textClipBBoxEmpty; // true if textClipBBox has not been
        //   initialized yet
    bool ocState; // true if drawing is enabled, false if
        //   disabled
    GList *markedContentStack; // BMC/BDC/EMC stack [GfxMarkedContent]

    Parser *parser; // parser for page content stream(s)

    std::vector< Object > contentStreamStack;
    // GList* contentStreamStack; // stack of open content streams, used
    //                            //   for loop-checking

    enum typeCheckType {
        typeCheckBool, // boolean
        typeCheckInt, // integer
        typeCheckNum, // number (integer or real)
        typeCheckString, // string
        typeCheckName, // name
        typeCheckArray, // array
        typeCheckProps, // properties (dictionary or name)
        typeCheckSCN, // scn/SCN args (number of name)
        typeCheckNone // used to avoid empty initializer lists
    };

    struct Operator
    {
        char          name[4];
        int           numArgs;
        typeCheckType tchk[maxArgs];
        void (Gfx::*func)(Object *, int);
    };

    static Operator opTab[];

    // callback to check for an abort
    bool (*abortCheckCbk)(void *data);
    void *abortCheckCbkData;

    bool        checkForContentStreamLoop(Object *ref);
    void        go(bool topLevel);
    bool        execOp(Object *cmd, Object args[], int numArgs);
    Operator *  findOp(const char *name);
    bool        checkArg(Object *arg, typeCheckType type);
    off_t tellg();

    // graphics state operators
    void opSave(Object args[], int numArgs);
    void opRestore(Object args[], int numArgs);
    void opConcat(Object args[], int numArgs);
    void opSetDash(Object args[], int numArgs);
    void opSetFlat(Object args[], int numArgs);
    void opSetLineJoin(Object args[], int numArgs);
    void opSetLineCap(Object args[], int numArgs);
    void opSetMiterLimit(Object args[], int numArgs);
    void opSetLineWidth(Object args[], int numArgs);
    void opSetExtGState(Object args[], int numArgs);
    void doSoftMask(Object *str, Object *strRef, bool alpha,
                    GfxColorSpace *blendingColorSpace, bool isolated,
                    bool knockout, const Function &transferFunc,
                    GfxColor *backdropColor);
    void opSetRenderingIntent(Object args[], int numArgs);

    // color operators
    void opSetFillGray(Object args[], int numArgs);
    void opSetStrokeGray(Object args[], int numArgs);
    void opSetFillCMYKColor(Object args[], int numArgs);
    void opSetStrokeCMYKColor(Object args[], int numArgs);
    void opSetFillRGBColor(Object args[], int numArgs);
    void opSetStrokeRGBColor(Object args[], int numArgs);
    void opSetFillColorSpace(Object args[], int numArgs);
    void opSetStrokeColorSpace(Object args[], int numArgs);
    void opSetFillColor(Object args[], int numArgs);
    void opSetStrokeColor(Object args[], int numArgs);
    void opSetFillColorN(Object args[], int numArgs);
    void opSetStrokeColorN(Object args[], int numArgs);

    // path segment operators
    void opMoveTo(Object args[], int numArgs);
    void opLineTo(Object args[], int numArgs);
    void opCurveTo(Object args[], int numArgs);
    void opCurveTo1(Object args[], int numArgs);
    void opCurveTo2(Object args[], int numArgs);
    void opRectangle(Object args[], int numArgs);
    void opClosePath(Object args[], int numArgs);

    // path painting operators
    void opEndPath(Object args[], int numArgs);
    void opStroke(Object args[], int numArgs);
    void opCloseStroke(Object args[], int numArgs);
    void opFill(Object args[], int numArgs);
    void opEOFill(Object args[], int numArgs);
    void opFillStroke(Object args[], int numArgs);
    void opCloseFillStroke(Object args[], int numArgs);
    void opEOFillStroke(Object args[], int numArgs);
    void opCloseEOFillStroke(Object args[], int numArgs);
    void doPatternFill(bool eoFill);
    void doPatternStroke();
    void doPatternText();
    void doPatternImageMask(Object *ref, StreamBase *str, int width, int height,
                            bool invert, bool inlineImg, bool interpolate);
    void doTilingPatternFill(GfxTilingPattern *tPat, bool stroke, bool eoFill,
                             bool text);
    void doShadingPatternFill(GfxShadingPattern *sPat, bool stroke, bool eoFill,
                              bool text);
    void opShFill(Object args[], int numArgs);
    void doFunctionShFill(GfxFunctionShading *shading);
    void doFunctionShFill1(GfxFunctionShading *shading, double x0, double y0,
                           double x1, double y1, GfxColor *colors, int depth);
    void doAxialShFill(GfxAxialShading *shading);
    void doRadialShFill(GfxRadialShading *shading);
    void doGouraudTriangleShFill(GfxGouraudTriangleShading *shading);
    void gouraudFillTriangle(double x0, double y0, double *color0, double x1,
                             double y1, double *color1, double x2, double y2,
                             double *color2, GfxGouraudTriangleShading *shading,
                             int depth);
    void doPatchMeshShFill(GfxPatchMeshShading *shading);
    void fillPatch(GfxPatch *patch, GfxPatchMeshShading *shading, int depth);
    void doEndPath();

    // path clipping operators
    void opClip(Object args[], int numArgs);
    void opEOClip(Object args[], int numArgs);

    // text object operators
    void opBeginText(Object args[], int numArgs);
    void opEndText(Object args[], int numArgs);

    // text state operators
    void opSetCharSpacing(Object args[], int numArgs);
    void opSetFont(Object args[], int numArgs);
    void doSetFont(GfxFont *font, double size);
    void opSetTextLeading(Object args[], int numArgs);
    void opSetTextRender(Object args[], int numArgs);
    void opSetTextRise(Object args[], int numArgs);
    void opSetWordSpacing(Object args[], int numArgs);
    void opSetHorizScaling(Object args[], int numArgs);

    // text positioning operators
    void opTextMove(Object args[], int numArgs);
    void opTextMoveSet(Object args[], int numArgs);
    void opSetTextMatrix(Object args[], int numArgs);
    void opTextNextLine(Object args[], int numArgs);

    // text string operators
    void opShowText(Object args[], int numArgs);
    void opMoveShowText(Object args[], int numArgs);
    void opMoveSetShowText(Object args[], int numArgs);
    void opShowSpaceText(Object args[], int numArgs);
    void doShowText(GString *s);
    void doIncCharCount(GString *s);

    // XObject operators
    void opXObject(Object args[], int numArgs);
    void doImage(Object *ref, StreamBase *str, bool inlineImg);
    void doForm(Object *strRef, Object *str);

    // in-line image operators
    void    opBeginImage(Object args[], int numArgs);
    StreamBase *buildImageStream();
    void    opImageData(Object args[], int numArgs);
    void    opEndImage(Object args[], int numArgs);

    // type 3 font operators
    void opSetCharWidth(Object args[], int numArgs);
    void opSetCacheDevice(Object args[], int numArgs);

    // compatibility operators
    void opBeginIgnoreUndef(Object args[], int numArgs);
    void opEndIgnoreUndef(Object args[], int numArgs);

    // marked content operators
    void opBeginMarkedContent(Object args[], int numArgs);
    void opEndMarkedContent(Object args[], int numArgs);
    void opMarkPoint(Object args[], int numArgs);

    GfxState *saveStateStack();
    void      restoreStateStack(GfxState *oldState);
    void      pushResources(Dict *resDict);
    void      popResources();
};

#endif // XPDF_XPDF_GFX_HH

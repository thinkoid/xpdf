// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_PSOUTPUTDEV_HH
#define XPDF_XPDF_PSOUTPUTDEV_HH

#include <defs.hh>

#include <cstddef>

#include <xpdf/function.hh>
#include <xpdf/object.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/OutputDev.hh>

class GHash;
class PDFDoc;
class XRef;
class GfxPath;
class GfxFont;
class GfxColorSpace;
class GfxSeparationColorSpace;
class PDFRectangle;
class PSOutCustomColor;
class PSOutputDev;
class PSFontFileInfo;

//------------------------------------------------------------------------
// PSOutputDev
//------------------------------------------------------------------------

enum PSOutMode { psModePS, psModeEPS, psModeForm };

enum PSFileType {
    psFile,   // write to file
    psPipe,   // write to pipe
    psStdout, // write to stdout
    psGeneric // write to a generic stream
};

enum PSOutCustomCodeLocation { psOutCustomDocSetup, psOutCustomPageSetup };

typedef void (*PSOutputFunc) (void* stream, const char* data, int len);

typedef GString* (*PSOutCustomCodeCbk) (
    PSOutputDev* psOut, PSOutCustomCodeLocation loc, int n, void* data);

class PSOutputDev : public OutputDev {
public:
    // Open a PostScript output file, and write the prolog.
    PSOutputDev (
        const char* fileName, PDFDoc* docA, int firstPage, int lastPage,
        PSOutMode modeA, int imgLLXA = 0, int imgLLYA = 0, int imgURXA = 0,
        int imgURYA = 0, bool manualCtrlA = false,
        PSOutCustomCodeCbk customCodeCbkA = NULL,
        void* customCodeCbkDataA = NULL);

    // Open a PSOutputDev that will write to a generic stream.
    PSOutputDev (
        PSOutputFunc outputFuncA, void* outputStreamA, PDFDoc* docA,
        int firstPage, int lastPage, PSOutMode modeA, int imgLLXA = 0,
        int imgLLYA = 0, int imgURXA = 0, int imgURYA = 0,
        bool manualCtrlA = false, PSOutCustomCodeCbk customCodeCbkA = NULL,
        void* customCodeCbkDataA = NULL);

    // Destructor -- writes the trailer and closes the file.
    virtual ~PSOutputDev ();

    // Check if file was successfully created.
    virtual bool isOk () { return ok; }

    // Returns false if there have been any errors on the output stream.
    bool checkIO ();

    //---- get info about output device

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    virtual bool upsideDown () { return false; }

    // Does this device use drawChar() or drawString()?
    virtual bool useDrawChar () { return false; }

    // Does this device use tilingPatternFill()?  If this returns false,
    // tiling pattern fills will be reduced to a series of other drawing
    // operations.
    virtual bool useTilingPatternFill () { return true; }

    // Does this device use functionShadedFill(), axialShadedFill(), and
    // radialShadedFill()?  If this returns false, these shaded fills
    // will be reduced to a series of other drawing operations.
    virtual bool useShadedFills () { return level >= psLevel2; }

    // Does this device use drawForm()?  If this returns false,
    // form-type XObjects will be interpreted (i.e., unrolled).
    virtual bool useDrawForm () { return preload; }

    // Does this device use beginType3Char/endType3Char?  Otherwise,
    // text in Type 3 fonts will be drawn with drawChar/drawString.
    virtual bool interpretType3Chars () { return false; }

    //----- header/trailer (used only if manualCtrl is true)

    // Write the document-level header.
    void writeHeader (
        int firstPage, int lastPage, PDFRectangle* mediaBox,
        PDFRectangle* cropBox, int pageRotate);

    // Write the Xpdf procset.
    void writeXpdfProcset ();

    // Write the document-level setup.
    void writeDocSetup (Catalog* catalog, int firstPage, int lastPage);

    // Write the trailer for the current page.
    void writePageTrailer ();

    // Write the document trailer.
    void writeTrailer ();

    //----- initialization and control

    // Check to see if a page slice should be displayed.  If this
    // returns false, the page display is aborted.  Typically, an
    // OutputDev will use some alternate means to display the page
    // before returning false.
    virtual bool checkPageSlice (
        Page* page, double hDPI, double vDPI, int rotate, bool useMediaBox,
        bool crop, int sliceX, int sliceY, int sliceW, int sliceH,
        bool printing, bool (*abortCheckCbk) (void* data) = NULL,
        void* abortCheckCbkData = NULL);

    // Start a page.
    virtual void startPage (int pageNum, GfxState* state);

    // End a page.
    virtual void endPage ();

    //----- save/restore graphics state
    virtual void saveState (GfxState* state);
    virtual void restoreState (GfxState* state);

    //----- update graphics state
    virtual void updateCTM (
        GfxState* state, double m11, double m12, double m21, double m22,
        double m31, double m32);
    virtual void updateLineDash (GfxState* state);
    virtual void updateFlatness (GfxState* state);
    virtual void updateLineJoin (GfxState* state);
    virtual void updateLineCap (GfxState* state);
    virtual void updateMiterLimit (GfxState* state);
    virtual void updateLineWidth (GfxState* state);
    virtual void updateFillColorSpace (GfxState* state);
    virtual void updateStrokeColorSpace (GfxState* state);
    virtual void updateFillColor (GfxState* state);
    virtual void updateStrokeColor (GfxState* state);
    virtual void updateFillOverprint (GfxState* state);
    virtual void updateStrokeOverprint (GfxState* state);
    virtual void updateTransfer (GfxState* state);

    //----- update text state
    virtual void updateFont (GfxState* state);
    virtual void updateTextMat (GfxState* state);
    virtual void updateCharSpace (GfxState* state);
    virtual void updateRender (GfxState* state);
    virtual void updateRise (GfxState* state);
    virtual void updateWordSpace (GfxState* state);
    virtual void updateHorizScaling (GfxState* state);
    virtual void updateTextPos (GfxState* state);
    virtual void updateTextShift (GfxState* state, double shift);
    virtual void saveTextPos (GfxState* state);
    virtual void restoreTextPos (GfxState* state);

    //----- path painting
    virtual void stroke (GfxState* state);
    virtual void fill (GfxState* state);
    virtual void eoFill (GfxState* state);
    virtual void tilingPatternFill (
        GfxState* state, Gfx* gfx, Object* strRef, int paintType, Dict* resDict,
        double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep,
        double yStep);
    virtual bool
    functionShadedFill (GfxState* state, GfxFunctionShading* shading);
    virtual bool axialShadedFill (GfxState* state, GfxAxialShading* shading);
    virtual bool radialShadedFill (GfxState* state, GfxRadialShading* shading);

    //----- path clipping
    virtual void clip (GfxState* state);
    virtual void eoClip (GfxState* state);
    virtual void clipToStrokePath (GfxState* state);

    //----- text drawing
    virtual void drawString (GfxState* state, GString* s);
    virtual void endTextObject (GfxState* state);

    //----- image drawing
    virtual void drawImageMask (
        GfxState* state, Object* ref, Stream* str, int width, int height,
        bool invert, bool inlineImg, bool interpolate);
    virtual void drawImage (
        GfxState* state, Object* ref, Stream* str, int width, int height,
        GfxImageColorMap* colorMap, int* maskColors, bool inlineImg,
        bool interpolate);
    virtual void drawMaskedImage (
        GfxState* state, Object* ref, Stream* str, int width, int height,
        GfxImageColorMap* colorMap, Stream* maskStr, int maskWidth,
        int maskHeight, bool maskInvert, bool interpolate);

#if OPI_SUPPORT
    //----- OPI functions
    virtual void opiBegin (GfxState* state, Dict* opiDict);
    virtual void opiEnd (GfxState* state, Dict* opiDict);
#endif

    //----- Type 3 font operators
    virtual void type3D0 (GfxState* state, double wx, double wy);
    virtual void type3D1 (
        GfxState* state, double wx, double wy, double llx, double lly,
        double urx, double ury);

    //----- form XObjects
    virtual void drawForm (Ref ref);

    //----- PostScript XObjects
    virtual void psXObject (Stream* psStream, Stream* level1Stream);

    //----- miscellaneous
    void setOffset (double x, double y) {
        tx0 = x;
        ty0 = y;
    }
    void setScale (double x, double y) {
        xScale0 = x;
        yScale0 = y;
    }
    void setRotate (int rotateA) { rotate0 = rotateA; }
    void setClip (double llx, double lly, double urx, double ury) {
        clipLLX0 = llx;
        clipLLY0 = lly;
        clipURX0 = urx;
        clipURY0 = ury;
    }
    void
    setUnderlayCbk (void (*cbk) (PSOutputDev* psOut, void* data), void* data) {
        underlayCbk = cbk;
        underlayCbkData = data;
    }
    void
    setOverlayCbk (void (*cbk) (PSOutputDev* psOut, void* data), void* data) {
        overlayCbk = cbk;
        overlayCbkData = data;
    }

    void writePSChar (char c);
    void writePSBlock (const char* s, int len);
    void writePS (const char* s);
    void writePSFmt (const char* fmt, ...);
    void writePSString (GString* s);
    void writePSName (const char* s);

private:
    void init (
        PSOutputFunc outputFuncA, void* outputStreamA, PSFileType fileTypeA,
        PDFDoc* docA, int firstPage, int lastPage, PSOutMode modeA, int imgLLXA,
        int imgLLYA, int imgURXA, int imgURYA, bool manualCtrlA);
    void setupResources (Dict* resDict);
    void setupFonts (Dict* resDict);
    void setupFont (GfxFont* font, Dict* parentResDict);
    PSFontFileInfo* setupEmbeddedType1Font (GfxFont* font, Ref* id);
    PSFontFileInfo* setupExternalType1Font (GfxFont* font, GString* fileName);
    PSFontFileInfo* setupEmbeddedType1CFont (GfxFont* font, Ref* id);
    PSFontFileInfo* setupEmbeddedOpenTypeT1CFont (GfxFont* font, Ref* id);
    PSFontFileInfo* setupEmbeddedTrueTypeFont (GfxFont* font, Ref* id);
    PSFontFileInfo*
    setupExternalTrueTypeFont (GfxFont* font, GString* fileName, int fontNum);
    PSFontFileInfo* setupEmbeddedCIDType0Font (GfxFont* font, Ref* id);
    PSFontFileInfo* setupEmbeddedCIDTrueTypeFont (
        GfxFont* font, Ref* id, bool needVerticalMetrics);
    PSFontFileInfo* setupExternalCIDTrueTypeFont (
        GfxFont* font, GString* fileName, int fontNum,
        bool needVerticalMetrics);
    PSFontFileInfo* setupEmbeddedOpenTypeCFFFont (GfxFont* font, Ref* id);
    PSFontFileInfo* setupType3Font (GfxFont* font, Dict* parentResDict);
    GString* makePSFontName (GfxFont* font, Ref* id);
    void setupImages (Dict* resDict);
    void setupImage (Ref id, Stream* str, bool mask);
    void setupForms (Dict* resDict);
    void setupForm (Object* strRef, Object* strObj);
    void addProcessColor (double c, double m, double y, double k);
    void addCustomColor (GfxSeparationColorSpace* sepCS);
    void doPath (GfxPath* path);
    void doImageL1 (
        Object* ref, GfxImageColorMap* colorMap, bool invert, bool inlineImg,
        Stream* str, int width, int height, int len);
    void doImageL1Sep (
        GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str,
        int width, int height, int len);
    void doImageL2 (
        Object* ref, GfxImageColorMap* colorMap, bool invert, bool inlineImg,
        Stream* str, int width, int height, int len, int* maskColors,
        Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert);
    void doImageL3 (
        Object* ref, GfxImageColorMap* colorMap, bool invert, bool inlineImg,
        Stream* str, int width, int height, int len, int* maskColors,
        Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert);
    void dumpColorSpaceL2 (
        GfxColorSpace* colorSpace, bool genXform, bool updateColors,
        bool map01);
#if OPI_SUPPORT
    void opiBegin20 (GfxState* state, Dict* dict);
    void opiBegin13 (GfxState* state, Dict* dict);
    void opiTransform (
        GfxState* state, double x0, double y0, double* x1, double* y1);
    bool getFileSpec (Object* fileSpec, Object* fileName);
#endif
    void cvtFunction (const Function& func);
    GString* filterPSName (GString* name);
    void writePSTextLine (GString* s);

    PSLevel level;      // PostScript level (1, 2, separation)
    PSOutMode mode;     // PostScript mode (PS, EPS, form)
    int paperWidth;     // width of paper, in pts
    int paperHeight;    // height of paper, in pts
    bool paperMatch;   // true if paper size is set to match each page
    int imgLLX, imgLLY, // imageable area, in pts
        imgURX, imgURY;
    bool preload; // load all images into memory, and
                   //   predefine forms

    PSOutputFunc outputFunc;
    void* outputStream;
    PSFileType fileType; // file / pipe / stdout
    bool manualCtrl;
    int seqPage; // current sequential page number
    void (*underlayCbk) (PSOutputDev* psOut, void* data);
    void* underlayCbkData;
    void (*overlayCbk) (PSOutputDev* psOut, void* data);
    void* overlayCbkData;
    GString* (*customCodeCbk) (
        PSOutputDev* psOut, PSOutCustomCodeLocation loc, int n, void* data);
    void* customCodeCbkData;

    PDFDoc* doc;
    XRef* xref; // the xref table for this PDF file

    GList* fontInfo;       // info for each font [PSFontInfo]
    GHash* fontFileInfo;   // info for each font file [PSFontFileInfo]
    Ref* imgIDs;           // list of image IDs for in-memory images
    int imgIDLen;          // number of entries in imgIDs array
    int imgIDSize;         // size of imgIDs array
    Ref* formIDs;          // list of IDs for predefined forms
    int formIDLen;         // number of entries in formIDs array
    int formIDSize;        // size of formIDs array
    GList* xobjStack;      // stack of XObject dicts currently being
                           //   processed
    int numSaves;          // current number of gsaves
    int numTilingPatterns; // current number of nested tiling patterns
    int nextFunc;          // next unique number to use for a function

    GList* paperSizes;       // list of used paper sizes, if paperMatch
                             //   is true [PSOutPaperSize]
    double tx0, ty0;         // global translation
    double xScale0, yScale0; // global scaling
    int rotate0;             // rotation angle (0, 90, 180, 270)
    double clipLLX0, clipLLY0, clipURX0, clipURY0;
    double tx, ty;         // global translation for current page
    double xScale, yScale; // global scaling for current page
    int rotate;            // rotation angle for current page
    double epsX1, epsY1,   // EPS bounding box (unrotated)
        epsX2, epsY2;

    GString* embFontList; // resource comments for embedded fonts

    int processColors; // used process colors
    PSOutCustomColor   // used custom colors
        * customColors;

    bool haveTextClip; // set if text has been drawn with a
                        //   clipping render mode

    bool inType3Char; // inside a Type 3 CharProc
    GString* t3String; // Type 3 content string
    double t3WX, t3WY, // Type 3 character parameters
        t3LLX, t3LLY, t3URX, t3URY;
    bool t3FillColorOnly; // operators should only use the fill color
    bool t3Cacheable;     // cleared if char is not cacheable
    bool t3NeedsRestore;  // set if a 'q' operator was issued

#if OPI_SUPPORT
    int opi13Nest; // nesting level of OPI 1.3 objects
    int opi20Nest; // nesting level of OPI 2.0 objects
#endif

    bool ok; // set up ok?

    friend class WinPDFPrinter;
};

#endif // XPDF_XPDF_PSOUTPUTDEV_HH

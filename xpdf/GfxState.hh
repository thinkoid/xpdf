// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_GFXSTATE_HH
#define XPDF_XPDF_GFXSTATE_HH

#include <defs.hh>

#include <xpdf/array_fwd.hh>
#include <xpdf/obj.hh>
#include <xpdf/function.hh>

#include <xpdf/Stream.hh>

class GfxFont;
class PDFRectangle;
class GfxShading;

namespace xpdf {

#define XPDF_FIXED_POINT_ONE 0x00010000

using color_t = int32_t;
using small_color_t = uint8_t;

//
// Map double [0÷1] -> int [0÷65535] (16-bit fixed-point)
//
inline color_t to_color(double x)
{
    return std::clamp(x, 0., 1.) * XPDF_FIXED_POINT_ONE;
}

//
// Map int [0÷65535] (16-bit fixed-point) -> double [0÷1]
//
inline double to_double(color_t x)
{
    return double(std::clamp(x, 0, XPDF_FIXED_POINT_ONE)) / XPDF_FIXED_POINT_ONE;
}

//
// Map unsigned char [0÷255] -> int [0÷65535] (16-bit fixed-point)
//
inline color_t to_color(small_color_t x)
{
    return x << 8;
}

//
// Map int [0÷65535] (16-bit fixed-point) -> unsigned char [0÷255]
//
inline small_color_t to_small_color(color_t x)
{
    return std::clamp(x >> 8, 0, 255);
}

} // namespace xpdf

//------------------------------------------------------------------------
// GfxBlendMode
//------------------------------------------------------------------------

enum GfxBlendMode {
    gfxBlendNormal,
    gfxBlendMultiply,
    gfxBlendScreen,
    gfxBlendOverlay,
    gfxBlendDarken,
    gfxBlendLighten,
    gfxBlendColorDodge,
    gfxBlendColorBurn,
    gfxBlendHardLight,
    gfxBlendSoftLight,
    gfxBlendDifference,
    gfxBlendExclusion,
    gfxBlendHue,
    gfxBlendSaturation,
    gfxBlendColor,
    gfxBlendLuminosity
};

//------------------------------------------------------------------------
// GfxColor
//------------------------------------------------------------------------

#define gfxColorMaxComps xpdf::function_t::max_arity

struct GfxColor
{
    xpdf::color_t c[gfxColorMaxComps];
};

//------------------------------------------------------------------------
// GfxGray
//------------------------------------------------------------------------

struct GfxGray
{
    xpdf::color_t x;
};

//------------------------------------------------------------------------
// GfxRGB
//------------------------------------------------------------------------

struct GfxRGB
{
    xpdf::color_t r, g, b;
};

//------------------------------------------------------------------------
// GfxCMYK
//------------------------------------------------------------------------

struct GfxCMYK
{
    xpdf::color_t c, m, y, k;
};

//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

// NB: The nGfxColorSpaceModes constant and the gfxColorSpaceModeNames
// array defined in GfxState.cc must match this enum.
enum GfxColorSpaceMode {
    csDeviceGray,
    csCalGray,
    csDeviceRGB,
    csCalRGB,
    csDeviceCMYK,
    csLab,
    csICCBased,
    csIndexed,
    csSeparation,
    csDeviceN,
    csPattern
};

class GfxColorSpace
{
public:
    GfxColorSpace();
    virtual ~GfxColorSpace();
    virtual GfxColorSpace *   copy() = 0;
    virtual GfxColorSpaceMode getMode() = 0;

    // Construct a color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Object *csObj, int recursion = 0);

    // Construct a simple color space.  The <mode> argument can be
    // csDeviceGray, csDeviceRGB, or csDeviceCMYK.
    static GfxColorSpace *create(GfxColorSpaceMode mode);

    // Convert to gray, RGB, or CMYK.
    virtual void getGray(GfxColor *color, GfxGray *gray) = 0;
    virtual void getRGB(GfxColor *color, GfxRGB *rgb) = 0;
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk) = 0;

    // Return the number of color components.
    virtual int getNComps() = 0;

    // Get this color space's default color.
    virtual void getDefaultColor(GfxColor *color) = 0;

    // Return the default ranges for each component, assuming an image
    // with a max pixel value of <maxImgPixel>.
    virtual void getDefaultRanges(double *decodeLow, double *decodeRange,
                                  int maxImgPixel);

    // Returns true if painting operations in this color space never
    // mark the page (e.g., the "None" colorant).
    virtual bool isNonMarking() { return false; }

    // Return the color space's overprint mask.
    unsigned getOverprintMask() { return overprintMask; }

    // Return the number of color space modes
    static int getNumColorSpaceModes();

    // Return the name of the <idx>th color space mode.
    static const char *getColorSpaceModeName(int idx);

protected:
    unsigned overprintMask;
};

//------------------------------------------------------------------------
// GfxDeviceGrayColorSpace
//------------------------------------------------------------------------

class GfxDeviceGrayColorSpace : public GfxColorSpace
{
public:
    GfxDeviceGrayColorSpace();
    virtual ~GfxDeviceGrayColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csDeviceGray; }

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 1; }
    virtual void getDefaultColor(GfxColor *color);

private:
};

//------------------------------------------------------------------------
// GfxCalibratedGrayColorSpace
//------------------------------------------------------------------------

class GfxCalibratedGrayColorSpace : public GfxColorSpace
{
public:
    GfxCalibratedGrayColorSpace();
    virtual ~GfxCalibratedGrayColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csCalGray; }

    // Construct a CalGray color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Array &arr, int recursion);

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 1; }
    virtual void getDefaultColor(GfxColor *color);

    // CalGray-specific access.
    double getWhiteX() { return whiteX; }
    double getWhiteY() { return whiteY; }
    double getWhiteZ() { return whiteZ; }
    double getBlackX() { return blackX; }
    double getBlackY() { return blackY; }
    double getBlackZ() { return blackZ; }
    double getGamma() { return gamma; }

private:
    double whiteX, whiteY, whiteZ; // white point
    double blackX, blackY, blackZ; // black point
    double gamma; // gamma value
};

//------------------------------------------------------------------------
// GfxDeviceRGBColorSpace
//------------------------------------------------------------------------

class GfxDeviceRGBColorSpace : public GfxColorSpace
{
public:
    GfxDeviceRGBColorSpace();
    virtual ~GfxDeviceRGBColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csDeviceRGB; }

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 3; }
    virtual void getDefaultColor(GfxColor *color);

private:
};

//------------------------------------------------------------------------
// GfxCalibratedRGBColorSpace
//------------------------------------------------------------------------

class GfxCalibratedRGBColorSpace : public GfxColorSpace
{
public:
    GfxCalibratedRGBColorSpace();
    virtual ~GfxCalibratedRGBColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csCalRGB; }

    // Construct a CalRGB color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Array &arr, int recursion);

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 3; }
    virtual void getDefaultColor(GfxColor *color);

    // CalRGB-specific access.
    double  getWhiteX() { return whiteX; }
    double  getWhiteY() { return whiteY; }
    double  getWhiteZ() { return whiteZ; }
    double  getBlackX() { return blackX; }
    double  getBlackY() { return blackY; }
    double  getBlackZ() { return blackZ; }
    double  getGammaR() { return gammaR; }
    double  getGammaG() { return gammaG; }
    double  getGammaB() { return gammaB; }
    double *getMatrix() { return mat; }

private:
    double whiteX, whiteY, whiteZ; // white point
    double blackX, blackY, blackZ; // black point
    double gammaR, gammaG, gammaB; // gamma values
    double mat[9]; // ABC -> XYZ transform matrix
};

//------------------------------------------------------------------------
// GfxDeviceCMYKColorSpace
//------------------------------------------------------------------------

class GfxDeviceCMYKColorSpace : public GfxColorSpace
{
public:
    GfxDeviceCMYKColorSpace();
    virtual ~GfxDeviceCMYKColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csDeviceCMYK; }

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 4; }
    virtual void getDefaultColor(GfxColor *color);

private:
};

//------------------------------------------------------------------------
// GfxLabColorSpace
//------------------------------------------------------------------------

class GfxLabColorSpace : public GfxColorSpace
{
public:
    GfxLabColorSpace();
    virtual ~GfxLabColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csLab; }

    // Construct a Lab color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Array &arr, int recursion);

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 3; }
    virtual void getDefaultColor(GfxColor *color);

    virtual void getDefaultRanges(double *decodeLow, double *decodeRange,
                                  int maxImgPixel);

    // Lab-specific access.
    double getWhiteX() { return whiteX; }
    double getWhiteY() { return whiteY; }
    double getWhiteZ() { return whiteZ; }
    double getBlackX() { return blackX; }
    double getBlackY() { return blackY; }
    double getBlackZ() { return blackZ; }
    double getAMin() { return aMin; }
    double getAMax() { return aMax; }
    double getBMin() { return bMin; }
    double getBMax() { return bMax; }

private:
    double whiteX, whiteY, whiteZ; // white point
    double blackX, blackY, blackZ; // black point
    double aMin, aMax, bMin, bMax; // range for the a and b components
    double kr, kg, kb; // gamut mapping mulitpliers
};

//------------------------------------------------------------------------
// GfxICCBasedColorSpace
//------------------------------------------------------------------------

class GfxICCBasedColorSpace : public GfxColorSpace
{
public:
    GfxICCBasedColorSpace(int nCompsA, GfxColorSpace *altA,
                          Ref *iccProfileStreamA);
    virtual ~GfxICCBasedColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csICCBased; }

    // Construct an ICCBased color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Array &arr, int recursion);

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return nComps; }
    virtual void getDefaultColor(GfxColor *color);

    virtual void getDefaultRanges(double *decodeLow, double *decodeRange,
                                  int maxImgPixel);

    // ICCBased-specific access.
    GfxColorSpace *getAlt() { return alt; }

private:
    int            nComps; // number of color components (1, 3, or 4)
    GfxColorSpace *alt; // alternate color space
    double         rangeMin[4]; // min values for each component
    double         rangeMax[4]; // max values for each component
    Ref            iccProfileStream; // the ICC profile
};

//------------------------------------------------------------------------
// GfxIndexedColorSpace
//------------------------------------------------------------------------

class GfxIndexedColorSpace : public GfxColorSpace
{
public:
    GfxIndexedColorSpace(GfxColorSpace *baseA, int indexHighA);
    virtual ~GfxIndexedColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csIndexed; }

    // Construct an Indexed color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Array &arr, int recursion);

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 1; }
    virtual void getDefaultColor(GfxColor *color);

    virtual void getDefaultRanges(double *decodeLow, double *decodeRange,
                                  int maxImgPixel);

    // Indexed-specific access.
    GfxColorSpace *getBase() { return base; }
    int            getIndexHigh() { return indexHigh; }
    unsigned char *getLookup() { return lookup; }
    GfxColor *     mapColorToBase(GfxColor *color, GfxColor *baseColor);

private:
    GfxColorSpace *base; // base color space
    int            indexHigh; // max pixel value
    unsigned char *lookup; // lookup table
};

//------------------------------------------------------------------------
// GfxSeparationColorSpace
//------------------------------------------------------------------------

class GfxSeparationColorSpace : public GfxColorSpace
{
public:
    GfxSeparationColorSpace(GString *nameA, GfxColorSpace *altA,
                            const Function &funcA);
    virtual ~GfxSeparationColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csSeparation; }

    // Construct a Separation color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Array &arr, int recursion);

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 1; }
    virtual void getDefaultColor(GfxColor *color);

    virtual bool isNonMarking() { return nonMarking; }

    // Separation-specific access.
    GString *       as_name() { return name; }
    GfxColorSpace * getAlt() { return alt; }
    const Function &getFunc() const { return func; }

private:
    GfxSeparationColorSpace(GString *nameA, GfxColorSpace *altA,
                            const Function &funcA, bool nonMarkingA,
                            unsigned overprintMaskA);

    GString *      name; // colorant name
    GfxColorSpace *alt; // alternate color space
    Function       func; // tint transform (into alternate color space)
    bool           nonMarking;
};

//------------------------------------------------------------------------
// GfxDeviceNColorSpace
//------------------------------------------------------------------------

class GfxDeviceNColorSpace : public GfxColorSpace
{
public:
    GfxDeviceNColorSpace(int nCompsA, GString **namesA, GfxColorSpace *alt,
                         const Function &func);
    virtual ~GfxDeviceNColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csDeviceN; }

    // Construct a DeviceN color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Array &arr, int recursion);

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return nComps; }
    virtual void getDefaultColor(GfxColor *color);

    virtual bool isNonMarking() { return nonMarking; }

    // DeviceN-specific access.
    GString *       getColorantName(int i) { return names[i]; }
    GfxColorSpace * getAlt() { return alt; }
    const Function &getTintTransformFunc() const { return func; }

private:
    GfxDeviceNColorSpace(int nCompsA, GString **namesA, GfxColorSpace *alt,
                         const Function &func, bool nonMarkingA,
                         unsigned overprintMaskA);

    int nComps; // number of components
    GString // colorant names
        *          names[gfxColorMaxComps];
    GfxColorSpace *alt; // alternate color space
    Function       func; // tint transform (into alternate color space)
    bool           nonMarking;
};

//------------------------------------------------------------------------
// GfxPatternColorSpace
//------------------------------------------------------------------------

class GfxPatternColorSpace : public GfxColorSpace
{
public:
    GfxPatternColorSpace(GfxColorSpace *underA);
    virtual ~GfxPatternColorSpace();
    virtual GfxColorSpace *   copy();
    virtual GfxColorSpaceMode getMode() { return csPattern; }

    // Construct a Pattern color space.  Returns NULL if unsuccessful.
    static GfxColorSpace *parse(Array &arr, int recursion);

    virtual void getGray(GfxColor *color, GfxGray *gray);
    virtual void getRGB(GfxColor *color, GfxRGB *rgb);
    virtual void getCMYK(GfxColor *color, GfxCMYK *cmyk);

    virtual int  getNComps() { return 0; }
    virtual void getDefaultColor(GfxColor *color);

    // Pattern-specific access.
    GfxColorSpace *getUnder() { return under; }

private:
    GfxColorSpace *under; // underlying color space (for uncolored
        //   patterns)
};

//------------------------------------------------------------------------
// GfxPattern
//------------------------------------------------------------------------

class GfxPattern
{
public:
    GfxPattern(int typeA);
    virtual ~GfxPattern();

    static GfxPattern *parse(Object *objRef, Object *obj);

    virtual GfxPattern *copy() = 0;

    int getType() { return type; }

private:
    int type;
};

//------------------------------------------------------------------------
// GfxTilingPattern
//------------------------------------------------------------------------

class GfxTilingPattern : public GfxPattern
{
public:
    static GfxTilingPattern *parse(Object *patObjRef, Object *patObj);
    virtual ~GfxTilingPattern();

    virtual GfxPattern *copy();

    int     getPaintType() { return paintType; }
    int     getTilingType() { return tilingType; }
    double *getBBox() { return bbox; }
    double  getXStep() { return xStep; }
    double  getYStep() { return yStep; }
    Dict *  getResDict()
    {
        return resDict.is_dict() ? &resDict.as_dict() : (Dict *)NULL;
    }
    double *getMatrix() { return matrix; }
    Object *getContentStreamRef() { return &contentStreamRef; }

private:
    GfxTilingPattern(int paintTypeA, int tilingTypeA, double *bboxA,
                     double xStepA, double yStepA, Object *resDictA,
                     double *matrixA, Object *contentStreamA);

    int    paintType;
    int    tilingType;
    double bbox[4];
    double xStep, yStep;
    Object resDict;
    double matrix[6];
    Object contentStreamRef;
};

//------------------------------------------------------------------------
// GfxShadingPattern
//------------------------------------------------------------------------

class GfxShadingPattern : public GfxPattern
{
public:
    static GfxShadingPattern *parse(Object *patObj);
    virtual ~GfxShadingPattern();

    virtual GfxPattern *copy();

    GfxShading *getShading() { return shading; }
    double *    getMatrix() { return matrix; }

private:
    GfxShadingPattern(GfxShading *shadingA, double *matrixA);

    GfxShading *shading;
    double      matrix[6];
};

//------------------------------------------------------------------------
// GfxShading
//------------------------------------------------------------------------

class GfxShading
{
public:
    GfxShading(int typeA);
    GfxShading(GfxShading *shading);
    virtual ~GfxShading();

    static GfxShading *parse(Object *obj);

    virtual GfxShading *copy() = 0;

    int            getType() { return type; }
    GfxColorSpace *getColorSpace() { return colorSpace; }
    GfxColor *     getBackground() { return &background; }
    bool           getHasBackground() { return hasBackground; }
    void getBBox(double *xMinA, double *yMinA, double *xMaxA, double *yMaxA)
    {
        *xMinA = xMin;
        *yMinA = yMin;
        *xMaxA = xMax;
        *yMaxA = yMax;
    }
    bool getHasBBox() { return hasBBox; }

protected:
    bool init(Dict *dict);

    int            type;
    GfxColorSpace *colorSpace;
    GfxColor       background;
    bool           hasBackground;
    double         xMin, yMin, xMax, yMax;
    bool           hasBBox;
};

//------------------------------------------------------------------------
// GfxFunctionShading
//------------------------------------------------------------------------

class GfxFunctionShading : public GfxShading
{
public:
    GfxFunctionShading(double x0A, double y0A, double x1A, double y1A,
                       double *matrixA, Function *funcsA, int nFuncsA);
    GfxFunctionShading(GfxFunctionShading *shading);
    virtual ~GfxFunctionShading();

    static GfxFunctionShading *parse(Dict *dict);

    virtual GfxShading *copy();

    void getDomain(double *x0A, double *y0A, double *x1A, double *y1A)
    {
        *x0A = x0;
        *y0A = y0;
        *x1A = x1;
        *y1A = y1;
    }
    double *        getMatrix() { return matrix; }
    int             getNFuncs() { return nFuncs; }
    const Function &getFunc(int i) const { return funcs[i]; }
    void            getColor(double x, double y, GfxColor *color);

private:
    double   x0, y0, x1, y1;
    double   matrix[6];
    Function funcs[gfxColorMaxComps];
    int      nFuncs;
};

//------------------------------------------------------------------------
// GfxAxialShading
//------------------------------------------------------------------------

class GfxAxialShading : public GfxShading
{
public:
    GfxAxialShading(double x0A, double y0A, double x1A, double y1A, double t0A,
                    double t1A, Function *funcsA, int nFuncsA, bool extend0A,
                    bool extend1A);
    GfxAxialShading(GfxAxialShading *shading);
    virtual ~GfxAxialShading();

    static GfxAxialShading *parse(Dict *dict);

    virtual GfxShading *copy();

    void getCoords(double *x0A, double *y0A, double *x1A, double *y1A)
    {
        *x0A = x0;
        *y0A = y0;
        *x1A = x1;
        *y1A = y1;
    }
    double          getDomain0() { return t0; }
    double          getDomain1() { return t1; }
    bool            getExtend0() { return extend0; }
    bool            getExtend1() { return extend1; }
    int             getNFuncs() { return nFuncs; }
    const Function &getFunc(int i) const { return funcs[i]; }
    void            getColor(double t, GfxColor *color);

private:
    double   x0, y0, x1, y1;
    double   t0, t1;
    Function funcs[gfxColorMaxComps];
    int      nFuncs;
    bool     extend0, extend1;
};

//------------------------------------------------------------------------
// GfxRadialShading
//------------------------------------------------------------------------

class GfxRadialShading : public GfxShading
{
public:
    GfxRadialShading(double x0A, double y0A, double r0A, double x1A, double y1A,
                     double r1A, double t0A, double t1A, Function *funcsA,
                     int nFuncsA, bool extend0A, bool extend1A);
    GfxRadialShading(GfxRadialShading *shading);
    virtual ~GfxRadialShading();

    static GfxRadialShading *parse(Dict *dict);

    virtual GfxShading *copy();

    void getCoords(double *x0A, double *y0A, double *r0A, double *x1A,
                   double *y1A, double *r1A)
    {
        *x0A = x0;
        *y0A = y0;
        *r0A = r0;
        *x1A = x1;
        *y1A = y1;
        *r1A = r1;
    }
    double          getDomain0() { return t0; }
    double          getDomain1() { return t1; }
    bool            getExtend0() { return extend0; }
    bool            getExtend1() { return extend1; }
    int             getNFuncs() { return nFuncs; }
    const Function &getFunc(int i) const { return funcs[i]; }
    void            getColor(double t, GfxColor *color);

private:
    double   x0, y0, r0, x1, y1, r1;
    double   t0, t1;
    Function funcs[gfxColorMaxComps];
    int      nFuncs;
    bool     extend0, extend1;
};

//------------------------------------------------------------------------
// GfxGouraudTriangleShading
//------------------------------------------------------------------------

struct GfxGouraudVertex
{
    double x, y;
    double color[gfxColorMaxComps];
};

class GfxGouraudTriangleShading : public GfxShading
{
public:
    GfxGouraudTriangleShading(int typeA, GfxGouraudVertex *verticesA,
                              int nVerticesA, int (*trianglesA)[3],
                              int nTrianglesA, int nCompsA, Function *funcsA,
                              int nFuncsA);
    GfxGouraudTriangleShading(GfxGouraudTriangleShading *shading);
    virtual ~GfxGouraudTriangleShading();

    static GfxGouraudTriangleShading *parse(int typeA, Dict *dict, Stream *str);

    virtual GfxShading *copy();

    int  getNComps() { return nComps; }
    int  getNTriangles() { return nTriangles; }
    void getTriangle(int i, double *x0, double *y0, double *color0, double *x1,
                     double *y1, double *color1, double *x2, double *y2,
                     double *color2);
    void getColor(const double *, const double *, GfxColor *);

private:
    GfxGouraudVertex *vertices;
    int               nVertices;
    int (*triangles)[3];
    int      nTriangles;
    Function funcs[gfxColorMaxComps];
    int      nComps; // number of color components (1 if nFuncs > 0)
    int      nFuncs;
};

//------------------------------------------------------------------------
// GfxPatchMeshShading
//------------------------------------------------------------------------

struct GfxPatch
{
    double x[4][4];
    double y[4][4];
    double color[2][2][gfxColorMaxComps];
};

class GfxPatchMeshShading : public GfxShading
{
public:
    GfxPatchMeshShading(int typeA, GfxPatch *patchesA, int nPatchesA, int nCompsA,
                        Function *funcsA, int nFuncsA);
    GfxPatchMeshShading(GfxPatchMeshShading *shading);
    virtual ~GfxPatchMeshShading();

    static GfxPatchMeshShading *parse(int typeA, Dict *dict, Stream *str);

    virtual GfxShading *copy();

    int       getNComps() { return nComps; }
    int       getNPatches() { return nPatches; }
    GfxPatch *getPatch(int i) { return &patches[i]; }
    void      getColor(const double *, const double *, GfxColor *);

private:
    GfxPatch *patches;
    int       nPatches;
    Function  funcs[gfxColorMaxComps];
    int       nComps; // number of color components (1 if nFuncs > 0)
    int       nFuncs;
};

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

class GfxImageColorMap
{
public:
    // Constructor.
    GfxImageColorMap(int bitsA, Object *decode, GfxColorSpace *colorSpaceA);

    // Destructor.
    ~GfxImageColorMap();

    // Return a copy of this color map.
    GfxImageColorMap *copy() { return new GfxImageColorMap(this); }

    // Is color map valid?
    bool isOk() { return ok; }

    // Get the color space.
    GfxColorSpace *getColorSpace() { return colorSpace; }

    // Get stream decoding info.
    int getNumPixelComps() { return nComps; }
    int getBits() { return bits; }

    // Get decode table.
    double getDecodeLow(int i) { return decodeLow[i]; }
    double getDecodeHigh(int i) { return decodeLow[i] + decodeRange[i]; }

    // Convert an image pixel to a color.
    void getGray(unsigned char *x, GfxGray *gray);
    void getRGB(unsigned char *x, GfxRGB *rgb);
    void getCMYK(unsigned char *x, GfxCMYK *cmyk);
    void getColor(unsigned char *x, GfxColor *color);

    // Convert a line of <n> pixels to 8-bit colors.
    void getGrayByteLine(unsigned char *in, unsigned char *out, int n);
    void getRGBByteLine(unsigned char *in, unsigned char *out, int n);
    void getCMYKByteLine(unsigned char *in, unsigned char *out, int n);

private:
    GfxImageColorMap(GfxImageColorMap *colorMap);

    GfxColorSpace *colorSpace; // the image color space
    int            bits; // bits per component
    int            nComps; // number of components in a pixel
    GfxColorSpace *colorSpace2; // secondary color space
    int            nComps2; // number of components in colorSpace2
    xpdf::color_t * // lookup table
        lookup[gfxColorMaxComps];
    xpdf::color_t * // optimized case lookup table
        lookup2[gfxColorMaxComps];
    double // minimum values for each component
        decodeLow[gfxColorMaxComps];
    double // max - min value for each component
         decodeRange[gfxColorMaxComps];
    bool ok;
};

//------------------------------------------------------------------------
// GfxSubpath and GfxPath
//------------------------------------------------------------------------

class GfxSubpath
{
public:
    // Constructor.
    GfxSubpath(double x1, double y1);

    // Destructor.
    ~GfxSubpath();

    // Copy.
    GfxSubpath *copy() { return new GfxSubpath(this); }

    // Get points.
    int    getNumPoints() { return n; }
    double getX(int i) { return x[i]; }
    double getY(int i) { return y[i]; }
    bool   getCurve(int i) { return curve[i]; }

    // Get last point.
    double getLastX() { return x[n - 1]; }
    double getLastY() { return y[n - 1]; }

    // Add a line segment.
    void lineTo(double x1, double y1);

    // Add a Bezier curve.
    void curveTo(double x1, double y1, double x2, double y2, double x3,
                 double y3);

    // Close the subpath.
    void close();
    bool isClosed() { return closed; }

    // Add (<dx>, <dy>) to each point in the subpath.
    void offset(double dx, double dy);

private:
    double *x, *y; // points
    bool *  curve; // curve[i] => point i is a control point
        //   for a Bezier curve
    int  n; // number of points
    int  size; // size of x/y arrays
    bool closed; // set if path is closed

    GfxSubpath(GfxSubpath *subpath);
};

class GfxPath
{
public:
    // Constructor.
    GfxPath();

    // Destructor.
    ~GfxPath();

    // Copy.
    GfxPath *copy()
    {
        return new GfxPath(justMoved, firstX, firstY, subpaths, n, size);
    }

    // Is there a current point?
    bool isCurPt() { return n > 0 || justMoved; }

    // Is the path non-empty, i.e., is there at least one segment?
    bool isPath() { return n > 0; }

    // Get subpaths.
    int         getNumSubpaths() { return n; }
    GfxSubpath *getSubpath(int i) { return subpaths[i]; }

    // Get last point on last subpath.
    double getLastX() { return subpaths[n - 1]->getLastX(); }
    double getLastY() { return subpaths[n - 1]->getLastY(); }

    // Move the current point.
    void moveTo(double x, double y);

    // Add a segment to the last subpath.
    void lineTo(double x, double y);

    // Add a Bezier curve to the last subpath
    void curveTo(double x1, double y1, double x2, double y2, double x3,
                 double y3);

    // Close the last subpath.
    void close();

    // Append <path> to <this>.
    void append(GfxPath *path);

    // Add (<dx>, <dy>) to each point in the path.
    void offset(double dx, double dy);

private:
    bool         justMoved; // set if a new subpath was just started
    double       firstX, firstY; // first point in new subpath
    GfxSubpath **subpaths; // subpaths
    int          n; // number of subpaths
    int          size; // size of subpaths array

    GfxPath(bool justMoved1, double firstX1, double firstY1,
            GfxSubpath **subpaths1, int n1, int size1);
};

//------------------------------------------------------------------------
// GfxState
//------------------------------------------------------------------------

class GfxState
{
public:
    // Construct a default GfxState, for a device with resolution <hDPI>
    // x <vDPI>, page box <pageBox>, page rotation <rotateA>, and
    // coordinate system specified by <upsideDown>.
    GfxState(double hDPIA, double vDPIA, PDFRectangle *pageBox, int rotateA,
             bool upsideDown);

    // Destructor.
    ~GfxState();

    // Copy.
    GfxState *copy(bool copyPath = false) { return new GfxState(this, copyPath); }

    // Accessors.
    double    getHDPI() { return hDPI; }
    double    getVDPI() { return vDPI; }
    double *  getCTM() { return ctm; }
    double    getX1() { return px1; }
    double    getY1() { return py1; }
    double    getX2() { return px2; }
    double    getY2() { return py2; }
    double    getPageWidth() { return pageWidth; }
    double    getPageHeight() { return pageHeight; }
    int       getRotate() { return rotate; }
    GfxColor *getFillColor() { return &fillColor; }
    GfxColor *getStrokeColor() { return &strokeColor; }
    void getFillGray(GfxGray *gray) { fillColorSpace->getGray(&fillColor, gray); }
    void getStrokeGray(GfxGray *gray)
    {
        strokeColorSpace->getGray(&strokeColor, gray);
    }
    void getFillRGB(GfxRGB *rgb) { fillColorSpace->getRGB(&fillColor, rgb); }
    void getStrokeRGB(GfxRGB *rgb)
    {
        strokeColorSpace->getRGB(&strokeColor, rgb);
    }
    void getFillCMYK(GfxCMYK *cmyk) { fillColorSpace->getCMYK(&fillColor, cmyk); }
    void getStrokeCMYK(GfxCMYK *cmyk)
    {
        strokeColorSpace->getCMYK(&strokeColor, cmyk);
    }
    GfxColorSpace *getFillColorSpace() { return fillColorSpace; }
    GfxColorSpace *getStrokeColorSpace() { return strokeColorSpace; }
    GfxPattern *   getFillPattern() { return fillPattern; }
    GfxPattern *   getStrokePattern() { return strokePattern; }
    GfxBlendMode   getBlendMode() { return blendMode; }
    double         getFillOpacity() { return fillOpacity; }
    double         getStrokeOpacity() { return strokeOpacity; }
    bool           getFillOverprint() { return fillOverprint; }
    bool           getStrokeOverprint() { return strokeOverprint; }
    int            getOverprintMode() { return overprintMode; }
    Function *     getTransfer() { return transfer; }
    double         getLineWidth() { return lineWidth; }
    void           getLineDash(double **dash, int *length, double *start)
    {
        *dash = lineDash;
        *length = lineDashLength;
        *start = lineDashStart;
    }
    int      getFlatness() { return flatness; }
    int      getLineJoin() { return lineJoin; }
    int      getLineCap() { return lineCap; }
    double   getMiterLimit() { return miterLimit; }
    bool     getStrokeAdjust() { return strokeAdjust; }
    GfxFont *getFont() { return font; }
    double   getFontSize() { return fontSize; }
    double * getTextMat() { return textMat; }
    double   getCharSpace() { return charSpace; }
    double   getWordSpace() { return wordSpace; }
    double   getHorizScaling() { return horizScaling; }
    double   getLeading() { return leading; }
    double   getRise() { return rise; }
    int      getRender() { return render; }
    GfxPath *getPath() { return path; }
    void     setPath(GfxPath *pathA);
    double   getCurX() { return curX; }
    double   getCurY() { return curY; }
    void     getClipBBox(double *xMin, double *yMin, double *xMax, double *yMax)
    {
        *xMin = clipXMin;
        *yMin = clipYMin;
        *xMax = clipXMax;
        *yMax = clipYMax;
    }
    void getUserClipBBox(double *xMin, double *yMin, double *xMax, double *yMax);
    double getLineX() { return lineX; }
    double getLineY() { return lineY; }

    // Is there a current point/path?
    bool isCurPt() { return path->isCurPt(); }
    bool isPath() { return path->isPath(); }

    // Transforms.
    void transform(double x1, double y1, double *x2, double *y2)
    {
        *x2 = ctm[0] * x1 + ctm[2] * y1 + ctm[4];
        *y2 = ctm[1] * x1 + ctm[3] * y1 + ctm[5];
    }
    void transformDelta(double x1, double y1, double *x2, double *y2)
    {
        *x2 = ctm[0] * x1 + ctm[2] * y1;
        *y2 = ctm[1] * x1 + ctm[3] * y1;
    }
    void textTransform(double x1, double y1, double *x2, double *y2)
    {
        *x2 = textMat[0] * x1 + textMat[2] * y1 + textMat[4];
        *y2 = textMat[1] * x1 + textMat[3] * y1 + textMat[5];
    }
    void textTransformDelta(double x1, double y1, double *x2, double *y2)
    {
        *x2 = textMat[0] * x1 + textMat[2] * y1;
        *y2 = textMat[1] * x1 + textMat[3] * y1;
    }
    double transformWidth(double w);
    double getTransformedLineWidth() { return transformWidth(lineWidth); }
    double getTransformedFontSize();
    void   getFontTransMat(double *m11, double *m12, double *m21, double *m22);

    // Change state parameters.
    void setCTM(double a, double b, double c, double d, double e, double f);
    void concatCTM(double a, double b, double c, double d, double e, double f);
    void shiftCTM(double tx, double ty);
    void setFillColorSpace(GfxColorSpace *colorSpace);
    void setStrokeColorSpace(GfxColorSpace *colorSpace);
    void setFillColor(GfxColor *color) { fillColor = *color; }
    void setStrokeColor(GfxColor *color) { strokeColor = *color; }
    void setFillPattern(GfxPattern *pattern);
    void setStrokePattern(GfxPattern *pattern);
    void setBlendMode(GfxBlendMode mode) { blendMode = mode; }
    void setFillOpacity(double opac) { fillOpacity = opac; }
    void setStrokeOpacity(double opac) { strokeOpacity = opac; }
    void setFillOverprint(bool op) { fillOverprint = op; }
    void setStrokeOverprint(bool op) { strokeOverprint = op; }
    void setOverprintMode(int opm) { overprintMode = opm; }
    void setTransfer(Function *funcs);
    void setLineWidth(double width) { lineWidth = width; }
    void setLineDash(double *dash, int length, double start);
    void setFlatness(int flatness1) { flatness = flatness1; }
    void setLineJoin(int lineJoin1) { lineJoin = lineJoin1; }
    void setLineCap(int lineCap1) { lineCap = lineCap1; }
    void setMiterLimit(double limit) { miterLimit = limit; }
    void setStrokeAdjust(bool sa) { strokeAdjust = sa; }
    void setFont(GfxFont *fontA, double fontSizeA)
    {
        font = fontA;
        fontSize = fontSizeA;
    }
    void setTextMat(double a, double b, double c, double d, double e, double f)
    {
        textMat[0] = a;
        textMat[1] = b;
        textMat[2] = c;
        textMat[3] = d;
        textMat[4] = e;
        textMat[5] = f;
    }
    void setCharSpace(double space) { charSpace = space; }
    void setWordSpace(double space) { wordSpace = space; }
    void setHorizScaling(double scale) { horizScaling = 0.01 * scale; }
    void setLeading(double leadingA) { leading = leadingA; }
    void setRise(double riseA) { rise = riseA; }
    void setRender(int renderA) { render = renderA; }

    // Add to path.
    void moveTo(double x, double y) { path->moveTo(curX = x, curY = y); }
    void lineTo(double x, double y) { path->lineTo(curX = x, curY = y); }
    void curveTo(double x1, double y1, double x2, double y2, double x3, double y3)
    {
        path->curveTo(x1, y1, x2, y2, curX = x3, curY = y3);
    }
    void closePath()
    {
        path->close();
        curX = path->getLastX();
        curY = path->getLastY();
    }
    void clearPath();

    // Update clip region.
    void clip();
    void clipToStrokePath();
    void clipToRect(double xMin, double yMin, double xMax, double yMax);

    // Text position.
    void textSetPos(double tx, double ty)
    {
        lineX = tx;
        lineY = ty;
    }
    void textMoveTo(double tx, double ty)
    {
        lineX = tx;
        lineY = ty;
        textTransform(tx, ty, &curX, &curY);
    }
    void textShift(double tx, double ty);
    void shift(double dx, double dy);

    // Push/pop GfxState on/off stack.
    GfxState *save();
    GfxState *restore();
    bool      hasSaves() { return saved != NULL; }

    // Misc
    bool parseBlendMode(Object *obj, GfxBlendMode *mode);

private:
    double hDPI, vDPI; // resolution
    double ctm[6]; // coord transform matrix
    double px1, py1, px2, py2; // page corners (user coords)
    double pageWidth, pageHeight; // page size (pixels)
    int    rotate; // page rotation angle

    GfxColorSpace *fillColorSpace; // fill color space
    GfxColorSpace *strokeColorSpace; // stroke color space
    GfxColor       fillColor; // fill color
    GfxColor       strokeColor; // stroke color
    GfxPattern *   fillPattern; // fill pattern
    GfxPattern *   strokePattern; // stroke pattern
    GfxBlendMode   blendMode; // transparency blend mode
    double         fillOpacity; // fill opacity
    double         strokeOpacity; // stroke opacity
    bool           fillOverprint; // fill overprint
    bool           strokeOverprint; // stroke overprint
    int            overprintMode; // overprint mode ("OPM")
    Function       transfer[4]; // transfer function (entries may be: all
        //   NULL = identity; last three NULL =
        //   single function; all four non-NULL =
        //   R,G,B,gray functions)

    double  lineWidth; // line width
    double *lineDash; // line dash
    int     lineDashLength;
    double  lineDashStart;
    int     flatness; // curve flatness
    int     lineJoin; // line join style
    int     lineCap; // line cap style
    double  miterLimit; // line miter limit
    bool    strokeAdjust; // stroke adjustment

    GfxFont *font; // font
    double   fontSize; // font size
    double   textMat[6]; // text matrix
    double   charSpace; // character spacing
    double   wordSpace; // word spacing
    double   horizScaling; // horizontal scaling
    double   leading; // text leading
    double   rise; // text rise
    int      render; // text rendering mode

    GfxPath *path; // array of path elements
    double   curX, curY; // current point (user coords)
    double   lineX, lineY; // start of current text line (text coords)

    double clipXMin, clipYMin, // bounding box for clip region
        clipXMax, clipYMax;

    GfxState *saved; // next GfxState on stack

    GfxState(GfxState *state, bool copyPath);
};

#endif // XPDF_XPDF_GFXSTATE_HH

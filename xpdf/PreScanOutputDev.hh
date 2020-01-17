// -*- mode: c++; -*-
// Copyright 2005 Glyph & Cog, LLC

#ifndef PRESCANOUTPUTDEV_H
#define PRESCANOUTPUTDEV_H

#include <defs.hh>

#include <xpdf/GfxState.hh>
#include <xpdf/OutputDev.hh>

//------------------------------------------------------------------------
// PreScanOutputDev
//------------------------------------------------------------------------

class PreScanOutputDev : public OutputDev {
public:
    // Constructor.
    PreScanOutputDev ();

    // Destructor.
    virtual ~PreScanOutputDev ();

    //----- get info about output device

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    virtual bool upsideDown () { return true; }

    // Does this device use drawChar() or drawString()?
    virtual bool useDrawChar () { return true; }

    // Does this device use tilingPatternFill()?  If this returns false,
    // tiling pattern fills will be reduced to a series of other drawing
    // operations.
    virtual bool useTilingPatternFill () { return true; }

    // Does this device use functionShadedFill(), axialShadedFill(), and
    // radialShadedFill()?  If this returns false, these shaded fills
    // will be reduced to a series of other drawing operations.
    virtual bool useShadedFills () { return true; }

    // Does this device use beginType3Char/endType3Char?  Otherwise,
    // text in Type 3 fonts will be drawn with drawChar/drawString.
    virtual bool interpretType3Chars () { return true; }

    //----- initialization and control

    // Start a page.
    virtual void startPage (int pageNum, GfxState* state);

    // End a page.
    virtual void endPage ();

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

    //----- text drawing
    virtual void beginStringOp (GfxState* state);
    virtual void endStringOp (GfxState* state);
    virtual bool beginType3Char (
        GfxState* state, double x, double y, double dx, double dy,
        CharCode code, Unicode* u, int uLen);
    virtual void endType3Char (GfxState* state);

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
    virtual void drawSoftMaskedImage (
        GfxState* state, Object* ref, Stream* str, int width, int height,
        GfxImageColorMap* colorMap, Stream* maskStr, int maskWidth,
        int maskHeight, GfxImageColorMap* maskColorMap, bool interpolate);

    //----- transparency groups and soft masks
    virtual void beginTransparencyGroup (
        GfxState* state, double* bbox, GfxColorSpace* blendingColorSpace,
        bool isolated, bool knockout, bool forSoftMask);

    //----- special access

    // Returns true if the operations performed since the last call to
    // clearStats() are all monochrome (black or white).
    bool isMonochrome () { return mono; }

    // Returns true if the operations performed since the last call to
    // clearStats() are all gray.
    bool isGray () { return gray; }

    // Returns true if the operations performed since the last call to
    // clearStats() included any transparency.
    bool usesTransparency () { return transparency; }

    // Returns true if the operations performed since the last call to
    // clearStats() included any image mask fills with a pattern color
    // space.
    bool usesPatternImageMask () { return patternImgMask; }

    // Returns true if the operations performed since the last call to
    // clearStats() are all rasterizable by GDI calls in GDIOutputDev.
    bool isAllGDI () { return gdi; }

    // Clear the stats used by the above functions.
    void clearStats ();

private:
    void check (
        GfxColorSpace* colorSpace, GfxColor* color, double opacity,
        GfxBlendMode blendMode);

    bool mono;
    bool gray;
    bool transparency;
    bool patternImgMask;
    bool gdi;
};

#endif

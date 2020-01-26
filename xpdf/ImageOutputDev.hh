// -*- mode: c++; -*-
// Copyright 1998-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_IMAGEOUTPUTDEV_HH
#define XPDF_XPDF_IMAGEOUTPUTDEV_HH

#include <defs.hh>

#include <cstdio>
#include <xpdf/OutputDev.hh>

class GfxState;

//------------------------------------------------------------------------
// ImageOutputDev
//------------------------------------------------------------------------

class ImageOutputDev : public OutputDev {
public:
    // Create an OutputDev which will write images to files named
    // <fileRoot>-NNN.<type>.  Normally, all images are written as PBM
    // (.pbm) or PPM (.ppm) files.  If <dumpJPEG> is set, JPEG images are
    // written as JPEG (.jpg) files.
    ImageOutputDev (char* fileRootA, bool dumpJPEGA);

    // Destructor.
    virtual ~ImageOutputDev ();

    // Check if file was successfully created.
    virtual bool isOk () { return ok; }

    // Does this device use tilingPatternFill()?  If this returns false,
    // tiling pattern fills will be reduced to a series of other drawing
    // operations.
    virtual bool useTilingPatternFill () { return true; }

    // Does this device use beginType3Char/endType3Char?  Otherwise,
    // text in Type 3 fonts will be drawn with drawChar/drawString.
    virtual bool interpretType3Chars () { return false; }

    // Does this device need non-text content?
    virtual bool needNonText () { return true; }

    //---- get info about output device

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    virtual bool upsideDown () { return true; }

    // Does this device use drawChar() or drawString()?
    virtual bool useDrawChar () { return false; }

    //----- path painting
    virtual void tilingPatternFill (
        GfxState* state, Gfx* gfx, Object* strRef, int paintType, Dict* resDict,
        double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep,
        double yStep);

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

private:
    char* fileRoot; // root of output file names
    char* fileName; // buffer for output file names
    bool dumpJPEG; // set to dump native JPEG files
    int imgNum;     // current image number
    bool ok;       // set up ok?
};

#endif // XPDF_XPDF_IMAGEOUTPUTDEV_HH

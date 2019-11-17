//========================================================================
//
// ImageOutputDev.h
//
// Copyright 1998-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef IMAGEOUTPUTDEV_H
#define IMAGEOUTPUTDEV_H

#include <defs.hh>

#include <stdio.h>
#include <goo/gtypes.hh>
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
    ImageOutputDev (char* fileRootA, GBool dumpJPEGA);

    // Destructor.
    virtual ~ImageOutputDev ();

    // Check if file was successfully created.
    virtual GBool isOk () { return ok; }

    // Does this device use tilingPatternFill()?  If this returns false,
    // tiling pattern fills will be reduced to a series of other drawing
    // operations.
    virtual GBool useTilingPatternFill () { return gTrue; }

    // Does this device use beginType3Char/endType3Char?  Otherwise,
    // text in Type 3 fonts will be drawn with drawChar/drawString.
    virtual GBool interpretType3Chars () { return gFalse; }

    // Does this device need non-text content?
    virtual GBool needNonText () { return gTrue; }

    //---- get info about output device

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    virtual GBool upsideDown () { return gTrue; }

    // Does this device use drawChar() or drawString()?
    virtual GBool useDrawChar () { return gFalse; }

    //----- path painting
    virtual void tilingPatternFill (
        GfxState* state, Gfx* gfx, Object* strRef, int paintType, Dict* resDict,
        double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep,
        double yStep);

    //----- image drawing
    virtual void drawImageMask (
        GfxState* state, Object* ref, Stream* str, int width, int height,
        GBool invert, GBool inlineImg, GBool interpolate);
    virtual void drawImage (
        GfxState* state, Object* ref, Stream* str, int width, int height,
        GfxImageColorMap* colorMap, int* maskColors, GBool inlineImg,
        GBool interpolate);
    virtual void drawMaskedImage (
        GfxState* state, Object* ref, Stream* str, int width, int height,
        GfxImageColorMap* colorMap, Stream* maskStr, int maskWidth,
        int maskHeight, GBool maskInvert, GBool interpolate);
    virtual void drawSoftMaskedImage (
        GfxState* state, Object* ref, Stream* str, int width, int height,
        GfxImageColorMap* colorMap, Stream* maskStr, int maskWidth,
        int maskHeight, GfxImageColorMap* maskColorMap, GBool interpolate);

private:
    char* fileRoot; // root of output file names
    char* fileName; // buffer for output file names
    GBool dumpJPEG; // set to dump native JPEG files
    int imgNum;     // current image number
    GBool ok;       // set up ok?
};

#endif

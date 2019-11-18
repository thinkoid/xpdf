//========================================================================
//
// PreScanOutputDev.cc
//
// Copyright 2005 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <math.h>
#include <xpdf/GlobalParams.hh>
#include <xpdf/Page.hh>
#include <xpdf/Gfx.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/Link.hh>
#include <xpdf/PreScanOutputDev.hh>

//------------------------------------------------------------------------
// PreScanOutputDev
//------------------------------------------------------------------------

PreScanOutputDev::PreScanOutputDev () { clearStats (); }

PreScanOutputDev::~PreScanOutputDev () {}

void PreScanOutputDev::startPage (int pageNum, GfxState* state) {}

void PreScanOutputDev::endPage () {}

void PreScanOutputDev::stroke (GfxState* state) {
    double* dash;
    int dashLen;
    double dashStart;

    check (
        state->getStrokeColorSpace (), state->getStrokeColor (),
        state->getStrokeOpacity (), state->getBlendMode ());
    state->getLineDash (&dash, &dashLen, &dashStart);
    if (dashLen != 0) { gdi = false; }
}

void PreScanOutputDev::fill (GfxState* state) {
    check (
        state->getFillColorSpace (), state->getFillColor (),
        state->getFillOpacity (), state->getBlendMode ());
}

void PreScanOutputDev::eoFill (GfxState* state) {
    check (
        state->getFillColorSpace (), state->getFillColor (),
        state->getFillOpacity (), state->getBlendMode ());
}

void PreScanOutputDev::tilingPatternFill (
    GfxState* state, Gfx* gfx, Object* strRef, int paintType, Dict* resDict,
    double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep,
    double yStep) {
    if (paintType == 1) { gfx->drawForm (strRef, resDict, mat, bbox); }
    else {
        check (
            state->getFillColorSpace (), state->getFillColor (),
            state->getFillOpacity (), state->getBlendMode ());
    }
}

bool PreScanOutputDev::functionShadedFill (
    GfxState* state, GfxFunctionShading* shading) {
    if (shading->getColorSpace ()->getMode () != csDeviceGray &&
        shading->getColorSpace ()->getMode () != csCalGray) {
        gray = false;
    }
    mono = false;
    if (state->getFillOpacity () != 1 ||
        state->getBlendMode () != gfxBlendNormal) {
        transparency = true;
    }
    return true;
}

bool PreScanOutputDev::axialShadedFill (
    GfxState* state, GfxAxialShading* shading) {
    if (shading->getColorSpace ()->getMode () != csDeviceGray &&
        shading->getColorSpace ()->getMode () != csCalGray) {
        gray = false;
    }
    mono = false;
    if (state->getFillOpacity () != 1 ||
        state->getBlendMode () != gfxBlendNormal) {
        transparency = true;
    }
    return true;
}

bool PreScanOutputDev::radialShadedFill (
    GfxState* state, GfxRadialShading* shading) {
    if (shading->getColorSpace ()->getMode () != csDeviceGray &&
        shading->getColorSpace ()->getMode () != csCalGray) {
        gray = false;
    }
    mono = false;
    if (state->getFillOpacity () != 1 ||
        state->getBlendMode () != gfxBlendNormal) {
        transparency = true;
    }
    return true;
}

void PreScanOutputDev::clip (GfxState* state) {
    //~ check for a rectangle "near" the edge of the page;
    //~   else set gdi to false
}

void PreScanOutputDev::eoClip (GfxState* state) {
    //~ see clip()
}

void PreScanOutputDev::beginStringOp (GfxState* state) {
    int render;
    GfxFont* font;
    double m11, m12, m21, m22;
    bool simpleTTF;

    render = state->getRender ();
    if (!(render & 1)) {
        check (
            state->getFillColorSpace (), state->getFillColor (),
            state->getFillOpacity (), state->getBlendMode ());
    }
    if ((render & 3) == 1 || (render & 3) == 2) {
        check (
            state->getStrokeColorSpace (), state->getStrokeColor (),
            state->getStrokeOpacity (), state->getBlendMode ());
    }

    font = state->getFont ();
    state->getFontTransMat (&m11, &m12, &m21, &m22);
    //~ this should check for external fonts that are non-TrueType
    simpleTTF = fabs (m11 + m22) < 0.01 && m11 > 0 && fabs (m12) < 0.01 &&
                fabs (m21) < 0.01 &&
                fabs (state->getHorizScaling () - 1) < 0.001 &&
                (font->getType () == fontTrueType ||
                 font->getType () == fontTrueTypeOT);
    if (simpleTTF) {
        //~ need to create a FoFiTrueType object, and check for a Unicode cmap
    }
    if (state->getRender () != 0 || !simpleTTF) { gdi = false; }
}

void PreScanOutputDev::endStringOp (GfxState* state) {}

bool PreScanOutputDev::beginType3Char (
    GfxState* state, double x, double y, double dx, double dy, CharCode code,
    Unicode* u, int uLen) {
    // return false so all Type 3 chars get rendered (no caching)
    return false;
}

void PreScanOutputDev::endType3Char (GfxState* state) {}

void PreScanOutputDev::drawImageMask (
    GfxState* state, Object* ref, Stream* str, int width, int height,
    bool invert, bool inlineImg, bool interpolate) {
    check (
        state->getFillColorSpace (), state->getFillColor (),
        state->getFillOpacity (), state->getBlendMode ());
    if (state->getFillColorSpace ()->getMode () == csPattern) {
        patternImgMask = true;
    }
    gdi = false;

    if (inlineImg) {
        str->reset ();
        str->discardChars (height * ((width + 7) / 8));
        str->close ();
    }
}

void PreScanOutputDev::drawImage (
    GfxState* state, Object* ref, Stream* str, int width, int height,
    GfxImageColorMap* colorMap, int* maskColors, bool inlineImg,
    bool interpolate) {
    GfxColorSpace* colorSpace;

    colorSpace = colorMap->getColorSpace ();
    if (colorSpace->getMode () == csIndexed) {
        colorSpace = ((GfxIndexedColorSpace*)colorSpace)->getBase ();
    }
    if (colorSpace->getMode () == csDeviceGray ||
        colorSpace->getMode () == csCalGray) {
        if (colorMap->getBits () > 1) { mono = false; }
    }
    else {
        gray = false;
        mono = false;
    }
    if (state->getFillOpacity () != 1 ||
        state->getBlendMode () != gfxBlendNormal) {
        transparency = true;
    }
    gdi = false;

    if (inlineImg) {
        str->reset ();
        str->discardChars (
            height *
            ((width * colorMap->getNumPixelComps () * colorMap->getBits () +
              7) /
             8));
        str->close ();
    }
}

void PreScanOutputDev::drawMaskedImage (
    GfxState* state, Object* ref, Stream* str, int width, int height,
    GfxImageColorMap* colorMap, Stream* maskStr, int maskWidth, int maskHeight,
    bool maskInvert, bool interpolate) {
    GfxColorSpace* colorSpace;

    colorSpace = colorMap->getColorSpace ();
    if (colorSpace->getMode () == csIndexed) {
        colorSpace = ((GfxIndexedColorSpace*)colorSpace)->getBase ();
    }
    if (colorSpace->getMode () == csDeviceGray ||
        colorSpace->getMode () == csCalGray) {
        if (colorMap->getBits () > 1) { mono = false; }
    }
    else {
        gray = false;
        mono = false;
    }
    if (state->getFillOpacity () != 1 ||
        state->getBlendMode () != gfxBlendNormal) {
        transparency = true;
    }
    gdi = false;
}

void PreScanOutputDev::drawSoftMaskedImage (
    GfxState* state, Object* ref, Stream* str, int width, int height,
    GfxImageColorMap* colorMap, Stream* maskStr, int maskWidth, int maskHeight,
    GfxImageColorMap* maskColorMap, bool interpolate) {
    GfxColorSpace* colorSpace;

    colorSpace = colorMap->getColorSpace ();
    if (colorSpace->getMode () == csIndexed) {
        colorSpace = ((GfxIndexedColorSpace*)colorSpace)->getBase ();
    }
    if (colorSpace->getMode () != csDeviceGray &&
        colorSpace->getMode () != csCalGray) {
        gray = false;
    }
    mono = false;
    transparency = true;
    gdi = false;
}

void PreScanOutputDev::beginTransparencyGroup (
    GfxState* state, double* bbox, GfxColorSpace* blendingColorSpace,
    bool isolated, bool knockout, bool forSoftMask) {
    transparency = true;
    gdi = false;
}

void PreScanOutputDev::check (
    GfxColorSpace* colorSpace, GfxColor* color, double opacity,
    GfxBlendMode blendMode) {
    GfxRGB rgb;

    if (colorSpace->getMode () == csPattern) {
        mono = false;
        gray = false;
        gdi = false;
    }
    else {
        colorSpace->getRGB (color, &rgb);
        if (rgb.r != rgb.g || rgb.g != rgb.b || rgb.b != rgb.r) {
            mono = false;
            gray = false;
        }
        else if (!((rgb.r == 0 && rgb.g == 0 && rgb.b == 0) ||
                   (rgb.r == gfxColorComp1 && rgb.g == gfxColorComp1 &&
                    rgb.b == gfxColorComp1))) {
            mono = false;
        }
    }
    if (opacity != 1 || blendMode != gfxBlendNormal) { transparency = true; }
}

void PreScanOutputDev::clearStats () {
    mono = true;
    gray = true;
    transparency = false;
    patternImgMask = false;
    gdi = true;
}

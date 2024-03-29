// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <climits>
#include <cmath>
#include <cstring>

#include <utils/path.hh>

#include <fofi/FoFiTrueType.hh>

#include <splash/Splash.hh>
#include <splash/SplashBitmap.hh>
#include <splash/SplashErrorCodes.hh>
#include <splash/SplashFont.hh>
#include <splash/SplashFontEngine.hh>
#include <splash/SplashFontFile.hh>
#include <splash/SplashFontFileID.hh>
#include <splash/SplashGlyphBitmap.hh>
#include <splash/SplashPath.hh>
#include <splash/SplashPattern.hh>
#include <splash/SplashScreen.hh>
#include <splash/SplashState.hh>

#include <xpdf/array.hh>
#include <xpdf/BuiltinFont.hh>
#include <xpdf/CharCodeToUnicode.hh>
#include <xpdf/Error.hh>
#include <xpdf/FontEncodingTables.hh>
#include <xpdf/Gfx.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/JPXStream.hh>
#include <xpdf/Link.hh>
#include <xpdf/SplashOutputDev.hh>
#include <xpdf/obj.hh>

//------------------------------------------------------------------------

// Type 3 font cache size parameters
#define type3FontCacheAssoc 8
#define type3FontCacheMaxSets 8
#define type3FontCacheSize (128 * 1024)

//------------------------------------------------------------------------
// Blend functions
//------------------------------------------------------------------------

static void splashOutBlendMultiply(SplashColorPtr src, SplashColorPtr dest,
                                   SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        blend[i] = (dest[i] * src[i]) / 255;
    }
}

static void splashOutBlendScreen(SplashColorPtr src, SplashColorPtr dest,
                                 SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        blend[i] = dest[i] + src[i] - (dest[i] * src[i]) / 255;
    }
}

static void splashOutBlendOverlay(SplashColorPtr src, SplashColorPtr dest,
                                  SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        blend[i] = dest[i] < 0x80 ?
                       (src[i] * 2 * dest[i]) / 255 :
                       255 - 2 * ((255 - src[i]) * (255 - dest[i])) / 255;
    }
}

static void splashOutBlendDarken(SplashColorPtr src, SplashColorPtr dest,
                                 SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        blend[i] = dest[i] < src[i] ? dest[i] : src[i];
    }
}

static void splashOutBlendLighten(SplashColorPtr src, SplashColorPtr dest,
                                  SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        blend[i] = dest[i] > src[i] ? dest[i] : src[i];
    }
}

static void splashOutBlendColorDodge(SplashColorPtr src, SplashColorPtr dest,
                                     SplashColorPtr blend, SplashColorMode cm)
{
    int i, x;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        if (dest[i] == 0) {
            blend[i] = 0;
        } else if (src[i] == 255) {
            blend[i] = 255;
        } else {
            x = (dest[i] * 255) / (255 - src[i]);
            blend[i] = x <= 255 ? x : 255;
        }
    }
}

static void splashOutBlendColorBurn(SplashColorPtr src, SplashColorPtr dest,
                                    SplashColorPtr blend, SplashColorMode cm)
{
    int i, x;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        if (dest[i] == 255) {
            blend[i] = 255;
        } else if (src[i] == 0) {
            blend[i] = 0;
        } else {
            x = ((255 - dest[i]) * 255) / src[i];
            blend[i] = x <= 255 ? 255 - x : 0;
        }
    }
}

static void splashOutBlendHardLight(SplashColorPtr src, SplashColorPtr dest,
                                    SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        blend[i] = src[i] < 0x80 ?
                       (dest[i] * 2 * src[i]) / 255 :
                       255 - 2 * ((255 - dest[i]) * (255 - src[i])) / 255;
    }
}

static void splashOutBlendSoftLight(SplashColorPtr src, SplashColorPtr dest,
                                    SplashColorPtr blend, SplashColorMode cm)
{
    int i, x;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        if (src[i] < 0x80) {
            blend[i] = dest[i] - (255 - 2 * src[i]) * dest[i] * (255 - dest[i]) /
                                     (255 * 255);
        } else {
            if (dest[i] < 0x40) {
                x = (((((16 * dest[i] - 12 * 255) * dest[i]) / 255) + 4 * 255) *
                     dest[i]) /
                    255;
            } else {
                x = (int)sqrt(255.0 * dest[i]);
            }
            blend[i] = dest[i] + (2 * src[i] - 255) * (x - dest[i]) / 255;
        }
    }
}

static void splashOutBlendDifference(SplashColorPtr src, SplashColorPtr dest,
                                     SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        blend[i] = dest[i] < src[i] ? src[i] - dest[i] : dest[i] - src[i];
    }
}

static void splashOutBlendExclusion(SplashColorPtr src, SplashColorPtr dest,
                                    SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    for (i = 0; i < splashColorModeNComps[cm]; ++i) {
        blend[i] = dest[i] + src[i] - (2 * dest[i] * src[i]) / 255;
    }
}

static int getLum(int r, int g, int b)
{
    return (int)(0.3 * r + 0.59 * g + 0.11 * b);
}

static int getSat(int r, int g, int b)
{
    int rgbMin, rgbMax;

    rgbMin = rgbMax = r;
    if (g < rgbMin) {
        rgbMin = g;
    } else if (g > rgbMax) {
        rgbMax = g;
    }
    if (b < rgbMin) {
        rgbMin = b;
    } else if (b > rgbMax) {
        rgbMax = b;
    }
    return rgbMax - rgbMin;
}

static void clipColor(int rIn, int gIn, int bIn, unsigned char *rOut,
                      unsigned char *gOut, unsigned char *bOut)
{
    int lum, rgbMin, rgbMax;

    lum = getLum(rIn, gIn, bIn);
    rgbMin = rgbMax = rIn;
    if (gIn < rgbMin) {
        rgbMin = gIn;
    } else if (gIn > rgbMax) {
        rgbMax = gIn;
    }
    if (bIn < rgbMin) {
        rgbMin = bIn;
    } else if (bIn > rgbMax) {
        rgbMax = bIn;
    }
    if (rgbMin < 0) {
        *rOut = (unsigned char)(lum + ((rIn - lum) * lum) / (lum - rgbMin));
        *gOut = (unsigned char)(lum + ((gIn - lum) * lum) / (lum - rgbMin));
        *bOut = (unsigned char)(lum + ((bIn - lum) * lum) / (lum - rgbMin));
    } else if (rgbMax > 255) {
        *rOut =
            (unsigned char)(lum + ((rIn - lum) * (255 - lum)) / (rgbMax - lum));
        *gOut =
            (unsigned char)(lum + ((gIn - lum) * (255 - lum)) / (rgbMax - lum));
        *bOut =
            (unsigned char)(lum + ((bIn - lum) * (255 - lum)) / (rgbMax - lum));
    } else {
        *rOut = rIn;
        *gOut = gIn;
        *bOut = bIn;
    }
}

static void setLum(unsigned char rIn, unsigned char gIn, unsigned char bIn,
                   int lum, unsigned char *rOut, unsigned char *gOut,
                   unsigned char *bOut)
{
    int d;

    d = lum - getLum(rIn, gIn, bIn);
    clipColor(rIn + d, gIn + d, bIn + d, rOut, gOut, bOut);
}

static void setSat(unsigned char rIn, unsigned char gIn, unsigned char bIn,
                   int sat, unsigned char *rOut, unsigned char *gOut,
                   unsigned char *bOut)
{
    int            rgbMin, rgbMid, rgbMax;
    unsigned char *minOut, *midOut, *maxOut;

    if (rIn < gIn) {
        rgbMin = rIn;
        minOut = rOut;
        rgbMid = gIn;
        midOut = gOut;
    } else {
        rgbMin = gIn;
        minOut = gOut;
        rgbMid = rIn;
        midOut = rOut;
    }
    if (bIn > rgbMid) {
        rgbMax = bIn;
        maxOut = bOut;
    } else if (bIn > rgbMin) {
        rgbMax = rgbMid;
        maxOut = midOut;
        rgbMid = bIn;
        midOut = bOut;
    } else {
        rgbMax = rgbMid;
        maxOut = midOut;
        rgbMid = rgbMin;
        midOut = minOut;
        rgbMin = bIn;
        minOut = bOut;
    }
    if (rgbMax > rgbMin) {
        *midOut = (unsigned char)((rgbMid - rgbMin) * sat) / (rgbMax - rgbMin);
        *maxOut = (unsigned char)sat;
    } else {
        *midOut = *maxOut = 0;
    }
    *minOut = 0;
}

static void splashOutBlendHue(SplashColorPtr src, SplashColorPtr dest,
                              SplashColorPtr blend, SplashColorMode cm)
{
    unsigned char r0, g0, b0;
#if SPLASH_CMYK
    unsigned char r1, g1, b1;
#endif

    switch (cm) {
    case splashModeMono1:
    case splashModeMono8:
        blend[0] = dest[0];
        break;
    case splashModeRGB8:
    case splashModeBGR8:
        setSat(src[0], src[1], src[2], getSat(dest[0], dest[1], dest[2]), &r0,
               &g0, &b0);
        setLum(r0, g0, b0, getLum(dest[0], dest[1], dest[2]), &blend[0],
               &blend[1], &blend[2]);
        break;
#if SPLASH_CMYK
    case splashModeCMYK8:
        // NB: inputs have already been converted to additive mode
        setSat(src[0], src[1], src[2], getSat(dest[0], dest[1], dest[2]), &r0,
               &g0, &b0);
        setLum(r0, g0, b0, getLum(dest[0], dest[1], dest[2]), &r1, &g1, &b1);
        blend[0] = r1;
        blend[1] = g1;
        blend[2] = b1;
        blend[3] = dest[3];
        break;
#endif
    }
}

static void splashOutBlendSaturation(SplashColorPtr src, SplashColorPtr dest,
                                     SplashColorPtr blend, SplashColorMode cm)
{
    unsigned char r0, g0, b0;
#if SPLASH_CMYK
    unsigned char r1, g1, b1;
#endif

    switch (cm) {
    case splashModeMono1:
    case splashModeMono8:
        blend[0] = dest[0];
        break;
    case splashModeRGB8:
    case splashModeBGR8:
        setSat(dest[0], dest[1], dest[2], getSat(src[0], src[1], src[2]), &r0,
               &g0, &b0);
        setLum(r0, g0, b0, getLum(dest[0], dest[1], dest[2]), &blend[0],
               &blend[1], &blend[2]);
        break;
#if SPLASH_CMYK
    case splashModeCMYK8:
        // NB: inputs have already been converted to additive mode
        setSat(dest[0], dest[1], dest[2], getSat(src[0], src[1], src[2]), &r0,
               &g0, &b0);
        setLum(r0, g0, b0, getLum(dest[0], dest[1], dest[2]), &r1, &g1, &b1);
        blend[0] = r1;
        blend[1] = g1;
        blend[2] = b1;
        blend[3] = dest[3];
        break;
#endif
    }
}

static void splashOutBlendColor(SplashColorPtr src, SplashColorPtr dest,
                                SplashColorPtr blend, SplashColorMode cm)
{
#if SPLASH_CMYK
    unsigned char r, g, b;
#endif

    switch (cm) {
    case splashModeMono1:
    case splashModeMono8:
        blend[0] = dest[0];
        break;
    case splashModeRGB8:
    case splashModeBGR8:
        setLum(src[0], src[1], src[2], getLum(dest[0], dest[1], dest[2]),
               &blend[0], &blend[1], &blend[2]);
        break;
#if SPLASH_CMYK
    case splashModeCMYK8:
        // NB: inputs have already been converted to additive mode
        setLum(src[0], src[1], src[2], getLum(dest[0], dest[1], dest[2]), &r, &g,
               &b);
        blend[0] = r;
        blend[1] = g;
        blend[2] = b;
        blend[3] = dest[3];
        break;
#endif
    }
}

static void splashOutBlendLuminosity(SplashColorPtr src, SplashColorPtr dest,
                                     SplashColorPtr blend, SplashColorMode cm)
{
#if SPLASH_CMYK
    unsigned char r, g, b;
#endif

    switch (cm) {
    case splashModeMono1:
    case splashModeMono8:
        blend[0] = dest[0];
        break;
    case splashModeRGB8:
    case splashModeBGR8:
        setLum(dest[0], dest[1], dest[2], getLum(src[0], src[1], src[2]),
               &blend[0], &blend[1], &blend[2]);
        break;
#if SPLASH_CMYK
    case splashModeCMYK8:
        // NB: inputs have already been converted to additive mode
        setLum(dest[0], dest[1], dest[2], getLum(src[0], src[1], src[2]), &r, &g,
               &b);
        blend[0] = r;
        blend[1] = g;
        blend[2] = b;
        blend[3] = src[3];
        break;
#endif
    }
}

// NB: This must match the GfxBlendMode enum defined in GfxState.h.
SplashBlendFunc splashOutBlendFuncs[] = { NULL,
                                          &splashOutBlendMultiply,
                                          &splashOutBlendScreen,
                                          &splashOutBlendOverlay,
                                          &splashOutBlendDarken,
                                          &splashOutBlendLighten,
                                          &splashOutBlendColorDodge,
                                          &splashOutBlendColorBurn,
                                          &splashOutBlendHardLight,
                                          &splashOutBlendSoftLight,
                                          &splashOutBlendDifference,
                                          &splashOutBlendExclusion,
                                          &splashOutBlendHue,
                                          &splashOutBlendSaturation,
                                          &splashOutBlendColor,
                                          &splashOutBlendLuminosity };

//------------------------------------------------------------------------
// SplashOutFontFileID
//------------------------------------------------------------------------

class SplashOutFontFileID : public SplashFontFileID
{
public:
    SplashOutFontFileID(Ref *rA)
    {
        r = *rA;
        substIdx = -1;
        oblique = 0;
    }

    ~SplashOutFontFileID() { }

    bool matches(SplashFontFileID *id)
    {
        return ((SplashOutFontFileID *)id)->r.num == r.num &&
               ((SplashOutFontFileID *)id)->r.gen == r.gen;
    }

    void   setOblique(double obliqueA) { oblique = obliqueA; }
    double getOblique() { return oblique; }
    void   setSubstIdx(int substIdxA) { substIdx = substIdxA; }
    int    getSubstIdx() { return substIdx; }

private:
    Ref    r;
    double oblique;
    int    substIdx;
};

//------------------------------------------------------------------------
// T3FontCache
//------------------------------------------------------------------------

struct T3FontCacheTag
{
    unsigned short code;
    unsigned short mru; // valid bit (0x8000) and MRU index
};

class T3FontCache
{
public:
    T3FontCache(Ref *fontID, double m11A, double m12A, double m21A, double m22A,
                int glyphXA, int glyphYA, int glyphWA, int glyphHA, bool aa,
                bool validBBoxA);
    ~T3FontCache();
    bool matches(Ref *idA, double m11A, double m12A, double m21A, double m22A)
    {
        return fontID.num == idA->num && fontID.gen == idA->gen && m11 == m11A &&
               m12 == m12A && m21 == m21A && m22 == m22A;
    }

    Ref             fontID; // PDF font ID
    double          m11, m12, m21, m22; // transform matrix
    int             glyphX, glyphY; // pixel offset of glyph bitmaps
    int             glyphW, glyphH; // size of glyph bitmaps, in pixels
    bool            validBBox; // false if the bbox was [0 0 0 0]
    int             glyphSize; // size of glyph bitmaps, in bytes
    int             cacheSets; // number of sets in cache
    int             cacheAssoc; // cache associativity (glyphs per set)
    unsigned char * cacheData; // glyph pixmap cache
    T3FontCacheTag *cacheTags; // cache tags, i.e., char codes
};

T3FontCache::T3FontCache(Ref *fontIDA, double m11A, double m12A, double m21A,
                         double m22A, int glyphXA, int glyphYA, int glyphWA,
                         int glyphHA, bool validBBoxA, bool aa)
{
    int i;

    fontID = *fontIDA;
    m11 = m11A;
    m12 = m12A;
    m21 = m21A;
    m22 = m22A;
    glyphX = glyphXA;
    glyphY = glyphYA;
    glyphW = glyphWA;
    glyphH = glyphHA;
    validBBox = validBBoxA;
    // sanity check for excessively large glyphs (which most likely
    // indicate an incorrect BBox)
    i = glyphW * glyphH;
    if (i > 100000 || glyphW > INT_MAX / glyphH || glyphW <= 0 || glyphH <= 0) {
        glyphW = glyphH = 100;
        validBBox = false;
    }
    if (aa) {
        glyphSize = glyphW * glyphH;
    } else {
        glyphSize = ((glyphW + 7) >> 3) * glyphH;
    }
    cacheAssoc = type3FontCacheAssoc;
    for (cacheSets = type3FontCacheMaxSets;
         cacheSets > 1 && cacheSets * cacheAssoc * glyphSize > type3FontCacheSize;
         cacheSets >>= 1)
        ;
    cacheData = (unsigned char *)calloc(cacheSets * cacheAssoc, glyphSize);
    cacheTags =
        (T3FontCacheTag *)calloc(cacheSets * cacheAssoc, sizeof(T3FontCacheTag));
    for (i = 0; i < cacheSets * cacheAssoc; ++i) {
        cacheTags[i].mru = i & (cacheAssoc - 1);
    }
}

T3FontCache::~T3FontCache()
{
    free(cacheData);
    free(cacheTags);
}

struct T3GlyphStack
{
    unsigned short code; // character code

    bool haveDx; // set after seeing a d0/d1 operator
    bool doNotCache; // set if we see a gsave/grestore before
        //   the d0/d1

    //----- cache info
    T3FontCache *   cache; // font cache for the current font
    T3FontCacheTag *cacheTag; // pointer to cache tag for the glyph
    unsigned char * cacheData; // pointer to cache data for the glyph

    //----- saved state
    SplashBitmap *origBitmap;
    Splash *      origSplash;
    double        origCTM4, origCTM5;

    T3GlyphStack *next; // next object on stack
};

//------------------------------------------------------------------------
// SplashTransparencyGroup
//------------------------------------------------------------------------

struct SplashTransparencyGroup
{
    int            tx, ty; // translation coordinates
    SplashBitmap * tBitmap; // bitmap for transparency group
    GfxColorSpace *blendingColorSpace;
    bool           isolated;

    //----- saved state
    SplashBitmap *origBitmap;
    Splash *      origSplash;

    SplashTransparencyGroup *next;
};

//------------------------------------------------------------------------
// SplashOutputDev
//------------------------------------------------------------------------

SplashOutputDev::SplashOutputDev(SplashColorMode colorModeA, int bitmapRowPadA,
                                 bool reverseVideoA, SplashColorPtr paperColorA,
                                 bool bitmapTopDownA, bool allowAntialiasA)
{
    colorMode = colorModeA;
    bitmapRowPad = bitmapRowPadA;
    bitmapTopDown = bitmapTopDownA;
    bitmapUpsideDown = false;
    noComposite = false;
    allowAntialias = allowAntialiasA;
    vectorAntialias = allowAntialias && globalParams->getVectorAntialias() &&
                      colorMode != splashModeMono1;
    setupScreenParams(72.0, 72.0);
    reverseVideo = reverseVideoA;
    splashColorCopy(paperColor, paperColorA);
    skipHorizText = false;
    skipRotatedText = false;

    xref = NULL;

    bitmap = new SplashBitmap(1, 1, bitmapRowPad, colorMode,
                              colorMode != splashModeMono1, bitmapTopDown);
    splash = new Splash(bitmap, vectorAntialias, &screenParams);
    splash->setMinLineWidth(globalParams->getMinLineWidth());
    splash->setStrokeAdjust(globalParams->getStrokeAdjust());
    splash->clear(paperColor, 0);

    fontEngine = NULL;

    nT3Fonts = 0;
    t3GlyphStack = NULL;

    font = NULL;
    needFontUpdate = false;
    textClipPath = NULL;

    transpGroupStack = NULL;

    nestCount = 0;
}

void SplashOutputDev::setupScreenParams(double hDPI, double vDPI)
{
    screenParams.size = globalParams->getScreenSize();
    screenParams.dotRadius = globalParams->getScreenDotRadius();
    screenParams.gamma = (SplashCoord)globalParams->getScreenGamma();
    screenParams.blackThreshold =
        (SplashCoord)globalParams->getScreenBlackThreshold();
    screenParams.whiteThreshold =
        (SplashCoord)globalParams->getScreenWhiteThreshold();
    switch (globalParams->getScreenType()) {
    case screenDispersed:
        screenParams.type = splashScreenDispersed;
        if (screenParams.size < 0) {
            screenParams.size = 4;
        }
        break;
    case screenClustered:
        screenParams.type = splashScreenClustered;
        if (screenParams.size < 0) {
            screenParams.size = 10;
        }
        break;
    case screenStochasticClustered:
        screenParams.type = splashScreenStochasticClustered;
        if (screenParams.size < 0) {
            screenParams.size = 64;
        }
        if (screenParams.dotRadius < 0) {
            screenParams.dotRadius = 2;
        }
        break;
    case screenUnset:
    default:
        // use clustered dithering for resolution >= 300 dpi
        // (compare to 299.9 to avoid floating point issues)
        if (hDPI > 299.9 && vDPI > 299.9) {
            screenParams.type = splashScreenStochasticClustered;
            if (screenParams.size < 0) {
                screenParams.size = 64;
            }
            if (screenParams.dotRadius < 0) {
                screenParams.dotRadius = 2;
            }
        } else {
            screenParams.type = splashScreenDispersed;
            if (screenParams.size < 0) {
                screenParams.size = 4;
            }
        }
    }
}

SplashOutputDev::~SplashOutputDev()
{
    int i;

    for (i = 0; i < nT3Fonts; ++i) {
        delete t3FontCache[i];
    }
    if (fontEngine) {
        delete fontEngine;
    }
    if (splash) {
        delete splash;
    }
    if (bitmap) {
        delete bitmap;
    }
}

void SplashOutputDev::startDoc(XRef *xrefA)
{
    int i;

    xref = xrefA;
    if (fontEngine) {
        delete fontEngine;
    }
    fontEngine = new SplashFontEngine(
        globalParams->getEnableFreeType(),
        globalParams->getDisableFreeTypeHinting() ? splashFTNoHinting : 0,
        allowAntialias && globalParams->getAntialias() &&
            colorMode != splashModeMono1);
    for (i = 0; i < nT3Fonts; ++i) {
        delete t3FontCache[i];
    }
    nT3Fonts = 0;
}

void SplashOutputDev::startPage(int pageNum, GfxState *state)
{
    int         w, h;
    double *    ctm;
    SplashCoord mat[6];
    SplashColor color;

    if (state) {
        setupScreenParams(state->getHDPI(), state->getVDPI());
        w = (int)(state->getPageWidth() + 0.5);
        if (w <= 0) {
            w = 1;
        }
        h = (int)(state->getPageHeight() + 0.5);
        if (h <= 0) {
            h = 1;
        }
    } else {
        w = h = 1;
    }
    if (splash) {
        delete splash;
        splash = NULL;
    }
    if (!bitmap || w != bitmap->getWidth() || h != bitmap->getHeight()) {
        if (bitmap) {
            delete bitmap;
            bitmap = NULL;
        }
        bitmap = new SplashBitmap(w, h, bitmapRowPad, colorMode,
                                  colorMode != splashModeMono1, bitmapTopDown);
    }
    splash = new Splash(bitmap, vectorAntialias, &screenParams);
    splash->setMinLineWidth(globalParams->getMinLineWidth());
    if (state) {
        ctm = state->getCTM();
        mat[0] = (SplashCoord)ctm[0];
        mat[1] = (SplashCoord)ctm[1];
        mat[2] = (SplashCoord)ctm[2];
        mat[3] = (SplashCoord)ctm[3];
        mat[4] = (SplashCoord)ctm[4];
        mat[5] = (SplashCoord)ctm[5];
        splash->setMatrix(mat);
    }
    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        color[0] = 0;
        break;
    case splashModeRGB8:
    case splashModeBGR8:
        color[0] = color[1] = color[2] = 0;
        break;
#if SPLASH_CMYK
    case splashModeCMYK8:
        color[0] = color[1] = color[2] = color[3] = 0;
        break;
#endif
    }
    splash->setStrokePattern(new SplashSolidColor(color));
    splash->setFillPattern(new SplashSolidColor(color));
    splash->setLineCap(splashLineCapButt);
    splash->setLineJoin(splashLineJoinMiter);
    splash->setLineDash(NULL, 0, 0);
    splash->setMiterLimit(10);
    splash->setFlatness(1);
    // the SA parameter supposedly defaults to false, but Acrobat
    // apparently hardwires it to true
    splash->setStrokeAdjust(globalParams->getStrokeAdjust());
    splash->clear(paperColor, 0);
}

void SplashOutputDev::endPage()
{
    if (colorMode != splashModeMono1 && !noComposite) {
        splash->compositeBackground(paperColor);
    }
}

void SplashOutputDev::saveState(GfxState *state)
{
    splash->saveState();
    if (t3GlyphStack && !t3GlyphStack->haveDx) {
        t3GlyphStack->doNotCache = true;
        error(errSyntaxWarning, -1,
              "Save (q) operator before d0/d1 in Type 3 glyph");
    }
}

void SplashOutputDev::restoreState(GfxState *state)
{
    splash->restoreState();
    needFontUpdate = true;
    if (t3GlyphStack && !t3GlyphStack->haveDx) {
        t3GlyphStack->doNotCache = true;
        error(errSyntaxWarning, -1,
              "Restore (Q) operator before d0/d1 in Type 3 glyph");
    }
}

void SplashOutputDev::updateAll(GfxState *state)
{
    updateLineDash(state);
    updateLineJoin(state);
    updateLineCap(state);
    updateLineWidth(state);
    updateFlatness(state);
    updateMiterLimit(state);
    updateStrokeAdjust(state);
    updateFillColor(state);
    updateStrokeColor(state);
    needFontUpdate = true;
}

void SplashOutputDev::updateCTM(GfxState *state, double m11, double m12,
                                double m21, double m22, double m31, double m32)
{
    double *    ctm;
    SplashCoord mat[6];

    ctm = state->getCTM();
    mat[0] = (SplashCoord)ctm[0];
    mat[1] = (SplashCoord)ctm[1];
    mat[2] = (SplashCoord)ctm[2];
    mat[3] = (SplashCoord)ctm[3];
    mat[4] = (SplashCoord)ctm[4];
    mat[5] = (SplashCoord)ctm[5];
    splash->setMatrix(mat);
}

void SplashOutputDev::updateLineDash(GfxState *state)
{
    double *    dashPattern;
    int         dashLength;
    double      dashStart;
    SplashCoord dash[20];
    int         i;

    state->getLineDash(&dashPattern, &dashLength, &dashStart);
    if (dashLength > 20) {
        dashLength = 20;
    }
    for (i = 0; i < dashLength; ++i) {
        dash[i] = (SplashCoord)dashPattern[i];
        if (dash[i] < 0) {
            dash[i] = 0;
        }
    }
    splash->setLineDash(dash, dashLength, (SplashCoord)dashStart);
}

void SplashOutputDev::updateFlatness(GfxState *state)
{
#if 0
    // Acrobat ignores the flatness setting, and always renders curves
    // with a fairly small flatness value
  splash->setFlatness(state->getFlatness());
#endif
}

void SplashOutputDev::updateLineJoin(GfxState *state)
{
    splash->setLineJoin(state->getLineJoin());
}

void SplashOutputDev::updateLineCap(GfxState *state)
{
    splash->setLineCap(state->getLineCap());
}

void SplashOutputDev::updateMiterLimit(GfxState *state)
{
    splash->setMiterLimit(state->getMiterLimit());
}

void SplashOutputDev::updateLineWidth(GfxState *state)
{
    splash->setLineWidth(state->getLineWidth());
}

void SplashOutputDev::updateStrokeAdjust(GfxState *state)
{
#if 0
    // the SA parameter supposedly defaults to false, but Acrobat
    // apparently hardwires it to true
  splash->setStrokeAdjust(state->getStrokeAdjust());
#endif
}

void SplashOutputDev::updateFillColor(GfxState *state)
{
    GfxGray gray;
    GfxRGB  rgb;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif

    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        state->getFillGray(&gray);
        splash->setFillPattern(getColor(gray));
        break;
    case splashModeRGB8:
    case splashModeBGR8:
        state->getFillRGB(&rgb);
        splash->setFillPattern(getColor(&rgb));
        break;
#if SPLASH_CMYK
    case splashModeCMYK8:
        state->getFillCMYK(&cmyk);
        splash->setFillPattern(getColor(&cmyk));
        break;
#endif
    }
}

void SplashOutputDev::updateStrokeColor(GfxState *state)
{
    GfxGray gray;
    GfxRGB  rgb;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif

    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        state->getStrokeGray(&gray);
        splash->setStrokePattern(getColor(gray));
        break;
    case splashModeRGB8:
    case splashModeBGR8:
        state->getStrokeRGB(&rgb);
        splash->setStrokePattern(getColor(&rgb));
        break;
#if SPLASH_CMYK
    case splashModeCMYK8:
        state->getStrokeCMYK(&cmyk);
        splash->setStrokePattern(getColor(&cmyk));
        break;
#endif
    }
}

SplashPattern *SplashOutputDev::getColor(GfxGray gray)
{
    SplashColor color;

    if (reverseVideo) {
        gray.x = XPDF_FIXED_POINT_ONE - gray.x;
    }
    color[0] = xpdf::to_small_color(gray.x);
    return new SplashSolidColor(color);
}

SplashPattern *SplashOutputDev::getColor(GfxRGB *rgb)
{
    xpdf::color_t r, g, b;
    SplashColor   color;

    if (reverseVideo) {
        r = XPDF_FIXED_POINT_ONE - rgb->r;
        g = XPDF_FIXED_POINT_ONE - rgb->g;
        b = XPDF_FIXED_POINT_ONE - rgb->b;
    } else {
        r = rgb->r;
        g = rgb->g;
        b = rgb->b;
    }
    color[0] = xpdf::to_small_color(r);
    color[1] = xpdf::to_small_color(g);
    color[2] = xpdf::to_small_color(b);
    return new SplashSolidColor(color);
}

#if SPLASH_CMYK
SplashPattern *SplashOutputDev::getColor(GfxCMYK *cmyk)
{
    SplashColor color;

    color[0] = xpdf::to_small_color(cmyk->c);
    color[1] = xpdf::to_small_color(cmyk->m);
    color[2] = xpdf::to_small_color(cmyk->y);
    color[3] = xpdf::to_small_color(cmyk->k);
    return new SplashSolidColor(color);
}
#endif

void SplashOutputDev::setOverprintMask(GfxColorSpace *colorSpace,
                                       bool overprintFlag, int overprintMode,
                                       GfxColor *singleColor)
{
#if SPLASH_CMYK
    unsigned mask;
    GfxCMYK  cmyk;

    if (overprintFlag && globalParams->getOverprintPreview()) {
        mask = colorSpace->getOverprintMask();
        // The OPM (overprintMode) setting is only relevant when the color
        // space is DeviceCMYK or is "implicitly converted to DeviceCMYK".
        // Per the PDF spec, this happens with ICCBased color spaces only
        // if the profile matches the output device -- Acrobat's output
        // preview mode does NOT honor OPM=1 for ICCBased CMYK color
        // spaces.  To change the behavior here, use:
        //    if (singleColor && overprintMode &&
        //        (colorSpace->getMode() == csDeviceCMYK ||
        //         (colorSpace->getMode() == csICCBased &&
        //          colorSpace->getNComps() == 4 &&
        //          <...the profile matches...>)))
        if (singleColor && overprintMode &&
            colorSpace->getMode() == csDeviceCMYK) {
            colorSpace->getCMYK(singleColor, &cmyk);
            if (cmyk.c == 0) {
                mask &= ~1;
            }
            if (cmyk.m == 0) {
                mask &= ~2;
            }
            if (cmyk.y == 0) {
                mask &= ~4;
            }
            if (cmyk.k == 0) {
                mask &= ~8;
            }
        }
    } else {
        mask = 0xffffffff;
    }
    splash->setOverprintMask(mask);
#endif
}

void SplashOutputDev::updateBlendMode(GfxState *state)
{
    splash->setBlendFunc(splashOutBlendFuncs[state->getBlendMode()]);
}

void SplashOutputDev::updateFillOpacity(GfxState *state)
{
    splash->setFillAlpha((SplashCoord)state->getFillOpacity());
}

void SplashOutputDev::updateStrokeOpacity(GfxState *state)
{
    splash->setStrokeAlpha((SplashCoord)state->getStrokeOpacity());
}

void SplashOutputDev::updateTransfer(GfxState *state)
{
    unsigned char red[256], green[256], blue[256], gray[256];
    double        x, y;
    int           i;

    Function *ts = state->getTransfer();

    auto is_unary = [](auto &f) { return 1 == f.arity() && 1 == f.coarity(); };

    if (ts[0] && is_unary(ts[0])) {
        if (ts[1] && is_unary(ts[1]) && ts[2] && is_unary(ts[2]) && ts[3] &&
            is_unary(ts[3])) {
            for (i = 0; i < 256; ++i) {
                x = i / 255.0;

                ts[0](&x, &x + 1, &y);
                red[i] = (unsigned char)(y * 255.0 + 0.5);

                ts[1](&x, &x + 1, &y);
                green[i] = (unsigned char)(y * 255.0 + 0.5);

                ts[2](&x, &x + 1, &y);
                blue[i] = (unsigned char)(y * 255.0 + 0.5);

                ts[3](&x, &x + 1, &y);
                gray[i] = (unsigned char)(y * 255.0 + 0.5);
            }
        } else {
            for (i = 0; i < 256; ++i) {
                x = i / 255.0;
                ts[0](&x, &x + 1, &y);
                red[i] = green[i] = blue[i] = gray[i] =
                    (unsigned char)(y * 255.0 + 0.5);
            }
        }
    } else {
        for (i = 0; i < 256; ++i) {
            red[i] = green[i] = blue[i] = gray[i] = (unsigned char)i;
        }
    }

    splash->setTransfer(red, green, blue, gray);
}

void SplashOutputDev::updateFont(GfxState *state)
{
    needFontUpdate = true;
}

void SplashOutputDev::doUpdateFont(GfxState *state)
{
    GfxFont *            gfxFont;
    GfxFontLoc *         fontLoc;
    GfxFontType          fontType;
    SplashOutFontFileID *id;
    SplashFontFile *     fontFile;
    int                  fontNum;
    FoFiTrueType *       ff;
    Ref                  embRef;
    Object               refObj, strObj;
    GString *            fontBuf;
    FILE *               extFontFile;
    char                 blk[4096];
    int *                codeToGID;
    CharCodeToUnicode *  ctu;
    double *             textMat;
    double               m11, m12, m21, m22, fontSize, oblique;
    double               fsx, fsy, w, fontScaleMin, fontScaleAvg, fontScale;
    SplashCoord          mat[4];
    char *               name;
    Unicode              uBuf[8];
    int                  substIdx, n, code, cmap, i;

    needFontUpdate = false;
    font = NULL;
    fontBuf = NULL;
    substIdx = -1;

    if (!(gfxFont = state->getFont())) {
        goto err1;
    }
    fontType = gfxFont->getType();
    if (fontType == fontType3) {
        goto err1;
    }

    // sanity-check the font size - skip anything larger than 20 inches
    // (this avoids problems allocating memory for the font cache)
    state->textTransformDelta(state->getFontSize(), state->getFontSize(), &fsx,
                              &fsy);
    state->transformDelta(fsx, fsy, &fsx, &fsy);
    if (fabs(fsx) > 20 * state->getHDPI() || fabs(fsy) > 20 * state->getVDPI()) {
        goto err1;
    }

    // check the font file cache
    id = new SplashOutFontFileID(gfxFont->getID());
    if ((fontFile = fontEngine->getFontFile(id))) {
        delete id;
    } else {
        fontNum = 0;

        if (!(fontLoc = gfxFont->locateFont(xref, false))) {
            error(errSyntaxError, -1, "(Splash) Couldn't find a font for '{0:s}'",
                  gfxFont->as_name() ? gfxFont->as_name()->c_str() : "(unnamed)");
            goto err2;
        }

        // embedded font
        if (fontLoc->locType == gfxFontLocEmbedded) {
            gfxFont->getEmbeddedFontID(&embRef);
            fontBuf = new GString();
            refObj = xpdf::make_ref_obj(embRef.num, embRef.gen, xref);
            strObj = resolve(refObj);
            if (!strObj.is_stream()) {
                error(errSyntaxError, -1, "Embedded font object is wrong type");
                delete fontLoc;
                goto err2;
            }
            strObj.streamReset();
            while ((n = strObj.streamGetBlock(blk, sizeof(blk))) > 0) {
                fontBuf->append(blk, n);
            }
            strObj.streamClose();
            // external font
        } else { // gfxFontLocExternal
            if (!(extFontFile = fopen(fontLoc->path->c_str(), "rb"))) {
                error(errSyntaxError, -1,
                      "Couldn't open external font file '{0:t}'", fontLoc->path);
                delete fontLoc;
                goto err2;
            }
            fontBuf = new GString();
            while ((n = fread(blk, 1, sizeof(blk), extFontFile)) > 0) {
                fontBuf->append(blk, n);
            }
            fclose(extFontFile);
            fontNum = fontLoc->fontNum;
            if (fontLoc->substIdx >= 0) {
                id->setSubstIdx(fontLoc->substIdx);
            }
            if (fontLoc->oblique != 0) {
                id->setOblique(fontLoc->oblique);
            }
        }

        // load the font file
        switch (fontLoc->fontType) {
        case fontType1:
            if (!(fontFile = fontEngine->loadType1Font(
                      id, fontBuf,
                      (const char **)((Gfx8BitFont *)gfxFont)->getEncoding()))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'",
                      gfxFont->as_name() ? gfxFont->as_name()->c_str() :
                                           "(unnamed)");
                delete fontLoc;
                goto err2;
            }
            break;
        case fontType1C:
            if (!(fontFile = fontEngine->loadType1CFont(
                      id, fontBuf,
                      (const char **)((Gfx8BitFont *)gfxFont)->getEncoding()))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'",
                      gfxFont->as_name() ? gfxFont->as_name()->c_str() :
                                           "(unnamed)");
                delete fontLoc;
                goto err2;
            }
            break;
        case fontType1COT:
            if (!(fontFile = fontEngine->loadOpenTypeT1CFont(
                      id, fontBuf,
                      (const char **)((Gfx8BitFont *)gfxFont)->getEncoding()))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'",
                      gfxFont->as_name() ? gfxFont->as_name()->c_str() :
                                           "(unnamed)");
                delete fontLoc;
                goto err2;
            }
            break;
        case fontTrueType:
        case fontTrueTypeOT:
            if ((ff = FoFiTrueType::make(fontBuf->c_str(), fontBuf->getLength(),
                                         fontNum))) {
                codeToGID = ((Gfx8BitFont *)gfxFont)->getCodeToGIDMap(ff);
                n = 256;
                delete ff;
                // if we're substituting for a non-TrueType font, we need to mark
                // all notdef codes as "do not draw" (rather than drawing TrueType
                // notdef glyphs)
                if (gfxFont->getType() != fontTrueType &&
                    gfxFont->getType() != fontTrueTypeOT) {
                    for (i = 0; i < 256; ++i) {
                        if (codeToGID[i] == 0) {
                            codeToGID[i] = -1;
                        }
                    }
                }
            } else {
                codeToGID = NULL;
                n = 0;
            }
            if (!(fontFile = fontEngine->loadTrueTypeFont(
                      id, fontBuf, fontNum, codeToGID, n,
                      gfxFont->getEmbeddedFontName() ?
                          gfxFont->getEmbeddedFontName()->c_str() :
                          (char *)NULL))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'",
                      gfxFont->as_name() ? gfxFont->as_name()->c_str() :
                                           "(unnamed)");
                delete fontLoc;
                goto err2;
            }
            break;
        case fontCIDType0:
        case fontCIDType0C:
            if (!(fontFile = fontEngine->loadCIDFont(id, fontBuf))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'",
                      gfxFont->as_name() ? gfxFont->as_name()->c_str() :
                                           "(unnamed)");
                delete fontLoc;
                goto err2;
            }
            break;
        case fontCIDType0COT:
            if (((GfxCIDFont *)gfxFont)->getCIDToGID()) {
                n = ((GfxCIDFont *)gfxFont)->getCIDToGIDLen();
                codeToGID = (int *)calloc(n, sizeof(int));
                memcpy(codeToGID, ((GfxCIDFont *)gfxFont)->getCIDToGID(),
                       n * sizeof(int));
            } else {
                codeToGID = NULL;
                n = 0;
            }
            if (!(fontFile = fontEngine->loadOpenTypeCFFFont(id, fontBuf,
                                                             codeToGID, n))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'",
                      gfxFont->as_name() ? gfxFont->as_name()->c_str() :
                                           "(unnamed)");
                delete fontLoc;
                goto err2;
            }
            break;
        case fontCIDType2:
        case fontCIDType2OT:
            codeToGID = NULL;
            n = 0;
            if (fontLoc->locType == gfxFontLocEmbedded) {
                if (((GfxCIDFont *)gfxFont)->getCIDToGID()) {
                    n = ((GfxCIDFont *)gfxFont)->getCIDToGIDLen();
                    codeToGID = (int *)calloc(n, sizeof(int));
                    memcpy(codeToGID, ((GfxCIDFont *)gfxFont)->getCIDToGID(),
                           n * sizeof(int));
                }
            } else if (!globalParams->getMapExtTrueTypeFontsViaUnicode()) {
                codeToGID = NULL;
                n = 0;
            } else {
                // create a CID-to-GID mapping, via Unicode
                if ((ctu = ((GfxCIDFont *)gfxFont)->getToUnicode())) {
                    if ((ff = FoFiTrueType::make(
                             fontBuf->c_str(), fontBuf->getLength(), fontNum))) {
                        // look for a Unicode cmap
                        for (cmap = 0; cmap < ff->getNumCmaps(); ++cmap) {
                            if ((ff->getCmapPlatform(cmap) == 3 &&
                                 ff->getCmapEncoding(cmap) == 1) ||
                                ff->getCmapPlatform(cmap) == 0) {
                                break;
                            }
                        }
                        if (cmap < ff->getNumCmaps()) {
                            // map CID -> Unicode -> GID
                            if (ctu->isIdentity()) {
                                n = 65536;
                            } else {
                                n = ctu->getLength();
                            }
                            codeToGID = (int *)calloc(n, sizeof(int));
                            for (code = 0; code < n; ++code) {
                                if (ctu->mapToUnicode(code, uBuf, 8) > 0) {
                                    codeToGID[code] =
                                        ff->mapCodeToGID(cmap, uBuf[0]);
                                } else {
                                    codeToGID[code] = -1;
                                }
                            }
                        }
                        delete ff;
                    }
                    ctu->decRefCnt();
                } else {
                    error(errSyntaxError, -1,
                          "Couldn't find a mapping to Unicode for font '{0:s}'",
                          gfxFont->as_name() ? gfxFont->as_name()->c_str() :
                                               "(unnamed)");
                }
            }
            if (!(fontFile = fontEngine->loadTrueTypeFont(
                      id, fontBuf, fontNum, codeToGID, n,
                      gfxFont->getEmbeddedFontName() ?
                          gfxFont->getEmbeddedFontName()->c_str() :
                          (char *)NULL))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'",
                      gfxFont->as_name() ? gfxFont->as_name()->c_str() :
                                           "(unnamed)");
                delete fontLoc;
                goto err2;
            }
            break;
        default:
            // this shouldn't happen
            goto err2;
        }

        delete fontLoc;
    }

    // get the font matrix
    textMat = state->getTextMat();
    fontSize = state->getFontSize();
    oblique = ((SplashOutFontFileID *)fontFile->getID())->getOblique();
    m11 = state->getHorizScaling() * textMat[0];
    m12 = state->getHorizScaling() * textMat[1];
    m21 = oblique * m11 + textMat[2];
    m22 = oblique * m12 + textMat[3];
    m11 *= fontSize;
    m12 *= fontSize;
    m21 *= fontSize;
    m22 *= fontSize;

    // for substituted fonts: adjust the font matrix -- compare the
    // widths of letters and digits (A-Z, a-z, 0-9) in the original font
    // and the substituted font
    substIdx = ((SplashOutFontFileID *)fontFile->getID())->getSubstIdx();

    if (substIdx >= 0 && substIdx < 12) {
        fontScaleMin = 1;
        fontScaleAvg = 0;

        n = 0;
        for (code = 0; code < 256; ++code) {
            if ((name = ((Gfx8BitFont *)gfxFont)->getCharName(code)) && name[0] &&
                !name[1] &&
                ((name[0] >= 'A' && name[0] <= 'Z') ||
                 (name[0] >= 'a' && name[0] <= 'z') ||
                 (name[0] >= '0' && name[0] <= '9'))) {
                w = ((Gfx8BitFont *)gfxFont)->getWidth(code);

                if (auto p = xpdf::builtin_substitute_font(substIdx)) {
                    int width = 0;

                    try {
                        width = p->widths.at(name);
                    } catch(...) { }

                    if (w > 0.01 && width > 10) {
                        w /= width * .001;

                        if (w < fontScaleMin)
                            fontScaleMin = w;

                        fontScaleAvg += w;
                        ++n;
                    }
                }
            }
        }
        // if real font is narrower than substituted font, reduce the font
        // size accordingly -- this currently uses a scale factor halfway
        // between the minimum and average computed scale factors, which
        // is a bit of a kludge, but seems to produce mostly decent
        // results
        if (n) {
            fontScaleAvg /= n;
            if (fontScaleAvg < 1) {
                fontScale = 0.5 * (fontScaleMin + fontScaleAvg);
                m11 *= fontScale;
                m12 *= fontScale;
            }
        }
    }

    // create the scaled font
    mat[0] = m11;
    mat[1] = m12;
    mat[2] = m21;
    mat[3] = m22;
    font = fontEngine->getFont(fontFile, mat, splash->getMatrix());

    return;

err2:
    delete id;
err1:
    if (fontBuf) {
        delete fontBuf;
    }
    return;
}

void SplashOutputDev::stroke(GfxState *state)
{
    SplashPath *path;

    if (state->getStrokeColorSpace()->isNonMarking()) {
        return;
    }
    setOverprintMask(state->getStrokeColorSpace(), state->getStrokeOverprint(),
                     state->getOverprintMode(), state->getStrokeColor());
    path = convertPath(state, state->getPath(), false);
    splash->stroke(path);
    delete path;
}

void SplashOutputDev::fill(GfxState *state)
{
    SplashPath *path;

    if (state->getFillColorSpace()->isNonMarking()) {
        return;
    }
    setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(),
                     state->getOverprintMode(), state->getFillColor());
    path = convertPath(state, state->getPath(), true);
    splash->fill(path, false);
    delete path;
}

void SplashOutputDev::eoFill(GfxState *state)
{
    SplashPath *path;

    if (state->getFillColorSpace()->isNonMarking()) {
        return;
    }
    setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(),
                     state->getOverprintMode(), state->getFillColor());
    path = convertPath(state, state->getPath(), true);
    splash->fill(path, true);
    delete path;
}

void SplashOutputDev::tilingPatternFill(GfxState *state, Gfx *gfx, Object *strRef,
                                        int paintType, Dict *resDict, double *mat,
                                        double *bbox, int x0, int y0, int x1,
                                        int y1, double xStep, double yStep)
{
    double        tileXMin, tileYMin, tileXMax, tileYMax, tx, ty;
    int           tileX0, tileY0, tileW, tileH, tileSize;
    SplashBitmap *origBitmap, *tileBitmap;
    Splash *      origSplash;
    SplashColor   color;
    double        mat1[6];
    double        xa, ya, xb, yb, xc, yc;
    int           x, y, xx, yy, i;

    // transform the four corners of the bbox from pattern space to
    // device space and compute the device space bbox
    state->transform(bbox[0] * mat[0] + bbox[1] * mat[2] + mat[4],
                     bbox[0] * mat[1] + bbox[1] * mat[3] + mat[5], &tx, &ty);
    tileXMin = tileXMax = tx;
    tileYMin = tileYMax = ty;
    state->transform(bbox[2] * mat[0] + bbox[1] * mat[2] + mat[4],
                     bbox[2] * mat[1] + bbox[1] * mat[3] + mat[5], &tx, &ty);
    if (tx < tileXMin) {
        tileXMin = tx;
    } else if (tx > tileXMax) {
        tileXMax = tx;
    }
    if (ty < tileYMin) {
        tileYMin = ty;
    } else if (ty > tileYMax) {
        tileYMax = ty;
    }
    state->transform(bbox[2] * mat[0] + bbox[3] * mat[2] + mat[4],
                     bbox[2] * mat[1] + bbox[3] * mat[3] + mat[5], &tx, &ty);
    if (tx < tileXMin) {
        tileXMin = tx;
    } else if (tx > tileXMax) {
        tileXMax = tx;
    }
    if (ty < tileYMin) {
        tileYMin = ty;
    } else if (ty > tileYMax) {
        tileYMax = ty;
    }
    state->transform(bbox[0] * mat[0] + bbox[3] * mat[2] + mat[4],
                     bbox[0] * mat[1] + bbox[3] * mat[3] + mat[5], &tx, &ty);
    if (tx < tileXMin) {
        tileXMin = tx;
    } else if (tx > tileXMax) {
        tileXMax = tx;
    }
    if (ty < tileYMin) {
        tileYMin = ty;
    } else if (ty > tileYMax) {
        tileYMax = ty;
    }
    if (tileXMin == tileXMax || tileYMin == tileYMax) {
        return;
    }

    tileX0 = (int)floor(tileXMin);
    tileY0 = (int)floor(tileYMin);
    tileW = (int)ceil(tileXMax) - tileX0;
    tileH = (int)ceil(tileYMax) - tileY0;

    // check for an excessively large tile size
    tileSize = tileW * tileH;
    if (tileSize > 1000000 || tileSize < 0) {
        mat1[0] = mat[0];
        mat1[1] = mat[1];
        mat1[2] = mat[2];
        mat1[3] = mat[3];
        for (y = y0; y < y1; ++y) {
            for (x = x0; x < x1; ++x) {
                xa = x * xStep;
                ya = y * yStep;
                mat1[4] = xa * mat[0] + ya * mat[2] + mat[4];
                mat1[5] = xa * mat[1] + ya * mat[3] + mat[5];
                gfx->drawForm(strRef, resDict, mat1, bbox);
            }
        }
        return;
    }

    // create a temporary bitmap
    origBitmap = bitmap;
    origSplash = splash;
    bitmap = tileBitmap = new SplashBitmap(tileW, tileH, bitmapRowPad, colorMode,
                                           true, bitmapTopDown);
    splash = new Splash(bitmap, vectorAntialias, origSplash->getScreen());
    splash->setMinLineWidth(globalParams->getMinLineWidth());
    splash->setStrokeAdjust(globalParams->getStrokeAdjust());
    for (i = 0; i < splashMaxColorComps; ++i) {
        color[i] = 0;
    }
    splash->clear(color);
    ++nestCount;

    // copy the fill color (for uncolored tiling patterns)
    // (and stroke color, to handle buggy PDF files)
    splash->setFillPattern(origSplash->getFillPattern()->copy());
    splash->setStrokePattern(origSplash->getStrokePattern()->copy());

    // render the tile
    state->shiftCTM(-tileX0, -tileY0);
    updateCTM(state, 0, 0, 0, 0, 0, 0);
    gfx->drawForm(strRef, resDict, mat, bbox);
    state->shiftCTM(tileX0, tileY0);
    updateCTM(state, 0, 0, 0, 0, 0, 0);

    // restore the original bitmap
    --nestCount;
    delete splash;
    bitmap = origBitmap;
    splash = origSplash;
    splash->setOverprintMask(0xffffffff);

    // draw the tiles
    for (y = y0; y < y1; ++y) {
        for (x = x0; x < x1; ++x) {
            xa = x * xStep;
            ya = y * yStep;
            xb = xa * mat[0] + ya * mat[2];
            yb = xa * mat[1] + ya * mat[3];
            state->transformDelta(xb, yb, &xc, &yc);
            xx = (int)(xc + tileX0 + 0.5);
            yy = (int)(yc + tileY0 + 0.5);
            splash->composite(tileBitmap, 0, 0, xx, yy, tileW, tileH, false,
                              false);
        }
    }

    delete tileBitmap;
}

void SplashOutputDev::clip(GfxState *state)
{
    SplashPath *path;

    path = convertPath(state, state->getPath(), true);
    splash->clipToPath(path, false);
    delete path;
}

void SplashOutputDev::eoClip(GfxState *state)
{
    SplashPath *path;

    path = convertPath(state, state->getPath(), true);
    splash->clipToPath(path, true);
    delete path;
}

void SplashOutputDev::clipToStrokePath(GfxState *state)
{
    SplashPath *path, *path2;

    path = convertPath(state, state->getPath(), false);
    path2 = splash->makeStrokePath(path, state->getLineWidth());
    delete path;
    splash->clipToPath(path2, false);
    delete path2;
}

SplashPath *SplashOutputDev::convertPath(GfxState *state, GfxPath *path,
                                         bool dropEmptySubpaths)
{
    SplashPath *sPath;
    GfxSubpath *subpath;
    int         n, i, j;

    n = dropEmptySubpaths ? 1 : 0;
    sPath = new SplashPath();
    for (i = 0; i < path->getNumSubpaths(); ++i) {
        subpath = path->getSubpath(i);
        if (subpath->getNumPoints() > n) {
            sPath->moveTo((SplashCoord)subpath->getX(0),
                          (SplashCoord)subpath->getY(0));
            j = 1;
            while (j < subpath->getNumPoints()) {
                if (subpath->getCurve(j)) {
                    sPath->curveTo((SplashCoord)subpath->getX(j),
                                   (SplashCoord)subpath->getY(j),
                                   (SplashCoord)subpath->getX(j + 1),
                                   (SplashCoord)subpath->getY(j + 1),
                                   (SplashCoord)subpath->getX(j + 2),
                                   (SplashCoord)subpath->getY(j + 2));
                    j += 3;
                } else {
                    sPath->lineTo((SplashCoord)subpath->getX(j),
                                  (SplashCoord)subpath->getY(j));
                    ++j;
                }
            }
            if (subpath->isClosed()) {
                sPath->close();
            }
        }
    }
    return sPath;
}

void SplashOutputDev::drawChar(GfxState *state, double x, double y, double dx,
                               double dy, double originX, double originY,
                               CharCode code, int nBytes, Unicode *u, int uLen)
{
    SplashPath *path;
    int         render;
    bool        doFill, doStroke, doClip, strokeAdjust;
    double      m[4];
    bool        horiz;

    if (skipHorizText || skipRotatedText) {
        state->getFontTransMat(&m[0], &m[1], &m[2], &m[3]);
        horiz = m[0] > 0 && fabs(m[1]) < 0.001 && fabs(m[2]) < 0.001 && m[3] < 0;
        if ((skipHorizText && horiz) || (skipRotatedText && !horiz)) {
            return;
        }
    }

    // check for invisible text -- this is used by Acrobat Capture
    render = state->getRender();
    if (render == 3) {
        return;
    }

    if (needFontUpdate) {
        doUpdateFont(state);
    }
    if (!font) {
        return;
    }

    x -= originX;
    y -= originY;

    doFill = !(render & 1) && !state->getFillColorSpace()->isNonMarking();
    doStroke = ((render & 3) == 1 || (render & 3) == 2) &&
               !state->getStrokeColorSpace()->isNonMarking();
    doClip = render & 4;

    path = NULL;
    if (doStroke || doClip) {
        if ((path = font->getGlyphPath(code))) {
            path->offset((SplashCoord)x, (SplashCoord)y);
        }
    }

    // don't use stroke adjustment when stroking text -- the results
    // tend to be ugly (because characters with horizontal upper or
    // lower edges get misaligned relative to the other characters)
    strokeAdjust = false; // make gcc happy
    if (doStroke) {
        strokeAdjust = splash->getStrokeAdjust();
        splash->setStrokeAdjust(false);
    }

    // fill and stroke
    if (doFill && doStroke) {
        if (path) {
            setOverprintMask(state->getFillColorSpace(),
                             state->getFillOverprint(), state->getOverprintMode(),
                             state->getFillColor());
            splash->fill(path, false);
            setOverprintMask(state->getStrokeColorSpace(),
                             state->getStrokeOverprint(),
                             state->getOverprintMode(), state->getStrokeColor());
            splash->stroke(path);
        }

        // fill
    } else if (doFill) {
        setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(),
                         state->getOverprintMode(), state->getFillColor());
        splash->fillChar((SplashCoord)x, (SplashCoord)y, code, font);

        // stroke
    } else if (doStroke) {
        if (path) {
            setOverprintMask(state->getStrokeColorSpace(),
                             state->getStrokeOverprint(),
                             state->getOverprintMode(), state->getStrokeColor());
            splash->stroke(path);
        }
    }

    // clip
    if (doClip) {
        if (path) {
            if (textClipPath) {
                textClipPath->append(path);
            } else {
                textClipPath = path;
                path = NULL;
            }
        }
    }

    if (doStroke) {
        splash->setStrokeAdjust(strokeAdjust);
    }

    if (path) {
        delete path;
    }
}

bool SplashOutputDev::beginType3Char(GfxState *state, double x, double y,
                                     double dx, double dy, CharCode code,
                                     Unicode *u, int uLen)
{
    GfxFont *     gfxFont;
    Ref *         fontID;
    double *      ctm, *bbox;
    T3FontCache * t3Font;
    T3GlyphStack *t3gs;
    bool          validBBox;
    double        m[4];
    bool          horiz;
    double        x1, y1, xMin, yMin, xMax, yMax, xt, yt;
    int           i, j;

    if (skipHorizText || skipRotatedText) {
        state->getFontTransMat(&m[0], &m[1], &m[2], &m[3]);
        horiz = m[0] > 0 && fabs(m[1]) < 0.001 && fabs(m[2]) < 0.001 && m[3] < 0;
        if ((skipHorizText && horiz) || (skipRotatedText && !horiz)) {
            return true;
        }
    }

    if (!(gfxFont = state->getFont())) {
        return false;
    }
    fontID = gfxFont->getID();
    ctm = state->getCTM();
    state->transform(0, 0, &xt, &yt);

    // is it the first (MRU) font in the cache?
    if (!(nT3Fonts > 0 &&
          t3FontCache[0]->matches(fontID, ctm[0], ctm[1], ctm[2], ctm[3]))) {
        // is the font elsewhere in the cache?
        for (i = 1; i < nT3Fonts; ++i) {
            if (t3FontCache[i]->matches(fontID, ctm[0], ctm[1], ctm[2], ctm[3])) {
                t3Font = t3FontCache[i];
                for (j = i; j > 0; --j) {
                    t3FontCache[j] = t3FontCache[j - 1];
                }
                t3FontCache[0] = t3Font;
                break;
            }
        }
        if (i >= nT3Fonts) {
            // create new entry in the font cache
            if (nT3Fonts == splashOutT3FontCacheSize) {
                delete t3FontCache[nT3Fonts - 1];
                --nT3Fonts;
            }
            for (j = nT3Fonts; j > 0; --j) {
                t3FontCache[j] = t3FontCache[j - 1];
            }
            ++nT3Fonts;
            bbox = gfxFont->getFontBBox();
            if (bbox[0] == 0 && bbox[1] == 0 && bbox[2] == 0 && bbox[3] == 0) {
                // unspecified bounding box -- just take a guess
                xMin = xt - 5;
                xMax = xMin + 30;
                yMax = yt + 15;
                yMin = yMax - 45;
                validBBox = false;
            } else {
                state->transform(bbox[0], bbox[1], &x1, &y1);
                xMin = xMax = x1;
                yMin = yMax = y1;
                state->transform(bbox[0], bbox[3], &x1, &y1);
                if (x1 < xMin) {
                    xMin = x1;
                } else if (x1 > xMax) {
                    xMax = x1;
                }
                if (y1 < yMin) {
                    yMin = y1;
                } else if (y1 > yMax) {
                    yMax = y1;
                }
                state->transform(bbox[2], bbox[1], &x1, &y1);
                if (x1 < xMin) {
                    xMin = x1;
                } else if (x1 > xMax) {
                    xMax = x1;
                }
                if (y1 < yMin) {
                    yMin = y1;
                } else if (y1 > yMax) {
                    yMax = y1;
                }
                state->transform(bbox[2], bbox[3], &x1, &y1);
                if (x1 < xMin) {
                    xMin = x1;
                } else if (x1 > xMax) {
                    xMax = x1;
                }
                if (y1 < yMin) {
                    yMin = y1;
                } else if (y1 > yMax) {
                    yMax = y1;
                }
                validBBox = true;
            }
            t3FontCache[0] = new T3FontCache(
                fontID, ctm[0], ctm[1], ctm[2], ctm[3], (int)floor(xMin - xt) - 2,
                (int)floor(yMin - yt) - 2, (int)ceil(xMax) - (int)floor(xMin) + 4,
                (int)ceil(yMax) - (int)floor(yMin) + 4, validBBox,
                colorMode != splashModeMono1);
        }
    }
    t3Font = t3FontCache[0];

    // is the glyph in the cache?
    i = (code & (t3Font->cacheSets - 1)) * t3Font->cacheAssoc;
    for (j = 0; j < t3Font->cacheAssoc; ++j) {
        if ((t3Font->cacheTags[i + j].mru & 0x8000) &&
            t3Font->cacheTags[i + j].code == code) {
            drawType3Glyph(state, t3Font, &t3Font->cacheTags[i + j],
                           t3Font->cacheData + (i + j) * t3Font->glyphSize);
            return true;
        }
    }

    // push a new Type 3 glyph record
    t3gs = new T3GlyphStack();
    t3gs->next = t3GlyphStack;
    t3GlyphStack = t3gs;
    t3GlyphStack->code = code;
    t3GlyphStack->cache = t3Font;
    t3GlyphStack->cacheTag = NULL;
    t3GlyphStack->cacheData = NULL;
    t3GlyphStack->haveDx = false;
    t3GlyphStack->doNotCache = false;

    return false;
}

void SplashOutputDev::endType3Char(GfxState *state)
{
    T3GlyphStack *t3gs;
    double *      ctm;

    if (t3GlyphStack->cacheTag) {
        --nestCount;
        memcpy(t3GlyphStack->cacheData, bitmap->getDataPtr(),
               t3GlyphStack->cache->glyphSize);
        delete bitmap;
        delete splash;
        bitmap = t3GlyphStack->origBitmap;
        splash = t3GlyphStack->origSplash;
        ctm = state->getCTM();
        state->setCTM(ctm[0], ctm[1], ctm[2], ctm[3], t3GlyphStack->origCTM4,
                      t3GlyphStack->origCTM5);
        updateCTM(state, 0, 0, 0, 0, 0, 0);
        drawType3Glyph(state, t3GlyphStack->cache, t3GlyphStack->cacheTag,
                       t3GlyphStack->cacheData);
    }
    t3gs = t3GlyphStack;
    t3GlyphStack = t3gs->next;
    delete t3gs;
}

void SplashOutputDev::type3D0(GfxState *state, double wx, double wy)
{
    t3GlyphStack->haveDx = true;
}

void SplashOutputDev::type3D1(GfxState *state, double wx, double wy, double llx,
                              double lly, double urx, double ury)
{
    double *     ctm;
    T3FontCache *t3Font;
    SplashColor  color;
    double       xt, yt, xMin, xMax, yMin, yMax, x1, y1;
    int          i, j;

    // ignore multiple d0/d1 operators
    if (t3GlyphStack->haveDx) {
        return;
    }
    t3GlyphStack->haveDx = true;
    // don't cache if we got a gsave/grestore before the d1
    if (t3GlyphStack->doNotCache) {
        return;
    }

    t3Font = t3GlyphStack->cache;

    // check for a valid bbox
    state->transform(0, 0, &xt, &yt);
    state->transform(llx, lly, &x1, &y1);
    xMin = xMax = x1;
    yMin = yMax = y1;
    state->transform(llx, ury, &x1, &y1);
    if (x1 < xMin) {
        xMin = x1;
    } else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) {
        yMin = y1;
    } else if (y1 > yMax) {
        yMax = y1;
    }
    state->transform(urx, lly, &x1, &y1);
    if (x1 < xMin) {
        xMin = x1;
    } else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) {
        yMin = y1;
    } else if (y1 > yMax) {
        yMax = y1;
    }
    state->transform(urx, ury, &x1, &y1);
    if (x1 < xMin) {
        xMin = x1;
    } else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) {
        yMin = y1;
    } else if (y1 > yMax) {
        yMax = y1;
    }
    if (xMin - xt < t3Font->glyphX || yMin - yt < t3Font->glyphY ||
        xMax - xt > t3Font->glyphX + t3Font->glyphW ||
        yMax - yt > t3Font->glyphY + t3Font->glyphH) {
        if (t3Font->validBBox) {
            error(errSyntaxWarning, -1, "Bad bounding box in Type 3 glyph");
        }
        return;
    }

    // allocate a cache entry
    i = (t3GlyphStack->code & (t3Font->cacheSets - 1)) * t3Font->cacheAssoc;
    for (j = 0; j < t3Font->cacheAssoc; ++j) {
        if ((t3Font->cacheTags[i + j].mru & 0x7fff) == t3Font->cacheAssoc - 1) {
            t3Font->cacheTags[i + j].mru = 0x8000;
            t3Font->cacheTags[i + j].code = t3GlyphStack->code;
            t3GlyphStack->cacheTag = &t3Font->cacheTags[i + j];
            t3GlyphStack->cacheData =
                t3Font->cacheData + (i + j) * t3Font->glyphSize;
        } else {
            ++t3Font->cacheTags[i + j].mru;
        }
    }

    // save state
    t3GlyphStack->origBitmap = bitmap;
    t3GlyphStack->origSplash = splash;
    ctm = state->getCTM();
    t3GlyphStack->origCTM4 = ctm[4];
    t3GlyphStack->origCTM5 = ctm[5];

    // create the temporary bitmap
    if (colorMode == splashModeMono1) {
        bitmap = new SplashBitmap(t3Font->glyphW, t3Font->glyphH, 1,
                                  splashModeMono1, false);
        splash = new Splash(bitmap, false, t3GlyphStack->origSplash->getScreen());
        color[0] = 0;
        splash->clear(color);
        color[0] = 0xff;
    } else {
        bitmap = new SplashBitmap(t3Font->glyphW, t3Font->glyphH, 1,
                                  splashModeMono8, false);
        splash = new Splash(bitmap, vectorAntialias,
                            t3GlyphStack->origSplash->getScreen());
        color[0] = 0x00;
        splash->clear(color);
        color[0] = 0xff;
    }
    splash->setMinLineWidth(globalParams->getMinLineWidth());
    splash->setStrokeAdjust(t3GlyphStack->origSplash->getStrokeAdjust());
    splash->setFillPattern(new SplashSolidColor(color));
    splash->setStrokePattern(new SplashSolidColor(color));
    //~ this should copy other state from t3GlyphStack->origSplash?
    //~ [this is likely the same situation as in beginTransparencyGroup()]
    state->setCTM(ctm[0], ctm[1], ctm[2], ctm[3], -t3Font->glyphX,
                  -t3Font->glyphY);
    updateCTM(state, 0, 0, 0, 0, 0, 0);
    ++nestCount;
}

void SplashOutputDev::drawType3Glyph(GfxState *state, T3FontCache *t3Font,
                                     T3FontCacheTag *tag, unsigned char *data)
{
    SplashGlyphBitmap glyph;

    setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(),
                     state->getOverprintMode(), state->getFillColor());
    glyph.x = -t3Font->glyphX;
    glyph.y = -t3Font->glyphY;
    glyph.w = t3Font->glyphW;
    glyph.h = t3Font->glyphH;
    glyph.aa = colorMode != splashModeMono1;
    glyph.data = data;
    glyph.freeData = false;
    splash->fillGlyph(0, 0, &glyph);
}

void SplashOutputDev::endTextObject(GfxState *state)
{
    if (textClipPath) {
        splash->clipToPath(textClipPath, false);
        delete textClipPath;
        textClipPath = NULL;
    }
}

struct SplashOutImageMaskData
{
    ImageStream *imgStr;
    bool         invert;
    int          width, height, y;
};

bool SplashOutputDev::imageMaskSrc(void *data, SplashColorPtr line)
{
    SplashOutImageMaskData *imgMaskData = (SplashOutImageMaskData *)data;
    unsigned char *         p;
    SplashColorPtr          q;
    int                     x;

    if (imgMaskData->y == imgMaskData->height ||
        !(p = imgMaskData->imgStr->readline())) {
        memset(line, 0, imgMaskData->width);
        return false;
    }
    for (x = 0, q = line; x < imgMaskData->width; ++x) {
        *q++ = *p++ ^ imgMaskData->invert;
    }
    ++imgMaskData->y;
    return true;
}

void SplashOutputDev::drawImageMask(GfxState *state, Object *ref, Stream *str,
                                    int width, int height, bool invert,
                                    bool inlineImg, bool interpolate)
{
    double *               ctm;
    SplashCoord            mat[6];
    SplashOutImageMaskData imgMaskData;

    if (state->getFillColorSpace()->isNonMarking()) {
        return;
    }
    setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(),
                     state->getOverprintMode(), state->getFillColor());

    ctm = state->getCTM();
    mat[0] = ctm[0];
    mat[1] = ctm[1];
    mat[2] = -ctm[2];
    mat[3] = -ctm[3];
    mat[4] = ctm[2] + ctm[4];
    mat[5] = ctm[3] + ctm[5];

    reduceImageResolution(str, ctm, &width, &height);

    imgMaskData.imgStr = new ImageStream(str, width, 1, 1);
    imgMaskData.imgStr->reset();
    imgMaskData.invert = invert ? 0 : 1;
    imgMaskData.width = width;
    imgMaskData.height = height;
    imgMaskData.y = 0;

    splash->fillImageMask(&imageMaskSrc, &imgMaskData, width, height, mat,
                          t3GlyphStack != NULL, interpolate);
    if (inlineImg) {
        while (imgMaskData.y < height) {
            imgMaskData.imgStr->readline();
            ++imgMaskData.y;
        }
    }

    delete imgMaskData.imgStr;
    str->close();
}

void SplashOutputDev::setSoftMaskFromImageMask(GfxState *state, Object *ref,
                                               Stream *str, int width, int height,
                                               bool invert, bool inlineImg,
                                               bool interpolate)
{
    double *               ctm;
    SplashCoord            mat[6];
    SplashOutImageMaskData imgMaskData;
    SplashBitmap *         maskBitmap;
    Splash *               maskSplash;
    SplashColor            maskColor;

    ctm = state->getCTM();
    mat[0] = ctm[0];
    mat[1] = ctm[1];
    mat[2] = -ctm[2];
    mat[3] = -ctm[3];
    mat[4] = ctm[2] + ctm[4];
    mat[5] = ctm[3] + ctm[5];
    reduceImageResolution(str, ctm, &width, &height);
    imgMaskData.imgStr = new ImageStream(str, width, 1, 1);
    imgMaskData.imgStr->reset();
    imgMaskData.invert = invert ? 0 : 1;
    imgMaskData.width = width;
    imgMaskData.height = height;
    imgMaskData.y = 0;
    maskBitmap = new SplashBitmap(bitmap->getWidth(), bitmap->getHeight(), 1,
                                  splashModeMono8, false);
    maskSplash = new Splash(maskBitmap, true);
    maskSplash->setStrokeAdjust(globalParams->getStrokeAdjust());
    clearMaskRegion(state, maskSplash, 0, 0, 1, 1);
    maskColor[0] = 0xff;
    maskSplash->setFillPattern(new SplashSolidColor(maskColor));
    maskSplash->fillImageMask(&imageMaskSrc, &imgMaskData, width, height, mat,
                              false, interpolate);
    delete imgMaskData.imgStr;
    str->close();
    delete maskSplash;
    splash->setSoftMask(maskBitmap);
}

struct SplashOutImageData
{
    ImageStream *     imgStr;
    GfxImageColorMap *colorMap;
    SplashColorPtr    lookup;
    int *             maskColors;
    SplashColorMode   colorMode;
    int               width, height, y;
};

bool SplashOutputDev::imageSrc(void *data, SplashColorPtr colorLine,
                               unsigned char *alphaLine)
{
    SplashOutImageData *imgData = (SplashOutImageData *)data;
    unsigned char *     p;
    SplashColorPtr      q, col;
    int                 x;

    if (imgData->y == imgData->height || !(p = imgData->imgStr->readline())) {
        memset(colorLine, 0,
               imgData->width * splashColorModeNComps[imgData->colorMode]);
        return false;
    }

    // nComps = imgData->colorMap->getNumPixelComps();

    if (imgData->lookup) {
        switch (imgData->colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, ++p) {
                *q++ = imgData->lookup[*p];
            }
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, ++p) {
                col = &imgData->lookup[3 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
            }
            break;
#if SPLASH_CMYK
        case splashModeCMYK8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, ++p) {
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = col[3];
            }
            break;
#endif
        }
    } else {
        switch (imgData->colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            imgData->colorMap->getGrayByteLine(p, colorLine, imgData->width);
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            imgData->colorMap->getRGBByteLine(p, colorLine, imgData->width);
            break;
#if SPLASH_CMYK
        case splashModeCMYK8:
            imgData->colorMap->getCMYKByteLine(p, colorLine, imgData->width);
            break;
#endif
        }
    }

    ++imgData->y;
    return true;
}

bool SplashOutputDev::alphaImageSrc(void *data, SplashColorPtr colorLine,
                                    unsigned char *alphaLine)
{
    SplashOutImageData *imgData = (SplashOutImageData *)data;
    unsigned char *     p, *aq;
    SplashColorPtr      q, col;
    GfxRGB              rgb;
    GfxGray             gray;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif
    unsigned char alpha;
    int           nComps, x, i;

    if (imgData->y == imgData->height || !(p = imgData->imgStr->readline())) {
        memset(colorLine, 0,
               imgData->width * splashColorModeNComps[imgData->colorMode]);
        memset(alphaLine, 0, imgData->width);
        return false;
    }

    nComps = imgData->colorMap->getNumPixelComps();

    for (x = 0, q = colorLine, aq = alphaLine; x < imgData->width;
         ++x, p += nComps) {
        alpha = 0;
        for (i = 0; i < nComps; ++i) {
            if (p[i] < imgData->maskColors[2 * i] ||
                p[i] > imgData->maskColors[2 * i + 1]) {
                alpha = 0xff;
                break;
            }
        }
        if (imgData->lookup) {
            switch (imgData->colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                *q++ = imgData->lookup[*p];
                break;
            case splashModeRGB8:
            case splashModeBGR8:
                col = &imgData->lookup[3 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                break;
#if SPLASH_CMYK
            case splashModeCMYK8:
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = col[3];
                break;
#endif
            }
            *aq++ = alpha;
        } else {
            switch (imgData->colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                imgData->colorMap->getGray(p, &gray);
                *q++ = xpdf::to_small_color(gray.x);
                break;
            case splashModeRGB8:
            case splashModeBGR8:
                imgData->colorMap->getRGB(p, &rgb);
                *q++ = xpdf::to_small_color(rgb.r);
                *q++ = xpdf::to_small_color(rgb.g);
                *q++ = xpdf::to_small_color(rgb.b);
                break;
#if SPLASH_CMYK
            case splashModeCMYK8:
                imgData->colorMap->getCMYK(p, &cmyk);
                *q++ = xpdf::to_small_color(cmyk.c);
                *q++ = xpdf::to_small_color(cmyk.m);
                *q++ = xpdf::to_small_color(cmyk.y);
                *q++ = xpdf::to_small_color(cmyk.k);
                break;
#endif
            }
            *aq++ = alpha;
        }
    }

    ++imgData->y;
    return true;
}

void SplashOutputDev::drawImage(GfxState *state, Object *ref, Stream *str,
                                int width, int height, GfxImageColorMap *colorMap,
                                int *maskColors, bool inlineImg, bool interpolate)
{
    double *           ctm;
    SplashCoord        mat[6];
    SplashOutImageData imgData;
    SplashColorMode    srcMode;
    SplashImageSource  src;
    GfxGray            gray;
    GfxRGB             rgb;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif
    unsigned char pix;
    int           n, i;

    setOverprintMask(colorMap->getColorSpace(), state->getFillOverprint(),
                     state->getOverprintMode(), NULL);

    ctm = state->getCTM();
    mat[0] = ctm[0];
    mat[1] = ctm[1];
    mat[2] = -ctm[2];
    mat[3] = -ctm[3];
    mat[4] = ctm[2] + ctm[4];
    mat[5] = ctm[3] + ctm[5];

    reduceImageResolution(str, ctm, &width, &height);

    imgData.imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
                                     colorMap->getBits());
    imgData.imgStr->reset();
    imgData.colorMap = colorMap;
    imgData.maskColors = maskColors;
    imgData.colorMode = colorMode;
    imgData.width = width;
    imgData.height = height;
    imgData.y = 0;

    // special case for one-channel (monochrome/gray/separation) images:
    // build a lookup table here
    imgData.lookup = NULL;
    if (colorMap->getNumPixelComps() == 1) {
        n = 1 << colorMap->getBits();
        switch (colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            imgData.lookup = (SplashColorPtr)malloc(n);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getGray(&pix, &gray);
                imgData.lookup[i] = xpdf::to_small_color(gray.x);
            }
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            imgData.lookup = (SplashColorPtr)calloc(n, 3);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getRGB(&pix, &rgb);
                imgData.lookup[3 * i] = xpdf::to_small_color(rgb.r);
                imgData.lookup[3 * i + 1] = xpdf::to_small_color(rgb.g);
                imgData.lookup[3 * i + 2] = xpdf::to_small_color(rgb.b);
            }
            break;
#if SPLASH_CMYK
        case splashModeCMYK8:
            imgData.lookup = (SplashColorPtr)calloc(n, 4);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getCMYK(&pix, &cmyk);
                imgData.lookup[4 * i] = xpdf::to_small_color(cmyk.c);
                imgData.lookup[4 * i + 1] = xpdf::to_small_color(cmyk.m);
                imgData.lookup[4 * i + 2] = xpdf::to_small_color(cmyk.y);
                imgData.lookup[4 * i + 3] = xpdf::to_small_color(cmyk.k);
            }
            break;
#endif
        }
    }

    if (colorMode == splashModeMono1) {
        srcMode = splashModeMono8;
    } else if (colorMode == splashModeBGR8) {
        srcMode = splashModeRGB8;
    } else {
        srcMode = colorMode;
    }
    src = maskColors ? &alphaImageSrc : &imageSrc;
    splash->drawImage(src, &imgData, srcMode, maskColors ? true : false, width,
                      height, mat, interpolate);
    if (inlineImg) {
        while (imgData.y < height) {
            imgData.imgStr->readline();
            ++imgData.y;
        }
    }

    free(imgData.lookup);
    delete imgData.imgStr;
    str->close();
}

struct SplashOutMaskedImageData
{
    ImageStream *     imgStr;
    GfxImageColorMap *colorMap;
    SplashBitmap *    mask;
    SplashColorPtr    lookup;
    SplashColorMode   colorMode;
    int               width, height, y;
};

bool SplashOutputDev::maskedImageSrc(void *data, SplashColorPtr colorLine,
                                     unsigned char *alphaLine)
{
    SplashOutMaskedImageData *imgData = (SplashOutMaskedImageData *)data;
    unsigned char *           p, *aq;
    SplashColorPtr            q, col;
    GfxRGB                    rgb;
    GfxGray                   gray;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif
    static unsigned char bitToByte[2] = { 0x00, 0xff };
    unsigned char        alpha;
    unsigned char *      maskPtr;
    int                  maskShift;
    int                  nComps, x;

    if (imgData->y == imgData->height || !(p = imgData->imgStr->readline())) {
        memset(colorLine, 0,
               imgData->width * splashColorModeNComps[imgData->colorMode]);
        memset(alphaLine, 0, imgData->width);
        return false;
    }

    nComps = imgData->colorMap->getNumPixelComps();

    maskPtr =
        imgData->mask->getDataPtr() + imgData->y * imgData->mask->getRowSize();
    maskShift = 7;
    for (x = 0, q = colorLine, aq = alphaLine; x < imgData->width;
         ++x, p += nComps) {
        alpha = bitToByte[(*maskPtr >> maskShift) & 1];
        maskPtr += (8 - maskShift) >> 3;
        maskShift = (maskShift - 1) & 7;
        if (imgData->lookup) {
            switch (imgData->colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                *q++ = imgData->lookup[*p];
                break;
            case splashModeRGB8:
            case splashModeBGR8:
                col = &imgData->lookup[3 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                break;
#if SPLASH_CMYK
            case splashModeCMYK8:
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = col[3];
                break;
#endif
            }
            *aq++ = alpha;
        } else {
            switch (imgData->colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                imgData->colorMap->getGray(p, &gray);
                *q++ = xpdf::to_small_color(gray.x);
                break;
            case splashModeRGB8:
            case splashModeBGR8:
                imgData->colorMap->getRGB(p, &rgb);
                *q++ = xpdf::to_small_color(rgb.r);
                *q++ = xpdf::to_small_color(rgb.g);
                *q++ = xpdf::to_small_color(rgb.b);
                break;
#if SPLASH_CMYK
            case splashModeCMYK8:
                imgData->colorMap->getCMYK(p, &cmyk);
                *q++ = xpdf::to_small_color(cmyk.c);
                *q++ = xpdf::to_small_color(cmyk.m);
                *q++ = xpdf::to_small_color(cmyk.y);
                *q++ = xpdf::to_small_color(cmyk.k);
                break;
#endif
            }
            *aq++ = alpha;
        }
    }

    ++imgData->y;
    return true;
}

void SplashOutputDev::drawMaskedImage(GfxState *state, Object *ref, Stream *str,
                                      int width, int height,
                                      GfxImageColorMap *colorMap, Stream *maskStr,
                                      int maskWidth, int maskHeight,
                                      bool maskInvert, bool interpolate)
{
    GfxImageColorMap *       maskColorMap;
    Object                   maskDecode, decodeLow, decodeHigh;
    double *                 ctm;
    SplashCoord              mat[6];
    SplashOutMaskedImageData imgData;
    SplashOutImageMaskData   imgMaskData;
    SplashColorMode          srcMode;
    SplashBitmap *           maskBitmap;
    Splash *                 maskSplash;
    SplashColor              maskColor;
    GfxGray                  gray;
    GfxRGB                   rgb;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif
    unsigned char pix;
    int           n, i;

    setOverprintMask(colorMap->getColorSpace(), state->getFillOverprint(),
                     state->getOverprintMode(), NULL);

    ctm = state->getCTM();
    reduceImageResolution(str, ctm, &width, &height);
    reduceImageResolution(maskStr, ctm, &maskWidth, &maskHeight);

    // If the mask is higher resolution than the image, use
    // drawSoftMaskedImage() instead.
    if (maskWidth > width || maskHeight > height) {
        decodeLow = xpdf::make_int_obj(maskInvert ? 0 : 1);
        decodeHigh = xpdf::make_int_obj(maskInvert ? 1 : 0);

        maskDecode = xpdf::make_arr_obj();
        maskDecode.as_array().push_back(decodeLow);
        maskDecode.as_array().push_back(decodeHigh);

        maskColorMap =
            new GfxImageColorMap(1, &maskDecode, new GfxDeviceGrayColorSpace());

        drawSoftMaskedImage(state, ref, str, width, height, colorMap, maskStr,
                            maskWidth, maskHeight, maskColorMap, interpolate);

        delete maskColorMap;
    } else {
        //----- scale the mask image to the same size as the source image

        mat[0] = (SplashCoord)width;
        mat[1] = 0;
        mat[2] = 0;
        mat[3] = (SplashCoord)height;
        mat[4] = 0;
        mat[5] = 0;
        imgMaskData.imgStr = new ImageStream(maskStr, maskWidth, 1, 1);
        imgMaskData.imgStr->reset();
        imgMaskData.invert = maskInvert ? 0 : 1;
        imgMaskData.width = maskWidth;
        imgMaskData.height = maskHeight;
        imgMaskData.y = 0;
        maskBitmap = new SplashBitmap(width, height, 1, splashModeMono1, false);
        maskSplash = new Splash(maskBitmap, false);
        maskSplash->setStrokeAdjust(globalParams->getStrokeAdjust());
        maskColor[0] = 0;
        maskSplash->clear(maskColor);
        maskColor[0] = 0xff;
        maskSplash->setFillPattern(new SplashSolidColor(maskColor));
        // use "glyph mode" here to get the correct scaled size
        maskSplash->fillImageMask(&imageMaskSrc, &imgMaskData, maskWidth,
                                  maskHeight, mat, true, interpolate);
        delete imgMaskData.imgStr;
        maskStr->close();
        delete maskSplash;

        //----- draw the source image

        mat[0] = ctm[0];
        mat[1] = ctm[1];
        mat[2] = -ctm[2];
        mat[3] = -ctm[3];
        mat[4] = ctm[2] + ctm[4];
        mat[5] = ctm[3] + ctm[5];

        imgData.imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
                                         colorMap->getBits());
        imgData.imgStr->reset();
        imgData.colorMap = colorMap;
        imgData.mask = maskBitmap;
        imgData.colorMode = colorMode;
        imgData.width = width;
        imgData.height = height;
        imgData.y = 0;

        // special case for one-channel (monochrome/gray/separation) images:
        // build a lookup table here
        imgData.lookup = NULL;
        if (colorMap->getNumPixelComps() == 1) {
            n = 1 << colorMap->getBits();
            switch (colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                imgData.lookup = (SplashColorPtr)malloc(n);
                for (i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getGray(&pix, &gray);
                    imgData.lookup[i] = xpdf::to_small_color(gray.x);
                }
                break;
            case splashModeRGB8:
            case splashModeBGR8:
                imgData.lookup = (SplashColorPtr)calloc(n, 3);
                for (i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getRGB(&pix, &rgb);
                    imgData.lookup[3 * i] = xpdf::to_small_color(rgb.r);
                    imgData.lookup[3 * i + 1] = xpdf::to_small_color(rgb.g);
                    imgData.lookup[3 * i + 2] = xpdf::to_small_color(rgb.b);
                }
                break;
#if SPLASH_CMYK
            case splashModeCMYK8:
                imgData.lookup = (SplashColorPtr)calloc(n, 4);
                for (i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getCMYK(&pix, &cmyk);
                    imgData.lookup[4 * i] = xpdf::to_small_color(cmyk.c);
                    imgData.lookup[4 * i + 1] = xpdf::to_small_color(cmyk.m);
                    imgData.lookup[4 * i + 2] = xpdf::to_small_color(cmyk.y);
                    imgData.lookup[4 * i + 3] = xpdf::to_small_color(cmyk.k);
                }
                break;
#endif
            }
        }

        if (colorMode == splashModeMono1) {
            srcMode = splashModeMono8;
        } else if (colorMode == splashModeBGR8) {
            srcMode = splashModeRGB8;
        } else {
            srcMode = colorMode;
        }
        splash->drawImage(&maskedImageSrc, &imgData, srcMode, true, width, height,
                          mat, interpolate);

        delete maskBitmap;
        free(imgData.lookup);
        delete imgData.imgStr;
        str->close();
    }
}

void SplashOutputDev::drawSoftMaskedImage(
    GfxState *state, Object *ref, Stream *str, int width, int height,
    GfxImageColorMap *colorMap, Stream *maskStr, int maskWidth, int maskHeight,
    GfxImageColorMap *maskColorMap, bool interpolate)
{
    double *           ctm;
    SplashCoord        mat[6];
    SplashOutImageData imgData;
    SplashOutImageData imgMaskData;
    SplashColorMode    srcMode;
    SplashBitmap *     maskBitmap;
    Splash *           maskSplash;
    GfxGray            gray;
    GfxRGB             rgb;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif
    unsigned char pix;
    int           n, i;

    setOverprintMask(colorMap->getColorSpace(), state->getFillOverprint(),
                     state->getOverprintMode(), NULL);

    ctm = state->getCTM();
    mat[0] = ctm[0];
    mat[1] = ctm[1];
    mat[2] = -ctm[2];
    mat[3] = -ctm[3];
    mat[4] = ctm[2] + ctm[4];
    mat[5] = ctm[3] + ctm[5];

    reduceImageResolution(str, ctm, &width, &height);
    reduceImageResolution(maskStr, ctm, &maskWidth, &maskHeight);

    //----- set up the soft mask

    imgMaskData.imgStr = new ImageStream(maskStr, maskWidth,
                                         maskColorMap->getNumPixelComps(),
                                         maskColorMap->getBits());
    imgMaskData.imgStr->reset();
    imgMaskData.colorMap = maskColorMap;
    imgMaskData.maskColors = NULL;
    imgMaskData.colorMode = splashModeMono8;
    imgMaskData.width = maskWidth;
    imgMaskData.height = maskHeight;
    imgMaskData.y = 0;
    n = 1 << maskColorMap->getBits();
    imgMaskData.lookup = (SplashColorPtr)malloc(n);
    for (i = 0; i < n; ++i) {
        pix = (unsigned char)i;
        maskColorMap->getGray(&pix, &gray);
        imgMaskData.lookup[i] = xpdf::to_small_color(gray.x);
    }
    maskBitmap = new SplashBitmap(bitmap->getWidth(), bitmap->getHeight(), 1,
                                  splashModeMono8, false);
    maskSplash = new Splash(maskBitmap, vectorAntialias);
    maskSplash->setStrokeAdjust(globalParams->getStrokeAdjust());
    clearMaskRegion(state, maskSplash, 0, 0, 1, 1);
    maskSplash->drawImage(&imageSrc, &imgMaskData, splashModeMono8, false,
                          maskWidth, maskHeight, mat, interpolate);
    delete imgMaskData.imgStr;
    maskStr->close();
    free(imgMaskData.lookup);
    delete maskSplash;
    splash->setSoftMask(maskBitmap);

    //----- draw the source image

    imgData.imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
                                     colorMap->getBits());
    imgData.imgStr->reset();
    imgData.colorMap = colorMap;
    imgData.maskColors = NULL;
    imgData.colorMode = colorMode;
    imgData.width = width;
    imgData.height = height;
    imgData.y = 0;

    // special case for one-channel (monochrome/gray/separation) images:
    // build a lookup table here
    imgData.lookup = NULL;
    if (colorMap->getNumPixelComps() == 1) {
        n = 1 << colorMap->getBits();
        switch (colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            imgData.lookup = (SplashColorPtr)malloc(n);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getGray(&pix, &gray);
                imgData.lookup[i] = xpdf::to_small_color(gray.x);
            }
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            imgData.lookup = (SplashColorPtr)calloc(n, 3);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getRGB(&pix, &rgb);
                imgData.lookup[3 * i] = xpdf::to_small_color(rgb.r);
                imgData.lookup[3 * i + 1] = xpdf::to_small_color(rgb.g);
                imgData.lookup[3 * i + 2] = xpdf::to_small_color(rgb.b);
            }
            break;
#if SPLASH_CMYK
        case splashModeCMYK8:
            imgData.lookup = (SplashColorPtr)calloc(n, 4);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getCMYK(&pix, &cmyk);
                imgData.lookup[4 * i] = xpdf::to_small_color(cmyk.c);
                imgData.lookup[4 * i + 1] = xpdf::to_small_color(cmyk.m);
                imgData.lookup[4 * i + 2] = xpdf::to_small_color(cmyk.y);
                imgData.lookup[4 * i + 3] = xpdf::to_small_color(cmyk.k);
            }
            break;
#endif
        }
    }

    if (colorMode == splashModeMono1) {
        srcMode = splashModeMono8;
    } else if (colorMode == splashModeBGR8) {
        srcMode = splashModeRGB8;
    } else {
        srcMode = colorMode;
    }
    splash->drawImage(&imageSrc, &imgData, srcMode, false, width, height, mat,
                      interpolate);

    splash->setSoftMask(NULL);
    free(imgData.lookup);
    delete imgData.imgStr;
    str->close();
}

void SplashOutputDev::reduceImageResolution(Stream *str, double *ctm, int *width,
                                            int *height)
{
    double sw, sh;
    int    reduction;

    if (is_stream< JPXStream >(*str) && *width * *height > 10000000) {
        sw = (double)*width / (fabs(ctm[2]) + fabs(ctm[3]));
        sh = (double)*height / (fabs(ctm[0]) + fabs(ctm[1]));
        if (sw > 8 && sh > 8) {
            reduction = 3;
        } else if (sw > 4 && sh > 4) {
            reduction = 2;
        } else if (sw > 2 && sh > 2) {
            reduction = 1;
        } else {
            reduction = 0;
        }
        if (reduction > 0) {
            ((JPXStream *)str)->reduceResolution(reduction);
            *width >>= reduction;
            *height >>= reduction;
        }
    }
}

void SplashOutputDev::clearMaskRegion(GfxState *state, Splash *maskSplash,
                                      double xMin, double yMin, double xMax,
                                      double yMax)
{
    SplashBitmap * maskBitmap;
    double         xxMin, yyMin, xxMax, yyMax, xx, yy;
    int            xxMinI, yyMinI, xxMaxI, yyMaxI, y, n;
    unsigned char *p;

    maskBitmap = maskSplash->getBitmap();
    xxMin = maskBitmap->getWidth();
    xxMax = 0;
    yyMin = maskBitmap->getHeight();
    yyMax = 0;
    state->transform(xMin, yMin, &xx, &yy);
    if (xx < xxMin) {
        xxMin = xx;
    }
    if (xx > xxMax) {
        xxMax = xx;
    }
    if (yy < yyMin) {
        yyMin = yy;
    }
    if (yy > yyMax) {
        yyMax = yy;
    }
    state->transform(xMin, yMax, &xx, &yy);
    if (xx < xxMin) {
        xxMin = xx;
    }
    if (xx > xxMax) {
        xxMax = xx;
    }
    if (yy < yyMin) {
        yyMin = yy;
    }
    if (yy > yyMax) {
        yyMax = yy;
    }
    state->transform(xMax, yMin, &xx, &yy);
    if (xx < xxMin) {
        xxMin = xx;
    }
    if (xx > xxMax) {
        xxMax = xx;
    }
    if (yy < yyMin) {
        yyMin = yy;
    }
    if (yy > yyMax) {
        yyMax = yy;
    }
    state->transform(xMax, yMax, &xx, &yy);
    if (xx < xxMin) {
        xxMin = xx;
    }
    if (xx > xxMax) {
        xxMax = xx;
    }
    if (yy < yyMin) {
        yyMin = yy;
    }
    if (yy > yyMax) {
        yyMax = yy;
    }
    xxMinI = (int)floor(xxMin);
    if (xxMinI < 0) {
        xxMinI = 0;
    }
    xxMaxI = (int)ceil(xxMax);
    if (xxMaxI > maskBitmap->getWidth()) {
        xxMaxI = maskBitmap->getWidth();
    }
    yyMinI = (int)floor(yyMin);
    if (yyMinI < 0) {
        yyMinI = 0;
    }
    yyMaxI = (int)ceil(yyMax);
    if (yyMaxI > maskBitmap->getHeight()) {
        yyMaxI = maskBitmap->getHeight();
    }
    p = maskBitmap->getDataPtr() + yyMinI * maskBitmap->getRowSize();
    if (maskBitmap->getMode() == splashModeMono1) {
        n = (xxMaxI + 7) / 8 - xxMinI / 8;
        p += xxMinI / 8;
    } else {
        n = xxMaxI - xxMinI;
        p += xxMinI;
    }
    if (xxMaxI > xxMinI) {
        for (y = yyMinI; y < yyMaxI; ++y) {
            memset(p, 0, n);
            p += maskBitmap->getRowSize();
        }
    }
}

void SplashOutputDev::beginTransparencyGroup(GfxState *state, double *bbox,
                                             GfxColorSpace *blendingColorSpace,
                                             bool isolated, bool knockout,
                                             bool forSoftMask)
{
    SplashTransparencyGroup *transpGroup;
    SplashColor              color;
    double                   xMin, yMin, xMax, yMax, x, y;
    int                      tx, ty, w, h, i;

    // transform the bbox
    state->transform(bbox[0], bbox[1], &x, &y);
    xMin = xMax = x;
    yMin = yMax = y;
    state->transform(bbox[0], bbox[3], &x, &y);
    if (x < xMin) {
        xMin = x;
    } else if (x > xMax) {
        xMax = x;
    }
    if (y < yMin) {
        yMin = y;
    } else if (y > yMax) {
        yMax = y;
    }
    state->transform(bbox[2], bbox[1], &x, &y);
    if (x < xMin) {
        xMin = x;
    } else if (x > xMax) {
        xMax = x;
    }
    if (y < yMin) {
        yMin = y;
    } else if (y > yMax) {
        yMax = y;
    }
    state->transform(bbox[2], bbox[3], &x, &y);
    if (x < xMin) {
        xMin = x;
    } else if (x > xMax) {
        xMax = x;
    }
    if (y < yMin) {
        yMin = y;
    } else if (y > yMax) {
        yMax = y;
    }
    tx = (int)floor(xMin);
    if (tx < 0) {
        tx = 0;
    } else if (tx >= bitmap->getWidth()) {
        tx = bitmap->getWidth() - 1;
    }
    ty = (int)floor(yMin);
    if (ty < 0) {
        ty = 0;
    } else if (ty >= bitmap->getHeight()) {
        ty = bitmap->getHeight() - 1;
    }
    w = (int)ceil(xMax) - tx + 1;
    if (tx + w > bitmap->getWidth()) {
        w = bitmap->getWidth() - tx;
    }
    if (w < 1) {
        w = 1;
    }
    h = (int)ceil(yMax) - ty + 1;
    if (ty + h > bitmap->getHeight()) {
        h = bitmap->getHeight() - ty;
    }
    if (h < 1) {
        h = 1;
    }

    // push a new stack entry
    transpGroup = new SplashTransparencyGroup();
    transpGroup->tx = tx;
    transpGroup->ty = ty;
    transpGroup->blendingColorSpace = blendingColorSpace;
    transpGroup->isolated = isolated;
    transpGroup->next = transpGroupStack;
    transpGroupStack = transpGroup;

    // save state
    transpGroup->origBitmap = bitmap;
    transpGroup->origSplash = splash;

    //~ this handles the blendingColorSpace arg for soft masks, but
    //~   not yet for transparency groups

    // switch to the blending color space
    if (forSoftMask && isolated && !knockout && blendingColorSpace) {
        if (blendingColorSpace->getMode() == csDeviceGray ||
            blendingColorSpace->getMode() == csCalGray ||
            (blendingColorSpace->getMode() == csICCBased &&
             blendingColorSpace->getNComps() == 1)) {
            colorMode = splashModeMono8;
        } else if (blendingColorSpace->getMode() == csDeviceRGB ||
                   blendingColorSpace->getMode() == csCalRGB ||
                   (blendingColorSpace->getMode() == csICCBased &&
                    blendingColorSpace->getNComps() == 3)) {
            //~ does this need to use BGR8?
            colorMode = splashModeRGB8;
#if SPLASH_CMYK
        } else if (blendingColorSpace->getMode() == csDeviceCMYK ||
                   (blendingColorSpace->getMode() == csICCBased &&
                    blendingColorSpace->getNComps() == 4)) {
            colorMode = splashModeCMYK8;
#endif
        }
    }

    // create the temporary bitmap
    bitmap = new SplashBitmap(w, h, bitmapRowPad, colorMode, true, bitmapTopDown);
    splash =
        new Splash(bitmap, vectorAntialias, transpGroup->origSplash->getScreen());
    splash->setMinLineWidth(globalParams->getMinLineWidth());
    splash->setStrokeAdjust(globalParams->getStrokeAdjust());
    //~ Acrobat apparently copies at least the fill and stroke colors, and
    //~ maybe other state(?) -- but not the clipping path (and not sure
    //~ what else)
    //~ [this is likely the same situation as in type3D1()]
    splash->setFillPattern(transpGroup->origSplash->getFillPattern()->copy());
    splash->setStrokePattern(transpGroup->origSplash->getStrokePattern()->copy());
    if (isolated) {
        for (i = 0; i < splashMaxColorComps; ++i) {
            color[i] = 0;
        }
        splash->clear(color, 0);
    } else {
        splash->blitTransparent(transpGroup->origBitmap, tx, ty, 0, 0, w, h);
    }
    splash->setInTransparencyGroup(transpGroup->origBitmap, tx, ty, !isolated,
                                   knockout);
    transpGroup->tBitmap = bitmap;
    state->shiftCTM(-tx, -ty);
    updateCTM(state, 0, 0, 0, 0, 0, 0);
    ++nestCount;
}

void SplashOutputDev::endTransparencyGroup(GfxState *state)
{
    // restore state
    --nestCount;
    delete splash;
    bitmap = transpGroupStack->origBitmap;
    colorMode = bitmap->getMode();
    splash = transpGroupStack->origSplash;
    state->shiftCTM(transpGroupStack->tx, transpGroupStack->ty);
    updateCTM(state, 0, 0, 0, 0, 0, 0);
}

void SplashOutputDev::paintTransparencyGroup(GfxState *state, double *bbox)
{
    SplashBitmap *           tBitmap;
    SplashTransparencyGroup *transpGroup;
    bool                     isolated;
    int                      tx, ty;

    tx = transpGroupStack->tx;
    ty = transpGroupStack->ty;
    tBitmap = transpGroupStack->tBitmap;
    isolated = transpGroupStack->isolated;

    // paint the transparency group onto the parent bitmap
    // - the clip path was set in the parent's state)
    if (tx < bitmap->getWidth() && ty < bitmap->getHeight()) {
        splash->setOverprintMask(0xffffffff);
        splash->composite(tBitmap, 0, 0, tx, ty, tBitmap->getWidth(),
                          tBitmap->getHeight(), false, !isolated);
    }

    // pop the stack
    transpGroup = transpGroupStack;
    transpGroupStack = transpGroup->next;
    delete transpGroup;

    delete tBitmap;
}

void SplashOutputDev::setSoftMask(GfxState *state, double *bbox, bool alpha,
                                  const Function &transferFunc,
                                  GfxColor *      backdropColor)
{
    SplashBitmap *           softMask, *tBitmap;
    Splash *                 tSplash;
    SplashTransparencyGroup *transpGroup;
    SplashColor              color;
    SplashColorPtr           p;
    GfxGray                  gray;
    GfxRGB                   rgb;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif
    double backdrop, backdrop2, lum, lum2;
    int    tx, ty, x, y;

    tx = transpGroupStack->tx;
    ty = transpGroupStack->ty;
    tBitmap = transpGroupStack->tBitmap;

    // composite with backdrop color
    backdrop = 0;
    if (!alpha && tBitmap->getMode() != splashModeMono1) {
        //~ need to correctly handle the case where no blending color
        //~ space is given
        tSplash = new Splash(tBitmap, vectorAntialias,
                             transpGroupStack->origSplash->getScreen());
        tSplash->setStrokeAdjust(globalParams->getStrokeAdjust());
        if (transpGroupStack->blendingColorSpace) {
            switch (tBitmap->getMode()) {
            case splashModeMono1:
                // transparency is not supported in mono1 mode
                break;
            case splashModeMono8:
                transpGroupStack->blendingColorSpace->getGray(backdropColor,
                                                              &gray);
                backdrop = xpdf::to_double(gray.x);
                color[0] = xpdf::to_small_color(gray.x);
                tSplash->compositeBackground(color);
                break;
            case splashModeRGB8:
            case splashModeBGR8:
                transpGroupStack->blendingColorSpace->getRGB(backdropColor, &rgb);
                backdrop = 0.3 * xpdf::to_double(rgb.r) +
                           0.59 * xpdf::to_double(rgb.g) +
                           0.11 * xpdf::to_double(rgb.b);
                color[0] = xpdf::to_small_color(rgb.r);
                color[1] = xpdf::to_small_color(rgb.g);
                color[2] = xpdf::to_small_color(rgb.b);
                tSplash->compositeBackground(color);
                break;
#if SPLASH_CMYK
            case splashModeCMYK8:
                transpGroupStack->blendingColorSpace->getCMYK(backdropColor,
                                                              &cmyk);
                backdrop = (1 - xpdf::to_double(cmyk.k)) -
                           0.3 * xpdf::to_double(cmyk.c) -
                           0.59 * xpdf::to_double(cmyk.m) -
                           0.11 * xpdf::to_double(cmyk.y);
                if (backdrop < 0) {
                    backdrop = 0;
                }
                color[0] = xpdf::to_small_color(cmyk.c);
                color[1] = xpdf::to_small_color(cmyk.m);
                color[2] = xpdf::to_small_color(cmyk.y);
                color[3] = xpdf::to_small_color(cmyk.k);
                tSplash->compositeBackground(color);
                break;
#endif
            }
            delete tSplash;
        }
    }

    if (transferFunc) {
        transferFunc(&backdrop, &backdrop + 1, &backdrop2);
    } else {
        backdrop2 = backdrop;
    }

    softMask = new SplashBitmap(bitmap->getWidth(), bitmap->getHeight(), 1,
                                splashModeMono8, false);
    memset(softMask->getDataPtr(), (int)(backdrop2 * 255.0 + 0.5),
           softMask->getRowSize() * softMask->getHeight());
    if (tx < softMask->getWidth() && ty < softMask->getHeight()) {
        p = softMask->getDataPtr() + ty * softMask->getRowSize() + tx;
        for (y = 0; y < tBitmap->getHeight(); ++y) {
            for (x = 0; x < tBitmap->getWidth(); ++x) {
                if (alpha) {
                    lum = tBitmap->getAlpha(x, y) / 255.0;
                } else {
                    tBitmap->getPixel(x, y, color);
                    // convert to luminosity
                    switch (tBitmap->getMode()) {
                    case splashModeMono1:
                    case splashModeMono8:
                        lum = color[0] / 255.0;
                        break;
                    case splashModeRGB8:
                    case splashModeBGR8:
                        lum = (0.3 / 255.0) * color[0] +
                              (0.59 / 255.0) * color[1] +
                              (0.11 / 255.0) * color[2];
                        break;
#if SPLASH_CMYK
                    case splashModeCMYK8:
                        lum = (1 - color[3] / 255.0) - (0.3 / 255.0) * color[0] -
                              (0.59 / 255.0) * color[1] -
                              (0.11 / 255.0) * color[2];
                        if (lum < 0) {
                            lum = 0;
                        }
                        break;
#endif
                    }
                }
                if (transferFunc) {
                    transferFunc(&lum, &lum + 1, &lum2);
                } else {
                    lum2 = lum;
                }
                p[x] = (int)(lum2 * 255.0 + 0.5);
            }
            p += softMask->getRowSize();
        }
    }
    splash->setSoftMask(softMask);

    // pop the stack
    transpGroup = transpGroupStack;
    transpGroupStack = transpGroup->next;
    delete transpGroup;

    delete tBitmap;
}

void SplashOutputDev::clearSoftMask(GfxState *state)
{
    splash->setSoftMask(NULL);
}

void SplashOutputDev::setPaperColor(SplashColorPtr paperColorA)
{
    splashColorCopy(paperColor, paperColorA);
}

int SplashOutputDev::getBitmapWidth()
{
    return bitmap->getWidth();
}

int SplashOutputDev::getBitmapHeight()
{
    return bitmap->getHeight();
}

SplashBitmap *SplashOutputDev::takeBitmap()
{
    SplashBitmap *ret;

    ret = bitmap;
    bitmap = new SplashBitmap(1, 1, bitmapRowPad, colorMode,
                              colorMode != splashModeMono1, bitmapTopDown);
    return ret;
}

void SplashOutputDev::getModRegion(int *xMin, int *yMin, int *xMax, int *yMax)
{
    splash->getModRegion(xMin, yMin, xMax, yMax);
}

void SplashOutputDev::clearModRegion()
{
    splash->clearModRegion();
}

void SplashOutputDev::setFillColor(int r, int g, int b)
{
    GfxRGB  rgb;
    GfxGray gray;
#if SPLASH_CMYK
    GfxCMYK cmyk;
#endif

    rgb.r = xpdf::to_color(uint8_t(r));
    rgb.g = xpdf::to_color(uint8_t(g));
    rgb.b = xpdf::to_color(uint8_t(b));

    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        gray.x =
            (xpdf::color_t)(0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.g + 0.5);
        gray.x = std::clamp(gray.x, 0, XPDF_FIXED_POINT_ONE);
        splash->setFillPattern(getColor(gray));
        break;

    case splashModeRGB8:
    case splashModeBGR8:
        splash->setFillPattern(getColor(&rgb));
        break;

#if SPLASH_CMYK
    case splashModeCMYK8:
        cmyk.c = XPDF_FIXED_POINT_ONE - rgb.r;
        cmyk.m = XPDF_FIXED_POINT_ONE - rgb.g;
        cmyk.y = XPDF_FIXED_POINT_ONE - rgb.b;
        cmyk.k = 0;
        splash->setFillPattern(getColor(&cmyk));
        break;
#endif
    }
}

SplashFont *SplashOutputDev::getFont(GString *name, SplashCoord *textMatA)
{
    GfxFontLoc *         fontLoc;
    GString *            fontBuf;
    FILE *               extFontFile;
    char                 blk[4096];
    int                  n;
    SplashFontFile *     fontFile;
    SplashFont *         fontObj;
    FoFiTrueType *       ff;
    int *                codeToGID;
    Unicode              u;
    SplashCoord          textMat[4];
    SplashCoord          oblique;
    int                  cmap;

    const auto p = xpdf::builtin_font(name->c_str());

    if (0 == p)
        return 0;

    Ref ref = p->ref;
    SplashOutFontFileID *id = new SplashOutFontFileID(&ref);

    // check the font file cache
    if ((fontFile = fontEngine->getFontFile(id))) {
        delete id;
        // load the font file
    } else {
        if (!(fontLoc = GfxFont::locateBase14Font(name)))
            return 0;

        fontBuf = 0;

        if (fontLoc->fontType == fontType1 || fontLoc->fontType == fontTrueType) {
            if (!(extFontFile = fopen(fontLoc->path->c_str(), "rb"))) {
                delete fontLoc;
                delete id;
                return 0;
            }

            fontBuf = new GString();

            while ((n = fread(blk, 1, sizeof(blk), extFontFile)) > 0)
                fontBuf->append(blk, n);

            fclose(extFontFile);
        }

        if (fontLoc->fontType == fontType1) {
            fontFile = fontEngine->loadType1Font(id, fontBuf, winAnsiEncoding);
        } else if (fontLoc->fontType == fontTrueType) {
            if (!(ff = FoFiTrueType::make(
                      fontBuf->c_str(), fontBuf->getLength(), fontLoc->fontNum))) {
                delete fontLoc;
                delete id;
                return 0;
            }

            for (cmap = 0; cmap < ff->getNumCmaps(); ++cmap) {
                if ((ff->getCmapPlatform(cmap) == 3 &&
                     ff->getCmapEncoding(cmap) == 1) ||
                    ff->getCmapPlatform(cmap) == 0) {
                    break;
                }
            }

            if (cmap == ff->getNumCmaps()) {
                delete ff;
                delete fontLoc;
                delete id;
                return 0;
            }

            codeToGID = (int *)calloc(256, sizeof(int));

            for (size_t i = 0; i < 256; ++i) {
                codeToGID[i] = 0;

                if (winAnsiEncoding[i] &&
                    (u = globalParams->mapNameToUnicode(winAnsiEncoding[i]))) {
                    codeToGID[i] = ff->mapCodeToGID(cmap, u);
                }
            }

            delete ff;

            fontFile = fontEngine->loadTrueTypeFont(
                id, fontBuf, fontLoc->fontNum, codeToGID, 256, 0);
        } else {
            delete fontLoc;
            delete id;
            return 0;
        }

        delete fontLoc;
    }

    if (0 == fontFile)
        return 0;

    // create the scaled font
    oblique = (SplashCoord)((SplashOutFontFileID *)fontFile->getID())->getOblique();

    textMat[0] = (SplashCoord)textMatA[0];
    textMat[1] = (SplashCoord)textMatA[1];
    textMat[2] = oblique * textMatA[0] + textMatA[2];
    textMat[3] = oblique * textMatA[1] + textMatA[3];

    return fontObj = fontEngine->getFont(
        fontFile, textMat, splash->getMatrix());
}

#if 1 //~tmp: turn off anti-aliasing temporarily
void SplashOutputDev::setInShading(bool sh)
{
    splash->setInShading(sh);
}
#endif

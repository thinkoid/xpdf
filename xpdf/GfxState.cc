// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstddef>
#include <cmath>
#include <cstring>

#include <utils/memory.hh>

#include <xpdf/Error.hh>
#include <xpdf/obj.hh>
#include <xpdf/array.hh>
#include <xpdf/dict.hh>
#include <xpdf/Page.hh>
#include <xpdf/XRef.hh>
#include <xpdf/GfxState.hh>

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/fill.hpp>
using namespace ranges;

//------------------------------------------------------------------------

// Max depth of nested color spaces.  This is used to catch infinite
// loops in the color space object structure.
#define colorSpaceRecursionLimit 8

namespace xpdf {

inline xpdf::color_t clamp_color(xpdf::color_t x)
{
    return std::clamp(x, 0, XPDF_FIXED_POINT_ONE);
}

inline double clamp_color(double x)
{
    return std::clamp(x, 0., 1.);
}

} // namespace xpdf

//------------------------------------------------------------------------

struct GfxBlendModeInfo
{
    const char * name;
    GfxBlendMode mode;
};

static GfxBlendModeInfo gfxBlendModeNames[] = {
    { "Normal", gfxBlendNormal },         { "Compatible", gfxBlendNormal },
    { "Multiply", gfxBlendMultiply },     { "Screen", gfxBlendScreen },
    { "Overlay", gfxBlendOverlay },       { "Darken", gfxBlendDarken },
    { "Lighten", gfxBlendLighten },       { "ColorDodge", gfxBlendColorDodge },
    { "ColorBurn", gfxBlendColorBurn },   { "HardLight", gfxBlendHardLight },
    { "SoftLight", gfxBlendSoftLight },   { "Difference", gfxBlendDifference },
    { "Exclusion", gfxBlendExclusion },   { "Hue", gfxBlendHue },
    { "Saturation", gfxBlendSaturation }, { "Color", gfxBlendColor },
    { "Luminosity", gfxBlendLuminosity }
};

#define nGfxBlendModeNames                                                       \
    ((int)((sizeof(gfxBlendModeNames) / sizeof(GfxBlendModeInfo))))

//------------------------------------------------------------------------

// NB: This must match the GfxColorSpaceMode enum defined in
// GfxState.h
static const char *gfxColorSpaceModeNames[] = {
    "DeviceGray", "CalGray", "DeviceRGB",  "CalRGB",  "DeviceCMYK", "Lab",
    "ICCBased",   "Indexed", "Separation", "DeviceN", "Pattern"
};

#define nGfxColorSpaceModes ((sizeof(gfxColorSpaceModeNames) / sizeof(char *)))

//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

GfxColorSpace::GfxColorSpace()
{
    overprintMask = 0x0f;
}

GfxColorSpace::~GfxColorSpace() { }

GfxColorSpace *GfxColorSpace::parse(Object *csObj, int recursion)
{
    GfxColorSpace *cs;
    Object         obj1;

    if (recursion > colorSpaceRecursionLimit) {
        error(errSyntaxError, -1, "Loop detected in color space objects");
        return NULL;
    }
    cs = NULL;
    if (csObj->is_name()) {
        if (csObj->is_name("DeviceGray") || csObj->is_name("G")) {
            cs = GfxColorSpace::create(csDeviceGray);
        } else if (csObj->is_name("DeviceRGB") || csObj->is_name("RGB")) {
            cs = GfxColorSpace::create(csDeviceRGB);
        } else if (csObj->is_name("DeviceCMYK") || csObj->is_name("CMYK")) {
            cs = GfxColorSpace::create(csDeviceCMYK);
        } else if (csObj->is_name("Pattern")) {
            cs = new GfxPatternColorSpace(NULL);
        } else {
            error(errSyntaxError, -1, "Bad color space '{0:s}'",
                  csObj->as_name());
        }
    } else if (csObj->is_array() && csObj->as_array().size() > 0) {
        obj1 = resolve((*csObj)[0UL]);
        if (obj1.is_name("DeviceGray") || obj1.is_name("G")) {
            cs = GfxColorSpace::create(csDeviceGray);
        } else if (obj1.is_name("DeviceRGB") || obj1.is_name("RGB")) {
            cs = GfxColorSpace::create(csDeviceRGB);
        } else if (obj1.is_name("DeviceCMYK") || obj1.is_name("CMYK")) {
            cs = GfxColorSpace::create(csDeviceCMYK);
        } else if (obj1.is_name("CalGray")) {
            cs = GfxCalibratedGrayColorSpace::parse(csObj->as_array(), recursion);
        } else if (obj1.is_name("CalRGB")) {
            cs = GfxCalibratedRGBColorSpace::parse(csObj->as_array(), recursion);
        } else if (obj1.is_name("Lab")) {
            cs = GfxLabColorSpace::parse(csObj->as_array(), recursion);
        } else if (obj1.is_name("ICCBased")) {
            cs = GfxICCBasedColorSpace::parse(csObj->as_array(), recursion);
        } else if (obj1.is_name("Indexed") || obj1.is_name("I")) {
            cs = GfxIndexedColorSpace::parse(csObj->as_array(), recursion);
        } else if (obj1.is_name("Separation")) {
            cs = GfxSeparationColorSpace::parse(csObj->as_array(), recursion);
        } else if (obj1.is_name("DeviceN")) {
            cs = GfxDeviceNColorSpace::parse(csObj->as_array(), recursion);
        } else if (obj1.is_name("Pattern")) {
            cs = GfxPatternColorSpace::parse(csObj->as_array(), recursion);
        } else {
            error(errSyntaxError, -1, "Bad color space");
        }
    } else {
        error(errSyntaxError, -1, "Bad color space - expected name or array");
    }
    return cs;
}

GfxColorSpace *GfxColorSpace::create(GfxColorSpaceMode mode)
{
    GfxColorSpace *cs;

    cs = NULL;
    if (mode == csDeviceGray) {
        cs = new GfxDeviceGrayColorSpace();
    } else if (mode == csDeviceRGB) {
        cs = new GfxDeviceRGBColorSpace();
    } else if (mode == csDeviceCMYK) {
        cs = new GfxDeviceCMYKColorSpace();
    }
    return cs;
}

void GfxColorSpace::getDefaultRanges(double *decodeLow, double *decodeRange,
                                     int maxImgPixel)
{
    int i;

    for (i = 0; i < getNComps(); ++i) {
        decodeLow[i] = 0;
        decodeRange[i] = 1;
    }
}

int GfxColorSpace::getNumColorSpaceModes()
{
    return nGfxColorSpaceModes;
}

const char *GfxColorSpace::getColorSpaceModeName(int idx)
{
    return gfxColorSpaceModeNames[idx];
}

//------------------------------------------------------------------------
// GfxDeviceGrayColorSpace
//------------------------------------------------------------------------

GfxDeviceGrayColorSpace::GfxDeviceGrayColorSpace() { }

GfxDeviceGrayColorSpace::~GfxDeviceGrayColorSpace() { }

GfxColorSpace *GfxDeviceGrayColorSpace::copy()
{
    GfxDeviceGrayColorSpace *cs;

    cs = new GfxDeviceGrayColorSpace();
    return cs;
}

void GfxDeviceGrayColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    gray->x = xpdf::clamp_color(color->c[0]);
}

void GfxDeviceGrayColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    rgb->r = rgb->g = rgb->b = xpdf::clamp_color(color->c[0]);
}

void GfxDeviceGrayColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    cmyk->c = cmyk->m = cmyk->y = 0;
    cmyk->k = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - color->c[0]);
}

void GfxDeviceGrayColorSpace::getDefaultColor(GfxColor *color)
{
    color->c[0] = 0;
}

//------------------------------------------------------------------------
// GfxCalibratedGrayColorSpace
//------------------------------------------------------------------------

GfxCalibratedGrayColorSpace::GfxCalibratedGrayColorSpace()
{
    whiteX = whiteY = whiteZ = 1;
    blackX = blackY = blackZ = 0;
    gamma = 1;
}

GfxCalibratedGrayColorSpace::~GfxCalibratedGrayColorSpace() { }

GfxColorSpace *GfxCalibratedGrayColorSpace::copy()
{
    GfxCalibratedGrayColorSpace *cs;

    cs = new GfxCalibratedGrayColorSpace();
    cs->whiteX = whiteX;
    cs->whiteY = whiteY;
    cs->whiteZ = whiteZ;
    cs->blackX = blackX;
    cs->blackY = blackY;
    cs->blackZ = blackZ;
    cs->gamma = gamma;
    return cs;
}

GfxColorSpace *GfxCalibratedGrayColorSpace::parse(Array &arr, int recursion)
{
    GfxCalibratedGrayColorSpace *cs;
    Object                       obj1, obj2, obj3;

    if (arr.size() < 2) {
        error(errSyntaxError, -1, "Bad CalGray color space");
        return NULL;
    }
    obj1 = resolve(arr[1]);
    if (!obj1.is_dict()) {
        error(errSyntaxError, -1, "Bad CalGray color space");
        return NULL;
    }
    cs = new GfxCalibratedGrayColorSpace();
    if ((obj2 = resolve(obj1.as_dict()["WhitePoint"])).is_array() &&
        obj2.as_array().size() == 3) {
        obj3 = resolve(obj2[0UL]);
        cs->whiteX = obj3.as_num();
        obj3 = resolve(obj2[1]);
        cs->whiteY = obj3.as_num();
        obj3 = resolve(obj2[2]);
        cs->whiteZ = obj3.as_num();
    }
    if ((obj2 = resolve(obj1.as_dict()["BlackPoint"])).is_array() &&
        obj2.as_array().size() == 3) {
        obj3 = resolve(obj2[0UL]);
        cs->blackX = obj3.as_num();
        obj3 = resolve(obj2[1]);
        cs->blackY = obj3.as_num();
        obj3 = resolve(obj2[2]);
        cs->blackZ = obj3.as_num();
    }
    if ((obj2 = resolve(obj1.as_dict()["Gamma"])).is_num()) {
        cs->gamma = obj2.as_num();
    }
    return cs;
}

void GfxCalibratedGrayColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    gray->x = xpdf::clamp_color(color->c[0]);
}

void GfxCalibratedGrayColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    rgb->r = rgb->g = rgb->b = xpdf::clamp_color(color->c[0]);
}

void GfxCalibratedGrayColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    cmyk->c = cmyk->m = cmyk->y = 0;
    cmyk->k = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - color->c[0]);
}

void GfxCalibratedGrayColorSpace::getDefaultColor(GfxColor *color)
{
    color->c[0] = 0;
}

//------------------------------------------------------------------------
// GfxDeviceRGBColorSpace
//------------------------------------------------------------------------

GfxDeviceRGBColorSpace::GfxDeviceRGBColorSpace() { }

GfxDeviceRGBColorSpace::~GfxDeviceRGBColorSpace() { }

GfxColorSpace *GfxDeviceRGBColorSpace::copy()
{
    GfxDeviceRGBColorSpace *cs;

    cs = new GfxDeviceRGBColorSpace();
    return cs;
}

void GfxDeviceRGBColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    gray->x = xpdf::clamp_color((xpdf::color_t)(
        0.3 * color->c[0] + 0.59 * color->c[1] + 0.11 * color->c[2] + 0.5));
}

void GfxDeviceRGBColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    rgb->r = xpdf::clamp_color(color->c[0]);
    rgb->g = xpdf::clamp_color(color->c[1]);
    rgb->b = xpdf::clamp_color(color->c[2]);
}

void GfxDeviceRGBColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    xpdf::color_t c, m, y, k;

    c = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - color->c[0]);
    m = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - color->c[1]);
    y = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - color->c[2]);
    k = c;
    if (m < k) {
        k = m;
    }
    if (y < k) {
        k = y;
    }
    cmyk->c = c - k;
    cmyk->m = m - k;
    cmyk->y = y - k;
    cmyk->k = k;
}

void GfxDeviceRGBColorSpace::getDefaultColor(GfxColor *color)
{
    color->c[0] = 0;
    color->c[1] = 0;
    color->c[2] = 0;
}

//------------------------------------------------------------------------
// GfxCalibratedRGBColorSpace
//------------------------------------------------------------------------

GfxCalibratedRGBColorSpace::GfxCalibratedRGBColorSpace()
{
    whiteX = whiteY = whiteZ = 1;
    blackX = blackY = blackZ = 0;
    gammaR = gammaG = gammaB = 1;
    mat[0] = 1;
    mat[1] = 0;
    mat[2] = 0;
    mat[3] = 0;
    mat[4] = 1;
    mat[5] = 0;
    mat[6] = 0;
    mat[7] = 0;
    mat[8] = 1;
}

GfxCalibratedRGBColorSpace::~GfxCalibratedRGBColorSpace() { }

GfxColorSpace *GfxCalibratedRGBColorSpace::copy()
{
    GfxCalibratedRGBColorSpace *cs;
    int                         i;

    cs = new GfxCalibratedRGBColorSpace();
    cs->whiteX = whiteX;
    cs->whiteY = whiteY;
    cs->whiteZ = whiteZ;
    cs->blackX = blackX;
    cs->blackY = blackY;
    cs->blackZ = blackZ;
    cs->gammaR = gammaR;
    cs->gammaG = gammaG;
    cs->gammaB = gammaB;
    for (i = 0; i < 9; ++i) {
        cs->mat[i] = mat[i];
    }
    return cs;
}

GfxColorSpace *GfxCalibratedRGBColorSpace::parse(Array &arr, int recursion)
{
    GfxCalibratedRGBColorSpace *cs;
    Object                      obj1, obj2, obj3;
    int                         i;

    if (arr.size() < 2) {
        error(errSyntaxError, -1, "Bad CalRGB color space");
        return NULL;
    }
    obj1 = resolve(arr[1]);
    if (!obj1.is_dict()) {
        error(errSyntaxError, -1, "Bad CalRGB color space");
        return NULL;
    }
    cs = new GfxCalibratedRGBColorSpace();
    if ((obj2 = resolve(obj1.as_dict()["WhitePoint"])).is_array() &&
        obj2.as_array().size() == 3) {
        obj3 = resolve(obj2[0UL]);
        cs->whiteX = obj3.as_num();
        obj3 = resolve(obj2[1]);
        cs->whiteY = obj3.as_num();
        obj3 = resolve(obj2[2]);
        cs->whiteZ = obj3.as_num();
    }
    if ((obj2 = resolve(obj1.as_dict()["BlackPoint"])).is_array() &&
        obj2.as_array().size() == 3) {
        obj3 = resolve(obj2[0UL]);
        cs->blackX = obj3.as_num();
        obj3 = resolve(obj2[1]);
        cs->blackY = obj3.as_num();
        obj3 = resolve(obj2[2]);
        cs->blackZ = obj3.as_num();
    }
    if ((obj2 = resolve(obj1.as_dict()["Gamma"])).is_array() &&
        obj2.as_array().size() == 3) {
        obj3 = resolve(obj2[0UL]);
        cs->gammaR = obj3.as_num();
        obj3 = resolve(obj2[1]);
        cs->gammaG = obj3.as_num();
        obj3 = resolve(obj2[2]);
        cs->gammaB = obj3.as_num();
    }
    if ((obj2 = resolve(obj1.as_dict()["Matrix"])).is_array() &&
        obj2.as_array().size() == 9) {
        for (i = 0; i < 9; ++i) {
            obj3 = resolve(obj2[i]);
            cs->mat[i] = obj3.as_num();
        }
    }
    return cs;
}

void GfxCalibratedRGBColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    gray->x = xpdf::clamp_color((xpdf::color_t)(
        0.299 * color->c[0] + 0.587 * color->c[1] + 0.114 * color->c[2] + 0.5));
}

void GfxCalibratedRGBColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    rgb->r = xpdf::clamp_color(color->c[0]);
    rgb->g = xpdf::clamp_color(color->c[1]);
    rgb->b = xpdf::clamp_color(color->c[2]);
}

void GfxCalibratedRGBColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    xpdf::color_t c, m, y, k;

    c = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - color->c[0]);
    m = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - color->c[1]);
    y = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - color->c[2]);
    k = c;
    if (m < k) {
        k = m;
    }
    if (y < k) {
        k = y;
    }
    cmyk->c = c - k;
    cmyk->m = m - k;
    cmyk->y = y - k;
    cmyk->k = k;
}

void GfxCalibratedRGBColorSpace::getDefaultColor(GfxColor *color)
{
    color->c[0] = 0;
    color->c[1] = 0;
    color->c[2] = 0;
}

//------------------------------------------------------------------------
// GfxDeviceCMYKColorSpace
//------------------------------------------------------------------------

GfxDeviceCMYKColorSpace::GfxDeviceCMYKColorSpace() { }

GfxDeviceCMYKColorSpace::~GfxDeviceCMYKColorSpace() { }

GfxColorSpace *GfxDeviceCMYKColorSpace::copy()
{
    GfxDeviceCMYKColorSpace *cs;

    cs = new GfxDeviceCMYKColorSpace();
    return cs;
}

void GfxDeviceCMYKColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    gray->x = xpdf::clamp_color(
        (xpdf::color_t)(XPDF_FIXED_POINT_ONE - color->c[3] - 0.3 * color->c[0] -
                        0.59 * color->c[1] - 0.11 * color->c[2] + 0.5));
}

void GfxDeviceCMYKColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    double c, m, y, k, c1, m1, y1, k1, r, g, b, x;

    c = xpdf::to_double(color->c[0]);
    m = xpdf::to_double(color->c[1]);
    y = xpdf::to_double(color->c[2]);
    k = xpdf::to_double(color->c[3]);
    c1 = 1 - c;
    m1 = 1 - m;
    y1 = 1 - y;
    k1 = 1 - k;
    // this is a matrix multiplication, unrolled for performance
    //                        C M Y K
    x = c1 * m1 * y1 * k1; // 0 0 0 0
    r = g = b = x;
    x = c1 * m1 * y1 * k; // 0 0 0 1
    r += 0.1373 * x;
    g += 0.1216 * x;
    b += 0.1255 * x;
    x = c1 * m1 * y * k1; // 0 0 1 0
    r += x;
    g += 0.9490 * x;
    x = c1 * m1 * y * k; // 0 0 1 1
    r += 0.1098 * x;
    g += 0.1020 * x;
    x = c1 * m * y1 * k1; // 0 1 0 0
    r += 0.9255 * x;
    b += 0.5490 * x;
    x = c1 * m * y1 * k; // 0 1 0 1
    r += 0.1412 * x;
    x = c1 * m * y * k1; // 0 1 1 0
    r += 0.9294 * x;
    g += 0.1098 * x;
    b += 0.1412 * x;
    x = c1 * m * y * k; // 0 1 1 1
    r += 0.1333 * x;
    x = c * m1 * y1 * k1; // 1 0 0 0
    g += 0.6784 * x;
    b += 0.9373 * x;
    x = c * m1 * y1 * k; // 1 0 0 1
    g += 0.0588 * x;
    b += 0.1412 * x;
    x = c * m1 * y * k1; // 1 0 1 0
    g += 0.6510 * x;
    b += 0.3137 * x;
    x = c * m1 * y * k; // 1 0 1 1
    g += 0.0745 * x;
    x = c * m * y1 * k1; // 1 1 0 0
    r += 0.1804 * x;
    g += 0.1922 * x;
    b += 0.5725 * x;
    x = c * m * y1 * k; // 1 1 0 1
    b += 0.0078 * x;
    x = c * m * y * k1; // 1 1 1 0
    r += 0.2118 * x;
    g += 0.2119 * x;
    b += 0.2235 * x;
    rgb->r = xpdf::clamp_color(xpdf::to_color(r));
    rgb->g = xpdf::clamp_color(xpdf::to_color(g));
    rgb->b = xpdf::clamp_color(xpdf::to_color(b));
}

void GfxDeviceCMYKColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    cmyk->c = xpdf::clamp_color(color->c[0]);
    cmyk->m = xpdf::clamp_color(color->c[1]);
    cmyk->y = xpdf::clamp_color(color->c[2]);
    cmyk->k = xpdf::clamp_color(color->c[3]);
}

void GfxDeviceCMYKColorSpace::getDefaultColor(GfxColor *color)
{
    color->c[0] = 0;
    color->c[1] = 0;
    color->c[2] = 0;
    color->c[3] = XPDF_FIXED_POINT_ONE;
}

//------------------------------------------------------------------------
// GfxLabColorSpace
//------------------------------------------------------------------------

// This is the inverse of MatrixLMN in Example 4.10 from the PostScript
// Language Reference, Third Edition.
static double xyzrgb[3][3] = { { 3.240449, -1.537136, -0.498531 },
                               { -0.969265, 1.876011, 0.041556 },
                               { 0.055643, -0.204026, 1.057229 } };

GfxLabColorSpace::GfxLabColorSpace()
{
    whiteX = whiteY = whiteZ = 1;
    blackX = blackY = blackZ = 0;
    aMin = bMin = -100;
    aMax = bMax = 100;
}

GfxLabColorSpace::~GfxLabColorSpace() { }

GfxColorSpace *GfxLabColorSpace::copy()
{
    GfxLabColorSpace *cs;

    cs = new GfxLabColorSpace();
    cs->whiteX = whiteX;
    cs->whiteY = whiteY;
    cs->whiteZ = whiteZ;
    cs->blackX = blackX;
    cs->blackY = blackY;
    cs->blackZ = blackZ;
    cs->aMin = aMin;
    cs->aMax = aMax;
    cs->bMin = bMin;
    cs->bMax = bMax;
    cs->kr = kr;
    cs->kg = kg;
    cs->kb = kb;
    return cs;
}

GfxColorSpace *GfxLabColorSpace::parse(Array &arr, int recursion)
{
    GfxLabColorSpace *cs;
    Object            obj1, obj2, obj3;

    if (arr.size() < 2) {
        error(errSyntaxError, -1, "Bad Lab color space");
        return NULL;
    }
    obj1 = resolve(arr[1]);
    if (!obj1.is_dict()) {
        error(errSyntaxError, -1, "Bad Lab color space");
        return NULL;
    }
    cs = new GfxLabColorSpace();
    if ((obj2 = resolve(obj1.as_dict()["WhitePoint"])).is_array() &&
        obj2.as_array().size() == 3) {
        obj3 = resolve(obj2[0UL]);
        cs->whiteX = obj3.as_num();
        obj3 = resolve(obj2[1]);
        cs->whiteY = obj3.as_num();
        obj3 = resolve(obj2[2]);
        cs->whiteZ = obj3.as_num();
    }
    if ((obj2 = resolve(obj1.as_dict()["BlackPoint"])).is_array() &&
        obj2.as_array().size() == 3) {
        obj3 = resolve(obj2[0UL]);
        cs->blackX = obj3.as_num();
        obj3 = resolve(obj2[1]);
        cs->blackY = obj3.as_num();
        obj3 = resolve(obj2[2]);
        cs->blackZ = obj3.as_num();
    }
    if ((obj2 = resolve(obj1.as_dict()["Range"])).is_array() &&
        obj2.as_array().size() == 4) {
        obj3 = resolve(obj2[0UL]);
        cs->aMin = obj3.as_num();
        obj3 = resolve(obj2[1]);
        cs->aMax = obj3.as_num();
        obj3 = resolve(obj2[2]);
        cs->bMin = obj3.as_num();
        obj3 = resolve(obj2[3]);
        cs->bMax = obj3.as_num();
    }

    cs->kr = 1 / (xyzrgb[0][0] * cs->whiteX + xyzrgb[0][1] * cs->whiteY +
                  xyzrgb[0][2] * cs->whiteZ);
    cs->kg = 1 / (xyzrgb[1][0] * cs->whiteX + xyzrgb[1][1] * cs->whiteY +
                  xyzrgb[1][2] * cs->whiteZ);
    cs->kb = 1 / (xyzrgb[2][0] * cs->whiteX + xyzrgb[2][1] * cs->whiteY +
                  xyzrgb[2][2] * cs->whiteZ);

    return cs;
}

void GfxLabColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    GfxRGB rgb;

    getRGB(color, &rgb);
    gray->x = xpdf::clamp_color(
        (xpdf::color_t)(0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b + 0.5));
}

void GfxLabColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    double X, Y, Z;
    double t1, t2;
    double r, g, b;

    // convert L*a*b* to CIE 1931 XYZ color space
    t1 = (xpdf::to_double(color->c[0]) + 16) / 116;
    t2 = t1 + xpdf::to_double(color->c[1]) / 500;
    if (t2 >= (6.0 / 29.0)) {
        X = t2 * t2 * t2;
    } else {
        X = (108.0 / 841.0) * (t2 - (4.0 / 29.0));
    }
    X *= whiteX;
    if (t1 >= (6.0 / 29.0)) {
        Y = t1 * t1 * t1;
    } else {
        Y = (108.0 / 841.0) * (t1 - (4.0 / 29.0));
    }
    Y *= whiteY;
    t2 = t1 - xpdf::to_double(color->c[2]) / 200;
    if (t2 >= (6.0 / 29.0)) {
        Z = t2 * t2 * t2;
    } else {
        Z = (108.0 / 841.0) * (t2 - (4.0 / 29.0));
    }
    Z *= whiteZ;

    // convert XYZ to RGB, including gamut mapping and gamma correction
    r = xyzrgb[0][0] * X + xyzrgb[0][1] * Y + xyzrgb[0][2] * Z;
    g = xyzrgb[1][0] * X + xyzrgb[1][1] * Y + xyzrgb[1][2] * Z;
    b = xyzrgb[2][0] * X + xyzrgb[2][1] * Y + xyzrgb[2][2] * Z;
    rgb->r = xpdf::to_color(pow(xpdf::clamp_color(r * kr), 0.5));
    rgb->g = xpdf::to_color(pow(xpdf::clamp_color(g * kg), 0.5));
    rgb->b = xpdf::to_color(pow(xpdf::clamp_color(b * kb), 0.5));
}

void GfxLabColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    GfxRGB        rgb;
    xpdf::color_t c, m, y, k;

    getRGB(color, &rgb);
    c = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - rgb.r);
    m = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - rgb.g);
    y = xpdf::clamp_color(XPDF_FIXED_POINT_ONE - rgb.b);
    k = c;
    if (m < k) {
        k = m;
    }
    if (y < k) {
        k = y;
    }
    cmyk->c = c - k;
    cmyk->m = m - k;
    cmyk->y = y - k;
    cmyk->k = k;
}

void GfxLabColorSpace::getDefaultColor(GfxColor *color)
{
    color->c[0] = 0;
    if (aMin > 0) {
        color->c[1] = xpdf::to_color(aMin);
    } else if (aMax < 0) {
        color->c[1] = xpdf::to_color(aMax);
    } else {
        color->c[1] = 0;
    }
    if (bMin > 0) {
        color->c[2] = xpdf::to_color(bMin);
    } else if (bMax < 0) {
        color->c[2] = xpdf::to_color(bMax);
    } else {
        color->c[2] = 0;
    }
}

void GfxLabColorSpace::getDefaultRanges(double *decodeLow, double *decodeRange,
                                        int maxImgPixel)
{
    decodeLow[0] = 0;
    decodeRange[0] = 100;
    decodeLow[1] = aMin;
    decodeRange[1] = aMax - aMin;
    decodeLow[2] = bMin;
    decodeRange[2] = bMax - bMin;
}

//------------------------------------------------------------------------
// GfxICCBasedColorSpace
//------------------------------------------------------------------------

GfxICCBasedColorSpace::GfxICCBasedColorSpace(int nCompsA, GfxColorSpace *altA,
                                             Ref *iccProfileStreamA)
{
    nComps = nCompsA;
    alt = altA;
    iccProfileStream = *iccProfileStreamA;
    rangeMin[0] = rangeMin[1] = rangeMin[2] = rangeMin[3] = 0;
    rangeMax[0] = rangeMax[1] = rangeMax[2] = rangeMax[3] = 1;
}

GfxICCBasedColorSpace::~GfxICCBasedColorSpace()
{
    delete alt;
}

GfxColorSpace *GfxICCBasedColorSpace::copy()
{
    GfxICCBasedColorSpace *cs;
    int                    i;

    cs = new GfxICCBasedColorSpace(nComps, alt->copy(), &iccProfileStream);
    for (i = 0; i < 4; ++i) {
        cs->rangeMin[i] = rangeMin[i];
        cs->rangeMax[i] = rangeMax[i];
    }
    return cs;
}

GfxColorSpace *GfxICCBasedColorSpace::parse(Array &arr, int recursion)
{
    GfxICCBasedColorSpace *cs;
    Ref                    iccProfileStreamA;
    int                    nCompsA;
    GfxColorSpace *        altA;
    Dict *                 dict;
    Object                 obj1, obj2, obj3;
    int                    i;

    if (arr.size() < 2) {
        error(errSyntaxError, -1, "Bad ICCBased color space");
        return NULL;
    }
    obj1 = arr[1];
    if (obj1.is_ref()) {
        iccProfileStreamA = obj1.as_ref();
    } else {
        iccProfileStreamA.num = 0;
        iccProfileStreamA.gen = 0;
    }
    obj1 = resolve(arr[1]);
    if (!obj1.is_stream()) {
        error(errSyntaxError, -1, "Bad ICCBased color space (stream)");
        return NULL;
    }
    dict = obj1.streamGetDict();
    if (!(obj2 = resolve((*dict)["N"])).is_int()) {
        error(errSyntaxError, -1, "Bad ICCBased color space (N)");
        return NULL;
    }
    nCompsA = obj2.as_int();
    if (nCompsA > 4) {
        error(errSyntaxError, -1,
              "ICCBased color space with too many ({0:d} > 4) components",
              nCompsA);
        nCompsA = 4;
    }
    if ((obj2 = resolve((*dict)["Alternate"])).is_null() ||
        !(altA = GfxColorSpace::parse(&obj2, recursion + 1))) {
        switch (nCompsA) {
        case 1:
            altA = GfxColorSpace::create(csDeviceGray);
            break;
        case 3:
            altA = GfxColorSpace::create(csDeviceRGB);
            break;
        case 4:
            altA = GfxColorSpace::create(csDeviceCMYK);
            break;
        default:
            error(errSyntaxError, -1, "Bad ICCBased color space - invalid N");
            return NULL;
        }
    }
    cs = new GfxICCBasedColorSpace(nCompsA, altA, &iccProfileStreamA);
    if ((obj2 = resolve((*dict)["Range"])).is_array() &&
        obj2.as_array().size() == 2 * nCompsA) {
        for (i = 0; i < nCompsA; ++i) {
            obj3 = resolve(obj2[2 * i]);
            cs->rangeMin[i] = obj3.as_num();
            obj3 = resolve(obj2[2 * i + 1]);
            cs->rangeMax[i] = obj3.as_num();
        }
    }
    return cs;
}

void GfxICCBasedColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    alt->getGray(color, gray);
}

void GfxICCBasedColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    alt->getRGB(color, rgb);
}

void GfxICCBasedColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    alt->getCMYK(color, cmyk);
}

void GfxICCBasedColorSpace::getDefaultColor(GfxColor *color)
{
    int i;

    for (i = 0; i < nComps; ++i) {
        if (rangeMin[i] > 0) {
            color->c[i] = xpdf::to_color(rangeMin[i]);
        } else if (rangeMax[i] < 0) {
            color->c[i] = xpdf::to_color(rangeMax[i]);
        } else {
            color->c[i] = 0;
        }
    }
}

void GfxICCBasedColorSpace::getDefaultRanges(double *decodeLow,
                                             double *decodeRange, int maxImgPixel)
{
    alt->getDefaultRanges(decodeLow, decodeRange, maxImgPixel);

#if 0
  // this is nominally correct, but some PDF files don't set the
  // correct ranges in the ICCBased dict
  int i;

  for (i = 0; i < nComps; ++i) {
    decodeLow[i] = rangeMin[i];
    decodeRange[i] = rangeMax[i] - rangeMin[i];
  }
#endif
}

//------------------------------------------------------------------------
// GfxIndexedColorSpace
//------------------------------------------------------------------------

GfxIndexedColorSpace::GfxIndexedColorSpace(GfxColorSpace *baseA, int indexHighA)
{
    base = baseA;
    indexHigh = indexHighA;
    lookup = (unsigned char *)calloc((indexHigh + 1) * base->getNComps(),
                                     sizeof(unsigned char));
    overprintMask = base->getOverprintMask();
}

GfxIndexedColorSpace::~GfxIndexedColorSpace()
{
    delete base;
    free(lookup);
}

GfxColorSpace *GfxIndexedColorSpace::copy()
{
    GfxIndexedColorSpace *cs;

    cs = new GfxIndexedColorSpace(base->copy(), indexHigh);
    memcpy(cs->lookup, lookup,
           (indexHigh + 1) * base->getNComps() * sizeof(unsigned char));
    return cs;
}

GfxColorSpace *GfxIndexedColorSpace::parse(Array &arr, int recursion)
{
    GfxIndexedColorSpace *cs;
    GfxColorSpace *       baseA;
    int                   indexHighA;
    Object                obj1;
    int                   x;
    int                   n, i, j;

    if (arr.size() != 4) {
        error(errSyntaxError, -1, "Bad Indexed color space");
        goto err1;
    }
    obj1 = resolve(arr[1]);
    if (!(baseA = GfxColorSpace::parse(&obj1, recursion + 1))) {
        error(errSyntaxError, -1, "Bad Indexed color space (base color space)");
        goto err2;
    }
    if (!(obj1 = resolve(arr[2])).is_int()) {
        error(errSyntaxError, -1, "Bad Indexed color space (hival)");
        delete baseA;
        goto err2;
    }
    indexHighA = obj1.as_int();
    if (indexHighA < 0 || indexHighA > 255) {
        // the PDF spec requires indexHigh to be in [0,255] -- allowing
        // values larger than 255 creates a security hole: if nComps *
        // indexHigh is greater than 2^31, the loop below may overwrite
        // past the end of the array
        error(errSyntaxError, -1,
              "Bad Indexed color space (invalid indexHigh value)");
        delete baseA;
        goto err2;
    }
    cs = new GfxIndexedColorSpace(baseA, indexHighA);
    obj1 = resolve(arr[3]);
    n = baseA->getNComps();
    if (obj1.is_stream()) {
        obj1.streamReset();
        for (i = 0; i <= indexHighA; ++i) {
            for (j = 0; j < n; ++j) {
                if ((x = obj1.streamGetChar()) == EOF) {
                    error(errSyntaxError, -1,
                          "Bad Indexed color space (lookup table stream too "
                          "short)");
                    cs->indexHigh = indexHighA = i - 1;
                }
                cs->lookup[i * n + j] = (unsigned char)x;
            }
        }
        obj1.streamClose();
    } else if (obj1.is_string()) {
        if (obj1.as_string()->getLength() < (indexHighA + 1) * n) {
            error(errSyntaxError, -1,
                  "Bad Indexed color space (lookup table string too short)");
            cs->indexHigh = indexHighA = obj1.as_string()->getLength() / n - 1;
        }

        const char *s = obj1.as_string()->c_str();

        for (i = 0; i <= indexHighA; ++i) {
            for (j = 0; j < n; ++j) {
                cs->lookup[i * n + j] = (unsigned char)*s++;
            }
        }
    } else {
        error(errSyntaxError, -1, "Bad Indexed color space (lookup table)");
        goto err3;
    }
    return cs;

err3:
    delete cs;
err2:
err1:
    return NULL;
}

GfxColor *GfxIndexedColorSpace::mapColorToBase(GfxColor *color,
                                               GfxColor *baseColor)
{
    unsigned char *p;
    double         low[gfxColorMaxComps], range[gfxColorMaxComps];
    int            n, i;

    n = base->getNComps();
    base->getDefaultRanges(low, range, indexHigh);
    p = &lookup[(int)(xpdf::to_double(color->c[0]) + 0.5) * n];
    for (i = 0; i < n; ++i) {
        baseColor->c[i] = xpdf::to_color(low[i] + (p[i] / 255.0) * range[i]);
    }
    return baseColor;
}

void GfxIndexedColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    GfxColor color2;

    base->getGray(mapColorToBase(color, &color2), gray);
}

void GfxIndexedColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    GfxColor color2;

    base->getRGB(mapColorToBase(color, &color2), rgb);
}

void GfxIndexedColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    GfxColor color2;

    base->getCMYK(mapColorToBase(color, &color2), cmyk);
}

void GfxIndexedColorSpace::getDefaultColor(GfxColor *color)
{
    color->c[0] = 0;
}

void GfxIndexedColorSpace::getDefaultRanges(double *decodeLow,
                                            double *decodeRange, int maxImgPixel)
{
    decodeLow[0] = 0;
    decodeRange[0] = maxImgPixel;
}

//------------------------------------------------------------------------
// GfxSeparationColorSpace
//------------------------------------------------------------------------

GfxSeparationColorSpace::GfxSeparationColorSpace(GString *       nameA,
                                                 GfxColorSpace * altA,
                                                 const Function &funcA)
{
    name = nameA;
    alt = altA;
    func = funcA;
    nonMarking = !name->cmp("None");
    if (!name->cmp("Cyan")) {
        overprintMask = 0x01;
    } else if (!name->cmp("Magenta")) {
        overprintMask = 0x02;
    } else if (!name->cmp("Yellow")) {
        overprintMask = 0x04;
    } else if (!name->cmp("Black")) {
        overprintMask = 0x08;
    }
}

GfxSeparationColorSpace::GfxSeparationColorSpace(GString *       nameA,
                                                 GfxColorSpace * altA,
                                                 const Function &funcA,
                                                 bool            nonMarkingA,
                                                 unsigned        overprintMaskA)
{
    name = nameA;
    alt = altA;
    func = funcA;
    nonMarking = nonMarkingA;
    overprintMask = overprintMaskA;
}

GfxSeparationColorSpace::~GfxSeparationColorSpace()
{
    delete name;
    delete alt;
}

GfxColorSpace *GfxSeparationColorSpace::copy()
{
    GfxSeparationColorSpace *cs;

    cs = new GfxSeparationColorSpace(name->copy(), alt->copy(), func, nonMarking,
                                     overprintMask);
    return cs;
}

//~ handle the 'All' and 'None' colorants
GfxColorSpace *GfxSeparationColorSpace::parse(Array &arr, int recursion)
{
    GfxSeparationColorSpace *cs;
    GString *                nameA;
    GfxColorSpace *          altA;
    Function                 funcA;
    Object                   obj1;

    if (arr.size() != 4) {
        error(errSyntaxError, -1, "Bad Separation color space");
        goto err1;
    }
    if (!(obj1 = resolve(arr[1])).is_name()) {
        error(errSyntaxError, -1, "Bad Separation color space (name)");
        goto err2;
    }
    nameA = new GString(obj1.as_name());
    obj1 = resolve(arr[2]);
    if (!(altA = GfxColorSpace::parse(&obj1, recursion + 1))) {
        error(errSyntaxError, -1,
              "Bad Separation color space (alternate color space)");
        goto err3;
    }
    obj1 = resolve(arr[3]);
    if (!(funcA = xpdf::make_function(obj1))) {
        goto err4;
    }
    cs = new GfxSeparationColorSpace(nameA, altA, funcA);
    return cs;

err4:
    delete altA;
err3:
    delete nameA;
err2:
err1:
    return NULL;
}

void GfxSeparationColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    double   x;
    double   c[gfxColorMaxComps];
    GfxColor color2;
    int      i;

    x = xpdf::to_double(color->c[0]);
    func(&x, &x + 1, c);
    for (i = 0; i < alt->getNComps(); ++i) {
        color2.c[i] = xpdf::to_color(c[i]);
    }
    alt->getGray(&color2, gray);
}

void GfxSeparationColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    double   x;
    double   c[gfxColorMaxComps];
    GfxColor color2;
    int      i;

    x = xpdf::to_double(color->c[0]);
    func(&x, &x + 1, c);
    for (i = 0; i < alt->getNComps(); ++i) {
        color2.c[i] = xpdf::to_color(c[i]);
    }
    alt->getRGB(&color2, rgb);
}

void GfxSeparationColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    double   x;
    double   c[gfxColorMaxComps];
    GfxColor color2;
    int      i;

    x = xpdf::to_double(color->c[0]);
    func(&x, &x + 1, c);
    for (i = 0; i < alt->getNComps(); ++i) {
        color2.c[i] = xpdf::to_color(c[i]);
    }
    alt->getCMYK(&color2, cmyk);
}

void GfxSeparationColorSpace::getDefaultColor(GfxColor *color)
{
    color->c[0] = XPDF_FIXED_POINT_ONE;
}

//------------------------------------------------------------------------
// GfxDeviceNColorSpace
//------------------------------------------------------------------------

GfxDeviceNColorSpace::GfxDeviceNColorSpace(int nCompsA, GString **namesA,
                                           GfxColorSpace * altA,
                                           const Function &funcA)
{
    int i;

    nComps = nCompsA;
    alt = altA;
    func = funcA;
    nonMarking = true;
    overprintMask = 0;
    for (i = 0; i < nComps; ++i) {
        names[i] = namesA[i];
        if (names[i]->cmp("None")) {
            nonMarking = false;
        }
        if (!names[i]->cmp("Cyan")) {
            overprintMask |= 0x01;
        } else if (!names[i]->cmp("Magenta")) {
            overprintMask |= 0x02;
        } else if (!names[i]->cmp("Yellow")) {
            overprintMask |= 0x04;
        } else if (!names[i]->cmp("Black")) {
            overprintMask |= 0x08;
        } else {
            overprintMask = 0x0f;
        }
    }
}

GfxDeviceNColorSpace::GfxDeviceNColorSpace(int nCompsA, GString **namesA,
                                           GfxColorSpace * altA,
                                           const Function &funcA,
                                           bool            nonMarkingA,
                                           unsigned        overprintMaskA)
{
    int i;

    nComps = nCompsA;
    alt = altA;
    func = funcA;
    nonMarking = nonMarkingA;
    overprintMask = overprintMaskA;
    for (i = 0; i < nComps; ++i) {
        names[i] = namesA[i]->copy();
    }
}

GfxDeviceNColorSpace::~GfxDeviceNColorSpace()
{
    for (size_t i = 0; i < nComps; ++i) {
        delete names[i];
    }
    delete alt;
}

GfxColorSpace *GfxDeviceNColorSpace::copy()
{
    GfxDeviceNColorSpace *cs;
    cs = new GfxDeviceNColorSpace(nComps, names, alt->copy(), func, nonMarking,
                                  overprintMask);
    return cs;
}

//~ handle the 'None' colorant
GfxColorSpace *GfxDeviceNColorSpace::parse(Array &arr, int recursion)
{
    GfxDeviceNColorSpace *cs;
    int                   nCompsA;
    GString *             namesA[gfxColorMaxComps];
    GfxColorSpace *       altA;
    Function              funcA;
    Object                obj1, obj2;
    int                   i;

    if (arr.size() != 4 && arr.size() != 5) {
        error(errSyntaxError, -1, "Bad DeviceN color space");
        goto err1;
    }
    if (!(obj1 = resolve(arr[1])).is_array()) {
        error(errSyntaxError, -1, "Bad DeviceN color space (names)");
        goto err2;
    }
    nCompsA = obj1.as_array().size();
    if (nCompsA > gfxColorMaxComps) {
        error(errSyntaxError, -1,
              "DeviceN color space with too many ({0:d} > {1:d}) components",
              nCompsA, gfxColorMaxComps);
        nCompsA = gfxColorMaxComps;
    }
    for (i = 0; i < nCompsA; ++i) {
        if (!(obj2 = resolve(obj1[i])).is_name()) {
            error(errSyntaxError, -1, "Bad DeviceN color space (names)");
            goto err2;
        }
        namesA[i] = new GString(obj2.as_name());
    }
    obj1 = resolve(arr[2]);
    if (!(altA = GfxColorSpace::parse(&obj1, recursion + 1))) {
        error(errSyntaxError, -1,
              "Bad DeviceN color space (alternate color space)");
        goto err3;
    }
    obj1 = resolve(arr[3]);
    if (!(funcA = xpdf::make_function(obj1))) {
        goto err4;
    }
    cs = new GfxDeviceNColorSpace(nCompsA, namesA, altA, funcA);
    return cs;

err4:
    delete altA;
err3:
    for (i = 0; i < nCompsA; ++i) {
        delete namesA[i];
    }
err2:
err1:
    return NULL;
}

void GfxDeviceNColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    double   x[gfxColorMaxComps], c[gfxColorMaxComps];
    GfxColor color2;
    int      i;

    for (i = 0; i < nComps; ++i) {
        x[i] = xpdf::to_double(color->c[i]);
    }
    func(x, x + nComps, c);
    for (i = 0; i < alt->getNComps(); ++i) {
        color2.c[i] = xpdf::to_color(c[i]);
    }
    alt->getGray(&color2, gray);
}

void GfxDeviceNColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    double   x[gfxColorMaxComps], c[gfxColorMaxComps];
    GfxColor color2;
    int      i;

    for (i = 0; i < nComps; ++i) {
        x[i] = xpdf::to_double(color->c[i]);
    }
    func(x, x + nComps, c);
    for (i = 0; i < alt->getNComps(); ++i) {
        color2.c[i] = xpdf::to_color(c[i]);
    }
    alt->getRGB(&color2, rgb);
}

void GfxDeviceNColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    double   x[gfxColorMaxComps], c[gfxColorMaxComps];
    GfxColor color2;
    int      i;

    for (i = 0; i < nComps; ++i) {
        x[i] = xpdf::to_double(color->c[i]);
    }
    func(x, x + nComps, c);
    for (i = 0; i < alt->getNComps(); ++i) {
        color2.c[i] = xpdf::to_color(c[i]);
    }
    alt->getCMYK(&color2, cmyk);
}

void GfxDeviceNColorSpace::getDefaultColor(GfxColor *color)
{
    for (size_t i = 0; i < nComps; ++i) {
        color->c[i] = XPDF_FIXED_POINT_ONE;
    }
}

//------------------------------------------------------------------------
// GfxPatternColorSpace
//------------------------------------------------------------------------

GfxPatternColorSpace::GfxPatternColorSpace(GfxColorSpace *underA)
{
    under = underA;
}

GfxPatternColorSpace::~GfxPatternColorSpace()
{
    if (under) {
        delete under;
    }
}

GfxColorSpace *GfxPatternColorSpace::copy()
{
    GfxPatternColorSpace *cs;

    cs = new GfxPatternColorSpace(under ? under->copy() : (GfxColorSpace *)NULL);
    return cs;
}

GfxColorSpace *GfxPatternColorSpace::parse(Array &arr, int recursion)
{
    GfxPatternColorSpace *cs;
    GfxColorSpace *       underA;
    Object                obj1;

    if (arr.size() != 1 && arr.size() != 2) {
        error(errSyntaxError, -1, "Bad Pattern color space");
        return NULL;
    }
    underA = NULL;
    if (arr.size() == 2) {
        obj1 = resolve(arr[1]);
        if (!(underA = GfxColorSpace::parse(&obj1, recursion + 1))) {
            error(errSyntaxError, -1,
                  "Bad Pattern color space (underlying color space)");
            return NULL;
        }
    }
    cs = new GfxPatternColorSpace(underA);
    return cs;
}

void GfxPatternColorSpace::getGray(GfxColor *color, GfxGray *gray)
{
    gray->x = 0;
}

void GfxPatternColorSpace::getRGB(GfxColor *color, GfxRGB *rgb)
{
    rgb->r = rgb->g = rgb->b = 0;
}

void GfxPatternColorSpace::getCMYK(GfxColor *color, GfxCMYK *cmyk)
{
    cmyk->c = cmyk->m = cmyk->y = 0;
    cmyk->k = 1;
}

void GfxPatternColorSpace::getDefaultColor(GfxColor *color)
{
    // not used
}

//------------------------------------------------------------------------
// Pattern
//------------------------------------------------------------------------

GfxPattern::GfxPattern(int typeA)
{
    type = typeA;
}

GfxPattern::~GfxPattern() { }

GfxPattern *GfxPattern::parse(Object *objRef, Object *obj)
{
    GfxPattern *pattern;
    Object      typeObj;

    if (obj->is_dict()) {
        typeObj = resolve(obj->as_dict()["PatternType"]);
    } else if (obj->is_stream()) {
        typeObj = resolve((*obj->streamGetDict())["PatternType"]);
    } else {
        return NULL;
    }
    pattern = NULL;
    if (typeObj.is_int() && typeObj.as_int() == 1) {
        pattern = GfxTilingPattern::parse(objRef, obj);
    } else if (typeObj.is_int() && typeObj.as_int() == 2) {
        pattern = GfxShadingPattern::parse(obj);
    }
    return pattern;
}

//------------------------------------------------------------------------
// GfxTilingPattern
//------------------------------------------------------------------------

GfxTilingPattern *GfxTilingPattern::parse(Object *patObjRef, Object *patObj)
{
    GfxTilingPattern *pat;
    Dict *            dict;
    int               paintTypeA, tilingTypeA;
    double            bboxA[4], matrixA[6];
    double            xStepA, yStepA;
    Object            resDictA;
    Object            obj1, obj2;
    int               i;

    if (!patObj->is_stream()) {
        return NULL;
    }
    dict = patObj->streamGetDict();

    if ((obj1 = resolve((*dict)["PaintType"])).is_int()) {
        paintTypeA = obj1.as_int();
    } else {
        paintTypeA = 1;
        error(errSyntaxWarning, -1, "Invalid or missing PaintType in pattern");
    }
    if ((obj1 = resolve((*dict)["TilingType"])).is_int()) {
        tilingTypeA = obj1.as_int();
    } else {
        tilingTypeA = 1;
        error(errSyntaxWarning, -1, "Invalid or missing TilingType in pattern");
    }
    bboxA[0] = bboxA[1] = 0;
    bboxA[2] = bboxA[3] = 1;
    if ((obj1 = resolve((*dict)["BBox"])).is_array() &&
        obj1.as_array().size() == 4) {
        for (i = 0; i < 4; ++i) {
            if ((obj2 = resolve(obj1[i])).is_num()) {
                bboxA[i] = obj2.as_num();
            }
        }
    } else {
        error(errSyntaxError, -1, "Invalid or missing BBox in pattern");
    }
    if ((obj1 = resolve((*dict)["XStep"])).is_num()) {
        xStepA = obj1.as_num();
    } else {
        xStepA = 1;
        error(errSyntaxError, -1, "Invalid or missing XStep in pattern");
    }
    if ((obj1 = resolve((*dict)["YStep"])).is_num()) {
        yStepA = obj1.as_num();
    } else {
        yStepA = 1;
        error(errSyntaxError, -1, "Invalid or missing YStep in pattern");
    }
    if (!(resDictA = resolve((*dict)["Resources"])).is_dict()) {
        resDictA = {};
        error(errSyntaxError, -1, "Invalid or missing Resources in pattern");
    }
    matrixA[0] = 1;
    matrixA[1] = 0;
    matrixA[2] = 0;
    matrixA[3] = 1;
    matrixA[4] = 0;
    matrixA[5] = 0;
    if ((obj1 = resolve((*dict)["Matrix"])).is_array() &&
        obj1.as_array().size() == 6) {
        for (i = 0; i < 6; ++i) {
            if ((obj2 = resolve(obj1[i])).is_num()) {
                matrixA[i] = obj2.as_num();
            }
        }
    }

    pat = new GfxTilingPattern(paintTypeA, tilingTypeA, bboxA, xStepA, yStepA,
                               &resDictA, matrixA, patObjRef);
    return pat;
}

GfxTilingPattern::GfxTilingPattern(int paintTypeA, int tilingTypeA, double *bboxA,
                                   double xStepA, double yStepA, Object *resDictA,
                                   double *matrixA, Object *contentStreamRefA)
    : GfxPattern(1)
{
    int i;

    paintType = paintTypeA;
    tilingType = tilingTypeA;
    for (i = 0; i < 4; ++i) {
        bbox[i] = bboxA[i];
    }
    xStep = xStepA;
    yStep = yStepA;
    resDict = *resDictA;
    for (i = 0; i < 6; ++i) {
        matrix[i] = matrixA[i];
    }
    contentStreamRef = *contentStreamRefA;
}

GfxTilingPattern::~GfxTilingPattern() { }

GfxPattern *GfxTilingPattern::copy()
{
    return new GfxTilingPattern(paintType, tilingType, bbox, xStep, yStep,
                                &resDict, matrix, &contentStreamRef);
}

//------------------------------------------------------------------------
// GfxShadingPattern
//------------------------------------------------------------------------

GfxShadingPattern *GfxShadingPattern::parse(Object *patObj)
{
    Dict *      dict;
    GfxShading *shadingA;
    double      matrixA[6];
    Object      obj1, obj2;
    int         i;

    if (!patObj->is_dict()) {
        return NULL;
    }
    dict = &patObj->as_dict();

    obj1 = resolve((*dict)["Shading"]);
    shadingA = GfxShading::parse(&obj1);
    if (!shadingA) {
        return NULL;
    }

    matrixA[0] = 1;
    matrixA[1] = 0;
    matrixA[2] = 0;
    matrixA[3] = 1;
    matrixA[4] = 0;
    matrixA[5] = 0;
    if ((obj1 = resolve((*dict)["Matrix"])).is_array() &&
        obj1.as_array().size() == 6) {
        for (i = 0; i < 6; ++i) {
            if ((obj2 = resolve(obj1[i])).is_num()) {
                matrixA[i] = obj2.as_num();
            }
        }
    }

    return new GfxShadingPattern(shadingA, matrixA);
}

GfxShadingPattern::GfxShadingPattern(GfxShading *shadingA, double *matrixA)
    : GfxPattern(2)
{
    int i;

    shading = shadingA;
    for (i = 0; i < 6; ++i) {
        matrix[i] = matrixA[i];
    }
}

GfxShadingPattern::~GfxShadingPattern()
{
    delete shading;
}

GfxPattern *GfxShadingPattern::copy()
{
    return new GfxShadingPattern(shading->copy(), matrix);
}

//------------------------------------------------------------------------
// GfxShading
//------------------------------------------------------------------------

GfxShading::GfxShading(int typeA)
{
    type = typeA;
    colorSpace = NULL;
}

GfxShading::GfxShading(GfxShading *shading)
{
    int i;

    type = shading->type;
    colorSpace = shading->colorSpace->copy();
    for (i = 0; i < gfxColorMaxComps; ++i) {
        background.c[i] = shading->background.c[i];
    }
    hasBackground = shading->hasBackground;
    xMin = shading->xMin;
    yMin = shading->yMin;
    xMax = shading->xMax;
    yMax = shading->yMax;
    hasBBox = shading->hasBBox;
}

GfxShading::~GfxShading()
{
    if (colorSpace) {
        delete colorSpace;
    }
}

GfxShading *GfxShading::parse(Object *obj)
{
    GfxShading *shading;
    Dict *      dict;
    int         typeA;
    Object      obj1;

    if (obj->is_dict()) {
        dict = &obj->as_dict();
    } else if (obj->is_stream()) {
        dict = obj->streamGetDict();
    } else {
        return NULL;
    }

    if (!(obj1 = resolve((*dict)["ShadingType"])).is_int()) {
        error(errSyntaxError, -1, "Invalid ShadingType in shading dictionary");
        return NULL;
    }
    typeA = obj1.as_int();

    switch (typeA) {
    case 1:
        shading = GfxFunctionShading::parse(dict);
        break;
    case 2:
        shading = GfxAxialShading::parse(dict);
        break;
    case 3:
        shading = GfxRadialShading::parse(dict);
        break;
    case 4:
        if (obj->is_stream()) {
            shading = GfxGouraudTriangleShading::parse(4, dict, obj->as_stream());
        } else {
            error(errSyntaxError, -1, "Invalid Type 4 shading object");
            goto err1;
        }
        break;
    case 5:
        if (obj->is_stream()) {
            shading = GfxGouraudTriangleShading::parse(5, dict, obj->as_stream());
        } else {
            error(errSyntaxError, -1, "Invalid Type 5 shading object");
            goto err1;
        }
        break;
    case 6:
        if (obj->is_stream()) {
            shading = GfxPatchMeshShading::parse(6, dict, obj->as_stream());
        } else {
            error(errSyntaxError, -1, "Invalid Type 6 shading object");
            goto err1;
        }
        break;
    case 7:
        if (obj->is_stream()) {
            shading = GfxPatchMeshShading::parse(7, dict, obj->as_stream());
        } else {
            error(errSyntaxError, -1, "Invalid Type 7 shading object");
            goto err1;
        }
        break;
    default:
        error(errSyntaxError, -1, "Unknown shading type {0:d}", typeA);
        goto err1;
    }

    return shading;

err1:
    return NULL;
}

bool GfxShading::init(Dict *dict)
{
    Object obj1, obj2;
    int    i;

    obj1 = resolve((*dict)["ColorSpace"]);
    if (!(colorSpace = GfxColorSpace::parse(&obj1))) {
        error(errSyntaxError, -1, "Bad color space in shading dictionary");
        return false;
    }

    for (i = 0; i < gfxColorMaxComps; ++i) {
        background.c[i] = 0;
    }
    hasBackground = false;
    if ((obj1 = resolve((*dict)["Background"])).is_array()) {
        if (obj1.as_array().size() == colorSpace->getNComps()) {
            hasBackground = true;
            for (i = 0; i < colorSpace->getNComps(); ++i) {
                background.c[i] = xpdf::to_color(resolve(obj1[i]).as_num());
            }
        } else {
            error(errSyntaxError, -1, "Bad Background in shading dictionary");
        }
    }

    xMin = yMin = xMax = yMax = 0;
    hasBBox = false;
    if ((obj1 = resolve((*dict)["BBox"])).is_array()) {
        auto n = obj1.as_array().size();

        if (4 == n) {
            hasBBox = true;
            xMin = resolve(obj1[0UL]).as_num();
            yMin = resolve(obj1[1]).as_num();
            xMax = resolve(obj1[2]).as_num();
            yMax = resolve(obj1[3]).as_num();
        } else {
            error(errSyntaxError, -1, "invalid size of shading BBox array: {0:d}",
                  n);
        }
    }

    return true;
}

//------------------------------------------------------------------------
// GfxFunctionShading
//------------------------------------------------------------------------

GfxFunctionShading::GfxFunctionShading(double x0A, double y0A, double x1A,
                                       double y1A, double *matrixA,
                                       Function *funcsA, int nFuncsA)
    : GfxShading(1)
{
    int i;

    x0 = x0A;
    y0 = y0A;
    x1 = x1A;
    y1 = y1A;

    for (i = 0; i < 6; ++i) {
        matrix[i] = matrixA[i];
    }

    nFuncs = nFuncsA;
    ::copy(funcsA, funcsA + nFuncsA, funcs);
}

GfxFunctionShading::GfxFunctionShading(GfxFunctionShading *shading)
    : GfxShading(shading)
{
    int i;

    x0 = shading->x0;
    y0 = shading->y0;
    x1 = shading->x1;
    y1 = shading->y1;
    for (i = 0; i < 6; ++i) {
        matrix[i] = shading->matrix[i];
    }

    nFuncs = shading->nFuncs;
    ::copy(funcs, funcs + nFuncs, shading->funcs);
}

GfxFunctionShading::~GfxFunctionShading() { }

GfxFunctionShading *GfxFunctionShading::parse(Dict *dict)
{
    GfxFunctionShading *shading;
    double              x0A, y0A, x1A, y1A;
    double              matrixA[6];
    Function            funcsA[gfxColorMaxComps];
    int                 nFuncsA;
    Object              obj1, obj2;
    int                 i;

    x0A = y0A = 0;
    x1A = y1A = 1;
    if ((obj1 = resolve((*dict)["Domain"])).is_array() &&
        obj1.as_array().size() == 4) {
        x0A = resolve(obj1[0UL]).as_num();
        x1A = resolve(obj1[1]).as_num();
        y0A = resolve(obj1[2]).as_num();
        y1A = resolve(obj1[3]).as_num();
    }

    matrixA[0] = 1;
    matrixA[1] = 0;
    matrixA[2] = 0;
    matrixA[3] = 1;
    matrixA[4] = 0;
    matrixA[5] = 0;

    if ((obj1 = resolve((*dict)["Matrix"])).is_array() &&
        obj1.as_array().size() == 6) {
        matrixA[0] = resolve(obj1[0UL]).as_num();
        matrixA[1] = resolve(obj1[1]).as_num();
        matrixA[2] = resolve(obj1[2]).as_num();
        matrixA[3] = resolve(obj1[3]).as_num();
        matrixA[4] = resolve(obj1[4]).as_num();
        matrixA[5] = resolve(obj1[5]).as_num();
    }

    obj1 = resolve((*dict)["Function"]);
    if (obj1.is_array()) {
        nFuncsA = obj1.as_array().size();
        if (nFuncsA > gfxColorMaxComps) {
            error(errSyntaxError, -1,
                  "Invalid Function array in shading dictionary");
            goto err1;
        }
        for (i = 0; i < nFuncsA; ++i) {
            obj2 = resolve(obj1[i]);
            if (!(funcsA[i] = xpdf::make_function(obj2))) {
                goto err2;
            }
        }
    } else {
        nFuncsA = 1;
        if (!(funcsA[0] = xpdf::make_function(obj1))) {
            goto err1;
        }
    }

    shading =
        new GfxFunctionShading(x0A, y0A, x1A, y1A, matrixA, funcsA, nFuncsA);
    if (!shading->init(dict)) {
        delete shading;
        return NULL;
    }
    return shading;

err2:
err1:
    return NULL;
}

GfxShading *GfxFunctionShading::copy()
{
    return new GfxFunctionShading(this);
}

void GfxFunctionShading::getColor(double x, double y, GfxColor *color)
{
    double in[2] = { x, y }, out[gfxColorMaxComps] = {};

    // NB: there can be one function with n outputs or n functions with
    // one output each (where n = number of color components)
    for (size_t i = 0; i < nFuncs; ++i) {
        funcs[i](in, in + 2, &out[i]);
    }

    for (size_t i = 0; i < gfxColorMaxComps; ++i) {
        color->c[i] = xpdf::to_color(out[i]);
    }
}

//------------------------------------------------------------------------
// GfxAxialShading
//------------------------------------------------------------------------

GfxAxialShading::GfxAxialShading(double x0A, double y0A, double x1A, double y1A,
                                 double t0A, double t1A, Function *funcsA,
                                 int nFuncsA, bool extend0A, bool extend1A)
    : GfxShading(2)
{
    x0 = x0A;
    y0 = y0A;
    x1 = x1A;
    y1 = y1A;
    t0 = t0A;
    t1 = t1A;

    nFuncs = nFuncsA;
    ::copy(funcsA, funcsA + nFuncsA, funcs);

    extend0 = extend0A;
    extend1 = extend1A;
}

GfxAxialShading::GfxAxialShading(GfxAxialShading *shading)
    : GfxShading(shading)
{
    x0 = shading->x0;
    y0 = shading->y0;
    x1 = shading->x1;
    y1 = shading->y1;
    t0 = shading->t0;
    t1 = shading->t1;

    nFuncs = shading->nFuncs;
    ::copy(shading->funcs, shading->funcs + nFuncs, funcs);

    extend0 = shading->extend0;
    extend1 = shading->extend1;
}

GfxAxialShading::~GfxAxialShading() { }

GfxAxialShading *GfxAxialShading::parse(Dict *dict)
{
    GfxAxialShading *shading;
    double           x0A, y0A, x1A, y1A;
    double           t0A, t1A;
    Function         funcsA[gfxColorMaxComps];
    int              nFuncsA;
    bool             extend0A, extend1A;
    Object           obj1, obj2;
    int              i;

    x0A = y0A = x1A = y1A = 0;
    if ((obj1 = resolve((*dict)["Coords"])).is_array() &&
        obj1.as_array().size() == 4) {
        x0A = resolve(obj1[0UL]).as_num();
        y0A = resolve(obj1[1]).as_num();
        x1A = resolve(obj1[2]).as_num();
        y1A = resolve(obj1[3]).as_num();
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid Coords in shading dictionary");
        goto err1;
    }

    t0A = 0;
    t1A = 1;
    if ((obj1 = resolve((*dict)["Domain"])).is_array() &&
        obj1.as_array().size() == 2) {
        t0A = resolve(obj1[0UL]).as_num();
        t1A = resolve(obj1[1]).as_num();
    }

    obj1 = resolve((*dict)["Function"]);
    if (obj1.is_array()) {
        nFuncsA = obj1.as_array().size();
        if (nFuncsA > gfxColorMaxComps) {
            error(errSyntaxError, -1,
                  "Invalid Function array in shading dictionary");
            goto err1;
        }
        for (i = 0; i < nFuncsA; ++i) {
            obj2 = resolve(obj1[i]);
            if (!(funcsA[i] = xpdf::make_function(obj2))) {
                goto err1;
            }
        }
    } else {
        nFuncsA = 1;
        if (!(funcsA[0] = xpdf::make_function(obj1))) {
            goto err1;
        }
    }

    extend0A = extend1A = false;
    if ((obj1 = resolve((*dict)["Extend"])).is_array() &&
        obj1.as_array().size() == 2) {
        extend0A = resolve(obj1[0UL]).as_bool();
        extend1A = resolve(obj1[1]).as_bool();
    }

    shading = new GfxAxialShading(x0A, y0A, x1A, y1A, t0A, t1A, funcsA, nFuncsA,
                                  extend0A, extend1A);
    if (!shading->init(dict)) {
        delete shading;
        return NULL;
    }
    return shading;

err1:
    return NULL;
}

GfxShading *GfxAxialShading::copy()
{
    return new GfxAxialShading(this);
}

void GfxAxialShading::getColor(double t, GfxColor *color)
{
    double out[gfxColorMaxComps] = {};

    // NB: there can be one function with n outputs or n functions with
    // one output each (where n = number of color components)
    for (size_t i = 0; i < nFuncs; ++i) {
        funcs[i](&t, &t + 1, out + i);
    }

    for (size_t i = 0; i < gfxColorMaxComps; ++i) {
        color->c[i] = xpdf::to_color(out[i]);
    }
}

//------------------------------------------------------------------------
// GfxRadialShading
//------------------------------------------------------------------------

GfxRadialShading::GfxRadialShading(double x0A, double y0A, double r0A, double x1A,
                                   double y1A, double r1A, double t0A, double t1A,
                                   Function *funcsA, int nFuncsA, bool extend0A,
                                   bool extend1A)
    : GfxShading(3)
{
    x0 = x0A;
    y0 = y0A;
    r0 = r0A;
    x1 = x1A;
    y1 = y1A;
    r1 = r1A;
    t0 = t0A;
    t1 = t1A;

    nFuncs = nFuncsA;
    ::copy(funcsA, funcsA + nFuncsA, funcs);

    extend0 = extend0A;
    extend1 = extend1A;
}

GfxRadialShading::GfxRadialShading(GfxRadialShading *shading)
    : GfxShading(shading)
{
    x0 = shading->x0;
    y0 = shading->y0;
    r0 = shading->r0;
    x1 = shading->x1;
    y1 = shading->y1;
    r1 = shading->r1;
    t0 = shading->t0;
    t1 = shading->t1;

    nFuncs = shading->nFuncs;
    ::copy(shading->funcs, shading->funcs + nFuncs, funcs);

    extend0 = shading->extend0;
    extend1 = shading->extend1;
}

GfxRadialShading::~GfxRadialShading() { }

GfxRadialShading *GfxRadialShading::parse(Dict *dict)
{
    GfxRadialShading *shading;
    double            x0A, y0A, r0A, x1A, y1A, r1A;
    double            t0A, t1A;
    Function          funcsA[gfxColorMaxComps];
    int               nFuncsA;
    bool              extend0A, extend1A;
    Object            obj1, obj2;
    int               i;

    x0A = y0A = r0A = x1A = y1A = r1A = 0;
    if ((obj1 = resolve((*dict)["Coords"])).is_array() &&
        obj1.as_array().size() == 6) {
        x0A = resolve(obj1[0UL]).as_num();
        y0A = resolve(obj1[1]).as_num();
        r0A = resolve(obj1[2]).as_num();
        x1A = resolve(obj1[3]).as_num();
        y1A = resolve(obj1[4]).as_num();
        r1A = resolve(obj1[5]).as_num();
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid Coords in shading dictionary");
        goto err1;
    }

    t0A = 0;
    t1A = 1;
    if ((obj1 = resolve((*dict)["Domain"])).is_array() &&
        obj1.as_array().size() == 2) {
        t0A = resolve(obj1[0UL]).as_num();
        t1A = resolve(obj1[1]).as_num();
    }

    obj1 = resolve((*dict)["Function"]);
    if (obj1.is_array()) {
        nFuncsA = obj1.as_array().size();
        if (nFuncsA > gfxColorMaxComps) {
            error(errSyntaxError, -1,
                  "Invalid Function array in shading dictionary");
            goto err1;
        }
        for (i = 0; i < nFuncsA; ++i) {
            obj2 = resolve(obj1[i]);
            if (!(funcsA[i] = xpdf::make_function(obj2))) {
                goto err1;
            }
        }
    } else {
        nFuncsA = 1;
        if (!(funcsA[0] = xpdf::make_function(obj1))) {
            goto err1;
        }
    }

    extend0A = extend1A = false;
    if ((obj1 = resolve((*dict)["Extend"])).is_array() &&
        obj1.as_array().size() == 2) {
        extend0A = resolve(obj1[0UL]).as_bool();
        extend1A = resolve(obj1[1]).as_bool();
    }

    shading = new GfxRadialShading(x0A, y0A, r0A, x1A, y1A, r1A, t0A, t1A, funcsA,
                                   nFuncsA, extend0A, extend1A);
    if (!shading->init(dict)) {
        delete shading;
        return NULL;
    }
    return shading;

err1:
    return NULL;
}

GfxShading *GfxRadialShading::copy()
{
    return new GfxRadialShading(this);
}

void GfxRadialShading::getColor(double t, GfxColor *color)
{
    double out[gfxColorMaxComps] = {};

    // NB: there can be one function with n outputs or n functions with
    // one output each (where n = number of color components)
    for (size_t i = 0; i < nFuncs; ++i) {
        funcs[i](&t, &t + 1, out + i);
    }

    for (size_t i = 0; i < gfxColorMaxComps; ++i) {
        color->c[i] = xpdf::to_color(out[i]);
    }
}

//------------------------------------------------------------------------
// GfxShadingBitBuf
//------------------------------------------------------------------------

class GfxShadingBitBuf
{
public:
    GfxShadingBitBuf(Stream *strA);
    ~GfxShadingBitBuf();
    bool getBits(int n, unsigned *val);
    void flushBits();

private:
    Stream *str;
    int     bitBuf;
    int     nBits;
};

GfxShadingBitBuf::GfxShadingBitBuf(Stream *strA)
{
    str = strA;
    str->reset();
    bitBuf = 0;
    nBits = 0;
}

GfxShadingBitBuf::~GfxShadingBitBuf()
{
    str->close();
}

bool GfxShadingBitBuf::getBits(int n, unsigned *val)
{
    int x;

    if (nBits >= n) {
        x = (bitBuf >> (nBits - n)) & ((1 << n) - 1);
        nBits -= n;
    } else {
        x = 0;
        if (nBits > 0) {
            x = bitBuf & ((1 << nBits) - 1);
            n -= nBits;
            nBits = 0;
        }
        while (n > 0) {
            if ((bitBuf = str->get()) == EOF) {
                nBits = 0;
                return false;
            }
            if (n >= 8) {
                x = (x << 8) | bitBuf;
                n -= 8;
            } else {
                x = (x << n) | (bitBuf >> (8 - n));
                nBits = 8 - n;
                n = 0;
            }
        }
    }
    *val = x;
    return true;
}

void GfxShadingBitBuf::flushBits()
{
    bitBuf = 0;
    nBits = 0;
}

//------------------------------------------------------------------------
// GfxGouraudTriangleShading
//------------------------------------------------------------------------

GfxGouraudTriangleShading::GfxGouraudTriangleShading(
    int typeA, GfxGouraudVertex *verticesA, int nVerticesA, int (*trianglesA)[3],
    int nTrianglesA, int nCompsA, Function *funcsA, int nFuncsA)
    : GfxShading(typeA)
{
    vertices = verticesA;
    nVertices = nVerticesA;
    triangles = trianglesA;
    nTriangles = nTrianglesA;
    nComps = nCompsA;

    nFuncs = nFuncsA;
    ::copy(funcsA, funcsA + nFuncsA, funcs);
}

GfxGouraudTriangleShading::GfxGouraudTriangleShading(
    GfxGouraudTriangleShading *shading)
    : GfxShading(shading)
{
    nVertices = shading->nVertices;

    vertices = (GfxGouraudVertex *)calloc(nVertices, sizeof(GfxGouraudVertex));
    memcpy(vertices, shading->vertices, nVertices * sizeof(GfxGouraudVertex));

    nTriangles = shading->nTriangles;

    triangles = (int(*)[3])calloc(nTriangles * 3, sizeof(int));
    memcpy(triangles, shading->triangles, nTriangles * 3 * sizeof(int));

    nComps = shading->nComps;

    nFuncs = shading->nFuncs;
    ::copy(shading->funcs, shading->funcs + nFuncs, funcs);
}

GfxGouraudTriangleShading::~GfxGouraudTriangleShading()
{
    free(vertices);
    free(triangles);
}

GfxGouraudTriangleShading *GfxGouraudTriangleShading::parse(int typeA, Dict *dict,
                                                            Stream *str)
{
    GfxGouraudTriangleShading *shading;
    Function                   funcsA[gfxColorMaxComps];
    int                        nFuncsA;
    int                        coordBits, compBits, flagBits, vertsPerRow, nRows;
    double                     xMin, xMax, yMin, yMax;
    double                     cMin[gfxColorMaxComps], cMax[gfxColorMaxComps];
    double                     xMul, yMul;
    double                     cMul[gfxColorMaxComps];
    GfxGouraudVertex *         verticesA;
    int(*trianglesA)[3];
    int               nCompsA, nVerticesA, nTrianglesA, vertSize, triSize;
    unsigned          x, y, flag;
    unsigned          c[gfxColorMaxComps];
    GfxShadingBitBuf *bitBuf;
    Object            obj1, obj2;
    int               i, j, k, state;

    if ((obj1 = resolve((*dict)["BitsPerCoordinate"])).is_int()) {
        coordBits = obj1.as_int();
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid BitsPerCoordinate in shading dictionary");
        goto err2;
    }
    if ((obj1 = resolve((*dict)["BitsPerComponent"])).is_int()) {
        compBits = obj1.as_int();
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid BitsPerComponent in shading dictionary");
        goto err2;
    }
    flagBits = vertsPerRow = 0; // make gcc happy
    if (typeA == 4) {
        if ((obj1 = resolve((*dict)["BitsPerFlag"])).is_int()) {
            flagBits = obj1.as_int();
        } else {
            error(errSyntaxError, -1,
                  "Missing or invalid BitsPerFlag in shading dictionary");
            goto err2;
        }
    } else {
        if ((obj1 = resolve((*dict)["VerticesPerRow"])).is_int()) {
            vertsPerRow = obj1.as_int();
        } else {
            error(errSyntaxError, -1,
                  "Missing or invalid VerticesPerRow in shading dictionary");
            goto err2;
        }
    }
    if ((obj1 = resolve((*dict)["Decode"])).is_array() &&
        obj1.as_array().size() >= 6) {
        xMin = resolve(obj1[0UL]).as_num();
        xMax = resolve(obj1[1]).as_num();
        xMul = (xMax - xMin) / (pow(2.0, coordBits) - 1);
        yMin = resolve(obj1[2]).as_num();
        yMax = resolve(obj1[3]).as_num();
        yMul = (yMax - yMin) / (pow(2.0, coordBits) - 1);
        for (i = 0; 5 + 2 * i < obj1.as_array().size() && i < gfxColorMaxComps;
             ++i) {
            cMin[i] = resolve(obj1[4 + 2 * i]).as_num();
            cMax[i] = resolve(obj1[5 + 2 * i]).as_num();
            cMul[i] = (cMax[i] - cMin[i]) / (double)((1 << compBits) - 1);
        }
        nCompsA = i;
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid Decode array in shading dictionary");
        goto err2;
    }

    if (!(obj1 = resolve((*dict)["Function"])).is_null()) {
        if (obj1.is_array()) {
            nFuncsA = obj1.as_array().size();
            if (nFuncsA > gfxColorMaxComps) {
                error(errSyntaxError, -1,
                      "Invalid Function array in shading dictionary");
                goto err1;
            }
            for (i = 0; i < nFuncsA; ++i) {
                obj2 = resolve(obj1[i]);
                if (!(funcsA[i] = xpdf::make_function(obj2))) {
                    goto err1;
                }
            }
        } else {
            nFuncsA = 1;
            if (!(funcsA[0] = xpdf::make_function(obj1))) {
                goto err1;
            }
        }
    } else {
        nFuncsA = 0;
    }

    nVerticesA = nTrianglesA = 0;
    verticesA = NULL;
    trianglesA = NULL;
    vertSize = triSize = 0;
    state = 0;
    flag = 0; // make gcc happy
    bitBuf = new GfxShadingBitBuf(str);
    while (1) {
        if (typeA == 4) {
            if (!bitBuf->getBits(flagBits, &flag)) {
                break;
            }
        }
        if (!bitBuf->getBits(coordBits, &x) || !bitBuf->getBits(coordBits, &y)) {
            break;
        }
        for (i = 0; i < nCompsA; ++i) {
            if (!bitBuf->getBits(compBits, &c[i])) {
                break;
            }
        }
        if (i < nCompsA) {
            break;
        }
        if (nVerticesA == vertSize) {
            vertSize = (vertSize == 0) ? 16 : 2 * vertSize;
            verticesA = (GfxGouraudVertex *)reallocarray(
                verticesA, vertSize, sizeof(GfxGouraudVertex));
        }
        verticesA[nVerticesA].x = xMin + xMul * (double)x;
        verticesA[nVerticesA].y = yMin + yMul * (double)y;
        for (i = 0; i < nCompsA; ++i) {
            verticesA[nVerticesA].color[i] = cMin[i] + cMul[i] * (double)c[i];
        }
        ++nVerticesA;
        bitBuf->flushBits();
        if (typeA == 4) {
            if (state == 0 || state == 1) {
                ++state;
            } else if (state == 2 || flag > 0) {
                if (nTrianglesA == triSize) {
                    triSize = (triSize == 0) ? 16 : 2 * triSize;
                    trianglesA = (int(*)[3])reallocarray(trianglesA, triSize * 3,
                                                         sizeof(int));
                }
                if (state == 2) {
                    trianglesA[nTrianglesA][0] = nVerticesA - 3;
                    trianglesA[nTrianglesA][1] = nVerticesA - 2;
                    trianglesA[nTrianglesA][2] = nVerticesA - 1;
                    ++state;
                } else if (flag == 1) {
                    trianglesA[nTrianglesA][0] = trianglesA[nTrianglesA - 1][1];
                    trianglesA[nTrianglesA][1] = trianglesA[nTrianglesA - 1][2];
                    trianglesA[nTrianglesA][2] = nVerticesA - 1;
                } else { // flag == 2
                    trianglesA[nTrianglesA][0] = trianglesA[nTrianglesA - 1][0];
                    trianglesA[nTrianglesA][1] = trianglesA[nTrianglesA - 1][2];
                    trianglesA[nTrianglesA][2] = nVerticesA - 1;
                }
                ++nTrianglesA;
            } else { // state == 3 && flag == 0
                state = 1;
            }
        }
    }
    delete bitBuf;
    if (typeA == 5) {
        nRows = nVerticesA / vertsPerRow;
        nTrianglesA = (nRows - 1) * 2 * (vertsPerRow - 1);
        trianglesA = (int(*)[3])calloc(nTrianglesA * 3, sizeof(int));
        k = 0;
        for (i = 0; i < nRows - 1; ++i) {
            for (j = 0; j < vertsPerRow - 1; ++j) {
                trianglesA[k][0] = i * vertsPerRow + j;
                trianglesA[k][1] = i * vertsPerRow + j + 1;
                trianglesA[k][2] = (i + 1) * vertsPerRow + j;
                ++k;
                trianglesA[k][0] = i * vertsPerRow + j + 1;
                trianglesA[k][1] = (i + 1) * vertsPerRow + j;
                trianglesA[k][2] = (i + 1) * vertsPerRow + j + 1;
                ++k;
            }
        }
    }

    shading = new GfxGouraudTriangleShading(typeA, verticesA, nVerticesA,
                                            trianglesA, nTrianglesA, nCompsA,
                                            funcsA, nFuncsA);
    if (!shading->init(dict)) {
        delete shading;
        return NULL;
    }
    return shading;

err2:
err1:
    return NULL;
}

GfxShading *GfxGouraudTriangleShading::copy()
{
    return new GfxGouraudTriangleShading(this);
}

void GfxGouraudTriangleShading::getTriangle(int i, double *x0, double *y0,
                                            double *color0, double *x1,
                                            double *y1, double *color1,
                                            double *x2, double *y2,
                                            double *color2)
{
    int v, j;

    v = triangles[i][0];
    *x0 = vertices[v].x;
    *y0 = vertices[v].y;
    for (j = 0; j < nComps; ++j) {
        color0[j] = vertices[v].color[j];
    }
    v = triangles[i][1];
    *x1 = vertices[v].x;
    *y1 = vertices[v].y;
    for (j = 0; j < nComps; ++j) {
        color1[j] = vertices[v].color[j];
    }
    v = triangles[i][2];
    *x2 = vertices[v].x;
    *y2 = vertices[v].y;
    for (j = 0; j < nComps; ++j) {
        color2[j] = vertices[v].color[j];
    }
}

void GfxGouraudTriangleShading::getColor(const double *p, const double *pend,
                                         GfxColor *out)
{
    double c[gfxColorMaxComps] = {};

    if (nFuncs > 0) {
        for (size_t i = 0; i < nFuncs; ++i)
            funcs[i](p, pend, &c[i]);

        for (size_t i = 0; i < colorSpace->getNComps(); ++i)
            out->c[i] = xpdf::to_color(c[i]);
    } else {
        for (size_t i = 0; i < nComps; ++i)
            out->c[i] = xpdf::to_color(p[i]);
    }
}

//------------------------------------------------------------------------
// GfxPatchMeshShading
//------------------------------------------------------------------------

GfxPatchMeshShading::GfxPatchMeshShading(int typeA, GfxPatch *patchesA,
                                         int nPatchesA, int nCompsA,
                                         Function *funcsA, int nFuncsA)
    : GfxShading(typeA)
{
    patches = patchesA;
    nPatches = nPatchesA;
    nComps = nCompsA;

    nFuncs = nFuncsA;
    ::copy(funcsA, funcsA + nFuncsA, funcs);
}

GfxPatchMeshShading::GfxPatchMeshShading(GfxPatchMeshShading *shading)
    : GfxShading(shading)
{
    nPatches = shading->nPatches;
    patches = (GfxPatch *)calloc(nPatches, sizeof(GfxPatch));
    memcpy(patches, shading->patches, nPatches * sizeof(GfxPatch));
    nComps = shading->nComps;

    nFuncs = shading->nFuncs;
    ::copy(shading->funcs, shading->funcs + nFuncs, funcs);
}

GfxPatchMeshShading::~GfxPatchMeshShading()
{
    free(patches);
}

GfxPatchMeshShading *GfxPatchMeshShading::parse(int typeA, Dict *dict,
                                                Stream *str)
{
    GfxPatchMeshShading *shading;
    Function             funcsA[gfxColorMaxComps];
    int                  nFuncsA;
    int                  coordBits, compBits, flagBits;
    double               xMin, xMax, yMin, yMax;
    double               cMin[gfxColorMaxComps], cMax[gfxColorMaxComps];
    double               xMul, yMul;
    double               cMul[gfxColorMaxComps];
    GfxPatch *           patchesA, *p;
    int                  nCompsA, nPatchesA, patchesSize, nPts, nColors;
    unsigned             flag;
    double               x[16], y[16];
    unsigned             xi, yi;
    double               c[4][gfxColorMaxComps];
    unsigned             ci;
    GfxShadingBitBuf *   bitBuf;
    Object               obj1, obj2;
    int                  i, j;

    if ((obj1 = resolve((*dict)["BitsPerCoordinate"])).is_int()) {
        coordBits = obj1.as_int();
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid BitsPerCoordinate in shading dictionary");
        goto err2;
    }
    if ((obj1 = resolve((*dict)["BitsPerComponent"])).is_int()) {
        compBits = obj1.as_int();
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid BitsPerComponent in shading dictionary");
        goto err2;
    }
    if ((obj1 = resolve((*dict)["BitsPerFlag"])).is_int()) {
        flagBits = obj1.as_int();
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid BitsPerFlag in shading dictionary");
        goto err2;
    }
    if ((obj1 = resolve((*dict)["Decode"])).is_array() &&
        obj1.as_array().size() >= 6) {
        xMin = resolve(obj1[0UL]).as_num();
        xMax = resolve(obj1[1]).as_num();
        xMul = (xMax - xMin) / (pow(2.0, coordBits) - 1);
        yMin = resolve(obj1[2]).as_num();
        yMax = resolve(obj1[3]).as_num();
        yMul = (yMax - yMin) / (pow(2.0, coordBits) - 1);
        for (i = 0; 5 + 2 * i < obj1.as_array().size() && i < gfxColorMaxComps;
             ++i) {
            cMin[i] = resolve(obj1[4 + 2 * i]).as_num();
            cMax[i] = resolve(obj1[5 + 2 * i]).as_num();
            cMul[i] = (cMax[i] - cMin[i]) / (double)((1 << compBits) - 1);
        }
        nCompsA = i;
    } else {
        error(errSyntaxError, -1,
              "Missing or invalid Decode array in shading dictionary");
        goto err2;
    }

    if (!(obj1 = resolve((*dict)["Function"])).is_null()) {
        if (obj1.is_array()) {
            nFuncsA = obj1.as_array().size();
            if (nFuncsA > gfxColorMaxComps) {
                error(errSyntaxError, -1,
                      "Invalid Function array in shading dictionary");
                goto err1;
            }
            for (i = 0; i < nFuncsA; ++i) {
                obj2 = resolve(obj1[i]);
                if (!(funcsA[i] = xpdf::make_function(obj2))) {
                    goto err1;
                }
            }
        } else {
            nFuncsA = 1;
            if (!(funcsA[0] = xpdf::make_function(obj1))) {
                goto err1;
            }
        }
    } else {
        nFuncsA = 0;
    }

    nPatchesA = 0;
    patchesA = NULL;
    patchesSize = 0;
    bitBuf = new GfxShadingBitBuf(str);
    while (1) {
        if (!bitBuf->getBits(flagBits, &flag)) {
            break;
        }
        if (typeA == 6) {
            switch (flag) {
            case 0:
                nPts = 12;
                nColors = 4;
                break;
            case 1:
            case 2:
            case 3:
            default:
                nPts = 8;
                nColors = 2;
                break;
            }
        } else {
            switch (flag) {
            case 0:
                nPts = 16;
                nColors = 4;
                break;
            case 1:
            case 2:
            case 3:
            default:
                nPts = 12;
                nColors = 2;
                break;
            }
        }
        for (i = 0; i < nPts; ++i) {
            if (!bitBuf->getBits(coordBits, &xi) ||
                !bitBuf->getBits(coordBits, &yi)) {
                break;
            }
            x[i] = xMin + xMul * (double)xi;
            y[i] = yMin + yMul * (double)yi;
        }
        if (i < nPts) {
            break;
        }
        for (i = 0; i < nColors; ++i) {
            for (j = 0; j < nCompsA; ++j) {
                if (!bitBuf->getBits(compBits, &ci)) {
                    break;
                }
                c[i][j] = cMin[j] + cMul[j] * (double)ci;
            }
            if (j < nCompsA) {
                break;
            }
        }
        if (i < nColors) {
            break;
        }
        if (nPatchesA == patchesSize) {
            patchesSize = (patchesSize == 0) ? 16 : 2 * patchesSize;
            patchesA =
                (GfxPatch *)reallocarray(patchesA, patchesSize, sizeof(GfxPatch));
        }
        p = &patchesA[nPatchesA];
        if (typeA == 6) {
            switch (flag) {
            case 0:
                p->x[0][0] = x[0];
                p->y[0][0] = y[0];
                p->x[0][1] = x[1];
                p->y[0][1] = y[1];
                p->x[0][2] = x[2];
                p->y[0][2] = y[2];
                p->x[0][3] = x[3];
                p->y[0][3] = y[3];
                p->x[1][3] = x[4];
                p->y[1][3] = y[4];
                p->x[2][3] = x[5];
                p->y[2][3] = y[5];
                p->x[3][3] = x[6];
                p->y[3][3] = y[6];
                p->x[3][2] = x[7];
                p->y[3][2] = y[7];
                p->x[3][1] = x[8];
                p->y[3][1] = y[8];
                p->x[3][0] = x[9];
                p->y[3][0] = y[9];
                p->x[2][0] = x[10];
                p->y[2][0] = y[10];
                p->x[1][0] = x[11];
                p->y[1][0] = y[11];
                for (j = 0; j < nCompsA; ++j) {
                    p->color[0][0][j] = c[0][j];
                    p->color[0][1][j] = c[1][j];
                    p->color[1][1][j] = c[2][j];
                    p->color[1][0][j] = c[3][j];
                }
                break;
            case 1:
                p->x[0][0] = patchesA[nPatchesA - 1].x[0][3];
                p->y[0][0] = patchesA[nPatchesA - 1].y[0][3];
                p->x[0][1] = patchesA[nPatchesA - 1].x[1][3];
                p->y[0][1] = patchesA[nPatchesA - 1].y[1][3];
                p->x[0][2] = patchesA[nPatchesA - 1].x[2][3];
                p->y[0][2] = patchesA[nPatchesA - 1].y[2][3];
                p->x[0][3] = patchesA[nPatchesA - 1].x[3][3];
                p->y[0][3] = patchesA[nPatchesA - 1].y[3][3];
                p->x[1][3] = x[0];
                p->y[1][3] = y[0];
                p->x[2][3] = x[1];
                p->y[2][3] = y[1];
                p->x[3][3] = x[2];
                p->y[3][3] = y[2];
                p->x[3][2] = x[3];
                p->y[3][2] = y[3];
                p->x[3][1] = x[4];
                p->y[3][1] = y[4];
                p->x[3][0] = x[5];
                p->y[3][0] = y[5];
                p->x[2][0] = x[6];
                p->y[2][0] = y[6];
                p->x[1][0] = x[7];
                p->y[1][0] = y[7];
                for (j = 0; j < nCompsA; ++j) {
                    p->color[0][0][j] = patchesA[nPatchesA - 1].color[0][1][j];
                    p->color[0][1][j] = patchesA[nPatchesA - 1].color[1][1][j];
                    p->color[1][1][j] = c[0][j];
                    p->color[1][0][j] = c[1][j];
                }
                break;
            case 2:
                p->x[0][0] = patchesA[nPatchesA - 1].x[3][3];
                p->y[0][0] = patchesA[nPatchesA - 1].y[3][3];
                p->x[0][1] = patchesA[nPatchesA - 1].x[3][2];
                p->y[0][1] = patchesA[nPatchesA - 1].y[3][2];
                p->x[0][2] = patchesA[nPatchesA - 1].x[3][1];
                p->y[0][2] = patchesA[nPatchesA - 1].y[3][1];
                p->x[0][3] = patchesA[nPatchesA - 1].x[3][0];
                p->y[0][3] = patchesA[nPatchesA - 1].y[3][0];
                p->x[1][3] = x[0];
                p->y[1][3] = y[0];
                p->x[2][3] = x[1];
                p->y[2][3] = y[1];
                p->x[3][3] = x[2];
                p->y[3][3] = y[2];
                p->x[3][2] = x[3];
                p->y[3][2] = y[3];
                p->x[3][1] = x[4];
                p->y[3][1] = y[4];
                p->x[3][0] = x[5];
                p->y[3][0] = y[5];
                p->x[2][0] = x[6];
                p->y[2][0] = y[6];
                p->x[1][0] = x[7];
                p->y[1][0] = y[7];
                for (j = 0; j < nCompsA; ++j) {
                    p->color[0][0][j] = patchesA[nPatchesA - 1].color[1][1][j];
                    p->color[0][1][j] = patchesA[nPatchesA - 1].color[1][0][j];
                    p->color[1][1][j] = c[0][j];
                    p->color[1][0][j] = c[1][j];
                }
                break;
            case 3:
                p->x[0][0] = patchesA[nPatchesA - 1].x[3][0];
                p->y[0][0] = patchesA[nPatchesA - 1].y[3][0];
                p->x[0][1] = patchesA[nPatchesA - 1].x[2][0];
                p->y[0][1] = patchesA[nPatchesA - 1].y[2][0];
                p->x[0][2] = patchesA[nPatchesA - 1].x[1][0];
                p->y[0][2] = patchesA[nPatchesA - 1].y[1][0];
                p->x[0][3] = patchesA[nPatchesA - 1].x[0][0];
                p->y[0][3] = patchesA[nPatchesA - 1].y[0][0];
                p->x[1][3] = x[0];
                p->y[1][3] = y[0];
                p->x[2][3] = x[1];
                p->y[2][3] = y[1];
                p->x[3][3] = x[2];
                p->y[3][3] = y[2];
                p->x[3][2] = x[3];
                p->y[3][2] = y[3];
                p->x[3][1] = x[4];
                p->y[3][1] = y[4];
                p->x[3][0] = x[5];
                p->y[3][0] = y[5];
                p->x[2][0] = x[6];
                p->y[2][0] = y[6];
                p->x[1][0] = x[7];
                p->y[1][0] = y[7];
                for (j = 0; j < nCompsA; ++j) {
                    p->color[0][1][j] = patchesA[nPatchesA - 1].color[1][0][j];
                    p->color[0][1][j] = patchesA[nPatchesA - 1].color[0][0][j];
                    p->color[1][1][j] = c[0][j];
                    p->color[1][0][j] = c[1][j];
                }
                break;
            }
        } else {
            switch (flag) {
            case 0:
                p->x[0][0] = x[0];
                p->y[0][0] = y[0];
                p->x[0][1] = x[1];
                p->y[0][1] = y[1];
                p->x[0][2] = x[2];
                p->y[0][2] = y[2];
                p->x[0][3] = x[3];
                p->y[0][3] = y[3];
                p->x[1][3] = x[4];
                p->y[1][3] = y[4];
                p->x[2][3] = x[5];
                p->y[2][3] = y[5];
                p->x[3][3] = x[6];
                p->y[3][3] = y[6];
                p->x[3][2] = x[7];
                p->y[3][2] = y[7];
                p->x[3][1] = x[8];
                p->y[3][1] = y[8];
                p->x[3][0] = x[9];
                p->y[3][0] = y[9];
                p->x[2][0] = x[10];
                p->y[2][0] = y[10];
                p->x[1][0] = x[11];
                p->y[1][0] = y[11];
                p->x[1][1] = x[12];
                p->y[1][1] = y[12];
                p->x[1][2] = x[13];
                p->y[1][2] = y[13];
                p->x[2][2] = x[14];
                p->y[2][2] = y[14];
                p->x[2][1] = x[15];
                p->y[2][1] = y[15];
                for (j = 0; j < nCompsA; ++j) {
                    p->color[0][0][j] = c[0][j];
                    p->color[0][1][j] = c[1][j];
                    p->color[1][1][j] = c[2][j];
                    p->color[1][0][j] = c[3][j];
                }
                break;
            case 1:
                p->x[0][0] = patchesA[nPatchesA - 1].x[0][3];
                p->y[0][0] = patchesA[nPatchesA - 1].y[0][3];
                p->x[0][1] = patchesA[nPatchesA - 1].x[1][3];
                p->y[0][1] = patchesA[nPatchesA - 1].y[1][3];
                p->x[0][2] = patchesA[nPatchesA - 1].x[2][3];
                p->y[0][2] = patchesA[nPatchesA - 1].y[2][3];
                p->x[0][3] = patchesA[nPatchesA - 1].x[3][3];
                p->y[0][3] = patchesA[nPatchesA - 1].y[3][3];
                p->x[1][3] = x[0];
                p->y[1][3] = y[0];
                p->x[2][3] = x[1];
                p->y[2][3] = y[1];
                p->x[3][3] = x[2];
                p->y[3][3] = y[2];
                p->x[3][2] = x[3];
                p->y[3][2] = y[3];
                p->x[3][1] = x[4];
                p->y[3][1] = y[4];
                p->x[3][0] = x[5];
                p->y[3][0] = y[5];
                p->x[2][0] = x[6];
                p->y[2][0] = y[6];
                p->x[1][0] = x[7];
                p->y[1][0] = y[7];
                p->x[1][1] = x[8];
                p->y[1][1] = y[8];
                p->x[1][2] = x[9];
                p->y[1][2] = y[9];
                p->x[2][2] = x[10];
                p->y[2][2] = y[10];
                p->x[2][1] = x[11];
                p->y[2][1] = y[11];
                for (j = 0; j < nCompsA; ++j) {
                    p->color[0][0][j] = patchesA[nPatchesA - 1].color[0][1][j];
                    p->color[0][1][j] = patchesA[nPatchesA - 1].color[1][1][j];
                    p->color[1][1][j] = c[0][j];
                    p->color[1][0][j] = c[1][j];
                }
                break;
            case 2:
                p->x[0][0] = patchesA[nPatchesA - 1].x[3][3];
                p->y[0][0] = patchesA[nPatchesA - 1].y[3][3];
                p->x[0][1] = patchesA[nPatchesA - 1].x[3][2];
                p->y[0][1] = patchesA[nPatchesA - 1].y[3][2];
                p->x[0][2] = patchesA[nPatchesA - 1].x[3][1];
                p->y[0][2] = patchesA[nPatchesA - 1].y[3][1];
                p->x[0][3] = patchesA[nPatchesA - 1].x[3][0];
                p->y[0][3] = patchesA[nPatchesA - 1].y[3][0];
                p->x[1][3] = x[0];
                p->y[1][3] = y[0];
                p->x[2][3] = x[1];
                p->y[2][3] = y[1];
                p->x[3][3] = x[2];
                p->y[3][3] = y[2];
                p->x[3][2] = x[3];
                p->y[3][2] = y[3];
                p->x[3][1] = x[4];
                p->y[3][1] = y[4];
                p->x[3][0] = x[5];
                p->y[3][0] = y[5];
                p->x[2][0] = x[6];
                p->y[2][0] = y[6];
                p->x[1][0] = x[7];
                p->y[1][0] = y[7];
                p->x[1][1] = x[8];
                p->y[1][1] = y[8];
                p->x[1][2] = x[9];
                p->y[1][2] = y[9];
                p->x[2][2] = x[10];
                p->y[2][2] = y[10];
                p->x[2][1] = x[11];
                p->y[2][1] = y[11];
                for (j = 0; j < nCompsA; ++j) {
                    p->color[0][0][j] = patchesA[nPatchesA - 1].color[1][1][j];
                    p->color[0][1][j] = patchesA[nPatchesA - 1].color[1][0][j];
                    p->color[1][1][j] = c[0][j];
                    p->color[1][0][j] = c[1][j];
                }
                break;
            case 3:
                p->x[0][0] = patchesA[nPatchesA - 1].x[3][0];
                p->y[0][0] = patchesA[nPatchesA - 1].y[3][0];
                p->x[0][1] = patchesA[nPatchesA - 1].x[2][0];
                p->y[0][1] = patchesA[nPatchesA - 1].y[2][0];
                p->x[0][2] = patchesA[nPatchesA - 1].x[1][0];
                p->y[0][2] = patchesA[nPatchesA - 1].y[1][0];
                p->x[0][3] = patchesA[nPatchesA - 1].x[0][0];
                p->y[0][3] = patchesA[nPatchesA - 1].y[0][0];
                p->x[1][3] = x[0];
                p->y[1][3] = y[0];
                p->x[2][3] = x[1];
                p->y[2][3] = y[1];
                p->x[3][3] = x[2];
                p->y[3][3] = y[2];
                p->x[3][2] = x[3];
                p->y[3][2] = y[3];
                p->x[3][1] = x[4];
                p->y[3][1] = y[4];
                p->x[3][0] = x[5];
                p->y[3][0] = y[5];
                p->x[2][0] = x[6];
                p->y[2][0] = y[6];
                p->x[1][0] = x[7];
                p->y[1][0] = y[7];
                p->x[1][1] = x[8];
                p->y[1][1] = y[8];
                p->x[1][2] = x[9];
                p->y[1][2] = y[9];
                p->x[2][2] = x[10];
                p->y[2][2] = y[10];
                p->x[2][1] = x[11];
                p->y[2][1] = y[11];
                for (j = 0; j < nCompsA; ++j) {
                    p->color[0][0][j] = patchesA[nPatchesA - 1].color[1][0][j];
                    p->color[0][1][j] = patchesA[nPatchesA - 1].color[0][0][j];
                    p->color[1][1][j] = c[0][j];
                    p->color[1][0][j] = c[1][j];
                }
                break;
            }
        }
        ++nPatchesA;
        bitBuf->flushBits();
    }
    delete bitBuf;

    if (typeA == 6) {
        for (i = 0; i < nPatchesA; ++i) {
            p = &patchesA[i];
            p->x[1][1] = (-4 * p->x[0][0] + 6 * (p->x[0][1] + p->x[1][0]) -
                          2 * (p->x[0][3] + p->x[3][0]) +
                          3 * (p->x[3][1] + p->x[1][3]) - p->x[3][3]) /
                         9;
            p->y[1][1] = (-4 * p->y[0][0] + 6 * (p->y[0][1] + p->y[1][0]) -
                          2 * (p->y[0][3] + p->y[3][0]) +
                          3 * (p->y[3][1] + p->y[1][3]) - p->y[3][3]) /
                         9;
            p->x[1][2] = (-4 * p->x[0][3] + 6 * (p->x[0][2] + p->x[1][3]) -
                          2 * (p->x[0][0] + p->x[3][3]) +
                          3 * (p->x[3][2] + p->x[1][0]) - p->x[3][0]) /
                         9;
            p->y[1][2] = (-4 * p->y[0][3] + 6 * (p->y[0][2] + p->y[1][3]) -
                          2 * (p->y[0][0] + p->y[3][3]) +
                          3 * (p->y[3][2] + p->y[1][0]) - p->y[3][0]) /
                         9;
            p->x[2][1] = (-4 * p->x[3][0] + 6 * (p->x[3][1] + p->x[2][0]) -
                          2 * (p->x[3][3] + p->x[0][0]) +
                          3 * (p->x[0][1] + p->x[2][3]) - p->x[0][3]) /
                         9;
            p->y[2][1] = (-4 * p->y[3][0] + 6 * (p->y[3][1] + p->y[2][0]) -
                          2 * (p->y[3][3] + p->y[0][0]) +
                          3 * (p->y[0][1] + p->y[2][3]) - p->y[0][3]) /
                         9;
            p->x[2][2] = (-4 * p->x[3][3] + 6 * (p->x[3][2] + p->x[2][3]) -
                          2 * (p->x[3][0] + p->x[0][3]) +
                          3 * (p->x[0][2] + p->x[2][0]) - p->x[0][0]) /
                         9;
            p->y[2][2] = (-4 * p->y[3][3] + 6 * (p->y[3][2] + p->y[2][3]) -
                          2 * (p->y[3][0] + p->y[0][3]) +
                          3 * (p->y[0][2] + p->y[2][0]) - p->y[0][0]) /
                         9;
        }
    }

    shading = new GfxPatchMeshShading(typeA, patchesA, nPatchesA, nCompsA, funcsA,
                                      nFuncsA);
    if (!shading->init(dict)) {
        delete shading;
        return NULL;
    }
    return shading;

err2:
err1:
    return NULL;
}

GfxShading *GfxPatchMeshShading::copy()
{
    return new GfxPatchMeshShading(this);
}

void GfxPatchMeshShading::getColor(const double *p, const double *pend,
                                   GfxColor *out)
{
    double c[gfxColorMaxComps] = {};

    if (nFuncs > 0) {
        for (size_t i = 0; i < nFuncs; ++i)
            funcs[i](p, pend, &c[i]);

        for (size_t i = 0; i < colorSpace->getNComps(); ++i)
            out->c[i] = xpdf::to_color(c[i]);
    } else {
        for (size_t i = 0; i < nComps; ++i)
            out->c[i] = xpdf::to_color(p[i]);
    }
}

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

GfxImageColorMap::GfxImageColorMap(int bitsA, Object *decode,
                                   GfxColorSpace *colorSpaceA)
{
    GfxIndexedColorSpace *   indexedCS;
    GfxSeparationColorSpace *sepCS;
    int                      maxPixel, indexHigh;
    unsigned char *          indexedLookup;
    Function                 sepFunc;
    Object                   obj;
    double                   x[gfxColorMaxComps];
    double                   y[gfxColorMaxComps];
    int                      i, j, k;

    ok = true;

    // bits per component and color space
    bits = bitsA;
    if (bits <= 8) {
        maxPixel = (1 << bits) - 1;
    } else {
        maxPixel = 0xff;
    }
    colorSpace = colorSpaceA;

    // initialize
    for (k = 0; k < gfxColorMaxComps; ++k) {
        lookup[k] = NULL;
        lookup2[k] = NULL;
    }

    // get decode map
    if (decode->is_null()) {
        nComps = colorSpace->getNComps();
        colorSpace->getDefaultRanges(decodeLow, decodeRange, maxPixel);
    } else if (decode->is_array()) {
        nComps = decode->as_array().size() / 2;
        if (nComps < colorSpace->getNComps()) {
            goto err1;
        }
        if (nComps > colorSpace->getNComps()) {
            error(errSyntaxWarning, -1, "Too many elements in Decode array");
            nComps = colorSpace->getNComps();
        }
        for (i = 0; i < nComps; ++i) {
            obj = resolve((*decode)[2 * i]);
            if (!obj.is_num()) {
                goto err2;
            }
            decodeLow[i] = obj.as_num();
            obj = resolve((*decode)[2 * i + 1]);
            if (!obj.is_num()) {
                goto err2;
            }
            decodeRange[i] = obj.as_num() - decodeLow[i];
        }
    } else {
        goto err1;
    }

    // Construct a lookup table -- this stores pre-computed decoded
    // values for each component, i.e., the result of applying the
    // decode mapping to each possible image pixel component value.
    for (k = 0; k < nComps; ++k) {
        lookup[k] = (xpdf::color_t *)calloc(maxPixel + 1, sizeof(xpdf::color_t));
        for (i = 0; i <= maxPixel; ++i) {
            lookup[k][i] =
                xpdf::to_color(decodeLow[k] + (i * decodeRange[k]) / maxPixel);
        }
    }

    // Optimization: for Indexed and Separation color spaces (which have
    // only one component), we pre-compute a second lookup table with
    // color values
    colorSpace2 = NULL;
    nComps2 = 0;
    if (colorSpace->getMode() == csIndexed) {
        // Note that indexHigh may not be the same as maxPixel --
        // Distiller will remove unused palette entries, resulting in
        // indexHigh < maxPixel.
        indexedCS = (GfxIndexedColorSpace *)colorSpace;
        colorSpace2 = indexedCS->getBase();
        indexHigh = indexedCS->getIndexHigh();
        nComps2 = colorSpace2->getNComps();
        indexedLookup = indexedCS->getLookup();
        colorSpace2->getDefaultRanges(x, y, indexHigh);
        for (k = 0; k < nComps2; ++k) {
            lookup2[k] =
                (xpdf::color_t *)calloc(maxPixel + 1, sizeof(xpdf::color_t));
        }
        for (i = 0; i <= maxPixel; ++i) {
            j = (int)(decodeLow[0] + (i * decodeRange[0]) / maxPixel + 0.5);
            if (j < 0) {
                j = 0;
            } else if (j > indexHigh) {
                j = indexHigh;
            }
            for (k = 0; k < nComps2; ++k) {
                lookup2[k][i] = xpdf::to_color(
                    x[k] + (indexedLookup[j * nComps2 + k] / 255.0) * y[k]);
            }
        }
    } else if (colorSpace->getMode() == csSeparation) {
        sepCS = (GfxSeparationColorSpace *)colorSpace;
        colorSpace2 = sepCS->getAlt();
        nComps2 = colorSpace2->getNComps();
        sepFunc = sepCS->getFunc();
        for (k = 0; k < nComps2; ++k) {
            lookup2[k] =
                (xpdf::color_t *)calloc(maxPixel + 1, sizeof(xpdf::color_t));
        }
        for (i = 0; i <= maxPixel; ++i) {
            x[0] = decodeLow[0] + (i * decodeRange[0]) / maxPixel;
            sepFunc(x, x + 1, y);
            for (k = 0; k < nComps2; ++k) {
                lookup2[k][i] = xpdf::to_color(y[k]);
            }
        }
    }

    return;

err2:
err1:
    ok = false;
}

GfxImageColorMap::GfxImageColorMap(GfxImageColorMap *colorMap)
{
    int n, i, k;

    colorSpace = colorMap->colorSpace->copy();
    bits = colorMap->bits;
    nComps = colorMap->nComps;
    nComps2 = colorMap->nComps2;
    colorSpace2 = NULL;
    for (k = 0; k < gfxColorMaxComps; ++k) {
        lookup[k] = NULL;
        lookup2[k] = NULL;
    }
    if (bits <= 8) {
        n = 1 << bits;
    } else {
        n = 256;
    }
    for (k = 0; k < nComps; ++k) {
        lookup[k] = (xpdf::color_t *)calloc(n, sizeof(xpdf::color_t));
        memcpy(lookup[k], colorMap->lookup[k], n * sizeof(xpdf::color_t));
    }
    if (colorSpace->getMode() == csIndexed) {
        colorSpace2 = ((GfxIndexedColorSpace *)colorSpace)->getBase();
        for (k = 0; k < nComps2; ++k) {
            lookup2[k] = (xpdf::color_t *)calloc(n, sizeof(xpdf::color_t));
            memcpy(lookup2[k], colorMap->lookup2[k], n * sizeof(xpdf::color_t));
        }
    } else if (colorSpace->getMode() == csSeparation) {
        colorSpace2 = ((GfxSeparationColorSpace *)colorSpace)->getAlt();
        for (k = 0; k < nComps2; ++k) {
            lookup2[k] = (xpdf::color_t *)calloc(n, sizeof(xpdf::color_t));
            memcpy(lookup2[k], colorMap->lookup2[k], n * sizeof(xpdf::color_t));
        }
    }
    for (i = 0; i < nComps; ++i) {
        decodeLow[i] = colorMap->decodeLow[i];
        decodeRange[i] = colorMap->decodeRange[i];
    }
    ok = true;
}

GfxImageColorMap::~GfxImageColorMap()
{
    int i;

    delete colorSpace;
    for (i = 0; i < gfxColorMaxComps; ++i) {
        free(lookup[i]);
        free(lookup2[i]);
    }
}

void GfxImageColorMap::getGray(unsigned char *x, GfxGray *gray)
{
    GfxColor color;
    int      i;

    if (colorSpace2) {
        for (i = 0; i < nComps2; ++i) {
            color.c[i] = lookup2[i][x[0]];
        }
        colorSpace2->getGray(&color, gray);
    } else {
        for (i = 0; i < nComps; ++i) {
            color.c[i] = lookup[i][x[i]];
        }
        colorSpace->getGray(&color, gray);
    }
}

void GfxImageColorMap::getRGB(unsigned char *x, GfxRGB *rgb)
{
    GfxColor color;
    int      i;

    if (colorSpace2) {
        for (i = 0; i < nComps2; ++i) {
            color.c[i] = lookup2[i][x[0]];
        }
        colorSpace2->getRGB(&color, rgb);
    } else {
        for (i = 0; i < nComps; ++i) {
            color.c[i] = lookup[i][x[i]];
        }
        colorSpace->getRGB(&color, rgb);
    }
}

void GfxImageColorMap::getCMYK(unsigned char *x, GfxCMYK *cmyk)
{
    GfxColor color;
    int      i;

    if (colorSpace2) {
        for (i = 0; i < nComps2; ++i) {
            color.c[i] = lookup2[i][x[0]];
        }
        colorSpace2->getCMYK(&color, cmyk);
    } else {
        for (i = 0; i < nComps; ++i) {
            color.c[i] = lookup[i][x[i]];
        }
        colorSpace->getCMYK(&color, cmyk);
    }
}

void GfxImageColorMap::getColor(unsigned char *x, GfxColor *color)
{
    int maxPixel, i;

    if (bits <= 8) {
        maxPixel = (1 << bits) - 1;
    } else {
        maxPixel = 0xff;
    }
    for (i = 0; i < nComps; ++i) {
        color->c[i] =
            xpdf::to_color(decodeLow[i] + (x[i] * decodeRange[i]) / maxPixel);
    }
}

void GfxImageColorMap::getGrayByteLine(unsigned char *in, unsigned char *out,
                                       int n)
{
    GfxColor color;
    GfxGray  gray;
    int      i, j;

    if (colorSpace2) {
        for (j = 0; j < n; ++j) {
            for (i = 0; i < nComps2; ++i) {
                color.c[i] = lookup2[i][in[j]];
            }
            colorSpace2->getGray(&color, &gray);
            out[j] = xpdf::to_small_color(gray.x);
        }
    } else {
        for (j = 0; j < n; ++j) {
            for (i = 0; i < nComps; ++i) {
                color.c[i] = lookup[i][in[j * nComps + i]];
            }
            colorSpace->getGray(&color, &gray);
            out[j] = xpdf::to_small_color(gray.x);
        }
    }
}

void GfxImageColorMap::getRGBByteLine(unsigned char *in, unsigned char *out,
                                      int n)
{
    GfxColor color;
    GfxRGB   rgb;
    int      i, j;

    if (colorSpace2) {
        for (j = 0; j < n; ++j) {
            for (i = 0; i < nComps2; ++i) {
                color.c[i] = lookup2[i][in[j]];
            }
            colorSpace2->getRGB(&color, &rgb);
            out[j * 3] = xpdf::to_small_color(rgb.r);
            out[j * 3 + 1] = xpdf::to_small_color(rgb.g);
            out[j * 3 + 2] = xpdf::to_small_color(rgb.b);
        }
    } else {
        for (j = 0; j < n; ++j) {
            for (i = 0; i < nComps; ++i) {
                color.c[i] = lookup[i][in[j * nComps + i]];
            }
            colorSpace->getRGB(&color, &rgb);
            out[j * 3] = xpdf::to_small_color(rgb.r);
            out[j * 3 + 1] = xpdf::to_small_color(rgb.g);
            out[j * 3 + 2] = xpdf::to_small_color(rgb.b);
        }
    }
}

void GfxImageColorMap::getCMYKByteLine(unsigned char *in, unsigned char *out,
                                       int n)
{
    GfxColor color;
    GfxCMYK  cmyk;
    int      i, j;

    if (colorSpace2) {
        for (j = 0; j < n; ++j) {
            for (i = 0; i < nComps2; ++i) {
                color.c[i] = lookup2[i][in[j]];
            }
            colorSpace2->getCMYK(&color, &cmyk);
            out[j * 4] = xpdf::to_small_color(cmyk.c);
            out[j * 4 + 1] = xpdf::to_small_color(cmyk.m);
            out[j * 4 + 2] = xpdf::to_small_color(cmyk.y);
            out[j * 4 + 3] = xpdf::to_small_color(cmyk.k);
        }
    } else {
        for (j = 0; j < n; ++j) {
            for (i = 0; i < nComps; ++i) {
                color.c[i] = lookup[i][in[j * nComps + i]];
            }
            colorSpace->getCMYK(&color, &cmyk);
            out[j * 4] = xpdf::to_small_color(cmyk.c);
            out[j * 4 + 1] = xpdf::to_small_color(cmyk.m);
            out[j * 4 + 2] = xpdf::to_small_color(cmyk.y);
            out[j * 4 + 3] = xpdf::to_small_color(cmyk.k);
        }
    }
}

//------------------------------------------------------------------------
// GfxSubpath and GfxPath
//------------------------------------------------------------------------

GfxSubpath::GfxSubpath(double x1, double y1)
{
    size = 16;
    x = (double *)calloc(size, sizeof(double));
    y = (double *)calloc(size, sizeof(double));
    curve = (bool *)calloc(size, sizeof(bool));
    n = 1;
    x[0] = x1;
    y[0] = y1;
    curve[0] = false;
    closed = false;
}

GfxSubpath::~GfxSubpath()
{
    free(x);
    free(y);
    free(curve);
}

// Used for copy().
GfxSubpath::GfxSubpath(GfxSubpath *subpath)
{
    size = subpath->size;
    n = subpath->n;
    x = (double *)calloc(size, sizeof(double));
    y = (double *)calloc(size, sizeof(double));
    curve = (bool *)calloc(size, sizeof(bool));
    memcpy(x, subpath->x, n * sizeof(double));
    memcpy(y, subpath->y, n * sizeof(double));
    memcpy(curve, subpath->curve, n * sizeof(bool));
    closed = subpath->closed;
}

void GfxSubpath::lineTo(double x1, double y1)
{
    if (n >= size) {
        size *= 2;
        x = (double *)reallocarray(x, size, sizeof(double));
        y = (double *)reallocarray(y, size, sizeof(double));
        curve = (bool *)reallocarray(curve, size, sizeof(bool));
    }
    x[n] = x1;
    y[n] = y1;
    curve[n] = false;
    ++n;
}

void GfxSubpath::curveTo(double x1, double y1, double x2, double y2, double x3,
                         double y3)
{
    if (n + 3 > size) {
        size *= 2;
        x = (double *)reallocarray(x, size, sizeof(double));
        y = (double *)reallocarray(y, size, sizeof(double));
        curve = (bool *)reallocarray(curve, size, sizeof(bool));
    }
    x[n] = x1;
    y[n] = y1;
    x[n + 1] = x2;
    y[n + 1] = y2;
    x[n + 2] = x3;
    y[n + 2] = y3;
    curve[n] = curve[n + 1] = true;
    curve[n + 2] = false;
    n += 3;
}

void GfxSubpath::close()
{
    if (x[n - 1] != x[0] || y[n - 1] != y[0]) {
        lineTo(x[0], y[0]);
    }
    closed = true;
}

void GfxSubpath::offset(double dx, double dy)
{
    int i;

    for (i = 0; i < n; ++i) {
        x[i] += dx;
        y[i] += dy;
    }
}

GfxPath::GfxPath()
{
    justMoved = false;
    size = 16;
    n = 0;
    firstX = firstY = 0;
    subpaths = (GfxSubpath **)calloc(size, sizeof(GfxSubpath *));
}

GfxPath::~GfxPath()
{
    int i;

    for (i = 0; i < n; ++i)
        delete subpaths[i];
    free(subpaths);
}

// Used for copy().
GfxPath::GfxPath(bool justMoved1, double firstX1, double firstY1,
                 GfxSubpath **subpaths1, int n1, int size1)
{
    int i;

    justMoved = justMoved1;
    firstX = firstX1;
    firstY = firstY1;
    size = size1;
    n = n1;
    subpaths = (GfxSubpath **)calloc(size, sizeof(GfxSubpath *));
    for (i = 0; i < n; ++i)
        subpaths[i] = subpaths1[i]->copy();
}

void GfxPath::moveTo(double x, double y)
{
    justMoved = true;
    firstX = x;
    firstY = y;
}

void GfxPath::lineTo(double x, double y)
{
    if (justMoved || (n > 0 && subpaths[n - 1]->isClosed())) {
        if (n >= size) {
            size *= 2;
            subpaths =
                (GfxSubpath **)reallocarray(subpaths, size, sizeof(GfxSubpath *));
        }
        if (justMoved) {
            subpaths[n] = new GfxSubpath(firstX, firstY);
        } else {
            subpaths[n] = new GfxSubpath(subpaths[n - 1]->getLastX(),
                                         subpaths[n - 1]->getLastY());
        }
        ++n;
        justMoved = false;
    }
    subpaths[n - 1]->lineTo(x, y);
}

void GfxPath::curveTo(double x1, double y1, double x2, double y2, double x3,
                      double y3)
{
    if (justMoved || (n > 0 && subpaths[n - 1]->isClosed())) {
        if (n >= size) {
            size *= 2;
            subpaths =
                (GfxSubpath **)reallocarray(subpaths, size, sizeof(GfxSubpath *));
        }
        if (justMoved) {
            subpaths[n] = new GfxSubpath(firstX, firstY);
        } else {
            subpaths[n] = new GfxSubpath(subpaths[n - 1]->getLastX(),
                                         subpaths[n - 1]->getLastY());
        }
        ++n;
        justMoved = false;
    }
    subpaths[n - 1]->curveTo(x1, y1, x2, y2, x3, y3);
}

void GfxPath::close()
{
    // this is necessary to handle the pathological case of
    // moveto/closepath/clip, which defines an empty clipping region
    if (justMoved) {
        if (n >= size) {
            size *= 2;
            subpaths =
                (GfxSubpath **)reallocarray(subpaths, size, sizeof(GfxSubpath *));
        }
        subpaths[n] = new GfxSubpath(firstX, firstY);
        ++n;
        justMoved = false;
    }
    subpaths[n - 1]->close();
}

void GfxPath::append(GfxPath *path)
{
    int i;

    if (n + path->n > size) {
        size = n + path->n;
        subpaths =
            (GfxSubpath **)reallocarray(subpaths, size, sizeof(GfxSubpath *));
    }
    for (i = 0; i < path->n; ++i) {
        subpaths[n++] = path->subpaths[i]->copy();
    }
    justMoved = false;
}

void GfxPath::offset(double dx, double dy)
{
    int i;

    for (i = 0; i < n; ++i) {
        subpaths[i]->offset(dx, dy);
    }
}

//------------------------------------------------------------------------
// GfxState
//------------------------------------------------------------------------

GfxState::GfxState(double hDPIA, double vDPIA, PDFRectangle *pageBox, int rotateA,
                   bool upsideDown)
{
    double kx, ky;

    hDPI = hDPIA;
    vDPI = vDPIA;
    rotate = rotateA;
    px1 = pageBox->x1;
    py1 = pageBox->y1;
    px2 = pageBox->x2;
    py2 = pageBox->y2;
    kx = hDPI / 72.0;
    ky = vDPI / 72.0;
    if (rotate == 90) {
        ctm[0] = 0;
        ctm[1] = upsideDown ? ky : -ky;
        ctm[2] = kx;
        ctm[3] = 0;
        ctm[4] = -kx * py1;
        ctm[5] = ky * (upsideDown ? -px1 : px2);
        pageWidth = kx * (py2 - py1);
        pageHeight = ky * (px2 - px1);
    } else if (rotate == 180) {
        ctm[0] = -kx;
        ctm[1] = 0;
        ctm[2] = 0;
        ctm[3] = upsideDown ? ky : -ky;
        ctm[4] = kx * px2;
        ctm[5] = ky * (upsideDown ? -py1 : py2);
        pageWidth = kx * (px2 - px1);
        pageHeight = ky * (py2 - py1);
    } else if (rotate == 270) {
        ctm[0] = 0;
        ctm[1] = upsideDown ? -ky : ky;
        ctm[2] = -kx;
        ctm[3] = 0;
        ctm[4] = kx * py2;
        ctm[5] = ky * (upsideDown ? px2 : -px1);
        pageWidth = kx * (py2 - py1);
        pageHeight = ky * (px2 - px1);
    } else {
        ctm[0] = kx;
        ctm[1] = 0;
        ctm[2] = 0;
        ctm[3] = upsideDown ? -ky : ky;
        ctm[4] = -kx * px1;
        ctm[5] = ky * (upsideDown ? py2 : -py1);
        pageWidth = kx * (px2 - px1);
        pageHeight = ky * (py2 - py1);
    }

    fillColorSpace = GfxColorSpace::create(csDeviceGray);
    strokeColorSpace = GfxColorSpace::create(csDeviceGray);
    fillColor.c[0] = 0;
    strokeColor.c[0] = 0;
    fillPattern = NULL;
    strokePattern = NULL;
    blendMode = gfxBlendNormal;
    fillOpacity = 1;
    strokeOpacity = 1;
    fillOverprint = false;
    strokeOverprint = false;
    overprintMode = 0;

    ::fill(transfer, transfer + 4, Function{});

    lineWidth = 1;
    lineDash = NULL;
    lineDashLength = 0;
    lineDashStart = 0;
    flatness = 1;
    lineJoin = 0;
    lineCap = 0;
    miterLimit = 10;
    strokeAdjust = false;

    font = NULL;
    fontSize = 0;
    textMat[0] = 1;
    textMat[1] = 0;
    textMat[2] = 0;
    textMat[3] = 1;
    textMat[4] = 0;
    textMat[5] = 0;
    charSpace = 0;
    wordSpace = 0;
    horizScaling = 1;
    leading = 0;
    rise = 0;
    render = 0;

    path = new GfxPath();
    curX = curY = 0;
    lineX = lineY = 0;

    clipXMin = 0;
    clipYMin = 0;
    clipXMax = pageWidth;
    clipYMax = pageHeight;

    saved = NULL;
}

GfxState::~GfxState()
{
    if (fillColorSpace) {
        delete fillColorSpace;
    }
    if (strokeColorSpace) {
        delete strokeColorSpace;
    }
    if (fillPattern) {
        delete fillPattern;
    }
    if (strokePattern) {
        delete strokePattern;
    }

    free(lineDash);

    if (path) {
        // this gets set to NULL by restore()
        delete path;
    }
}

// Used for copy();
GfxState::GfxState(GfxState *other, bool copyPath)
    : hDPI{ other->hDPI }
    , vDPI{ other->vDPI }
    , ctm{}
    , px1{ other->px1 }
    , py1{ other->py1 }
    , px2{ other->px2 }
    , py2{ other->py2 }
    , pageWidth{ other->pageWidth }
    , pageHeight{ other->pageHeight }
    , rotate{ other->rotate }
    , fillColorSpace{ other->fillColorSpace }
    , strokeColorSpace{ other->strokeColorSpace }
    , fillColor{ other->fillColor }
    , strokeColor{ other->strokeColor }
    , fillPattern{ other->fillPattern }
    , strokePattern{ other->strokePattern }
    , blendMode{ other->blendMode }
    , fillOpacity{ other->fillOpacity }
    , strokeOpacity{ other->strokeOpacity }
    , fillOverprint{ other->fillOverprint }
    , strokeOverprint{ other->strokeOverprint }
    , overprintMode{ other->overprintMode }
    , transfer{}
    , lineWidth{ other->lineWidth }
    , lineDash{ other->lineDash }
    , lineDashLength{ other->lineDashLength }
    , lineDashStart{ other->lineDashStart }
    , flatness{ other->flatness }
    , lineJoin{ other->lineJoin }
    , lineCap{ other->lineCap }
    , miterLimit{ other->miterLimit }
    , strokeAdjust{ other->strokeAdjust }
    , font{ other->font }
    , fontSize{ other->fontSize }
    , textMat{}
    , charSpace{ other->charSpace }
    , wordSpace{ other->wordSpace }
    , horizScaling{ other->horizScaling }
    , leading{ other->leading }
    , rise{ other->rise }
    , render{ other->render }
    , path{ other->path }
    , curX{ other->curX }
    , curY{ other->curY }
    , lineX{ other->lineX }
    , lineY{ other->lineY }
    , clipXMin{ other->clipXMin }
    , clipYMin{ other->clipYMin }
    , clipXMax{ other->clipXMax }
    , clipYMax{ other->clipYMax }
    , saved{}
{
    // memcpy (this, other, sizeof (GfxState));
    ::copy(other->ctm, other->ctm + 6, ctm);
    ::copy(other->textMat, other->textMat + 6, textMat);

    if (fillColorSpace) {
        fillColorSpace = other->fillColorSpace->copy();
    }

    if (strokeColorSpace) {
        strokeColorSpace = other->strokeColorSpace->copy();
    }

    if (fillPattern) {
        fillPattern = other->fillPattern->copy();
    }

    if (strokePattern) {
        strokePattern = other->strokePattern->copy();
    }

    ::copy(other->transfer, other->transfer + 4, transfer);

    if (lineDashLength > 0) {
        lineDash = (double *)calloc(lineDashLength, sizeof(double));
        memcpy(lineDash, other->lineDash, lineDashLength * sizeof(double));
    }

    if (copyPath) {
        path = other->path->copy();
    }
}

void GfxState::setPath(GfxPath *pathA)
{
    if (path) {
        delete path;
    }

    path = pathA;
}

void GfxState::getUserClipBBox(double *xMin, double *yMin, double *xMax,
                               double *yMax)
{
    double ictm[6];
    double xMin1, yMin1, xMax1, yMax1, det, tx, ty;

    // invert the CTM
    det = 1 / (ctm[0] * ctm[3] - ctm[1] * ctm[2]);
    ictm[0] = ctm[3] * det;
    ictm[1] = -ctm[1] * det;
    ictm[2] = -ctm[2] * det;
    ictm[3] = ctm[0] * det;
    ictm[4] = (ctm[2] * ctm[5] - ctm[3] * ctm[4]) * det;
    ictm[5] = (ctm[1] * ctm[4] - ctm[0] * ctm[5]) * det;

    // transform all four corners of the clip bbox; find the min and max
    // x and y values
    xMin1 = xMax1 = clipXMin * ictm[0] + clipYMin * ictm[2] + ictm[4];
    yMin1 = yMax1 = clipXMin * ictm[1] + clipYMin * ictm[3] + ictm[5];
    tx = clipXMin * ictm[0] + clipYMax * ictm[2] + ictm[4];
    ty = clipXMin * ictm[1] + clipYMax * ictm[3] + ictm[5];
    if (tx < xMin1) {
        xMin1 = tx;
    } else if (tx > xMax1) {
        xMax1 = tx;
    }
    if (ty < yMin1) {
        yMin1 = ty;
    } else if (ty > yMax1) {
        yMax1 = ty;
    }
    tx = clipXMax * ictm[0] + clipYMin * ictm[2] + ictm[4];
    ty = clipXMax * ictm[1] + clipYMin * ictm[3] + ictm[5];
    if (tx < xMin1) {
        xMin1 = tx;
    } else if (tx > xMax1) {
        xMax1 = tx;
    }
    if (ty < yMin1) {
        yMin1 = ty;
    } else if (ty > yMax1) {
        yMax1 = ty;
    }
    tx = clipXMax * ictm[0] + clipYMax * ictm[2] + ictm[4];
    ty = clipXMax * ictm[1] + clipYMax * ictm[3] + ictm[5];
    if (tx < xMin1) {
        xMin1 = tx;
    } else if (tx > xMax1) {
        xMax1 = tx;
    }
    if (ty < yMin1) {
        yMin1 = ty;
    } else if (ty > yMax1) {
        yMax1 = ty;
    }

    *xMin = xMin1;
    *yMin = yMin1;
    *xMax = xMax1;
    *yMax = yMax1;
}

double GfxState::transformWidth(double w)
{
    double x, y;

    x = ctm[0] + ctm[2];
    y = ctm[1] + ctm[3];
    return w * sqrt(0.5 * (x * x + y * y));
}

double GfxState::getTransformedFontSize()
{
    double x1, y1, x2, y2;

    x1 = textMat[2] * fontSize;
    y1 = textMat[3] * fontSize;
    x2 = ctm[0] * x1 + ctm[2] * y1;
    y2 = ctm[1] * x1 + ctm[3] * y1;
    return sqrt(x2 * x2 + y2 * y2);
}

void GfxState::getFontTransMat(double *m11, double *m12, double *m21, double *m22)
{
    *m11 = (textMat[0] * ctm[0] + textMat[1] * ctm[2]) * fontSize;
    *m12 = (textMat[0] * ctm[1] + textMat[1] * ctm[3]) * fontSize;
    *m21 = (textMat[2] * ctm[0] + textMat[3] * ctm[2]) * fontSize;
    *m22 = (textMat[2] * ctm[1] + textMat[3] * ctm[3]) * fontSize;
}

void GfxState::setCTM(double a, double b, double c, double d, double e, double f)
{
    int i;

    ctm[0] = a;
    ctm[1] = b;
    ctm[2] = c;
    ctm[3] = d;
    ctm[4] = e;
    ctm[5] = f;

    // avoid FP exceptions on badly messed up PDF files
    for (i = 0; i < 6; ++i) {
        if (ctm[i] > 1e10) {
            ctm[i] = 1e10;
        } else if (ctm[i] < -1e10) {
            ctm[i] = -1e10;
        }
    }
}

void GfxState::concatCTM(double a, double b, double c, double d, double e,
                         double f)
{
    double a1 = ctm[0];
    double b1 = ctm[1];
    double c1 = ctm[2];
    double d1 = ctm[3];
    int    i;

    ctm[0] = a * a1 + b * c1;
    ctm[1] = a * b1 + b * d1;
    ctm[2] = c * a1 + d * c1;
    ctm[3] = c * b1 + d * d1;
    ctm[4] = e * a1 + f * c1 + ctm[4];
    ctm[5] = e * b1 + f * d1 + ctm[5];

    // avoid FP exceptions on badly messed up PDF files
    for (i = 0; i < 6; ++i) {
        if (ctm[i] > 1e10) {
            ctm[i] = 1e10;
        } else if (ctm[i] < -1e10) {
            ctm[i] = -1e10;
        }
    }
}

void GfxState::shiftCTM(double tx, double ty)
{
    ctm[4] += tx;
    ctm[5] += ty;
    clipXMin += tx;
    clipYMin += ty;
    clipXMax += tx;
    clipYMax += ty;
}

void GfxState::setFillColorSpace(GfxColorSpace *colorSpace)
{
    if (fillColorSpace) {
        delete fillColorSpace;
    }
    fillColorSpace = colorSpace;
}

void GfxState::setStrokeColorSpace(GfxColorSpace *colorSpace)
{
    if (strokeColorSpace) {
        delete strokeColorSpace;
    }
    strokeColorSpace = colorSpace;
}

void GfxState::setFillPattern(GfxPattern *pattern)
{
    if (fillPattern) {
        delete fillPattern;
    }
    fillPattern = pattern;
}

void GfxState::setStrokePattern(GfxPattern *pattern)
{
    if (strokePattern) {
        delete strokePattern;
    }
    strokePattern = pattern;
}

void GfxState::setTransfer(Function *funcs)
{
    ::copy(funcs, funcs + 4, transfer);
}

void GfxState::setLineDash(double *dash, int length, double start)
{
    if (lineDash)
        free(lineDash);
    lineDash = dash;
    lineDashLength = length;
    lineDashStart = start;
}

void GfxState::clearPath()
{
    delete path;
    path = new GfxPath();
}

void GfxState::clip()
{
    double      xMin, yMin, xMax, yMax, x, y;
    GfxSubpath *subpath;
    int         i, j;

    xMin = xMax = yMin = yMax = 0; // make gcc happy
    for (i = 0; i < path->getNumSubpaths(); ++i) {
        subpath = path->getSubpath(i);
        for (j = 0; j < subpath->getNumPoints(); ++j) {
            transform(subpath->getX(j), subpath->getY(j), &x, &y);
            if (i == 0 && j == 0) {
                xMin = xMax = x;
                yMin = yMax = y;
            } else {
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
            }
        }
    }
    if (xMin > clipXMin) {
        clipXMin = xMin;
    }
    if (yMin > clipYMin) {
        clipYMin = yMin;
    }
    if (xMax < clipXMax) {
        clipXMax = xMax;
    }
    if (yMax < clipYMax) {
        clipYMax = yMax;
    }
}

void GfxState::clipToStrokePath()
{
    double      xMin, yMin, xMax, yMax, x, y, t0, t1;
    GfxSubpath *subpath;
    int         i, j;

    xMin = xMax = yMin = yMax = 0; // make gcc happy
    for (i = 0; i < path->getNumSubpaths(); ++i) {
        subpath = path->getSubpath(i);
        for (j = 0; j < subpath->getNumPoints(); ++j) {
            transform(subpath->getX(j), subpath->getY(j), &x, &y);
            if (i == 0 && j == 0) {
                xMin = xMax = x;
                yMin = yMax = y;
            } else {
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
            }
        }
    }

    // allow for the line width
    //~ miter joins can extend farther than this
    t0 = fabs(ctm[0]);
    t1 = fabs(ctm[2]);
    if (t0 > t1) {
        xMin -= 0.5 * lineWidth * t0;
        xMax += 0.5 * lineWidth * t0;
    } else {
        xMin -= 0.5 * lineWidth * t1;
        xMax += 0.5 * lineWidth * t1;
    }
    t0 = fabs(ctm[0]);
    t1 = fabs(ctm[3]);
    if (t0 > t1) {
        yMin -= 0.5 * lineWidth * t0;
        yMax += 0.5 * lineWidth * t0;
    } else {
        yMin -= 0.5 * lineWidth * t1;
        yMax += 0.5 * lineWidth * t1;
    }

    if (xMin > clipXMin) {
        clipXMin = xMin;
    }
    if (yMin > clipYMin) {
        clipYMin = yMin;
    }
    if (xMax < clipXMax) {
        clipXMax = xMax;
    }
    if (yMax < clipYMax) {
        clipYMax = yMax;
    }
}

void GfxState::clipToRect(double xMin, double yMin, double xMax, double yMax)
{
    double x, y, xMin1, yMin1, xMax1, yMax1;

    transform(xMin, yMin, &x, &y);
    xMin1 = xMax1 = x;
    yMin1 = yMax1 = y;
    transform(xMax, yMin, &x, &y);
    if (x < xMin1) {
        xMin1 = x;
    } else if (x > xMax1) {
        xMax1 = x;
    }
    if (y < yMin1) {
        yMin1 = y;
    } else if (y > yMax1) {
        yMax1 = y;
    }
    transform(xMax, yMax, &x, &y);
    if (x < xMin1) {
        xMin1 = x;
    } else if (x > xMax1) {
        xMax1 = x;
    }
    if (y < yMin1) {
        yMin1 = y;
    } else if (y > yMax1) {
        yMax1 = y;
    }
    transform(xMin, yMax, &x, &y);
    if (x < xMin1) {
        xMin1 = x;
    } else if (x > xMax1) {
        xMax1 = x;
    }
    if (y < yMin1) {
        yMin1 = y;
    } else if (y > yMax1) {
        yMax1 = y;
    }

    if (xMin1 > clipXMin) {
        clipXMin = xMin1;
    }
    if (yMin1 > clipYMin) {
        clipYMin = yMin1;
    }
    if (xMax1 < clipXMax) {
        clipXMax = xMax1;
    }
    if (yMax1 < clipYMax) {
        clipYMax = yMax1;
    }
}

void GfxState::textShift(double tx, double ty)
{
    double dx, dy;

    textTransformDelta(tx, ty, &dx, &dy);
    curX += dx;
    curY += dy;
}

void GfxState::shift(double dx, double dy)
{
    curX += dx;
    curY += dy;
}

GfxState *GfxState::save()
{
    GfxState *newState;

    newState = copy();
    newState->saved = this;
    return newState;
}

GfxState *GfxState::restore()
{
    GfxState *oldState;

    if (saved) {
        oldState = saved;

        // these attributes aren't saved/restored by the q/Q operators
        oldState->path = path;
        oldState->curX = curX;
        oldState->curY = curY;
        oldState->lineX = lineX;
        oldState->lineY = lineY;

        path = NULL;
        saved = NULL;
        delete this;
    } else {
        oldState = this;
    }

    return oldState;
}

bool GfxState::parseBlendMode(Object *obj, GfxBlendMode *mode)
{
    Object obj2;
    int    i, j;

    if (obj->is_name()) {
        for (i = 0; i < nGfxBlendModeNames; ++i) {
            if (!strcmp(obj->as_name(), gfxBlendModeNames[i].name)) {
                *mode = gfxBlendModeNames[i].mode;
                return true;
            }
        }
        return false;
    } else if (obj->is_array()) {
        for (i = 0; i < obj->as_array().size(); ++i) {
            obj2 = resolve((*obj)[i]);
            if (!obj2.is_name()) {
                return false;
            }
            for (j = 0; j < nGfxBlendModeNames; ++j) {
                if (!strcmp(obj2.as_name(), gfxBlendModeNames[j].name)) {
                    *mode = gfxBlendModeNames[j].mode;
                    return true;
                }
            }
        }
        *mode = gfxBlendNormal;
        return true;
    } else {
        return false;
    }
}

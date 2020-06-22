// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHMATH_HH
#define XPDF_SPLASH_SPLASHMATH_HH

#include <defs.hh>

#if USE_FIXEDPONT
#include <utils/FixedPoint.hh>
#else
#include <cmath>
#endif
#include <splash/SplashTypes.hh>

static inline SplashCoord splashAbs(SplashCoord x)
{
    return fabs(x);
}

static inline int splashFloor(SplashCoord x)
{
#if __GNUC__ && __i386__
    // floor() and (int)() are implemented separately, which results
    // in changing the FPCW multiple times - so we optimize it with
    // some inline assembly
    unsigned short oldCW, newCW, t;
    int            result;

    __asm__ volatile("fnstcw %0\n"
                     "movw   %0, %3\n"
                     "andw   $0xf3ff, %3\n"
                     "orw    $0x0400, %3\n"
                     "movw   %3, %1\n" // round down
                     "fldcw  %1\n"
                     "fistl %2\n"
                     "fldcw  %0\n"
                     : "=m"(oldCW), "=m"(newCW), "=m"(result), "=r"(t)
                     : "t"(x));
    return result;
#else
    return (int)floor(x);
#endif
}

static inline int splashCeil(SplashCoord x)
{
#if __GNUC__ && __i386__
    // ceil() and (int)() are implemented separately, which results
    // in changing the FPCW multiple times - so we optimize it with
    // some inline assembly
    unsigned short oldCW, newCW, t;
    int            result;

    __asm__ volatile("fnstcw %0\n"
                     "movw   %0, %3\n"
                     "andw   $0xf3ff, %3\n"
                     "orw    $0x0800, %3\n"
                     "movw   %3, %1\n" // round up
                     "fldcw  %1\n"
                     "fistl %2\n"
                     "fldcw  %0\n"
                     : "=m"(oldCW), "=m"(newCW), "=m"(result), "=r"(t)
                     : "t"(x));
    return result;
#else
    return (int)ceil(x);
#endif
}

static inline int splashRound(SplashCoord x)
{
#if __GNUC__ && __i386__
    // this could use round-to-nearest mode and avoid the "+0.5",
    // but that produces slightly different results (because i+0.5
    // sometimes rounds up and sometimes down using the even rule)
    unsigned short oldCW, newCW, t;
    int            result;

    x += 0.5;
    __asm__ volatile("fnstcw %0\n"
                     "movw   %0, %3\n"
                     "andw   $0xf3ff, %3\n"
                     "orw    $0x0400, %3\n"
                     "movw   %3, %1\n" // round down
                     "fldcw  %1\n"
                     "fistl %2\n"
                     "fldcw  %0\n"
                     : "=m"(oldCW), "=m"(newCW), "=m"(result), "=r"(t)
                     : "t"(x));
    return result;
#else
    return (int)floor(x + 0.5);
#endif
}

static inline SplashCoord splashAvg(SplashCoord x, SplashCoord y)
{
    return 0.5 * (x + y);
}

static inline SplashCoord splashSqrt(SplashCoord x)
{
    return sqrt(x);
}

static inline SplashCoord splashPow(SplashCoord x, SplashCoord y)
{
    return pow(x, y);
}

static inline SplashCoord splashDist(SplashCoord x0, SplashCoord y0,
                                     SplashCoord x1, SplashCoord y1)
{
    SplashCoord dx, dy;
    dx = x1 - x0;
    dy = y1 - y0;
    return sqrt(dx * dx + dy * dy);
}

static inline bool splashCheckDet(SplashCoord m11, SplashCoord m12,
                                  SplashCoord m21, SplashCoord m22,
                                  SplashCoord epsilon)
{
    return fabs(m11 * m22 - m12 * m21) >= epsilon;
}

// Perform stroke adjustment on a SplashCoord range [xMin, xMax),
// resulting in an int range [*xMinI, *xMaxI).
//
// There are several options:
//
// 1. Round both edge coordinates.
//    Pro: adjacent strokes/fills line up without any gaps or
//         overlaps
//    Con: lines with the same original floating point width can
//         end up with different integer widths, e.g.:
//               xMin  = 10.1   xMax  = 11.3   (width = 1.2)
//           --> xMinI = 10     xMaxI = 11     (width = 1)
//         but
//               xMin  = 10.4   xMax  = 11.6   (width = 1.2)
//           --> xMinI = 10     xMaxI = 12     (width = 2)
//
// 2. Round the min coordinate; add the ceiling of the width.
//    Pro: lines with the same original floating point width will
//         always end up with the same integer width
//    Con: adjacent strokes/fills can have overlaps (which is
//         problematic with transparency)
//    (This could use floor on the min coordinate, instead of
//    rounding, with similar results.)
//    (If the width is rounded instead of using ceiling, the results
//    Are similar, except that adjacent strokes/fills can have gaps
//    as well as overlaps.)
//
// 3. Use floor on the min coordinate and ceiling on the max
//    coordinate.
//    Pro: lines always end up at least as wide as the original
//         floating point width
//    Con: adjacent strokes/fills can have overlaps, and lines with
//         the same original floating point width can end up with
//         different integer widths; the integer width can be more
//         than one pixel wider than the original width, e.g.:
//               xMin  = 10.9   xMax  = 12.1   (width = 1.2)
//           --> xMinI = 10     xMaxI = 13     (width = 3)
//         but
//               xMin  = 10.1   xMax  = 11.3   (width = 1.2)
//           --> xMinI = 10     xMaxI = 12     (width = 2)
static inline void splashStrokeAdjust(SplashCoord xMin, SplashCoord xMax,
                                      int *xMinI, int *xMaxI)
{
    int x0, x1;

    // NB: enable exactly one of these.
#if 1 // 1. Round both edge coordinates.
    x0 = splashRound(xMin);
    x1 = splashRound(xMax);
#endif
#if 0 // 2. Round the min coordinate; add the ceiling of the width.
  x0 = splashRound(xMin);
  x1 = x0 + splashCeil(xMax - xMin);
#endif
#if 0 // 3. Use floor on the min coord and ceiling on the max coord.
  x0 = splashFloor(xMin);
  x1 = splashCeil(xMax);
#endif
    if (x1 == x0) {
        ++x1;
    }
    *xMinI = x0;
    *xMaxI = x1;
}

#endif // XPDF_SPLASH_SPLASHMATH_HH

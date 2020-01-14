//========================================================================
//
// SplashXPathScanner.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <cstdlib>
#include <cstring>

#include <algorithm>

#include <goo/GList.hh>

#include <splash/SplashMath.hh>
#include <splash/SplashXPath.hh>
#include <splash/SplashXPathScanner.hh>

#include <range/v3/all.hpp>
using namespace ranges;

//------------------------------------------------------------------------

#define minVertStep 0.05

struct indirect_cmp_t {
    //
    // Increasing order of xCur0, or dxdy:
    //
    bool operator() (const SplashXPathSeg* plhs, const SplashXPathSeg* prhs) {
        const SplashXPathSeg &lhs = *plhs, &rhs = *prhs;

        auto x = lhs.xCur0 - rhs.xCur0;

        if (0 == x) {
            x = lhs.dxdy - rhs.dxdy;
        }

        return x < 0;
    }
};

//------------------------------------------------------------------------

SplashXPathScanner::SplashXPathScanner (
    SplashXPath* xPathA, bool eoA, int yMinA, int yMaxA) {
    xPath = xPathA;

    eo = eoA;

    yMin = yMinA;
    yMax = yMaxA;

    nextSeg = 0;
    yNext = xPath->yMin;
}

void SplashXPathScanner::getSpan (unsigned char* line, int y, int x0, int x1) {
    SplashXPathSeg *seg, *seg0;
    SplashCoord y0, y1, y1p;
    bool intersect, last;
    int eoMask, state0, state1, count;

    //--- clear the scan line buffer
    memset (line + x0, 0, x1 - x0 + 1);

    //--- reset the path
    if (yNext != y) {
        activeSegs.clear ();

        for (nextSeg = 0; nextSeg < xPath->length; ++nextSeg) {
            seg = &xPath->segs [nextSeg];

            if (seg->y0 >= y) {
                break;
            }

            if (seg->y0 != seg->y1 && seg->y1 > y) {
                if (seg->y0 == y) {
                    seg->xCur0 = seg->x0;
                }
                else {
                    seg->xCur0 = seg->x0 + ((SplashCoord)y - seg->y0) * seg->dxdy;
                }

                activeSegs.push_back (seg);
            }
        }

        sort (activeSegs, indirect_cmp_t{ });
    }

    //--- process the scan line
    for (y0 = y; y0 < y + 1;) {
        //
        // Delete finished segs:
        //
        actions::remove_if (activeSegs, [=](auto p) { return p->y1 <= y0; });

        //
        // Check for bottom of path:
        //
        if (activeSegs.empty () && nextSeg >= xPath->length) {
            break;
        }

        //
        // Add waiting segs:
        //
        for (; nextSeg < xPath->length; ++nextSeg) {
            auto p = &xPath->segs [nextSeg];

            if (p->y0 > y0) {
                break;
            }

            if (p->y0 != p->y1) {
                p->xCur0 = p->x0;
                activeSegs.push_back (p);
            }
        }

        //
        // Sort activeSegs:
        //
        sort (activeSegs, indirect_cmp_t{ });

        //--- get the next "interesting" y value
        y1 = y + 1;

        if (nextSeg < xPath->length && xPath->segs [nextSeg].y0 < y1) {
            y1 = xPath->segs [nextSeg].y0;
        }

        for (auto p : activeSegs) {
            if (p->y1 < y1) {
                y1 = p->y1;
            }
        }

        //--- compute xCur1 values, check for intersections
        seg0 = NULL;
        intersect = false;

        for (auto p : activeSegs) {
            if (p->y1 == y1) {
                p->xCur1 = p->x1;
            }
            else {
                p->xCur1 = p->x0 + (y1 - p->y0) * p->dxdy;
            }

            if (seg0 && seg0->xCur1 > p->xCur1) {
                intersect = true;
            }

            seg0 = p;
        }

        //--- draw rectangles
        if (intersect) {
            for (; y0 < y1; y0 += minVertStep) {
                if ((y1p = y0 + minVertStep) >= y1) {
                    y1p = y1;
                    last = true;
                }
                else {
                    last = false;
                }

                state0 = state1 = count = 0;
                seg0 = NULL;

                eoMask = eo ? 1 : 0xffffffff;

                for (auto p : activeSegs) {
                    if (last && p->y1 == y1) {
                        p->xCur1 = p->x1;
                    }
                    else {
                        p->xCur1 = p->x0 + (y1p - p->y0) * p->dxdy;
                    }

                    count += p->count;
                    state1 = count & eoMask;

                    if (!state0 && state1) {
                        seg0 = p;
                    }
                    else if (state0 && !state1) {
                        drawRectangle (line, x0, x1, y0, y1p, seg0->xCur0, p->xCur0);
                    }

                    state0 = state1;
                }

                for (auto p : activeSegs) {
                    p->xCur0 = p->xCur1;
                }

                sort (activeSegs, indirect_cmp_t{ });
            }
        }
        else {
            //
            // Draw trapezoids:
            //
            state0 = state1 = count = 0;
            seg0 = NULL;
            eoMask = eo ? 1 : 0xffffffff;

            for (auto p : activeSegs) {
                count += p->count;
                state1 = count & eoMask;

                if (!state0 && state1) {
                    seg0 = p;
                }
                else if (state0 && !state1) {
                    drawTrapezoid (
                        line, x0, x1, y0, y1, seg0->xCur0, seg0->xCur1,
                        seg0->dydx, p->xCur0, p->xCur1, p->dydx);
                }

                state0 = state1;
            }

            for (auto p : activeSegs) {
                p->xCur0 = p->xCur1;
            }
        }

        //--- next slice
        y0 = y1;
    }

    yNext = y + 1;
}

void SplashXPathScanner::getSpanBinary (unsigned char* line, int y, int x0, int x1) {
    SplashXPathSeg* seg;
    int xx0, xx1, xx;
    int eoMask, state0, state1, count, i;

    //--- clear the scan line buffer
    memset (line + x0, 0, x1 - x0 + 1);

    //--- reset the path
    if (yNext != y) {
        activeSegs.clear ();

        for (nextSeg = 0; nextSeg < xPath->length; ++nextSeg) {
            seg = &xPath->segs [nextSeg];

            if (seg->y0 >= y) {
                break;
            }

            if (seg->y1 > y) {
                if (seg->y0 == y) {
                    seg->xCur0 = seg->x0;
                }
                else {
                    seg->xCur0 = seg->x0 + ((SplashCoord)y - seg->y0) * seg->dxdy;
                }

                activeSegs.push_back (seg);
            }
        }

        sort (activeSegs, indirect_cmp_t{ });
    }

    //
    // Delete finished segs:
    //
    actions::remove_if (activeSegs, [=](auto p) { return p->y1 <= y; });

    //--- add waiting segs
    for (; nextSeg < xPath->length; ++nextSeg) {
        auto p = &xPath->segs [nextSeg];

        if (p->y0 >= y + 1) {
            break;
        }

        p->xCur0 = p->x0;
        activeSegs.push_back (p);
    }

    //--- sort activeSegs
    sort (activeSegs, indirect_cmp_t{ });

    //--- compute xCur1 values
    for (i = 0; i < activeSegs.size (); ++i) {
        auto p = activeSegs [i];

        if (p->y1 <= y + 1) {
            p->xCur1 = p->x1;
        }
        else {
            p->xCur1 = p->x0 + ((SplashCoord) (y + 1) - p->y0) * p->dxdy;
        }
    }

    //--- draw spans
    state0 = state1 = count = 0;

    eoMask = eo ? 1 : 0xffffffff;
    xx0 = xx1 = 0; // make gcc happy

    for (i = 0; i < activeSegs.size (); ++i) {
        seg = activeSegs [i];

        if (seg->y0 <= y && seg->y0 < seg->y1) {
            count += seg->count;
            state1 = count & eoMask;
        }

        if (state0) {
            xx = splashCeil (seg->xCur0) - 1;
            if (xx > xx1) { xx1 = xx; }
            xx = splashFloor (seg->xCur1);
            if (xx < xx0) { xx0 = xx; }
            xx = splashCeil (seg->xCur1) - 1;
            if (xx > xx1) { xx1 = xx; }
        }
        else {
            if (seg->xCur0 < seg->xCur1) {
                xx0 = splashFloor (seg->xCur0);
                xx1 = splashCeil (seg->xCur1) - 1;
            }
            else {
                xx0 = splashFloor (seg->xCur1);
                xx1 = splashCeil (seg->xCur0) - 1;
            }
        }

        if (!state1) {
            if (xx0 < x0) { xx0 = x0; }
            if (xx1 > x1) { xx1 = x1; }
            for (xx = xx0; xx <= xx1; ++xx) { line [xx] = 0xff; }
        }

        state0 = state1;
    }

    //--- update xCur0 values
    for (i = 0; i < activeSegs.size (); ++i) {
        seg = activeSegs [i];
        seg->xCur0 = seg->xCur1;
    }

    yNext = y + 1;
}

inline void SplashXPathScanner::addArea (unsigned char* line, int x, SplashCoord a) {
    int a2, t;

    a2 = splashRound (a * 255);
    if (a2 <= 0) { return; }
    t = line [x] + a2;
    if (t > 255) { t = 255; }
    line [x] = t;
}

// Draw a trapezoid with edges:
//   top:    (xa0, y0) - (xb0, y0)
//   left:   (xa0, y0) - (xa1, y1)
//   right:  (xb0, y0) - (xb1, y1)
//   bottom: (xa1, y1) - (xb1, y1)
void SplashXPathScanner::drawTrapezoid (
    unsigned char* line,
    int xMin, int xMax,
    SplashCoord y0,  SplashCoord y1,
    SplashCoord xa0, SplashCoord xa1, SplashCoord dydxa,
    SplashCoord xb0, SplashCoord xb1, SplashCoord dydxb) {

    SplashCoord a, dy;
    int x0, x1, x2, x3, x;

    // check for a rectangle
    if (dydxa == 0 && dydxb == 0 && xa0 >= xMin && xb0 <= xMax) {
        x0 = splashFloor (xa0);
        x3 = splashFloor (xb0);
        dy = y1 - y0;
        if (x0 == x3) { addArea (line, x0, (xb0 - xa0) * dy); }
        else {
            addArea (line, x0, ((SplashCoord)1 - (xa0 - x0)) * dy);
            for (x = x0 + 1; x <= x3 - 1; ++x) { addArea (line, x, y1 - y0); }
            addArea (line, x3, (xb0 - x3) * (y1 - y0));
        }
        return;
    }

    if (dydxa > 0) {
        x0 = splashFloor (xa0);
        x1 = splashFloor (xa1);
    }
    else {
        x0 = splashFloor (xa1);
        x1 = splashFloor (xa0);
    }
    if (x0 < xMin) { x0 = xMin; }
    if (dydxb > 0) {
        x2 = splashFloor (xb0);
        x3 = splashFloor (xb1);
    }
    else {
        x2 = splashFloor (xb1);
        x3 = splashFloor (xb0);
    }
    if (x3 > xMax) { x3 = xMax; }
    for (x = x0; x <= x3; ++x) {
        a = y1 - y0;
        if (x <= x1) { a -= areaLeft (x, xa0, y0, xa1, y1, dydxa); }
        if (x >= x2) { a -= areaRight (x, xb0, y0, xb1, y1, dydxb); }
        addArea (line, x, a);
    }
}

// Compute area within a pixel slice ((xp,y0)-(xp+1,y1)) to the left
// of a trapezoid edge ((x0,y0)-(x1,y1)).
SplashCoord SplashXPathScanner::areaLeft (
    int xp,
    SplashCoord x0, SplashCoord y0,
    SplashCoord x1, SplashCoord y1,
    SplashCoord dydx) {

    SplashCoord a, ya, yb;

    if (dydx >= 0) {
        if (x0 >= xp) {
            if (x1 <= xp + 1) { a = ((x0 + x1) * 0.5 - xp) * (y1 - y0); }
            else {
                yb = y0 + ((SplashCoord) (xp + 1) - x0) * dydx;
                a = (y1 - y0) - ((SplashCoord) (xp + 1) - x0) * (yb - y0) * 0.5;
            }
        }
        else {
            if (x1 <= xp + 1) {
                ya = y0 + ((SplashCoord)xp - x0) * dydx;
                a = (x1 - xp) * (y1 - ya) * 0.5;
            }
            else {
                // ya = y1 - (x1 - xp - 0.5) * dydx;
                // a = y1 - ya;
                a = (x1 - xp - 0.5) * dydx;
            }
        }
    }
    else {
        if (x0 <= xp + 1) {
            if (x1 >= xp) { a = ((x0 + x1) * 0.5 - xp) * (y1 - y0); }
            else {
                ya = y0 + ((SplashCoord)xp - x0) * dydx;
                a = (x0 - xp) * (ya - y0) * 0.5;
            }
        }
        else {
            if (x1 >= xp) {
                yb = y0 + ((SplashCoord) (xp + 1) - x0) * dydx;
                a = (y1 - y0) - ((SplashCoord) (xp + 1) - x1) * (y1 - yb) * 0.5;
            }
            else {
                // ya = y0 + (xp - x0 + 0.5) * dydx;
                // a = ya - y0;
                a = ((SplashCoord)xp - x0 + 0.5) * dydx;
            }
        }
    }
    return a;
}

// Compute area within a pixel slice ((xp,y0)-(xp+1,y1)) to the left
// of a trapezoid edge ((x0,y0)-(x1,y1)).
SplashCoord SplashXPathScanner::areaRight (
    int xp,
    SplashCoord x0, SplashCoord y0,
    SplashCoord x1, SplashCoord y1,
    SplashCoord dydx) {

    SplashCoord a, ya, yb;

    if (dydx >= 0) {
        if (x0 >= xp) {
            if (x1 <= xp + 1) {
                a = ((SplashCoord) (xp + 1) - (x0 + x1) * 0.5) * (y1 - y0);
            }
            else {
                yb = y0 + ((SplashCoord) (xp + 1) - x0) * dydx;
                a = ((SplashCoord) (xp + 1) - x0) * (yb - y0) * 0.5;
            }
        }
        else {
            if (x1 <= xp + 1) {
                ya = y0 + ((SplashCoord)xp - x0) * dydx;
                a = (y1 - y0) - (x1 - xp) * (y1 - ya) * 0.5;
            }
            else {
                // ya = y0 + (xp - x0 + 0.5) * dydx;
                // a = ya - y0;
                a = ((SplashCoord)xp + 0.5 - x0) * dydx;
            }
        }
    }
    else {
        if (x0 <= xp + 1) {
            if (x1 >= xp) {
                a = ((SplashCoord) (xp + 1) - (x0 + x1) * 0.5) * (y1 - y0);
            }
            else {
                ya = y0 + ((SplashCoord)xp - x0) * dydx;
                a = (y1 - y0) - (x0 - xp) * (ya - y0) * 0.5;
            }
        }
        else {
            if (x1 >= xp) {
                yb = y0 + ((SplashCoord) (xp + 1) - x0) * dydx;
                a = ((SplashCoord) (xp + 1) - x1) * (y1 - yb) * 0.5;
            }
            else {
                // ya = y1 - (x1 - xp - 0.5) * dydx;
                // a = y1 - ya;
                a = (x1 - xp - 0.5) * dydx;
            }
        }
    }
    return a;
}

void SplashXPathScanner::drawRectangle (
    unsigned char* line, int xMin, int xMax, SplashCoord y0, SplashCoord y1,
    SplashCoord x0, SplashCoord x1) {
    SplashCoord dy, a;
    int xx0, xx1, x;

    xx0 = splashFloor (x0);
    if (xx0 < xMin) { xx0 = xMin; }
    xx1 = splashFloor (x1);
    if (xx1 > xMax) { xx1 = xMax; }
    dy = y1 - y0;
    for (x = xx0; x <= xx1; ++x) {
        a = dy;
        if ((SplashCoord)x < x0) { a -= (x0 - x) * dy; }
        if ((SplashCoord) (x + 1) > x1) {
            a -= ((SplashCoord) (x + 1) - x1) * dy;
        }
        addArea (line, x, a);
    }
}

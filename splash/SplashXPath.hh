// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHXPATH_HH
#define XPDF_SPLASH_SPLASHXPATH_HH

#include <defs.hh>

#include <splash/SplashTypes.hh>

class SplashPath;
struct SplashXPathPoint;
struct SplashPathHint;

//------------------------------------------------------------------------

#define splashMaxCurveSplits (1 << 10)

//------------------------------------------------------------------------
// SplashXPathSeg
//------------------------------------------------------------------------

struct SplashXPathSeg {
    //
    // x0, y0       : first endpoint (y0 <= y1)
    // x1, y1       : second endpoint
    // dxdy         : slope: delta-x / delta-y
    // dydx         : slope: delta-y / delta-x
    // xCur0, xCur1 : current x values
    //
    SplashCoord x0, y0, x1, y1, dxdy, dydx, xCur0, xCur1;

    //
    // EO/NZWN counter increment
    //
    int count;
};

//------------------------------------------------------------------------
// SplashXPath
//------------------------------------------------------------------------

class SplashXPath {
public:
    // Expands (converts to segments) and flattens (converts curves to
    // lines) <path>.  Transforms all points from user space to device
    // space, via <matrix>.  If <closeSubpaths> is true, closes all open
    // subpaths.
    SplashXPath (
        SplashPath* path, SplashCoord* matrix, SplashCoord flatness,
        bool closeSubpaths);

    // Copy an expanded path.
    SplashXPath* copy () { return new SplashXPath (this); }

    ~SplashXPath ();

    int getXMin () { return xMin; }
    int getXMax () { return xMax; }
    int getYMin () { return yMin; }
    int getYMax () { return yMax; }

private:
    SplashXPath (SplashXPath* xPath);
    void transform (
        SplashCoord* matrix, SplashCoord xi, SplashCoord yi, SplashCoord* xo,
        SplashCoord* yo);
    void
    strokeAdjust (SplashXPathPoint* pts, SplashPathHint* hints, int nHints);
    void grow (int nSegs);
    void addCurve (
        SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1,
        SplashCoord x2, SplashCoord y2, SplashCoord x3, SplashCoord y3,
        SplashCoord flatness, bool first, bool last, bool end0, bool end1);
    void
    addSegment (SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);

    SplashXPathSeg* segs;
    int length, size; // length and size of segs array
    int xMin, xMax;
    int yMin, yMax;

    friend class SplashXPathScanner;
    friend class SplashClip;
    friend class Splash;
};

#endif // XPDF_SPLASH_SPLASHXPATH_HH

// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHCLIP_HH
#define XPDF_SPLASH_SPLASHCLIP_HH

#include <defs.hh>

#include <splash/SplashTypes.hh>
#include <splash/SplashMath.hh>

class SplashPath;
class SplashXPath;
class SplashXPathScanner;
class SplashBitmap;

//------------------------------------------------------------------------

enum SplashClipResult {
    splashClipAllInside,
    splashClipAllOutside,
    splashClipPartial
};

//------------------------------------------------------------------------
// SplashClip
//------------------------------------------------------------------------

class SplashClip {
public:
    // Create a clip, for the given rectangle.
    SplashClip (int hardXMinA, int hardYMinA, int hardXMaxA, int hardYMaxA);

    // Copy a clip.
    SplashClip* copy () { return new SplashClip (this); }

    ~SplashClip ();

    // Reset the clip to a rectangle.
    void resetToRect (
        SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);

    // Intersect the clip with a rectangle.
    SplashError
    clipToRect (SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);

    // Interesect the clip with <path>.
    SplashError clipToPath (
        SplashPath* path, SplashCoord* matrix, SplashCoord flatness, bool eoA);

    // Tests a rectangle against the clipping region.  Returns one of:
    //   - splashClipAllInside if the entire rectangle is inside the
    //     clipping region, i.e., all pixels in the rectangle are
    //     visible
    //   - splashClipAllOutside if the entire rectangle is outside the
    //     clipping region, i.e., all the pixels in the rectangle are
    //     clipped
    //   - splashClipPartial if the rectangle is part inside and part
    //     outside the clipping region
    SplashClipResult testRect (
        int rectXMin, int rectYMin, int rectXMax, int rectYMax,
        bool strokeAdjust);

    // Clip a scan line.  Modifies line[] by multiplying with clipping
    // shape values for one scan line: ([x0, x1], y).
    void clipSpan (unsigned char* line, int y, int x0, int x1, bool strokeAdjust);

    // Like clipSpan(), but uses the values 0 and 255 only.
    // Returns true if there are any non-zero values in the result
    // (i.e., returns false if the entire line is clipped out).
    bool
    clipSpanBinary (unsigned char* line, int y, int x0, int x1, bool strokeAdjust);

    // Get the rectangle part of the clip region.
    SplashCoord getXMin () { return xMin; }
    SplashCoord getXMax () { return xMax; }
    SplashCoord getYMin () { return yMin; }
    SplashCoord getYMax () { return yMax; }

    // Get the rectangle part of the clip region, in integer coordinates.
    int getXMinI (bool strokeAdjust);
    int getXMaxI (bool strokeAdjust);
    int getYMinI (bool strokeAdjust);
    int getYMaxI (bool strokeAdjust);

    // Get the number of arbitrary paths used by the clip region.
    int getNumPaths () { return length; }

private:
    SplashClip (SplashClip* clip);
    void grow (int nPaths);
    void updateIntBounds (bool strokeAdjust);

    int hardXMin, hardYMin, // coordinates cannot fall outside of
        hardXMax, hardYMax; //   [hardXMin, hardXMax), [hardYMin, hardYMax)

    SplashCoord xMin, yMin, // current clip bounding rectangle
        xMax, yMax;         //   (these coordinates may be adjusted if
                            //   stroke adjustment is enabled)

    int xMinI, yMinI, xMaxI, yMaxI;
    bool intBoundsValid;        // true if xMinI, etc. are valid
    bool intBoundsStrokeAdjust; // value of strokeAdjust used to compute
                                 //   xMinI, etc.

    SplashXPath** paths;
    unsigned char* eo;
    SplashXPathScanner** scanners;
    int length, size;
    unsigned char* buf;
};

#endif // XPDF_SPLASH_SPLASHCLIP_HH

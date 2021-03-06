// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#ifndef XPDF_SPLASH_SPLASHXPATHSCANNER_HH
#define XPDF_SPLASH_SPLASHXPATHSCANNER_HH

#include <defs.hh>

#include <splash/SplashTypes.hh>

#include <vector>

class SplashXPath;

//------------------------------------------------------------------------
// SplashXPathScanner
//------------------------------------------------------------------------

class SplashXPathScanner
{
public:
    // Create a new SplashXPathScanner object.  <xPathA> must be sorted.
    SplashXPathScanner(SplashXPath *xPathA, bool eoA, int yMinA, int yMaxA);

    // Compute shape values for a scan line.  Fills in line[] with shape
    // values for one scan line: ([x0, x1], y).  The values are in [0,
    // 255].
    void getSpan(unsigned char *line, int y, int x0, int x1);

    // Like getSpan(), but uses the values 0 and 255 only.  Writes 255
    // for all pixels which include non-zero area inside the path.
    void getSpanBinary(unsigned char *line, int y, int x0, int x1);

private:
    inline void addArea(unsigned char *line, int x, SplashCoord a);

    void drawTrapezoid(unsigned char *line, int xMin, int xMax, SplashCoord y0,
                       SplashCoord y1, SplashCoord xa0, SplashCoord xa1,
                       SplashCoord dydxa, SplashCoord xb0, SplashCoord xb1,
                       SplashCoord dydxb);

    SplashCoord areaLeft(int xp, SplashCoord x0, SplashCoord y0, SplashCoord x1,
                         SplashCoord y1, SplashCoord dydx);

    SplashCoord areaRight(int xp, SplashCoord x0, SplashCoord y0, SplashCoord x1,
                          SplashCoord y1, SplashCoord dydx);

    void drawRectangle(unsigned char *line, int xMin, int xMax, SplashCoord y0,
                       SplashCoord y1, SplashCoord x0, SplashCoord x1);

    SplashXPath *xPath;
    bool         eo;
    int          yMin, yMax, yNext;

    std::vector< SplashXPathSeg * > activeSegs;
    int                             nextSeg;
};

#endif // XPDF_SPLASH_SPLASHXPATHSCANNER_HH

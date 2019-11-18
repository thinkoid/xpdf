//========================================================================
//
// SplashBitmap.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#ifndef SPLASHBITMAP_H
#define SPLASHBITMAP_H

#include <defs.hh>

#include <cstdio>
#include <splash/SplashTypes.hh>

//------------------------------------------------------------------------
// SplashBitmap
//------------------------------------------------------------------------

class SplashBitmap {
public:
    // Create a new bitmap.  It will have <widthA> x <heightA> pixels in
    // color mode <modeA>.  Rows will be padded out to a multiple of
    // <rowPad> bytes.  If <topDown> is false, the bitmap will be stored
    // upside-down, i.e., with the last row first in memory.
    SplashBitmap (
        int widthA, int heightA, int rowPad, SplashColorMode modeA,
        bool alphaA, bool topDown = true);

    ~SplashBitmap ();

    int getWidth () { return width; }
    int getHeight () { return height; }
    int getRowSize () { return rowSize; }
    int getAlphaRowSize () { return width; }
    SplashColorMode getMode () { return mode; }
    SplashColorPtr getDataPtr () { return data; }
    unsigned char* getAlphaPtr () { return alpha; }

    SplashError writePNMFile (const char* fileName);
    SplashError writePNMFile (FILE* f);
    SplashError writeAlphaPGMFile (char* fileName);

    void getPixel (int x, int y, SplashColorPtr pixel);
    unsigned char getAlpha (int x, int y);

    // Caller takes ownership of the bitmap data.  The SplashBitmap
    // object is no longer valid -- the next call should be to the
    // destructor.
    SplashColorPtr takeData ();

private:
    int width, height;    // size of bitmap
    int rowSize;          // size of one row of data, in bytes
                          //   - negative for bottom-up bitmaps
    SplashColorMode mode; // color mode
    SplashColorPtr data;  // pointer to row zero of the color data
    unsigned char* alpha;        // pointer to row zero of the alpha data
                          //   (always top-down)

    friend class Splash;
};

#endif

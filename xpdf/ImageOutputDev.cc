// -*- mode: c++; -*-
// Copyright 1998-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cctype>
#include <defs.hh>
#include <xpdf/Error.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/object.hh>
#include <xpdf/Stream.hh>
#include <xpdf/ImageOutputDev.hh>

ImageOutputDev::ImageOutputDev (char* fileRootA, bool dumpJPEGA) {
    fileRoot = strdup (fileRootA);
    fileName = (char*)malloc ((int)strlen (fileRoot) + 20);
    dumpJPEG = dumpJPEGA;
    imgNum = 0;
    ok = true;
}

ImageOutputDev::~ImageOutputDev () {
    free (fileName);
    free (fileRoot);
}

void ImageOutputDev::tilingPatternFill (
    GfxState* state, Gfx* gfx, Object* strRef, int paintType, Dict* resDict,
    double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep,
    double yStep) {
    // do nothing -- this avoids the potentially slow loop in Gfx.cc
}

void ImageOutputDev::drawImageMask (
    GfxState* state, Object* ref, Stream* str, int width, int height,
    bool invert, bool inlineImg, bool interpolate) {
    FILE* f;
    char buf[4096];
    int size, n, i;

    // dump JPEG file
    if (dumpJPEG && str->getKind () == strDCT && !inlineImg) {
        // open the image file
        sprintf (fileName, "%s-%04d.jpg", fileRoot, imgNum);
        ++imgNum;
        if (!(f = fopen (fileName, "wb"))) {
            error (errIO, -1, "Couldn't open image file '{0:s}'", fileName);
            return;
        }

        // initialize stream
        str = ((DCTStream*)str)->getRawStream ();
        str->reset ();

        // copy the stream
        while ((n = str->getBlock (buf, sizeof (buf))) > 0) {
            fwrite (buf, 1, n, f);
        }

        str->close ();
        fclose (f);

        // dump PBM file
    }
    else {
        // open the image file and write the PBM header
        sprintf (fileName, "%s-%04d.pbm", fileRoot, imgNum);
        ++imgNum;
        if (!(f = fopen (fileName, "wb"))) {
            error (errIO, -1, "Couldn't open image file '{0:s}'", fileName);
            return;
        }
        fprintf (f, "P4\n");
        fprintf (f, "%d %d\n", width, height);

        // initialize stream
        str->reset ();

        // copy the stream
        size = height * ((width + 7) / 8);
        while (size > 0) {
            i = size < (int)sizeof (buf) ? size : (int)sizeof (buf);
            n = str->getBlock (buf, i);
            fwrite (buf, 1, n, f);
            if (n < i) { break; }
            size -= n;
        }

        str->close ();
        fclose (f);
    }
}

void ImageOutputDev::drawImage (
    GfxState* state, Object* ref, Stream* str, int width, int height,
    GfxImageColorMap* colorMap, int* maskColors, bool inlineImg,
    bool interpolate) {
    FILE* f;
    ImageStream* imgStr;
    unsigned char* p;
    GfxRGB rgb;
    int x, y;
    char buf[4096];
    int size, n, i;

    // dump JPEG file
    if (dumpJPEG && str->getKind () == strDCT &&
        (colorMap->getNumPixelComps () == 1 ||
         colorMap->getNumPixelComps () == 3) &&
        !inlineImg) {
        // open the image file
        sprintf (fileName, "%s-%04d.jpg", fileRoot, imgNum);
        ++imgNum;
        if (!(f = fopen (fileName, "wb"))) {
            error (errIO, -1, "Couldn't open image file '{0:s}'", fileName);
            return;
        }

        // initialize stream
        str = ((DCTStream*)str)->getRawStream ();
        str->reset ();

        // copy the stream
        while ((n = str->getBlock (buf, sizeof (buf))) > 0) {
            fwrite (buf, 1, n, f);
        }

        str->close ();
        fclose (f);

        // dump PBM file
    }
    else if (colorMap->getNumPixelComps () == 1 && colorMap->getBits () == 1) {
        // open the image file and write the PBM header
        sprintf (fileName, "%s-%04d.pbm", fileRoot, imgNum);
        ++imgNum;
        if (!(f = fopen (fileName, "wb"))) {
            error (errIO, -1, "Couldn't open image file '{0:s}'", fileName);
            return;
        }
        fprintf (f, "P4\n");
        fprintf (f, "%d %d\n", width, height);

        // initialize stream
        str->reset ();

        // copy the stream
        size = height * ((width + 7) / 8);
        while (size > 0) {
            i = size < (int)sizeof (buf) ? size : (int)sizeof (buf);
            n = str->getBlock (buf, i);
            fwrite (buf, 1, n, f);
            if (n < i) { break; }
            size -= n;
        }

        str->close ();
        fclose (f);

        // dump PPM file
    }
    else {
        // open the image file and write the PPM header
        sprintf (fileName, "%s-%04d.ppm", fileRoot, imgNum);
        ++imgNum;
        if (!(f = fopen (fileName, "wb"))) {
            error (errIO, -1, "Couldn't open image file '{0:s}'", fileName);
            return;
        }
        fprintf (f, "P6\n");
        fprintf (f, "%d %d\n", width, height);
        fprintf (f, "255\n");

        // initialize stream
        imgStr = new ImageStream (
            str, width, colorMap->getNumPixelComps (), colorMap->getBits ());
        imgStr->reset ();

        // for each line...
        for (y = 0; y < height; ++y) {
            // write the line
            if ((p = imgStr->getLine ())) {
                for (x = 0; x < width; ++x) {
                    colorMap->getRGB (p, &rgb);
                    fputc (xpdf::to_small_color (rgb.r), f);
                    fputc (xpdf::to_small_color (rgb.g), f);
                    fputc (xpdf::to_small_color (rgb.b), f);
                    p += colorMap->getNumPixelComps ();
                }
            }
            else {
                for (x = 0; x < width; ++x) {
                    fputc (0, f);
                    fputc (0, f);
                    fputc (0, f);
                }
            }
        }

        imgStr->close ();
        delete imgStr;

        fclose (f);
    }
}

void ImageOutputDev::drawMaskedImage (
    GfxState* state, Object* ref, Stream* str, int width, int height,
    GfxImageColorMap* colorMap, Stream* maskStr, int maskWidth, int maskHeight,
    bool maskInvert, bool interpolate) {
    drawImage (
        state, ref, str, width, height, colorMap, NULL, false, interpolate);
    drawImageMask (
        state, ref, maskStr, maskWidth, maskHeight, maskInvert, false,
        interpolate);
}

void ImageOutputDev::drawSoftMaskedImage (
    GfxState* state, Object* ref, Stream* str, int width, int height,
    GfxImageColorMap* colorMap, Stream* maskStr, int maskWidth, int maskHeight,
    GfxImageColorMap* maskColorMap, bool interpolate) {
    drawImage (
        state, ref, str, width, height, colorMap, NULL, false, interpolate);
    drawImage (
        state, ref, maskStr, maskWidth, maskHeight, maskColorMap, NULL, false,
        interpolate);
}

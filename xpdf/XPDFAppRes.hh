// -*- mode: c++; -*-
// Copyright 2002-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_XPDFAPPRES_HH
#define XPDF_XPDF_XPDFAPPRES_HH

#include <defs.hh>

#include <X11/X.h>
#include <X11/Xresource.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

struct XPDFAppResources
{
    String geometry, title;
    Bool   installCmap;
    int    rgbCubeSize;
    Bool   reverseVideo;
    String paperColor, matteColor, fullScreenMatteColor, initialZoom;
};

char **fallbackResources();
size_t fallbackResourcesSize();

XrmOptionDescRec *xOpts();
size_t            xOptsSize();

XtResource *xResources();
size_t      xResourcesSize();

#endif // XPDF_XPDF_XPDFAPPRES_HH

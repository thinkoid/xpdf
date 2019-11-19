//========================================================================
//
// XPDFAppRes.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <X11/X.h>
#include <X11/Xresource.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

struct XPDFAppResources {
    String geometry, title;
    Bool installCmap;
    int rgbCubeSize;
    Bool reverseVideo;
    String paperColor, matteColor, fullScreenMatteColor, initialZoom;
};

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

char** fallbackResources ();
size_t fallbackResourcesSize ();

XrmOptionDescRec* xOpts ();
size_t xOptsSize ();

XtResource* xResources ();
size_t xResourcesSize ();

#ifdef __cplusplus
}
#endif // __cplusplus

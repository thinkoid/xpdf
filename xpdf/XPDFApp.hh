//========================================================================
//
// XPDFApp.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef XPDFAPP_H
#define XPDFAPP_H

#include <defs.hh>

#define Object XtObject
#include <Xm/XmAll.h>
#undef Object
#include <splash/SplashTypes.hh>

class GString;
class GList;
class PDFDoc;
class XPDFViewer;

//------------------------------------------------------------------------

#define xpdfAppName "Xpdf"

//------------------------------------------------------------------------
// XPDFApp
//------------------------------------------------------------------------

class XPDFApp {
public:
    XPDFApp (int* argc, char* argv[]);
    ~XPDFApp ();

    XPDFViewer* open (
        GString* fileName, int page = 1, GString* ownerPassword = NULL,
        GString* userPassword = NULL);
    XPDFViewer* openAtDest (
        GString* fileName, GString* dest, GString* ownerPassword = NULL,
        GString* userPassword = NULL);
    XPDFViewer*
    reopen (XPDFViewer* viewer, PDFDoc* doc, int page, bool fullScreenA);
    void close (XPDFViewer* viewer, bool closeLast);
    void quit ();

    void run ();

    //----- remote server
    void setRemoteName (char* remoteName);
    bool remoteServerRunning ();
    void remoteExec (char* cmd);
    void remoteOpen (GString* fileName, int page, bool raise);
    void remoteOpenAtDest (GString* fileName, GString* dest, bool raise);
    void remoteReload (bool raise);
    void remoteRaise ();
    void remoteQuit ();

    //----- resource/option values
    GString* getGeometry () { return geometry; }
    GString* getTitle () { return title; }
    bool getInstallCmap () { return installCmap; }
    int getRGBCubeSize () { return rgbCubeSize; }
    bool getReverseVideo () { return reverseVideo; }
    SplashColorPtr getPaperRGB () { return paperRGB; }
    size_t getPaperPixel () { return paperPixel; }
    size_t getMattePixel (bool fullScreenA) {
        return fullScreenA ? fullScreenMattePixel : mattePixel;
    }
    GString* getInitialZoom () { return initialZoom; }
    void setFullScreen (bool fullScreenA) { fullScreen = fullScreenA; }
    bool getFullScreen () { return fullScreen; }

    XtAppContext getAppContext () { return appContext; }
    Widget getAppShell () { return appShell; }

private:
    void getResources ();
    static void
    remoteMsgCbk (Widget widget, XtPointer ptr, XEvent* event, Boolean* cont);

    Display* display;
    int screenNum;
    XtAppContext appContext;
    Widget appShell;
    GList* viewers; // [XPDFViewer]

    Atom remoteAtom;
    Window remoteXWin;
    XPDFViewer* remoteViewer;
    Widget remoteWin;

    //----- resource/option values
    GString* geometry;
    GString* title;
    bool installCmap;
    int rgbCubeSize;
    bool reverseVideo;
    SplashColor paperRGB;
    size_t paperPixel;
    size_t mattePixel;
    size_t fullScreenMattePixel;
    GString* initialZoom;
    bool fullScreen;
};

#endif

// -*- mode: c++; -*-
// Copyright 2002-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <goo/GString.hh>
#include <goo/GList.hh>

#include <xpdf/Error.hh>
#include <xpdf/XPDFUI.hh>
#include <xpdf/XPDFApp.hh>

#include <xpdf/XPDFAppRes.hh>

// these macro defns conflict with xpdf's Object class
#ifdef LESSTIF_VERSION
#undef XtDisplay
#undef XtScreen
#undef XtWindow
#undef XtParent
#undef XtIsRealized
#endif

//------------------------------------------------------------------------

#define remoteCmdSize 512

//------------------------------------------------------------------------

//------------------------------------------------------------------------
// XPDFApp
//------------------------------------------------------------------------

#if 0 //~ for debugging
static int xErrorHandler(Display *display, XErrorEvent *ev) {
  printf("X error:\n");
  printf("  resource ID = %08lx\n", ev->resourceid);
  printf("  serial = %lu\n", ev->serial);
  printf("  error_code = %d\n", ev->error_code);
  printf("  request_code = %d\n", ev->request_code);
  printf("  minor_code = %d\n", ev->minor_code);
  fflush(stdout);
  abort();
}
#endif

XPDFApp::XPDFApp (int* argc, char* argv[]) {
    appShell = XtAppInitialize (
        &appContext, xpdfAppName, xOpts (), xOptsSize (), argc, argv,
        fallbackResources (), NULL, 0);

    display = XtDisplay (appShell);
    screenNum = XScreenNumberOfScreen (XtScreen (appShell));
#if XmVERSION > 1
    XtVaSetValues (
        XmGetXmDisplay (XtDisplay (appShell)), XmNenableButtonTab, True, NULL);
#endif
#if XmVERSION > 1
    // Drag-and-drop appears to be buggy -- I'm seeing weird crashes
    // deep in the Motif code when I destroy widgets in the XpdfForms
    // code.  Xpdf doesn't use it, so just turn it off.
    XtVaSetValues (
        XmGetXmDisplay (XtDisplay (appShell)), XmNdragInitiatorProtocolStyle,
        XmDRAG_NONE, XmNdragReceiverProtocolStyle, XmDRAG_NONE, NULL);
#endif

#if 0 //~ for debugging
  XSynchronize(display, True);
  XSetErrorHandler(&xErrorHandler);
#endif

    fullScreen = false;
    remoteAtom = None;
    remoteViewer = NULL;
    remoteWin = None;

    getResources ();

    viewers = new GList ();
}

void XPDFApp::getResources () {
    XPDFAppResources resources;
    XColor xcol, xcol2;
    Colormap colormap;

    XtGetApplicationResources (
        appShell, &resources, xResources (), xResourcesSize (), NULL, 0);

    geometry = resources.geometry
        ? new GString (resources.geometry) : (GString*)NULL;

    title = resources.title
        ? new GString (resources.title) : (GString*)NULL;

    installCmap = (bool)resources.installCmap;
    rgbCubeSize = resources.rgbCubeSize;
    reverseVideo = (bool)resources.reverseVideo;

    if (reverseVideo) {
        paperRGB[0] = paperRGB[1] = paperRGB[2] = 0;
        paperPixel = BlackPixel (display, screenNum);
    }
    else {
        paperRGB[0] = paperRGB[1] = paperRGB[2] = 0xff;
        paperPixel = WhitePixel (display, screenNum);
    }

    XtVaGetValues (appShell, XmNcolormap, &colormap, NULL);

    if (resources.paperColor) {
        if (XAllocNamedColor (
                display, colormap, resources.paperColor, &xcol, &xcol2)) {
            paperRGB[0] = xcol.red >> 8;
            paperRGB[1] = xcol.green >> 8;
            paperRGB[2] = xcol.blue >> 8;
            paperPixel = xcol.pixel;
        }
        else {
            error (
                errIO, -1, "Couldn't allocate color '{0:s}'",
                resources.paperColor);
        }
    }
    if (XAllocNamedColor (
            display, colormap, resources.matteColor, &xcol, &xcol2)) {
        mattePixel = xcol.pixel;
    }
    else {
        mattePixel = paperPixel;
    }
    if (XAllocNamedColor (
            display, colormap, resources.fullScreenMatteColor, &xcol, &xcol2)) {
        fullScreenMattePixel = xcol.pixel;
    }
    else {
        fullScreenMattePixel = paperPixel;
    }
    initialZoom = resources.initialZoom ? new GString (resources.initialZoom)
                                        : (GString*)NULL;
}

XPDFApp::~XPDFApp () {
    deleteGList (viewers, XPDFUI);
    if (geometry) { delete geometry; }
    if (title) { delete title; }
    if (initialZoom) { delete initialZoom; }
}

XPDFUI* XPDFApp::open (
    GString* fileName, int page, GString* ownerPassword,
    GString* userPassword) {
    XPDFUI* viewer;

    viewer = new XPDFUI (
        this, fileName, page, NULL, fullScreen, ownerPassword, userPassword);
    if (!viewer->isOk ()) {
        delete viewer;
        return NULL;
    }
    if (remoteAtom != None) {
        remoteViewer = viewer;
        remoteWin = viewer->getWindow ();
        XtAddEventHandler (
            remoteWin, PropertyChangeMask, False, &remoteMsgCbk, this);
        XSetSelectionOwner (
            display, remoteAtom, XtWindow (remoteWin), CurrentTime);
    }
    viewers->append (viewer);
    return viewer;
}

XPDFUI* XPDFApp::openAtDest (
    GString* fileName, GString* dest, GString* ownerPassword,
    GString* userPassword) {
    XPDFUI* viewer;

    viewer = new XPDFUI (
        this, fileName, 1, dest, fullScreen, ownerPassword, userPassword);
    if (!viewer->isOk ()) {
        delete viewer;
        return NULL;
    }
    if (remoteAtom != None) {
        remoteViewer = viewer;
        remoteWin = viewer->getWindow ();
        XtAddEventHandler (
            remoteWin, PropertyChangeMask, False, &remoteMsgCbk, this);
        XSetSelectionOwner (
            display, remoteAtom, XtWindow (remoteWin), CurrentTime);
    }
    viewers->append (viewer);
    return viewer;
}

XPDFUI*
XPDFApp::reopen (XPDFUI* viewer, PDFDoc* doc, int page, bool fullScreenA) {
    int i;

    for (i = 0; i < viewers->getLength (); ++i) {
        if (((XPDFUI*)viewers->get (i)) == viewer) {
            viewers->del (i);
            delete viewer;
        }
    }
    viewer = new XPDFUI (this, doc, page, NULL, fullScreenA);
    if (!viewer->isOk ()) {
        delete viewer;
        return NULL;
    }
    if (remoteAtom != None) {
        remoteViewer = viewer;
        remoteWin = viewer->getWindow ();
        XtAddEventHandler (
            remoteWin, PropertyChangeMask, False, &remoteMsgCbk, this);
        XSetSelectionOwner (
            display, remoteAtom, XtWindow (remoteWin), CurrentTime);
    }
    viewers->append (viewer);
    return viewer;
}

void XPDFApp::close (XPDFUI* viewer, bool closeLast) {
    int i;

    if (viewers->getLength () == 1) {
        if (viewer != (XPDFUI*)viewers->get (0)) { return; }
        if (closeLast) { quit (); }
        else {
            viewer->clear ();
        }
    }
    else {
        for (i = 0; i < viewers->getLength (); ++i) {
            if (((XPDFUI*)viewers->get (i)) == viewer) {
                viewers->del (i);
                if (remoteAtom != None && remoteViewer == viewer) {
                    remoteViewer =
                        (XPDFUI*)viewers->get (viewers->getLength () - 1);
                    remoteWin = remoteViewer->getWindow ();
                    XSetSelectionOwner (
                        display, remoteAtom, XtWindow (remoteWin), CurrentTime);
                }
                delete viewer;
                return;
            }
        }
    }
}

void XPDFApp::quit () {
    if (remoteAtom != None) {
        XSetSelectionOwner (display, remoteAtom, None, CurrentTime);
    }
    while (viewers->getLength () > 0) { delete (XPDFUI*)viewers->del (0); }
    XtAppSetExitFlag (appContext);
}

void XPDFApp::run () { XtAppMainLoop (appContext); }

void XPDFApp::setRemoteName (char* remoteName) {
    remoteAtom = XInternAtom (display, remoteName, False);
    remoteXWin = XGetSelectionOwner (display, remoteAtom);
}

bool XPDFApp::remoteServerRunning () { return remoteXWin != None; }

void XPDFApp::remoteExec (char* cmd) {
    char cmd2[remoteCmdSize];
    int n;

    n = strlen (cmd);
    if (n > remoteCmdSize - 2) { n = remoteCmdSize - 2; }
    memcpy (cmd2, cmd, n);
    cmd2[n] = '\n';
    cmd2[n + 1] = '\0';
    XChangeProperty (
        display, remoteXWin, remoteAtom, remoteAtom, 8, PropModeReplace,
        (unsigned char*)cmd2, n + 2);
    XFlush (display);
}

void XPDFApp::remoteOpen (GString* fileName, int page, bool raise) {
    char cmd[remoteCmdSize];

    sprintf (cmd, "openFileAtPage(%.200s,%d)\n", fileName->c_str (), page);
    if (raise) { strcat (cmd, "raise\n"); }
    XChangeProperty (
        display, remoteXWin, remoteAtom, remoteAtom, 8, PropModeReplace,
        (unsigned char*)cmd, strlen (cmd) + 1);
    XFlush (display);
}

void XPDFApp::remoteOpenAtDest (GString* fileName, GString* dest, bool raise) {
    char cmd[remoteCmdSize];

    sprintf (
        cmd, "openFileAtDest(%.200s,%.256s)\n", fileName->c_str (),
        dest->c_str ());
    if (raise) { strcat (cmd, "raise\n"); }
    XChangeProperty (
        display, remoteXWin, remoteAtom, remoteAtom, 8, PropModeReplace,
        (unsigned char*)cmd, strlen (cmd) + 1);
    XFlush (display);
}

void XPDFApp::remoteReload (bool raise) {
    char cmd[remoteCmdSize];

    strcpy (cmd, "reload\n");
    if (raise) { strcat (cmd, "raise\n"); }
    XChangeProperty (
        display, remoteXWin, remoteAtom, remoteAtom, 8, PropModeReplace,
        (unsigned char*)cmd, strlen (cmd) + 1);
    XFlush (display);
}

void XPDFApp::remoteRaise () {
    XChangeProperty (
        display, remoteXWin, remoteAtom, remoteAtom, 8, PropModeReplace,
        (unsigned char*)"raise\n", 7);
    XFlush (display);
}

void XPDFApp::remoteQuit () {
    XChangeProperty (
        display, remoteXWin, remoteAtom, remoteAtom, 8, PropModeReplace,
        (unsigned char*)"quit\n", 6);
    XFlush (display);
}

void XPDFApp::remoteMsgCbk (
    Widget widget, XtPointer ptr, XEvent* event, Boolean* cont) {
    XPDFApp* app = (XPDFApp*)ptr;
    char *cmd, *p0, *p1;
    Atom type;
    int format;
    size_t size, remain;
    GString* cmdStr;

    if (event->xproperty.atom != app->remoteAtom) {
        *cont = True;
        return;
    }
    *cont = False;

    if (XGetWindowProperty (
            app->display, XtWindow (app->remoteWin), app->remoteAtom, 0,
            remoteCmdSize / 4, True, app->remoteAtom, &type, &format, &size,
            &remain, (unsigned char**)&cmd) != Success) {
        return;
    }
    if (!cmd) { return; }
    p0 = cmd;
    while (*p0 && (p1 = strchr (p0, '\n'))) {
        cmdStr = new GString (p0, p1 - p0);
        app->remoteViewer->execCmd (cmdStr, NULL);
        delete cmdStr;
        p0 = p1 + 1;
    }
    XFree ((XPointer)cmd);
}

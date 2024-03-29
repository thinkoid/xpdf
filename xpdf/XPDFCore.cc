// -*- mode: c++; -*-
// Copyright 2002-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstring>
#include <chrono>

#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include <utils/string.hh>
#include <utils/GList.hh>

#include <splash/SplashBitmap.hh>
#include <splash/SplashPattern.hh>

#include <xpdf/array.hh>
#include <xpdf/CoreOutputDev.hh>
#include <xpdf/dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/ErrorCodes.hh>
#include <xpdf/GfxState.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/Link.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/XPDFApp.hh>
#include <xpdf/XPDFCore.hh>

// these macro defns conflict with xpdf's Object class
#ifdef LESSTIF_VERSION
#undef XtDisplay
#undef XtScreen
#undef XtWindow
#undef XtParent
#undef XtIsRealized
#endif

//------------------------------------------------------------------------

// Divide a 16-bit value (in [0, 255*255]) by 255, returning an 8-bit result.
static inline unsigned char div255(int x)
{
    return (unsigned char)((x + (x >> 8) + 0x80) >> 8);
}

//------------------------------------------------------------------------

GString * XPDFCore::currentSelection = NULL;
XPDFCore *XPDFCore::currentSelectionOwner = NULL;
Atom      XPDFCore::targetsAtom;

//------------------------------------------------------------------------
// XPDFCoreTile
//------------------------------------------------------------------------

class XPDFCoreTile : public PDFCoreTile
{
public:
    XPDFCoreTile(int xDestA, int yDestA);
    virtual ~XPDFCoreTile();
    XImage *image;
};

XPDFCoreTile::XPDFCoreTile(int xDestA, int yDestA)
    : PDFCoreTile(xDestA, yDestA)
{
    image = NULL;
}

XPDFCoreTile::~XPDFCoreTile()
{
    if (image) {
        free(image->data);
        image->data = NULL;
        XDestroyImage(image);
    }
}

//------------------------------------------------------------------------
// XPDFCore
//------------------------------------------------------------------------

XPDFCore::XPDFCore(Widget shellA, Widget parentWidgetA,
                   SplashColorPtr paperColorA, size_t paperPixelA,
                   size_t mattePixelA, bool fullScreenA, bool reverseVideoA,
                   bool installCmap, int rgbCubeSizeA)
    : PDFCore(splashModeRGB8, 4, reverseVideoA, paperColorA, !fullScreenA)
{
    GString *initialZoom;

    shell = shellA;
    parentWidget = parentWidgetA;
    display = XtDisplay(parentWidget);
    screenNum = XScreenNumberOfScreen(XtScreen(parentWidget));
    targetsAtom = XInternAtom(display, "TARGETS", False);

    paperPixel = paperPixelA;
    mattePixel = mattePixelA;
    fullScreen = fullScreenA;

    setupX(installCmap, rgbCubeSizeA);

    scrolledWin = NULL;
    hScrollBar = NULL;
    vScrollBar = NULL;
    drawAreaFrame = NULL;
    drawArea = NULL;

    // get the initial zoom value
    if (fullScreen) {
        zoom = zoomPage;
    } else {
        initialZoom = globalParams->getInitialZoom();
        if (!initialZoom->cmp("page")) {
            zoom = zoomPage;
        } else if (!initialZoom->cmp("width")) {
            zoom = zoomWidth;
        } else {
            zoom = atoi(initialZoom->c_str());
            if (zoom <= 0) {
                zoom = defZoom;
            }
        }
        delete initialZoom;
    }

    linkAction = NULL;

    panning = false;

    updateCbk = NULL;
    actionCbk = NULL;
    keyPressCbk = NULL;
    mouseCbk = NULL;

    // optional features default to on
    hyperlinksEnabled = true;
    selectEnabled = true;

    // do X-specific initialization and create the widgets
    initWindow();
    initPasswordDialog();
}

XPDFCore::~XPDFCore()
{
    if (currentSelectionOwner == this && currentSelection) {
        delete currentSelection;
        currentSelection = NULL;
        currentSelectionOwner = NULL;
    }
    if (drawAreaGC) {
        XFreeGC(display, drawAreaGC);
    }
    if (scrolledWin) {
        XtDestroyWidget(scrolledWin);
    }
    if (busyCursor) {
        XFreeCursor(display, busyCursor);
    }
    if (linkCursor) {
        XFreeCursor(display, linkCursor);
    }
    if (selectCursor) {
        XFreeCursor(display, selectCursor);
    }
}

void XPDFCore::segment()
{
    PDFCore::segment();
}
//------------------------------------------------------------------------
// loadFile / displayPage / displayDest
//------------------------------------------------------------------------

int XPDFCore::loadFile(GString *fileName, GString *ownerPassword,
                       GString *userPassword)
{
    int err = PDFCore::loadFile(fileName, ownerPassword, userPassword);

    if (err == errNone) {
        //
        // Save the modification time:
        //
        modTime = xpdf::last_write_time(fs::path(doc->getFileName()->c_str()));

        // update the parent window
        if (updateCbk)
            (*updateCbk)(updateCbkData, doc->getFileName(), -1,
                         doc->getNumPages(), NULL);
    }

    return err;
}

int XPDFCore::loadFile(BaseStream *stream, GString *ownerPassword,
                       GString *userPassword)
{
    int err = PDFCore::loadFile(stream, ownerPassword, userPassword);

    if (err == errNone) {
        // no file
        modTime = 0;

        // update the parent window
        if (updateCbk) {
            (*updateCbk)(updateCbkData, doc->getFileName(), -1,
                         doc->getNumPages(), NULL);
        }
    }
    return err;
}

void XPDFCore::loadDoc(PDFDoc *docA)
{
    PDFCore::loadDoc(docA);

    // save the modification time
    if (const auto p = doc->getFileName())
        modTime = xpdf::last_write_time(fs::path(p->c_str()));

    // update the parent window
    if (updateCbk) {
        (*updateCbk)(updateCbkData, doc->getFileName(), -1, doc->getNumPages(),
                     NULL);
    }
}

void XPDFCore::resizeToPage(int pg)
{
    Dimension width, height;
    double    width1, height1;
    Dimension topW, topH, topBorder, daW, daH;
    Dimension displayW, displayH;

    displayW = DisplayWidth(display, screenNum);
    displayH = DisplayHeight(display, screenNum);
    if (fullScreen) {
        width = displayW;
        height = displayH;
    } else {
        if (!doc || pg <= 0 || pg > doc->getNumPages()) {
            width1 = 612;
            height1 = 792;
        } else if (doc->getPageRotate(pg) == 90 ||
                   doc->getPageRotate(pg) == 270) {
            width1 = doc->getPageCropHeight(pg);
            height1 = doc->getPageCropWidth(pg);
        } else {
            width1 = doc->getPageCropWidth(pg);
            height1 = doc->getPageCropHeight(pg);
        }
        if (zoom == zoomPage || zoom == zoomWidth) {
            width = (Dimension)(width1 * 0.01 * defZoom + 0.5);
            height = (Dimension)(height1 * 0.01 * defZoom + 0.5);
        } else {
            width = (Dimension)(width1 * 0.01 * zoom + 0.5);
            height = (Dimension)(height1 * 0.01 * zoom + 0.5);
        }
        if (continuousMode) {
            height += continuousModePageSpacing;
        }
        if (width > displayW - 100) {
            width = displayW - 100;
        }
        if (height > displayH - 100) {
            height = displayH - 100;
        }
    }

    if (XtIsRealized(shell)) {
        XtVaGetValues(shell, XmNwidth, &topW, XmNheight, &topH, XmNborderWidth,
                      &topBorder, NULL);
        XtVaGetValues(drawArea, XmNwidth, &daW, XmNheight, &daH, NULL);
        XtVaSetValues(shell, XmNwidth, width + (topW - daW), XmNheight,
                      height + (topH - daH), NULL);
    } else {
        XtVaSetValues(drawArea, XmNwidth, width, XmNheight, height, NULL);
    }
}

void XPDFCore::update(int topPageA, int scrollXA, int scrollYA, double zoomA,
                      int rotateA, bool force, bool addToHist, bool adjustScrollX)
{
    int oldPage;

    oldPage = topPage;
    PDFCore::update(topPageA, scrollXA, scrollYA, zoomA, rotateA, force,
                    addToHist, adjustScrollX);
    linkAction = NULL;
    if (doc && topPage != oldPage) {
        if (updateCbk) {
            (*updateCbk)(updateCbkData, NULL, topPage, -1, "");
        }
    }
}

bool XPDFCore::checkForNewFile()
{
    if (const auto p = doc->getFileName()) {
        const auto newModTime = xpdf::last_write_time(fs::path(p->c_str()));

        if (newModTime != modTime) {
            modTime = newModTime;
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------
// page/position changes
//------------------------------------------------------------------------

bool XPDFCore::gotoNextPage(int inc, bool top)
{
    if (!PDFCore::gotoNextPage(inc, top)) {
        XBell(display, 0);
        return false;
    }
    return true;
}

bool XPDFCore::gotoPrevPage(int dec, bool top, bool bottom)
{
    if (!PDFCore::gotoPrevPage(dec, top, bottom)) {
        XBell(display, 0);
        return false;
    }
    return true;
}

bool XPDFCore::goForward()
{
    if (!PDFCore::goForward()) {
        XBell(display, 0);
        return false;
    }
    return true;
}

bool XPDFCore::goBackward()
{
    if (!PDFCore::goBackward()) {
        XBell(display, 0);
        return false;
    }
    return true;
}

void XPDFCore::startPan(int wx, int wy)
{
    panning = true;
    panMX = wx;
    panMY = wy;
}

void XPDFCore::endPan(int wx, int wy)
{
    panning = false;
}

//------------------------------------------------------------------------
// selection
//------------------------------------------------------------------------

void XPDFCore::startSelection(int wx, int wy)
{
    int pg, x, y;

    takeFocus();
    if (doc && doc->getNumPages() > 0) {
        if (selectEnabled) {
            if (cvtWindowToDev(wx, wy, &pg, &x, &y)) {
                setSelection(pg, x, y, x, y);
                setCursor(selectCursor);
                dragging = true;
            }
        }
    }
}

void XPDFCore::endSelection(int wx, int wy)
{
    int  pg, x, y;
    bool ok;

    if (doc && doc->getNumPages() > 0) {
        ok = cvtWindowToDev(wx, wy, &pg, &x, &y);
        if (dragging) {
            dragging = false;
            setCursor(None);
            if (ok) {
                moveSelection(pg, x, y);
            }
#ifndef NO_TEXT_SELECT
            if (selectULX != selectLRX && selectULY != selectLRY) {
                if (doc->okToCopy()) {
                    copySelection();
                } else {
                    error(errNotAllowed, -1,
                          "Copying of text from this document is not allowed.");
                }
            }
#endif
        }
    }
}

// X's copy-and-paste mechanism is brain damaged.  Xt doesn't help
// any, but doesn't make it too much worse, either.  Motif, on the
// other hand, adds significant complexity to the mess.  So here we
// blow off the Motif junk and stick to plain old Xt.  The next two
// functions (copySelection and convertSelectionCbk) implement the
// magic needed to deal with Xt's mechanism.  Note that this requires
// global variables (currentSelection and currentSelectionOwner).

void XPDFCore::copySelection()
{
    int    pg;
    double ulx, uly, lrx, lry;

    if (!doc->okToCopy()) {
        return;
    }
    if (getSelection(&pg, &ulx, &uly, &lrx, &lry)) {
        if (currentSelection) {
            delete currentSelection;
        }
        currentSelection = extractText(pg, ulx, uly, lrx, lry);
        currentSelectionOwner = this;
        XtOwnSelection(drawArea, XA_PRIMARY, XtLastTimestampProcessed(display),
                       &convertSelectionCbk, NULL, NULL);
    }
}

Boolean XPDFCore::convertSelectionCbk(Widget widget, Atom *selection,
                                      Atom *target, Atom *type, XtPointer *value,
                                      unsigned long *length, int *format)
{
    Atom *array;

    // send back a list of supported conversion targets
    if (*target == targetsAtom) {
        if (!(array = (Atom *)XtMalloc(sizeof(Atom)))) {
            return False;
        }
        array[0] = XA_STRING;
        *value = (XtPointer)array;
        *type = XA_ATOM;
        *format = 32;
        *length = 1;
        return True;

        // send the selected text
    } else if (*target == XA_STRING) {
        *value = XtNewString(currentSelection->c_str());
        *length = currentSelection->getLength();
        *type = XA_STRING;
        *format = 8; // 8-bit elements
        return True;
    }

    return False;
}

//------------------------------------------------------------------------
// hyperlinks
//------------------------------------------------------------------------

void XPDFCore::doAction(LinkAction *action)
{
    LinkActionKind kind;
    LinkDest *     dest;
    GString *      namedDest;
    const char *   s;
    GString *      fileName;
    GString *      cmd;
    GString *      actionName;
    Object         movieAnnot, obj1, obj2;
    GString *      msg;
    int            i;

    switch (kind = action->getKind()) {
    // GoTo / GoToR action
    case actionGoTo:
    case actionGoToR:
        if (kind == actionGoTo) {
            dest = NULL;
            namedDest = NULL;
            if ((dest = ((LinkGoTo *)action)->getDest())) {
                dest = dest->copy();
            } else if ((namedDest = ((LinkGoTo *)action)->getNamedDest())) {
                namedDest = namedDest->copy();
            }
        } else {
            dest = NULL;
            namedDest = NULL;

            if ((dest = ((LinkGoToR *)action)->getDest())) {
                dest = dest->copy();
            } else if ((namedDest = ((LinkGoToR *)action)->getNamedDest())) {
                namedDest = namedDest->copy();
            }

            s = ((LinkGoToR *)action)->getFileName()->c_str();

            //~ translate path name for VMS (deal with '/')
            if (fs::path(s).is_absolute() || !doc->getFileName()) {
                fileName = new GString(s);
            } else {
                auto path = fs::path(doc->getFileName()->c_str()) / s;
                fileName = new GString(path.c_str());
            }

            if (loadFile(fileName) != errNone) {
                if (dest) {
                    delete dest;
                }
                if (namedDest) {
                    delete namedDest;
                }
                delete fileName;
                return;
            }
            delete fileName;
        }

        if (namedDest) {
            dest = doc->findDest(namedDest);
            delete namedDest;
        }
        if (dest) {
            displayDest(dest, zoom, rotate, true);
            delete dest;
        } else {
            if (kind == actionGoToR) {
                displayPage(1, zoom, 0, false, true);
            }
        }
        break;

    // Launch action
    case actionLaunch:
        fileName = ((LinkLaunch *)action)->getFileName();
        s = fileName->c_str();
        if (!strcmp(s + fileName->getLength() - 4, ".pdf") ||
            !strcmp(s + fileName->getLength() - 4, ".PDF")) {
            //~ translate path name for VMS (deal with '/')
            if (fs::path(s).is_absolute() || !doc->getFileName()) {
                fileName = fileName->copy();
            } else {
                auto path = fs::path(doc->getFileName()->c_str()) / s;
                fileName = new GString(path.c_str());
            }
            if (loadFile(fileName) != errNone) {
                delete fileName;
                return;
            }
            delete fileName;
            displayPage(1, zoom, rotate, false, true);
        } else {
            fileName = fileName->copy();
            if (((LinkLaunch *)action)->getParams()) {
                fileName->append(1UL, ' ');
                fileName->append(*((LinkLaunch *)action)->getParams());
            }
            fileName->append(" &");
            if (globalParams->getLaunchCommand()) {
                fileName->insert(0, ' ');
                fileName->insert(0, globalParams->getLaunchCommand());
                system(fileName->c_str());
            } else {
                msg = new GString("About to execute the command:\n");
                msg->append(*fileName);
                if (doQuestionDialog("Launching external application", msg)) {
                    system(fileName->c_str());
                }
                delete msg;
            }
            delete fileName;
        }
        break;

    // URI action
    case actionURI:
        if (!(cmd = globalParams->getURLCommand())) {
            error(errConfig, -1, "No urlCommand defined in config file");
            break;
        }
        runCommand(cmd, ((LinkURI *)action)->getURI());
        break;

    // Named action
    case actionNamed:
        actionName = ((LinkNamed *)action)->as_name();
        if (!actionName->cmp("NextPage")) {
            gotoNextPage(1, true);
        } else if (!actionName->cmp("PrevPage")) {
            gotoPrevPage(1, true, false);
        } else if (!actionName->cmp("FirstPage")) {
            if (topPage != 1) {
                displayPage(1, zoom, rotate, true, true);
            }
        } else if (!actionName->cmp("LastPage")) {
            if (topPage != doc->getNumPages()) {
                displayPage(doc->getNumPages(), zoom, rotate, true, true);
            }
        } else if (!actionName->cmp("GoBack")) {
            goBackward();
        } else if (!actionName->cmp("GoForward")) {
            goForward();
        } else if (!actionName->cmp("Quit")) {
            if (actionCbk) {
                (*actionCbk)(actionCbkData, actionName->c_str());
            }
        } else {
            error(errSyntaxError, -1, "Unknown named action: '{0:t}'",
                  actionName);
        }
        break;

    // Movie action
    case actionMovie:
        if (!(cmd = globalParams->getMovieCommand())) {
            error(errConfig, -1, "No movieCommand defined in config file");
            break;
        }
        if (((LinkMovie *)action)->hasAnnotRef()) {
            movieAnnot =
                doc->getXRef()->fetch(*((LinkMovie *)action)->getAnnotRef());
        } else {
            //~ need to use the correct page num here
            obj1 = doc->getCatalog()->getPage(topPage)->getAnnots();

            if (obj1.is_array()) {
                for (i = 0; i < obj1.as_array().size(); ++i) {
                    if ((movieAnnot = resolve(obj1[i])).is_dict()) {
                        if ((obj2 = resolve(movieAnnot.as_dict()["Subtype"]))
                                .is_name("Movie")) {
                            break;
                        }
                    }
                }
            }
        }
        if (movieAnnot.is_dict()) {
            if ((obj1 = resolve(movieAnnot.as_dict()["Movie"])).is_dict()) {
                if (!(obj2 = resolve(obj1.as_dict()["F"])).is_null()) {
                    if ((fileName = LinkAction::getFileSpecName(&obj2))) {
                        if (!fs::path(fileName->c_str()).is_absolute() && doc->getFileName()) {
                            auto path = fs::path(doc->getFileName()->c_str()) / fileName->c_str();
                            fileName = new GString(path.c_str());
                        }
                        runCommand(cmd, fileName);
                        delete fileName;
                    }
                }
            }
        }
        break;

    // unsupported action types
    case actionJavaScript:
    case actionSubmitForm:
    case actionHide:
        error(errSyntaxError, -1, "Unsupported link action type");
        break;

    // unknown action type
    case actionUnknown:
        error(errSyntaxError, -1, "Unknown link action type: '{0:t}'",
              ((LinkUnknown *)action)->getAction());
        break;
    }
}

// Run a command, given a <cmdFmt> string with one '%s' in it, and an
// <arg> string to insert in place of the '%s'.
void XPDFCore::runCommand(GString *cmdFmt, GString *arg)
{
    GString *   cmd;
    const char *s;

    if ((s = std::strstr(cmdFmt->c_str(), "%s"))) {
        cmd = mungeURL(arg);
        cmd->insert(0, cmdFmt->c_str(), s - cmdFmt->c_str());
        cmd->append(s + 2);
    } else {
        cmd = cmdFmt->copy();
    }
    cmd->append(" &");
    system(cmd->c_str());
    delete cmd;
}

// Escape any characters in a URL which might cause problems when
// calling system().
GString *XPDFCore::mungeURL(GString *url)
{
    static const char *allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz"
                                 "0123456789"
                                 "-_.~/?:@&=+,#%";
    GString *newURL;
    char     c;
    char     buf[4];
    int      i;

    newURL = new GString();
    for (i = 0; i < url->getLength(); ++i) {
        c = (*url)[i];
        if (strchr(allowed, c)) {
            newURL->append(1UL, c);
        } else {
            sprintf(buf, "%%%02x", c & 0xff);
            newURL->append(buf);
        }
    }
    return newURL;
}

//------------------------------------------------------------------------
// find
//------------------------------------------------------------------------

bool XPDFCore::find(char *s, bool caseSensitive, bool next, bool backward,
                    bool wholeWord, bool onePageOnly)
{
    if (!PDFCore::find(s, caseSensitive, next, backward, wholeWord,
                       onePageOnly)) {
        XBell(display, 0);
        return false;
    }
#ifndef NO_TEXT_SELECT
    copySelection();
#endif
    return true;
}

bool XPDFCore::findU(Unicode *u, int len, bool caseSensitive, bool next,
                     bool backward, bool wholeWord, bool onePageOnly)
{
    if (!PDFCore::findU(u, len, caseSensitive, next, backward, wholeWord,
                        onePageOnly)) {
        XBell(display, 0);
        return false;
    }
#ifndef NO_TEXT_SELECT
    copySelection();
#endif
    return true;
}

//------------------------------------------------------------------------
// misc access
//------------------------------------------------------------------------

void XPDFCore::setBusyCursor(bool busy)
{
    setCursor(busy ? busyCursor : None);
}

void XPDFCore::takeFocus()
{
    XmProcessTraversal(drawArea, XmTRAVERSE_CURRENT);
}

//------------------------------------------------------------------------
// GUI code
//------------------------------------------------------------------------

void XPDFCore::setupX(bool installCmap, int rgbCubeSizeA)
{
    XVisualInfo  visualTempl;
    XVisualInfo *visualList;
    size_t       mask;
    int          nVisuals;
    XColor       xcolor;
    XColor *     xcolors;
    int          r, g, b, n, m;
    bool         ok;

    // for some reason, querying XmNvisual doesn't work (even if done
    // after the window is mapped)
    visual = DefaultVisual(display, screenNum);
    XtVaGetValues(shell, XmNcolormap, &colormap, NULL);

    // check for TrueColor visual
    //~ this should scan the list, not just look at the first one
    visualTempl.visualid = XVisualIDFromVisual(visual);
    visualList = XGetVisualInfo(display, VisualIDMask, &visualTempl, &nVisuals);
    if (nVisuals < 1) {
        // this shouldn't happen
        XFree((XPointer)visualList);
        visualList =
            XGetVisualInfo(display, VisualNoMask, &visualTempl, &nVisuals);
    }
    depth = visualList->depth;
    if (visualList->c_class == TrueColor) {
        trueColor = true;
        for (mask = visualList->red_mask, rShift = 0; mask && !(mask & 1);
             mask >>= 1, ++rShift)
            ;
        for (rDiv = 8; mask; mask >>= 1, --rDiv)
            ;
        for (mask = visualList->green_mask, gShift = 0; mask && !(mask & 1);
             mask >>= 1, ++gShift)
            ;
        for (gDiv = 8; mask; mask >>= 1, --gDiv)
            ;
        for (mask = visualList->blue_mask, bShift = 0; mask && !(mask & 1);
             mask >>= 1, ++bShift)
            ;
        for (bDiv = 8; mask; mask >>= 1, --bDiv)
            ;
    } else {
        trueColor = false;
    }
    XFree((XPointer)visualList);

    // allocate a color cube
    if (!trueColor) {
        // set colors in private colormap
        if (installCmap) {
            for (rgbCubeSize = xMaxRGBCube; rgbCubeSize >= 2; --rgbCubeSize) {
                m = rgbCubeSize * rgbCubeSize * rgbCubeSize;
                if (XAllocColorCells(display, colormap, False, NULL, 0, colors,
                                     m)) {
                    break;
                }
            }
            if (rgbCubeSize >= 2) {
                m = rgbCubeSize * rgbCubeSize * rgbCubeSize;
                xcolors = (XColor *)calloc(m, sizeof(XColor));
                n = 0;
                for (r = 0; r < rgbCubeSize; ++r) {
                    for (g = 0; g < rgbCubeSize; ++g) {
                        for (b = 0; b < rgbCubeSize; ++b) {
                            xcolors[n].pixel = colors[n];
                            xcolors[n].red = (r * 65535) / (rgbCubeSize - 1);
                            xcolors[n].green = (g * 65535) / (rgbCubeSize - 1);
                            xcolors[n].blue = (b * 65535) / (rgbCubeSize - 1);
                            xcolors[n].flags = DoRed | DoGreen | DoBlue;
                            ++n;
                        }
                    }
                }
                XStoreColors(display, colormap, xcolors, m);
                free(xcolors);
            } else {
                rgbCubeSize = 1;
                colors[0] = BlackPixel(display, screenNum);
                colors[1] = WhitePixel(display, screenNum);
            }

            // allocate colors in shared colormap
        } else {
            if (rgbCubeSize > xMaxRGBCube) {
                rgbCubeSize = xMaxRGBCube;
            }
            ok = false;
            for (rgbCubeSize = rgbCubeSizeA; rgbCubeSize >= 2; --rgbCubeSize) {
                ok = true;
                n = 0;
                for (r = 0; r < rgbCubeSize && ok; ++r) {
                    for (g = 0; g < rgbCubeSize && ok; ++g) {
                        for (b = 0; b < rgbCubeSize && ok; ++b) {
                            if (n == 0) {
                                colors[n] = BlackPixel(display, screenNum);
                                ++n;
                            } else {
                                xcolor.red = (r * 65535) / (rgbCubeSize - 1);
                                xcolor.green = (g * 65535) / (rgbCubeSize - 1);
                                xcolor.blue = (b * 65535) / (rgbCubeSize - 1);
                                if (XAllocColor(display, colormap, &xcolor)) {
                                    colors[n++] = xcolor.pixel;
                                } else {
                                    ok = false;
                                }
                            }
                        }
                    }
                }
                if (ok) {
                    break;
                }
                XFreeColors(display, colormap, &colors[1], n - 1, 0);
            }
            if (!ok) {
                rgbCubeSize = 1;
                colors[0] = BlackPixel(display, screenNum);
                colors[1] = WhitePixel(display, screenNum);
            }
        }
    }
}

void XPDFCore::initWindow()
{
    Arg args[20];
    int n;

    // create the cursors
    busyCursor = XCreateFontCursor(display, XC_watch);
    linkCursor = XCreateFontCursor(display, XC_hand2);
    selectCursor = XCreateFontCursor(display, XC_cross);
    currentCursor = 0;

    // create the scrolled window and scrollbars
    n = 0;
    XtSetArg(args[n], XmNscrollingPolicy, XmAPPLICATION_DEFINED);
    ++n;
    XtSetArg(args[n], XmNvisualPolicy, XmVARIABLE);
    ++n;
    scrolledWin = XmCreateScrolledWindow(parentWidget, "scroll", args, n);
    XtManageChild(scrolledWin);
    n = 0;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL);
    ++n;
    XtSetArg(args[n], XmNminimum, 0);
    ++n;
    XtSetArg(args[n], XmNmaximum, 1);
    ++n;
    XtSetArg(args[n], XmNsliderSize, 1);
    ++n;
    XtSetArg(args[n], XmNvalue, 0);
    ++n;
    XtSetArg(args[n], XmNincrement, 1);
    ++n;
    XtSetArg(args[n], XmNpageIncrement, 1);
    ++n;
    hScrollBar = XmCreateScrollBar(scrolledWin, "hScrollBar", args, n);
    if (!fullScreen) {
        XtManageChild(hScrollBar);
    }
    XtAddCallback(hScrollBar, XmNvalueChangedCallback, &hScrollChangeCbk,
                  (XtPointer)this);
#ifndef DISABLE_SMOOTH_SCROLL
    XtAddCallback(hScrollBar, XmNdragCallback, &hScrollDragCbk, (XtPointer)this);
#endif
    n = 0;
    XtSetArg(args[n], XmNorientation, XmVERTICAL);
    ++n;
    XtSetArg(args[n], XmNminimum, 0);
    ++n;
    XtSetArg(args[n], XmNmaximum, 1);
    ++n;
    XtSetArg(args[n], XmNsliderSize, 1);
    ++n;
    XtSetArg(args[n], XmNvalue, 0);
    ++n;
    XtSetArg(args[n], XmNincrement, 1);
    ++n;
    XtSetArg(args[n], XmNpageIncrement, 1);
    ++n;
    vScrollBar = XmCreateScrollBar(scrolledWin, "vScrollBar", args, n);
    if (!fullScreen) {
        XtManageChild(vScrollBar);
    }
    XtAddCallback(vScrollBar, XmNvalueChangedCallback, &vScrollChangeCbk,
                  (XtPointer)this);
#ifndef DISABLE_SMOOTH_SCROLL
    XtAddCallback(vScrollBar, XmNdragCallback, &vScrollDragCbk, (XtPointer)this);
#endif

    // create the drawing area
    n = 0;
    XtSetArg(args[n], XmNshadowType, XmSHADOW_IN);
    ++n;
    XtSetArg(args[n], XmNmarginWidth, 0);
    ++n;
    XtSetArg(args[n], XmNmarginHeight, 0);
    ++n;
    if (fullScreen) {
        XtSetArg(args[n], XmNshadowThickness, 0);
        ++n;
    }
    drawAreaFrame = XmCreateFrame(scrolledWin, "drawAreaFrame", args, n);
    XtManageChild(drawAreaFrame);
    n = 0;
    XtSetArg(args[n], XmNresizePolicy, XmRESIZE_ANY);
    ++n;
    XtSetArg(args[n], XmNwidth, 700);
    ++n;
    XtSetArg(args[n], XmNheight, 500);
    ++n;
    drawArea = XmCreateDrawingArea(drawAreaFrame, "drawArea", args, n);
    XtManageChild(drawArea);
    XtAddCallback(drawArea, XmNresizeCallback, &resizeCbk, (XtPointer)this);
    XtAddCallback(drawArea, XmNexposeCallback, &redrawCbk, (XtPointer)this);
    XtAddCallback(drawArea, XmNinputCallback, &inputCbk, (XtPointer)this);
    resizeCbk(drawArea, this, NULL);

    // set up mouse motion translations
    XtOverrideTranslations(drawArea, XtParseTranslationTable(
                                         "<Btn1Down>:DrawingAreaInput()\n"
                                         "<Btn1Up>:DrawingAreaInput()\n"
                                         "<Btn1Motion>:DrawingAreaInput()\n"
                                         "<Motion>:DrawingAreaInput()"));

    // can't create a GC until the window gets mapped
    drawAreaGC = NULL;
}

void XPDFCore::hScrollChangeCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *                 core = (XPDFCore *)ptr;
    XmScrollBarCallbackStruct *data = (XmScrollBarCallbackStruct *)callData;

    core->scrollTo(data->value, core->scrollY);
}

void XPDFCore::hScrollDragCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *                 core = (XPDFCore *)ptr;
    XmScrollBarCallbackStruct *data = (XmScrollBarCallbackStruct *)callData;

    core->scrollTo(data->value, core->scrollY);
}

void XPDFCore::vScrollChangeCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *                 core = (XPDFCore *)ptr;
    XmScrollBarCallbackStruct *data = (XmScrollBarCallbackStruct *)callData;

    core->scrollTo(core->scrollX, data->value);
}

void XPDFCore::vScrollDragCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *                 core = (XPDFCore *)ptr;
    XmScrollBarCallbackStruct *data = (XmScrollBarCallbackStruct *)callData;

    core->scrollTo(core->scrollX, data->value);
}

void XPDFCore::resizeCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *core = (XPDFCore *)ptr;
    XEvent    event;
    Widget    top;
    Window    rootWin;
    int       x1, y1;
    unsigned  w1, h1, bw1, depth1;
    Arg       args[2];
    int       n;
    Dimension w, h;
    int       sx, sy;

    // find the top-most widget which has an associated window, and look
    // for a pending ConfigureNotify in the event queue -- if there is
    // one, and it specifies a different width or height, that means
    // we're still resizing, and we want to skip the current event
    for (top = core->parentWidget; XtParent(top) && XtWindow(XtParent(top));
         top = XtParent(top))
        ;
    if (XCheckTypedWindowEvent(core->display, XtWindow(top), ConfigureNotify,
                               &event)) {
        XPutBackEvent(core->display, &event);
        XGetGeometry(core->display, event.xconfigure.window, &rootWin, &x1, &y1,
                     &w1, &h1, &bw1, &depth1);
        if ((unsigned)event.xconfigure.width != w1 ||
            (unsigned)event.xconfigure.height != h1) {
            return;
        }
    }

    n = 0;
    XtSetArg(args[n], XmNwidth, &w);
    ++n;
    XtSetArg(args[n], XmNheight, &h);
    ++n;
    XtGetValues(core->drawArea, args, n);
    core->drawAreaWidth = (int)w;
    core->drawAreaHeight = (int)h;
    if (core->zoom == zoomPage || core->zoom == zoomWidth) {
        sx = sy = -1;
    } else {
        sx = core->scrollX;
        sy = core->scrollY;
    }
    core->update(core->topPage, sx, sy, core->zoom, core->rotate, true, false,
                 false);
}

void XPDFCore::redrawCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *                   core = (XPDFCore *)ptr;
    XmDrawingAreaCallbackStruct *data = (XmDrawingAreaCallbackStruct *)callData;
    int                          x, y, w, h;

    if (data->reason == XmCR_EXPOSE) {
        x = data->event->xexpose.x;
        y = data->event->xexpose.y;
        w = data->event->xexpose.width;
        h = data->event->xexpose.height;
    } else {
        x = 0;
        y = 0;
        w = core->drawAreaWidth;
        h = core->drawAreaHeight;
    }
    core->redrawWindow(x, y, w, h, false);
}

void XPDFCore::inputCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *                   core = (XPDFCore *)ptr;
    XmDrawingAreaCallbackStruct *data = (XmDrawingAreaCallbackStruct *)callData;
    LinkAction *                 action;
    int                          pg, x, y;
    double                       xu, yu;
    const char *                 s;
    KeySym                       key;
    bool                         ok;

    switch (data->event->type) {
    case ButtonPress:
        if (*core->mouseCbk) {
            (*core->mouseCbk)(core->mouseCbkData, data->event);
        }
        break;
    case ButtonRelease:
        if (*core->mouseCbk) {
            (*core->mouseCbk)(core->mouseCbkData, data->event);
        }
        break;
    case MotionNotify:
        if (core->doc && core->doc->getNumPages() > 0) {
            ok = core->cvtWindowToDev(data->event->xmotion.x,
                                      data->event->xmotion.y, &pg, &x, &y);
            if (core->dragging) {
                if (ok) {
                    core->moveSelection(pg, x, y);
                }
            } else if (core->hyperlinksEnabled) {
                core->cvtDevToUser(pg, x, y, &xu, &yu);
                if (ok && (action = core->findLink(pg, xu, yu))) {
                    core->setCursor(core->linkCursor);
                    if (action != core->linkAction) {
                        core->linkAction = action;
                        if (core->updateCbk) {
                            s = "";
                            switch (action->getKind()) {
                            case actionGoTo:
                                s = "[internal link]";
                                break;
                            case actionGoToR:
                                s = ((LinkGoToR *)action)->getFileName()->c_str();
                                break;
                            case actionLaunch:
                                s = ((LinkLaunch *)action)->getFileName()->c_str();
                                break;
                            case actionURI:
                                s = ((LinkURI *)action)->getURI()->c_str();
                                break;
                            case actionNamed:
                                s = ((LinkNamed *)action)->as_name()->c_str();
                                break;
                            case actionMovie:
                                s = "[movie]";
                                break;
                            case actionJavaScript:
                            case actionSubmitForm:
                            case actionHide:
                            case actionUnknown:
                                s = "[unknown link]";
                                break;
                            }
                            (*core->updateCbk)(core->updateCbkData, NULL, -1, -1,
                                               s);
                        }
                    }
                } else {
                    core->setCursor(None);
                    if (core->linkAction) {
                        core->linkAction = NULL;
                        if (core->updateCbk) {
                            (*core->updateCbk)(core->updateCbkData, NULL, -1, -1,
                                               "");
                        }
                    }
                }
            }
        }
        if (core->panning) {
            core->scrollTo(core->scrollX - (data->event->xmotion.x - core->panMX),
                           core->scrollY -
                               (data->event->xmotion.y - core->panMY));
            core->panMX = data->event->xmotion.x;
            core->panMY = data->event->xmotion.y;
        }
        break;
    case KeyPress:
        if (core->keyPressCbk) {
            key = XLookupKeysym(&data->event->xkey,
                                (data->event->xkey.state & ShiftMask) ? 1 : 0);
            (*core->keyPressCbk)(core->keyPressCbkData, key,
                                 data->event->xkey.state, data->event);
        }
        break;
    }
}

PDFCoreTile *XPDFCore::newTile(int xDestA, int yDestA)
{
    return new XPDFCoreTile(xDestA, yDestA);
}

void XPDFCore::updateTileData(PDFCoreTile *tileA, int xSrc, int ySrc, int width,
                              int height, bool composited)
{
    XPDFCoreTile * tile = (XPDFCoreTile *)tileA;
    XImage *       image;
    SplashColorPtr dataPtr, p;
    size_t         pixel;
    unsigned char *ap;
    unsigned char  alpha, alpha1;
    int            w, h, bw, x, y, r, g, b, gray;
    int *          errDownR, *errDownG, *errDownB;
    int            errRightR, errRightG, errRightB;
    int            errDownRightR, errDownRightG, errDownRightB;
    int            r0, g0, b0, re, ge, be;

    if (!tile->image) {
        w = tile->xMax - tile->xMin;
        h = tile->yMax - tile->yMin;
        image =
            XCreateImage(display, visual, depth, ZPixmap, 0, NULL, w, h, 8, 0);
        image->data = (char *)calloc(h, image->bytes_per_line);
        tile->image = image;
    } else {
        image = (XImage *)tile->image;
    }

    //~ optimize for known XImage formats
    bw = tile->bitmap->getRowSize();
    dataPtr = tile->bitmap->getDataPtr();

    if (trueColor) {
        for (y = 0; y < height; ++y) {
            p = dataPtr + (ySrc + y) * bw + xSrc * 3;
            if (!composited && tile->bitmap->getAlphaPtr()) {
                ap = tile->bitmap->getAlphaPtr() +
                     (ySrc + y) * tile->bitmap->getWidth() + xSrc;
            } else {
                ap = NULL;
            }
            for (x = 0; x < width; ++x) {
                r = splashRGB8R(p);
                g = splashRGB8G(p);
                b = splashRGB8B(p);
                if (ap) {
                    alpha = *ap++;
                    alpha1 = 255 - alpha;
                    r = div255(alpha1 * paperColor[0] + alpha * r);
                    g = div255(alpha1 * paperColor[1] + alpha * g);
                    b = div255(alpha1 * paperColor[2] + alpha * b);
                }
                r >>= rDiv;
                g >>= gDiv;
                b >>= bDiv;
                pixel = ((size_t)r << rShift) + ((size_t)g << gShift) +
                        ((size_t)b << bShift);
                XPutPixel(image, xSrc + x, ySrc + y, pixel);
                p += 3;
            }
        }
    } else if (rgbCubeSize == 1) {
        //~ this should really use splashModeMono, with non-clustered dithering
        for (y = 0; y < height; ++y) {
            p = dataPtr + (ySrc + y) * bw + xSrc * 3;
            if (!composited && tile->bitmap->getAlphaPtr()) {
                ap = tile->bitmap->getAlphaPtr() +
                     (ySrc + y) * tile->bitmap->getWidth() + xSrc;
            } else {
                ap = NULL;
            }
            for (x = 0; x < width; ++x) {
                r = splashRGB8R(p);
                g = splashRGB8G(p);
                b = splashRGB8B(p);
                if (ap) {
                    alpha = *ap++;
                    alpha1 = 255 - alpha;
                    r = div255(alpha1 * paperColor[0] + alpha * r);
                    g = div255(alpha1 * paperColor[1] + alpha * g);
                    b = div255(alpha1 * paperColor[2] + alpha * b);
                }
                gray = (int)(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
                if (gray < 128) {
                    pixel = colors[0];
                } else {
                    pixel = colors[1];
                }
                XPutPixel(image, xSrc + x, ySrc + y, pixel);
                p += 3;
            }
        }
    } else {
        // do Floyd-Steinberg dithering on the whole bitmap
        errDownR = (int *)calloc(width + 2, sizeof(int));
        errDownG = (int *)calloc(width + 2, sizeof(int));
        errDownB = (int *)calloc(width + 2, sizeof(int));
        errRightR = errRightG = errRightB = 0;
        errDownRightR = errDownRightG = errDownRightB = 0;
        memset(errDownR, 0, (width + 2) * sizeof(int));
        memset(errDownG, 0, (width + 2) * sizeof(int));
        memset(errDownB, 0, (width + 2) * sizeof(int));
        for (y = 0; y < height; ++y) {
            p = dataPtr + (ySrc + y) * bw + xSrc * 3;
            if (!composited && tile->bitmap->getAlphaPtr()) {
                ap = tile->bitmap->getAlphaPtr() +
                     (ySrc + y) * tile->bitmap->getWidth() + xSrc;
            } else {
                ap = NULL;
            }
            for (x = 0; x < width; ++x) {
                r = splashRGB8R(p);
                g = splashRGB8G(p);
                b = splashRGB8B(p);
                if (ap) {
                    alpha = *ap++;
                    alpha1 = 255 - alpha;
                    r = div255(alpha1 * paperColor[0] + alpha * r);
                    g = div255(alpha1 * paperColor[1] + alpha * g);
                    b = div255(alpha1 * paperColor[2] + alpha * b);
                }
                r0 = r + errRightR + errDownR[x + 1];
                g0 = g + errRightG + errDownG[x + 1];
                b0 = b + errRightB + errDownB[x + 1];
                if (r0 < 0) {
                    r = 0;
                } else if (r0 >= 255) {
                    r = rgbCubeSize - 1;
                } else {
                    r = div255(r0 * (rgbCubeSize - 1));
                }
                if (g0 < 0) {
                    g = 0;
                } else if (g0 >= 255) {
                    g = rgbCubeSize - 1;
                } else {
                    g = div255(g0 * (rgbCubeSize - 1));
                }
                if (b0 < 0) {
                    b = 0;
                } else if (b0 >= 255) {
                    b = rgbCubeSize - 1;
                } else {
                    b = div255(b0 * (rgbCubeSize - 1));
                }
                re = r0 - ((r << 8) - r) / (rgbCubeSize - 1);
                ge = g0 - ((g << 8) - g) / (rgbCubeSize - 1);
                be = b0 - ((b << 8) - b) / (rgbCubeSize - 1);
                errRightR = (re * 7) >> 4;
                errRightG = (ge * 7) >> 4;
                errRightB = (be * 7) >> 4;
                errDownR[x] += (re * 3) >> 4;
                errDownG[x] += (ge * 3) >> 4;
                errDownB[x] += (be * 3) >> 4;
                errDownR[x + 1] = ((re * 5) >> 4) + errDownRightR;
                errDownG[x + 1] = ((ge * 5) >> 4) + errDownRightG;
                errDownB[x + 1] = ((be * 5) >> 4) + errDownRightB;
                errDownRightR = re >> 4;
                errDownRightG = ge >> 4;
                errDownRightB = be >> 4;
                pixel = colors[(r * rgbCubeSize + g) * rgbCubeSize + b];
                XPutPixel(image, xSrc + x, ySrc + y, pixel);
                p += 3;
            }
        }
        free(errDownR);
        free(errDownG);
        free(errDownB);
    }
}

void XPDFCore::redrawRect(PDFCoreTile *tileA, int xSrc, int ySrc, int xDest,
                          int yDest, int width, int height, bool composited)
{
    XPDFCoreTile *tile = (XPDFCoreTile *)tileA;
    Window        drawAreaWin;
    XGCValues     gcValues;

    // create a GC for the drawing area
    drawAreaWin = XtWindow(drawArea);
    if (!drawAreaGC) {
        gcValues.foreground = mattePixel;
        drawAreaGC = XCreateGC(display, drawAreaWin, GCForeground, &gcValues);
    }

    // draw the document
    if (tile) {
        XPutImage(display, drawAreaWin, drawAreaGC, tile->image, xSrc, ySrc,
                  xDest, yDest, width, height);

        // draw the background
    } else {
        XFillRectangle(display, drawAreaWin, drawAreaGC, xDest, yDest, width,
                       height);
    }

    XFlush(display);
}

void XPDFCore::updateScrollbars()
{
    Arg args[20];
    int n;
    int maxPos;

    if (pages->getLength() > 0) {
        if (continuousMode) {
            maxPos = maxPageW;
        } else {
            maxPos = ((PDFCorePage *)pages->get(0))->w;
        }
    } else {
        maxPos = 1;
    }
    if (maxPos < drawAreaWidth) {
        maxPos = drawAreaWidth;
    }
    n = 0;
    XtSetArg(args[n], XmNvalue, scrollX);
    ++n;
    XtSetArg(args[n], XmNmaximum, maxPos);
    ++n;
    XtSetArg(args[n], XmNsliderSize, drawAreaWidth);
    ++n;
    XtSetArg(args[n], XmNincrement, 16);
    ++n;
    XtSetArg(args[n], XmNpageIncrement, drawAreaWidth);
    ++n;
    XtSetValues(hScrollBar, args, n);

    if (pages->getLength() > 0) {
        if (continuousMode) {
            maxPos = totalDocH;
        } else {
            maxPos = ((PDFCorePage *)pages->get(0))->h;
        }
    } else {
        maxPos = 1;
    }
    if (maxPos < drawAreaHeight) {
        maxPos = drawAreaHeight;
    }
    n = 0;
    XtSetArg(args[n], XmNvalue, scrollY);
    ++n;
    XtSetArg(args[n], XmNmaximum, maxPos);
    ++n;
    XtSetArg(args[n], XmNsliderSize, drawAreaHeight);
    ++n;
    XtSetArg(args[n], XmNincrement, 16);
    ++n;
    XtSetArg(args[n], XmNpageIncrement, drawAreaHeight);
    ++n;
    XtSetValues(vScrollBar, args, n);
}

void XPDFCore::setCursor(Cursor cursor)
{
    Window topWin;

    if (cursor == currentCursor) {
        return;
    }
    if (!(topWin = XtWindow(shell))) {
        return;
    }
    if (cursor == None) {
        XUndefineCursor(display, topWin);
    } else {
        XDefineCursor(display, topWin, cursor);
    }
    XFlush(display);
    currentCursor = cursor;
}

bool XPDFCore::doQuestionDialog(const char *title, GString *msg)
{
    return doDialog(XmDIALOG_QUESTION, true, title, msg);
}

void XPDFCore::doInfoDialog(const char *title, GString *msg)
{
    doDialog(XmDIALOG_INFORMATION, false, title, msg);
}

void XPDFCore::doErrorDialog(const char *title, GString *msg)
{
    doDialog(XmDIALOG_ERROR, false, title, msg);
}

bool XPDFCore::doDialog(int type, bool hasCancel, const char *title, GString *msg)
{
    Widget       dialog, scroll, text;
    XtAppContext appContext;
    Arg          args[20];
    int          n;
    XmString     s1, s2;
    XEvent       event;

    n = 0;
    XtSetArg(args[n], XmNdialogType, type);
    ++n;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL);
    ++n;
    s1 = XmStringCreateLocalized((char *)title);
    XtSetArg(args[n], XmNdialogTitle, s1);
    ++n;
    s2 = NULL; // make gcc happy
    if (msg->getLength() <= 80) {
        s2 = XmStringCreateLocalized((char *)msg->c_str());
        XtSetArg(args[n], XmNmessageString, s2);
        ++n;
    }
    dialog = XmCreateMessageDialog(drawArea, "questionDialog", args, n);
    XmStringFree(s1);
    if (msg->getLength() <= 80) {
        XmStringFree(s2);
    } else {
        n = 0;
        XtSetArg(args[n], XmNscrollingPolicy, XmAUTOMATIC);
        ++n;
        if (drawAreaWidth > 300) {
            XtSetArg(args[n], XmNwidth, drawAreaWidth - 100);
            ++n;
        }
        scroll = XmCreateScrolledWindow(dialog, "scroll", args, n);
        XtManageChild(scroll);
        n = 0;
        XtSetArg(args[n], XmNeditable, False);
        ++n;
        XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
        ++n;
        XtSetArg(args[n], XmNvalue, msg->c_str());
        ++n;
        XtSetArg(args[n], XmNshadowThickness, 0);
        ++n;
        text = XmCreateText(scroll, "text", args, n);
        XtManageChild(text);
    }
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtAddCallback(dialog, XmNokCallback, &dialogOkCbk, (XtPointer)this);
    if (hasCancel) {
        XtAddCallback(dialog, XmNcancelCallback, &dialogCancelCbk,
                      (XtPointer)this);
    } else {
        XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    }

    XtManageChild(dialog);

    appContext = XtWidgetToApplicationContext(dialog);
    dialogDone = 0;
    do {
        XtAppNextEvent(appContext, &event);
        XtDispatchEvent(&event);
    } while (!dialogDone);

    XtUnmanageChild(dialog);
    XtDestroyWidget(dialog);

    return dialogDone > 0;
}

void XPDFCore::dialogOkCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *core = (XPDFCore *)ptr;

    core->dialogDone = 1;
}

void XPDFCore::dialogCancelCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *core = (XPDFCore *)ptr;

    core->dialogDone = -1;
}

//------------------------------------------------------------------------
// password dialog
//------------------------------------------------------------------------

void XPDFCore::initPasswordDialog()
{
    Widget   row, label, okBtn, cancelBtn;
    Arg      args[20];
    int      n;
    XmString s;

    //----- dialog
    n = 0;
    s = XmStringCreateLocalized(xpdfAppName ": Password");
    XtSetArg(args[n], XmNdialogTitle, s);
    ++n;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL);
    ++n;
    passwordDialog = XmCreateFormDialog(drawArea, "passwordDialog", args, n);
    XmStringFree(s);

    //----- message
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM);
    ++n;
    XtSetArg(args[n], XmNtopOffset, 4);
    ++n;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
    ++n;
    XtSetArg(args[n], XmNleftOffset, 4);
    ++n;
    s = XmStringCreateLocalized("This document requires a password.");
    XtSetArg(args[n], XmNlabelString, s);
    ++n;
    label = XmCreateLabel(passwordDialog, "msg", args, n);
    XmStringFree(s);
    XtManageChild(label);

    //----- label and password entry
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
    ++n;
    XtSetArg(args[n], XmNtopWidget, label);
    ++n;
    XtSetArg(args[n], XmNtopOffset, 4);
    ++n;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
    ++n;
    XtSetArg(args[n], XmNleftOffset, 4);
    ++n;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
    ++n;
    XtSetArg(args[n], XmNleftOffset, 4);
    ++n;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL);
    ++n;
    XtSetArg(args[n], XmNpacking, XmPACK_TIGHT);
    ++n;
    row = XmCreateRowColumn(passwordDialog, "row", args, n);
    XtManageChild(row);
    n = 0;
    s = XmStringCreateLocalized("Password: ");
    XtSetArg(args[n], XmNlabelString, s);
    ++n;
    label = XmCreateLabel(row, "label", args, n);
    XmStringFree(s);
    XtManageChild(label);
    n = 0;
    XtSetArg(args[n], XmNcolumns, 16);
    ++n;
    passwordText = XmCreateTextField(row, "text", args, n);
    XtManageChild(passwordText);
    XtAddCallback(passwordText, XmNmodifyVerifyCallback, &passwordTextVerifyCbk,
                  this);

    //----- "Ok" and "Cancel" buttons
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
    ++n;
    XtSetArg(args[n], XmNtopWidget, row);
    ++n;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
    ++n;
    XtSetArg(args[n], XmNleftOffset, 4);
    ++n;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM);
    ++n;
    XtSetArg(args[n], XmNbottomOffset, 4);
    ++n;
    XtSetArg(args[n], XmNnavigationType, XmEXCLUSIVE_TAB_GROUP);
    ++n;
    okBtn = XmCreatePushButton(passwordDialog, "Ok", args, n);
    XtManageChild(okBtn);
    XtAddCallback(okBtn, XmNactivateCallback, &passwordOkCbk, (XtPointer)this);
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
    ++n;
    XtSetArg(args[n], XmNtopWidget, row);
    ++n;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
    ++n;
    XtSetArg(args[n], XmNrightOffset, 4);
    ++n;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM);
    ++n;
    XtSetArg(args[n], XmNbottomOffset, 4);
    ++n;
    XtSetArg(args[n], XmNnavigationType, XmEXCLUSIVE_TAB_GROUP);
    ++n;
    cancelBtn = XmCreatePushButton(passwordDialog, "Cancel", args, n);
    XtManageChild(cancelBtn);
    XtAddCallback(cancelBtn, XmNactivateCallback, &passwordCancelCbk,
                  (XtPointer)this);
    n = 0;
    XtSetArg(args[n], XmNdefaultButton, okBtn);
    ++n;
    XtSetArg(args[n], XmNcancelButton, cancelBtn);
    ++n;
#if XmVersion > 1001
    XtSetArg(args[n], XmNinitialFocus, passwordText);
    ++n;
#endif
    XtSetValues(passwordDialog, args, n);
}

void XPDFCore::passwordTextVerifyCbk(Widget widget, XtPointer ptr,
                                     XtPointer callData)
{
    XPDFCore *                  core = (XPDFCore *)ptr;
    XmTextVerifyCallbackStruct *data = (XmTextVerifyCallbackStruct *)callData;
    int                         i, n;

    i = (int)data->startPos;
    n = (int)data->endPos - i;
    if (i > core->password->getLength()) {
        i = core->password->getLength();
    }
    if (i + n > core->password->getLength()) {
        n = core->password->getLength() - i;
    }
    core->password->del(i, n);
    core->password->insert(i, data->text->ptr, data->text->length);

    for (i = 0; i < data->text->length; ++i) {
        data->text->ptr[i] = '*';
    }
    data->doit = True;
}

void XPDFCore::passwordOkCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *core = (XPDFCore *)ptr;

    core->dialogDone = 1;
}

void XPDFCore::passwordCancelCbk(Widget widget, XtPointer ptr, XtPointer callData)
{
    XPDFCore *core = (XPDFCore *)ptr;

    core->dialogDone = -1;
}

GString *XPDFCore::getPassword()
{
    XtAppContext appContext;
    XEvent       event;

    // NB: set <password> before calling XmTextFieldSetString, because
    // SetString will trigger a call to passwordTextVerifyCbk, which
    // expects <password> to be valid
    password = new GString();
    XmTextFieldSetString(passwordText, "");
    XtManageChild(passwordDialog);

    appContext = XtWidgetToApplicationContext(passwordDialog);
    dialogDone = 0;
    do {
        XtAppNextEvent(appContext, &event);
        XtDispatchEvent(&event);
    } while (!dialogDone);
    XtUnmanageChild(passwordDialog);

    if (dialogDone < 0) {
        delete password;
        return NULL;
    }
    return password;
}

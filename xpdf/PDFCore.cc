// -*- mode: c++; -*-
// Copyright 2004-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <cmath>
#include <iostream>

#include <utils/memory.hh>
#include <utils/string.hh>
#include <utils/GList.hh>

#include <splash/Splash.hh>
#include <splash/SplashBitmap.hh>
#include <splash/SplashPattern.hh>
#include <splash/SplashPath.hh>

#include <xpdf/CoreOutputDev.hh>
#include <xpdf/Error.hh>
#include <xpdf/ErrorCodes.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/Link.hh>
#include <xpdf/PDFCore.hh>
#include <xpdf/PDFDoc.hh>
#include <xpdf/TextOutputDev.hh>
#include <xpdf/xpdf.hh>

#include <boost/scope_exit.hpp>

PDFCorePage::PDFCorePage (int pageA, int wA, int hA, int tileWA, int tileHA) {
    page = pageA;
    tiles = new GList ();
    w = wA;
    h = hA;
    tileW = tileWA;
    tileH = tileHA;
    links = NULL;
    text = NULL;
}

PDFCorePage::~PDFCorePage () {
    deleteGList (tiles, PDFCoreTile);
    if (links) { delete links; }
}

void PDFCore::segment () {
    setBusyCursor (true);

    BOOST_SCOPE_EXIT(this_) {
        this_->setBusyCursor (false);
    } BOOST_SCOPE_EXIT_END

    auto page = findPage (topPage);

    if (!page) {
        displayPage (topPage, zoom, rotate, true, false);

        if (!(page = findPage (topPage))) {
            return;
        }
    }

    for (auto& box : page->text->segment ()) {
        xorRectangle (page->page, box);
    }

    redrawWindow (0, 0, page->w, page->h, false);
}

//------------------------------------------------------------------------
// PDFCoreTile
//------------------------------------------------------------------------

PDFCoreTile::PDFCoreTile (int xDestA, int yDestA) {
    xMin = 0;
    yMin = 0;
    xMax = 0;
    yMax = 0;
    xDest = xDestA;
    yDest = yDestA;
    bitmap = NULL;
}

PDFCoreTile::~PDFCoreTile () {
    if (bitmap) { delete bitmap; }
}

//------------------------------------------------------------------------
// PDFCore
//------------------------------------------------------------------------

PDFCore::PDFCore (
    SplashColorMode colorModeA, int bitmapRowPadA, bool reverseVideoA,
    SplashColorPtr paperColorA, bool incrementalUpdate) {
    int i;

    doc = NULL;
    continuousMode = globalParams->getContinuousView ();
    drawAreaWidth = drawAreaHeight = 0;
    maxPageW = totalDocH = 0;
    pageY = NULL;
    topPage = 0;
    midPage = 0;
    scrollX = scrollY = 0;
    zoom = defZoom;
    dpi = 0;
    rotate = 0;

    selectPage = 0;
    selectULX = selectLRX = 0;
    selectULY = selectLRY = 0;
    dragging = false;
    lastDragLeft = lastDragTop = true;
    selectXorColor[0] = selectXorColor[1] = selectXorColor[2] =
        reverseVideoA ? 0xff : 0x00;
    splashColorXor (selectXorColor, paperColorA);

    historyCur = pdfHistorySize - 1;
    historyBLen = historyFLen = 0;
    for (i = 0; i < pdfHistorySize; ++i) { history[i].fileName = NULL; }

    pages = new GList ();
    curTile = NULL;

    splashColorCopy (paperColor, paperColorA);
    out = new CoreOutputDev (
        colorModeA, bitmapRowPadA, reverseVideoA, paperColorA,
        incrementalUpdate, &redrawCbk, this);
    out->startDoc (NULL);
}

PDFCore::~PDFCore () {
    int i;

    if (doc) { delete doc; }
    for (i = 0; i < pdfHistorySize; ++i) {
        if (history[i].fileName) { delete history[i].fileName; }
    }
    free (pageY);
    deleteGList (pages, PDFCorePage);
    delete out;
}

int PDFCore::loadFile (
    GString* fileName, GString* ownerPassword, GString* userPassword) {
    int err;

    setBusyCursor (true);
    err = loadFile2 (
        new PDFDoc (fileName->copy (), ownerPassword, userPassword, this));
    setBusyCursor (false);
    return err;
}

int PDFCore::loadFile (
    BaseStream* stream, GString* ownerPassword, GString* userPassword) {
    int err;

    setBusyCursor (true);
    err = loadFile2 (new PDFDoc (stream, ownerPassword, userPassword, this));
    setBusyCursor (false);
    return err;
}

void PDFCore::loadDoc (PDFDoc* docA) {
    setBusyCursor (true);
    loadFile2 (docA);
    setBusyCursor (false);
}

int PDFCore::loadFile2 (PDFDoc* newDoc) {
    int err;
    double w, h, t;
    int i;

    // open the PDF file
    if (!newDoc->isOk ()) {
        err = newDoc->getErrorCode ();
        delete newDoc;
        return err;
    }

    // replace old document
    if (doc) { delete doc; }
    doc = newDoc;
    if (out) { out->startDoc (doc->getXRef ()); }

    // nothing displayed yet
    topPage = -99;
    midPage = -99;
    while (pages->getLength () > 0) { delete (PDFCorePage*)pages->del (0); }

    // compute the max unscaled page size
    maxUnscaledPageW = maxUnscaledPageH = 0;
    for (i = 1; i <= doc->getNumPages (); ++i) {
        w = doc->getPageCropWidth (i);
        h = doc->getPageCropHeight (i);
        if (doc->getPageRotate (i) == 90 || doc->getPageRotate (i) == 270) {
            t = w;
            w = h;
            h = t;
        }
        if (w > maxUnscaledPageW) { maxUnscaledPageW = w; }
        if (h > maxUnscaledPageH) { maxUnscaledPageH = h; }
    }

    return errNone;
}

void PDFCore::clear () {
    if (!doc) { return; }

    // no document
    delete doc;
    doc = NULL;
    out->clear ();

    // no page displayed
    topPage = -99;
    midPage = -99;
    while (pages->getLength () > 0) { delete (PDFCorePage*)pages->del (0); }

    // redraw
    scrollX = scrollY = 0;
    redrawWindow (0, 0, drawAreaWidth, drawAreaHeight, true);
    updateScrollbars ();
}

PDFDoc* PDFCore::takeDoc (bool redraw) {
    PDFDoc* docA;

    if (!doc) { return NULL; }

    // no document
    docA = doc;
    doc = NULL;
    out->clear ();

    // no page displayed
    topPage = -99;
    midPage = -99;
    while (pages->getLength () > 0) { delete (PDFCorePage*)pages->del (0); }

    // redraw
    scrollX = scrollY = 0;
    if (redraw) {
        redrawWindow (0, 0, drawAreaWidth, drawAreaHeight, true);
        updateScrollbars ();
    }

    return docA;
}

void PDFCore::displayPage (
    int topPageA, double zoomA, int rotateA, bool scrollToTop,
    bool addToHist) {
    int scrollXA, scrollYA;

    scrollXA = scrollX;
    if (continuousMode) { scrollYA = -1; }
    else if (scrollToTop) {
        scrollYA = 0;
    }
    else {
        scrollYA = scrollY;
    }
    if (zoomA != zoom) {
        scrollXA = 0;
        scrollYA = continuousMode ? -1 : 0;
    }

    dragging = false;
    lastDragLeft = lastDragTop = true;

    update (
        topPageA, scrollXA, scrollYA, zoomA, rotateA, true, addToHist, true);
}

void PDFCore::displayDest (
    LinkDest* dest, double zoomA, int rotateA, bool addToHist) {
    Ref pageRef;
    int topPageA;
    int dx, dy, scrollXA, scrollYA;

    if (dest->isPageRef ()) {
        pageRef = dest->getPageRef ();
        topPageA = doc->findPage (pageRef.num, pageRef.gen);
    }
    else {
        topPageA = dest->getPageNum ();
    }
    if (topPageA <= 0 || topPageA > doc->getNumPages ()) { topPageA = 1; }
    scrollXA = scrollX;
    scrollYA = continuousMode ? -1 : scrollY;
    switch (dest->getKind ()) {
    case destXYZ:
        cvtUserToDev (topPageA, dest->getLeft (), dest->getTop (), &dx, &dy);
        scrollXA = dest->getChangeLeft () ? dx : scrollX;
        if (continuousMode) {
            if (topPage <= 0) { scrollYA = -1; }
            else if (dest->getChangeTop ()) {
                scrollYA = pageY[topPageA - 1] + dy;
            }
            else {
                scrollYA = pageY[topPageA - 1] + (scrollY - pageY[topPage - 1]);
            }
        }
        else {
            if (dest->getChangeTop ()) { scrollYA = dy; }
            else if (topPage > 0) {
                scrollYA = scrollY;
            }
            else {
                scrollYA = 0;
            }
        }
        //~ this doesn't currently handle the zoom parameter
        update (
            topPageA, scrollXA, scrollYA, zoom, rotate, false,
            addToHist && topPageA != topPage, true);
        break;
    case destFit:
    case destFitB:
        scrollXA = 0;
        scrollYA = continuousMode ? -1 : 0;
        update (
            topPageA, scrollXA, scrollYA, zoomPage, rotate, false,
            addToHist && topPageA != topPage, true);
        break;
    case destFitH:
    case destFitBH:
        //~ do fit: need a function similar to zoomToRect which will
        //~ accept an absolute top coordinate (rather than centering)
        scrollXA = 0;
        cvtUserToDev (topPageA, 0, dest->getTop (), &dx, &dy);
        if (continuousMode) {
            if (topPage <= 0) { scrollYA = -1; }
            else if (dest->getChangeTop ()) {
                scrollYA = pageY[topPageA - 1] + dy;
            }
            else {
                scrollYA = pageY[topPageA - 1] + (scrollY - pageY[topPage - 1]);
            }
        }
        else {
            if (dest->getChangeTop ()) { scrollYA = dy; }
            else if (topPage > 0) {
                scrollYA = scrollY;
            }
            else {
                scrollYA = 0;
            }
        }
        update (
            topPageA, scrollXA, scrollYA, zoom, rotate, false,
            addToHist && topPageA != topPage, true);
        break;
    case destFitV:
    case destFitBV:
        //~ do fit: need a function similar to zoomToRect which will
        //~ accept an absolute left coordinate (rather than centering)
        if (dest->getChangeLeft ()) {
            cvtUserToDev (topPageA, dest->getLeft (), 0, &dx, &dy);
            scrollXA = dx;
        }
        else {
            scrollXA = scrollX;
        }
        scrollYA = continuousMode ? -1 : 0;
        update (
            topPageA, scrollXA, scrollYA, zoom, rotate, false,
            addToHist && topPageA != topPage, true);
        break;
    case destFitR:
        zoomToRect (
            topPageA, dest->getLeft (), dest->getTop (), dest->getRight (),
            dest->getBottom ());
        break;
    }
}

void PDFCore::update (
    int topPageA, int scrollXA, int scrollYA, double zoomA, int rotateA,
    bool force, bool addToHist, bool adjustScrollX) {
    double hDPI, vDPI, dpiA, uw, uh, ut;
    int w, h, t, x0, x1, y0, y1, x, y;
    int rot;
    int pg0, pg1;
    PDFCoreTile* tile;
    PDFCorePage* page;
    PDFHistory* hist;
    bool needUpdate;
    int i, j;

    // check for document and valid page number
    if (!doc) {
        // save the new settings
        zoom = zoomA;
        rotate = rotateA;
        return;
    }
    if (topPageA <= 0 || topPageA > doc->getNumPages ()) { return; }

    needUpdate = false;

    // check for changes to the PDF file
    if ((force || (!continuousMode && topPage != topPageA)) &&
        doc->getFileName () && checkForNewFile ()) {
        if (loadFile (doc->getFileName ()) == errNone) {
            if (topPageA > doc->getNumPages ()) {
                topPageA = doc->getNumPages ();
            }
            needUpdate = true;
        }
    }

    // compute the DPI
    if (continuousMode) {
        uw = maxUnscaledPageW;
        uh = maxUnscaledPageH;
        rot = rotateA;
    }
    else {
        uw = doc->getPageCropWidth (topPageA);
        uh = doc->getPageCropHeight (topPageA);
        rot = rotateA + doc->getPageRotate (topPageA);
        if (rot >= 360) { rot -= 360; }
        else if (rot < 0) {
            rot += 360;
        }
    }
    if (rot == 90 || rot == 270) {
        ut = uw;
        uw = uh;
        uh = ut;
    }
    if (zoomA == zoomPage) {
        hDPI = (drawAreaWidth / uw) * 72;
        if (continuousMode) {
            vDPI = ((drawAreaHeight - continuousModePageSpacing) / uh) * 72;
        }
        else {
            vDPI = (drawAreaHeight / uh) * 72;
        }
        dpiA = (hDPI < vDPI) ? hDPI : vDPI;
    }
    else if (zoomA == zoomWidth) {
        dpiA = (drawAreaWidth / uw) * 72;
    }
    else {
        dpiA = 0.01 * zoomA * 72;
    }
    // this can happen if the window hasn't been sized yet
    if (dpiA <= 0) { dpiA = 1; }

    // if the display properties have changed, create a new PDFCorePage
    // object
    if (force || pages->getLength () == 0 ||
        (!continuousMode && topPageA != topPage) ||
        fabs (zoomA - zoom) > 1e-8 || fabs (dpiA - dpi) > 1e-8 ||
        rotateA != rotate) {
        needUpdate = true;
        setSelection (0, 0, 0, 0, 0);
        while (pages->getLength () > 0) { delete (PDFCorePage*)pages->del (0); }
        zoom = zoomA;
        rotate = rotateA;
        dpi = dpiA;
        if (continuousMode) {
            maxPageW = totalDocH = 0;
            pageY = (int*)reallocarray (pageY, doc->getNumPages (), sizeof (int));
            for (i = 1; i <= doc->getNumPages (); ++i) {
                pageY[i - 1] = totalDocH;
                w = (int)((doc->getPageCropWidth (i) * dpi) / 72 + 0.5);
                h = (int)((doc->getPageCropHeight (i) * dpi) / 72 + 0.5);
                rot = rotate + doc->getPageRotate (i);
                if (rot >= 360) { rot -= 360; }
                else if (rot < 0) {
                    rot += 360;
                }
                if (rot == 90 || rot == 270) {
                    t = w;
                    w = h;
                    h = t;
                }
                if (w > maxPageW) { maxPageW = w; }
                totalDocH += h;
                if (i < doc->getNumPages ()) {
                    totalDocH += continuousModePageSpacing;
                }
            }
        }
        else {
            rot = rotate + doc->getPageRotate (topPageA);
            if (rot >= 360) { rot -= 360; }
            else if (rot < 0) {
                rot += 360;
            }
            addPage (topPageA, rot);
        }
    }
    else {
        // erase the selection
        if (selectULX != selectLRX && selectULY != selectLRY) {
            xorRectangle (
                selectPage, selectULX, selectULY, selectLRX, selectLRY,
                new SplashSolidColor (selectXorColor));
        }
    }
    if (continuousMode) {
        page = NULL; // make gcc happy
    }
    else {
        page = (PDFCorePage*)pages->get (0);
    }
    topPage = topPageA;
    midPage = topPage;

    // adjust the scroll position
    scrollX = scrollXA;
    if (continuousMode && scrollYA < 0) { scrollY = pageY[topPage - 1]; }
    else {
        scrollY = scrollYA;
    }
    if (continuousMode && adjustScrollX) {
        rot = rotate + doc->getPageRotate (topPage);
        if (rot >= 360) { rot -= 360; }
        else if (rot < 0) {
            rot += 360;
        }
        if (rot == 90 || rot == 270) {
            w = (int)((doc->getPageCropHeight (topPage) * dpi) / 72 + 0.5);
        }
        else {
            w = (int)((doc->getPageCropWidth (topPage) * dpi) / 72 + 0.5);
        }
        if (scrollX < (maxPageW - w) / 2) { scrollX = (maxPageW - w) / 2; }
    }
    w = continuousMode ? maxPageW : page->w;
    if (scrollX > w - drawAreaWidth) { scrollX = w - drawAreaWidth; }
    if (scrollX < 0) { scrollX = 0; }
    h = continuousMode ? totalDocH : page->h;
    if (scrollY > h - drawAreaHeight) { scrollY = h - drawAreaHeight; }
    if (scrollY < 0) { scrollY = 0; }

    // find topPage, and the first and last pages to be rasterized
    if (continuousMode) {
        //~ should use a binary search
        for (i = 2; i <= doc->getNumPages (); ++i) {
            if (pageY[i - 1] > scrollY - drawAreaHeight / 2) { break; }
        }
        pg0 = i - 1;
        for (i = pg0 + 1; i <= doc->getNumPages (); ++i) {
            if (pageY[i - 1] > scrollY) { break; }
        }
        topPage = i - 1;
        for (i = topPage + 1; i <= doc->getNumPages (); ++i) {
            if (pageY[i - 1] > scrollY + drawAreaHeight / 2) { break; }
        }
        midPage = i - 1;
        for (i = midPage + 1; i <= doc->getNumPages (); ++i) {
            if (pageY[i - 1] > scrollY + drawAreaHeight + drawAreaHeight / 2) {
                break;
            }
        }
        pg1 = i - 1;

        // delete pages that are no longer needed and insert new pages
        // objects that are needed
        while (pages->getLength () > 0 &&
               ((PDFCorePage*)pages->get (0))->page < pg0) {
            delete (PDFCorePage*)pages->del (0);
        }
        i = pages->getLength () - 1;
        while (i > 0 && ((PDFCorePage*)pages->get (i))->page > pg1) {
            delete (PDFCorePage*)pages->del (i--);
        }
        j = pages->getLength () > 0 ? ((PDFCorePage*)pages->get (0))->page - 1
                                    : pg1;
        for (i = pg0; i <= j; ++i) {
            rot = rotate + doc->getPageRotate (i);
            if (rot >= 360) { rot -= 360; }
            else if (rot < 0) {
                rot += 360;
            }
            addPage (i, rot);
        }
        j = ((PDFCorePage*)pages->get (pages->getLength () - 1))->page;
        for (i = j + 1; i <= pg1; ++i) {
            rot = rotate + doc->getPageRotate (i);
            if (rot >= 360) { rot -= 360; }
            else if (rot < 0) {
                rot += 360;
            }
            addPage (i, rot);
        }
    }
    else {
        pg0 = pg1 = topPage;
    }

    // delete tiles that are no longer needed
    for (i = 0; i < pages->getLength (); ++i) {
        page = (PDFCorePage*)pages->get (i);
        j = 0;
        while (j < page->tiles->getLength ()) {
            tile = (PDFCoreTile*)page->tiles->get (j);
            if (continuousMode) {
                y0 = pageY[page->page - 1] + tile->yMin;
                y1 = pageY[page->page - 1] + tile->yMax;
            }
            else {
                y0 = tile->yMin;
                y1 = tile->yMax;
            }
            if (tile->xMax < scrollX - drawAreaWidth / 2 ||
                tile->xMin > scrollX + drawAreaWidth + drawAreaWidth / 2 ||
                y1 < scrollY - drawAreaHeight / 2 ||
                y0 > scrollY + drawAreaHeight + drawAreaHeight / 2) {
                delete (PDFCoreTile*)page->tiles->del (j);
            }
            else {
                ++j;
            }
        }
    }

    // update page positions
    for (i = 0; i < pages->getLength (); ++i) {
        page = (PDFCorePage*)pages->get (i);
        page->xDest = -scrollX;
        if (continuousMode) { page->yDest = pageY[page->page - 1] - scrollY; }
        else {
            page->yDest = -scrollY;
        }
        if (continuousMode) {
            if (page->w < maxPageW) { page->xDest += (maxPageW - page->w) / 2; }
            if (maxPageW < drawAreaWidth) {
                page->xDest += (drawAreaWidth - maxPageW) / 2;
            }
        }
        else if (page->w < drawAreaWidth) {
            page->xDest += (drawAreaWidth - page->w) / 2;
        }
        if (continuousMode && totalDocH < drawAreaHeight) {
            page->yDest += (drawAreaHeight - totalDocH) / 2;
        }
        else if (!continuousMode && page->h < drawAreaHeight) {
            page->yDest += (drawAreaHeight - page->h) / 2;
        }
    }

    // rasterize any new tiles
    for (i = 0; i < pages->getLength (); ++i) {
        page = (PDFCorePage*)pages->get (i);
        x0 = page->xDest;
        x1 = x0 + page->w - 1;
        if (x0 < -drawAreaWidth / 2) { x0 = -drawAreaWidth / 2; }
        if (x1 > drawAreaWidth + drawAreaWidth / 2) {
            x1 = drawAreaWidth + drawAreaWidth / 2;
        }
        x0 = ((x0 - page->xDest) / page->tileW) * page->tileW;
        x1 = ((x1 - page->xDest) / page->tileW) * page->tileW;
        y0 = page->yDest;
        y1 = y0 + page->h - 1;
        if (y0 < -drawAreaHeight / 2) { y0 = -drawAreaHeight / 2; }
        if (y1 > drawAreaHeight + drawAreaHeight / 2) {
            y1 = drawAreaHeight + drawAreaHeight / 2;
        }
        y0 = ((y0 - page->yDest) / page->tileH) * page->tileH;
        y1 = ((y1 - page->yDest) / page->tileH) * page->tileH;
        for (y = y0; y <= y1; y += page->tileH) {
            for (x = x0; x <= x1; x += page->tileW) { needTile (page, x, y); }
        }
    }

    // update tile positions
    for (i = 0; i < pages->getLength (); ++i) {
        page = (PDFCorePage*)pages->get (i);
        for (j = 0; j < page->tiles->getLength (); ++j) {
            tile = (PDFCoreTile*)page->tiles->get (j);
            tile->xDest = tile->xMin - scrollX;
            if (continuousMode) {
                tile->yDest = tile->yMin + pageY[page->page - 1] - scrollY;
            }
            else {
                tile->yDest = tile->yMin - scrollY;
            }
            if (continuousMode) {
                if (page->w < maxPageW) {
                    tile->xDest += (maxPageW - page->w) / 2;
                }
                if (maxPageW < drawAreaWidth) {
                    tile->xDest += (drawAreaWidth - maxPageW) / 2;
                }
            }
            else if (page->w < drawAreaWidth) {
                tile->xDest += (drawAreaWidth - page->w) / 2;
            }
            if (continuousMode && totalDocH < drawAreaHeight) {
                tile->yDest += (drawAreaHeight - totalDocH) / 2;
            }
            else if (!continuousMode && page->h < drawAreaHeight) {
                tile->yDest += (drawAreaHeight - page->h) / 2;
            }
        }
    }

    // redraw the selection
    if (selectULX != selectLRX && selectULY != selectLRY) {
        xorRectangle (
            selectPage, selectULX, selectULY, selectLRX, selectLRY,
            new SplashSolidColor (selectXorColor));
    }

    // redraw the window
    redrawWindow (0, 0, drawAreaWidth, drawAreaHeight, needUpdate);
    updateScrollbars ();

    // add to history
    if (addToHist) {
        if (++historyCur == pdfHistorySize) { historyCur = 0; }
        hist = &history[historyCur];
        if (hist->fileName) { delete hist->fileName; }
        if (doc->getFileName ()) {
            hist->fileName = doc->getFileName ()->copy ();
        }
        else {
            hist->fileName = NULL;
        }
        hist->page = topPage;
        if (historyBLen < pdfHistorySize) { ++historyBLen; }
        historyFLen = 0;
    }
}

void PDFCore::addPage (int pg, int rot) {
    PDFCorePage* page;
    int w, h, t, tileW, tileH, i;

    w = (int)((doc->getPageCropWidth (pg) * dpi) / 72 + 0.5);
    h = (int)((doc->getPageCropHeight (pg) * dpi) / 72 + 0.5);
    if (rot == 90 || rot == 270) {
        t = w;
        w = h;
        h = t;
    }
    tileW = 2 * drawAreaWidth;
    if (tileW < 1500) { tileW = 1500; }
    if (tileW > w) {
        // tileW can't be zero -- we end up with div-by-zero problems
        tileW = w ? w : 1;
    }
    tileH = 2 * drawAreaHeight;
    if (tileH < 1500) { tileH = 1500; }
    if (tileH > h) {
        // tileH can't be zero -- we end up with div-by-zero problems
        tileH = h ? h : 1;
    }
    page = new PDFCorePage (pg, w, h, tileW, tileH);
    for (i = 0;
         i < pages->getLength () && pg > ((PDFCorePage*)pages->get (i))->page;
         ++i)
        ;
    pages->insert (i, page);
}

void PDFCore::needTile (PDFCorePage* page, int x, int y) {
    PDFCoreTile* tile;
    int xDest, yDest, sliceW, sliceH;
    int i;

    //
    // Verify the tile cache for a matching tile:
    //
    for (i = 0; i < page->tiles->getLength (); ++i) {
        tile = (PDFCoreTile*)page->tiles->get (i);
        if (x == tile->xMin && y == tile->yMin) {
            return;
        }
    }

    setBusyCursor (true);

    sliceW = page->tileW;

    if (x + sliceW > page->w) {
        sliceW = page->w - x;
    }

    sliceH = page->tileH;

    if (y + sliceH > page->h) {
        sliceH = page->h - y;
    }

    xDest = x - scrollX;

    if (continuousMode) {
        yDest = y + pageY [page->page - 1] - scrollY;
    }
    else {
        yDest = y - scrollY;
    }

    if (continuousMode) {
        if (page->w < maxPageW) { xDest += (maxPageW - page->w) / 2; }
        if (maxPageW < drawAreaWidth) {
            xDest += (drawAreaWidth - maxPageW) / 2;
        }
    }
    else if (page->w < drawAreaWidth) {
        xDest += (drawAreaWidth - page->w) / 2;
    }

    if (continuousMode && totalDocH < drawAreaHeight) {
        yDest += (drawAreaHeight - totalDocH) / 2;
    }
    else if (!continuousMode && page->h < drawAreaHeight) {
        yDest += (drawAreaHeight - page->h) / 2;
    }

    curTile = tile = newTile (xDest, yDest);
    curPage = page;

    tile->xMin = x;
    tile->yMin = y;

    tile->xMax = x + sliceW;
    tile->yMax = y + sliceH;

    tile->edges = 0;

    if (tile->xMin == 0) {
        tile->edges |= pdfCoreTileLeftEdge;
    }

    if (tile->xMax == page->w) {
        tile->edges |= pdfCoreTileRightEdge;
    }

    if (continuousMode) {
        if (tile->yMin == 0) {
            tile->edges |= pdfCoreTileTopSpace;
            if (page->page == 1) {
                tile->edges |= pdfCoreTileTopEdge;
            }
        }

        if (tile->yMax == page->h) {
            tile->edges |= pdfCoreTileBottomSpace;
            if (page->page == doc->getNumPages ()) {
                tile->edges |= pdfCoreTileBottomEdge;
            }
        }
    }
    else {
        if (tile->yMin == 0) { tile->edges |= pdfCoreTileTopEdge; }
        if (tile->yMax == page->h) { tile->edges |= pdfCoreTileBottomEdge; }
    }

    doc->displayPageSlice (
        out, page->page, dpi, dpi, rotate, false, true, false, x, y, sliceW,
        sliceH);

    tile->bitmap = out->takeBitmap ();

    memcpy (tile->ctm, out->getDefCTM (), 6 * sizeof (double));
    memcpy (tile->ictm, out->getDefICTM (), 6 * sizeof (double));

    if (!page->links) { page->links = doc->getLinks (page->page); }

    if (!page->text) {
        TextOutputControl ctrl;
        ctrl.mode = textOutPhysLayout;

        auto pdev = std::make_unique< TextOutputDev > (nullptr, &ctrl, false);

        doc->displayPage (
            pdev.get (), page->page, dpi, dpi, rotate, false, true, false);

        page->text = pdev->takeText ();
    }

    page->tiles->append (tile);

    curTile = NULL;
    curPage = NULL;

    setBusyCursor (false);
}

bool PDFCore::gotoNextPage (int inc, bool top) {
    int pg, scrollYA;

    if (!doc || doc->getNumPages () == 0 || topPage >= doc->getNumPages ()) {
        return false;
    }
    if ((pg = topPage + inc) > doc->getNumPages ()) {
        pg = doc->getNumPages ();
    }
    if (continuousMode) { scrollYA = -1; }
    else if (top) {
        scrollYA = 0;
    }
    else {
        scrollYA = scrollY;
    }
    update (pg, scrollX, scrollYA, zoom, rotate, false, true, true);
    return true;
}

bool PDFCore::gotoPrevPage (int dec, bool top, bool bottom) {
    int pg, scrollYA;

    if (!doc || doc->getNumPages () == 0 || topPage <= 1) { return false; }
    if ((pg = topPage - dec) < 1) { pg = 1; }
    if (continuousMode) { scrollYA = -1; }
    else if (top) {
        scrollYA = 0;
    }
    else if (bottom) {
        scrollYA = ((PDFCorePage*)pages->get (0))->h - drawAreaHeight;
        if (scrollYA < 0) { scrollYA = 0; }
    }
    else {
        scrollYA = scrollY;
    }
    update (pg, scrollX, scrollYA, zoom, rotate, false, true, true);
    return true;
}

bool PDFCore::gotoNamedDestination (GString* dest) {
    LinkDest* d;

    if (!doc) { return false; }
    if (!(d = doc->findDest (dest))) { return false; }
    displayDest (d, zoom, rotate, true);
    delete d;
    return true;
}

bool PDFCore::goForward () {
    int pg;

    if (historyFLen == 0) { return false; }
    if (++historyCur == pdfHistorySize) { historyCur = 0; }
    --historyFLen;
    ++historyBLen;
    if (!history[historyCur].fileName) { return false; }
    if (!doc || !doc->getFileName () ||
        history[historyCur].fileName->cmp (doc->getFileName ()) != 0) {
        if (loadFile (history[historyCur].fileName) != errNone) {
            return false;
        }
    }
    pg = history[historyCur].page;
    update (
        pg, scrollX, continuousMode ? -1 : scrollY, zoom, rotate, false,
        false, true);
    return true;
}

bool PDFCore::goBackward () {
    int pg;

    if (historyBLen <= 1) { return false; }
    if (--historyCur < 0) { historyCur = pdfHistorySize - 1; }
    --historyBLen;
    ++historyFLen;
    if (!history[historyCur].fileName) { return false; }
    if (!doc || !doc->getFileName () ||
        history[historyCur].fileName->cmp (doc->getFileName ()) != 0) {
        if (loadFile (history[historyCur].fileName) != errNone) {
            return false;
        }
    }
    pg = history[historyCur].page;
    update (
        pg, scrollX, continuousMode ? -1 : scrollY, zoom, rotate, false,
        false, true);
    return true;
}

void PDFCore::scrollLeft (int nCols) { scrollTo (scrollX - nCols, scrollY); }

void PDFCore::scrollRight (int nCols) { scrollTo (scrollX + nCols, scrollY); }

void PDFCore::scrollUp (int nLines) { scrollTo (scrollX, scrollY - nLines); }

void PDFCore::scrollUpPrevPage (int nLines) {
    if (!continuousMode && scrollY == 0) { gotoPrevPage (1, false, true); }
    else {
        scrollTo (scrollX, scrollY - nLines);
    }
}

void PDFCore::scrollDown (int nLines) { scrollTo (scrollX, scrollY + nLines); }

void PDFCore::scrollDownNextPage (int nLines) {
    if (!continuousMode &&
        scrollY >= ((PDFCorePage*)pages->get (0))->h - drawAreaHeight) {
        gotoNextPage (1, true);
    }
    else {
        scrollTo (scrollX, scrollY + nLines);
    }
}

void PDFCore::scrollPageUp () {
    if (!continuousMode && scrollY == 0) { gotoPrevPage (1, false, true); }
    else {
        scrollTo (scrollX, scrollY - drawAreaHeight);
    }
}

void PDFCore::scrollPageDown () {
    if (!continuousMode && pages->getLength () > 0 &&
        scrollY >= ((PDFCorePage*)pages->get (0))->h - drawAreaHeight) {
        gotoNextPage (1, true);
    }
    else {
        scrollTo (scrollX, scrollY + drawAreaHeight);
    }
}

void PDFCore::scrollTo (int x, int y) {
    update (topPage, x, y < 0 ? 0 : y, zoom, rotate, false, false, false);
}

void PDFCore::scrollToLeftEdge () {
    update (topPage, 0, scrollY, zoom, rotate, false, false, false);
}

void PDFCore::scrollToRightEdge () {
    PDFCorePage* page;

    page = (PDFCorePage*)pages->get (0);
    update (
        topPage, page->w - drawAreaWidth, scrollY, zoom, rotate, false, false,
        false);
}

void PDFCore::scrollToTopEdge () {
    int y;

    y = continuousMode ? pageY[topPage - 1] : 0;
    update (topPage, scrollX, y, zoom, rotate, false, false, false);
}

void PDFCore::scrollToBottomEdge () {
    PDFCorePage* page;
    int y, i;

    for (i = pages->getLength () - 1; i > 0; --i) {
        page = (PDFCorePage*)pages->get (i);
        if (page->yDest < drawAreaHeight) { break; }
    }
    page = (PDFCorePage*)pages->get (i);
    if (continuousMode) {
        y = pageY[page->page - 1] + page->h - drawAreaHeight;
    }
    else {
        y = page->h - drawAreaHeight;
    }
    update (topPage, scrollX, y, zoom, rotate, false, false, false);
}

void PDFCore::scrollToTopLeft () {
    int y;

    y = continuousMode ? pageY[topPage - 1] : 0;
    update (topPage, 0, y, zoom, rotate, false, false, false);
}

void PDFCore::scrollToBottomRight () {
    PDFCorePage* page;
    int x, y, i;

    for (i = pages->getLength () - 1; i > 0; --i) {
        page = (PDFCorePage*)pages->get (i);
        if (page->yDest < drawAreaHeight) { break; }
    }
    page = (PDFCorePage*)pages->get (i);
    x = page->w - drawAreaWidth;
    if (continuousMode) {
        y = pageY[page->page - 1] + page->h - drawAreaHeight;
    }
    else {
        y = page->h - drawAreaHeight;
    }
    update (topPage, x, y, zoom, rotate, false, false, false);
}

void PDFCore::zoomToRect (
    int pg, double ulx, double uly, double lrx, double lry) {
    int x0, y0, x1, y1, u, sx, sy;
    double rx, ry, newZoom, t;
    PDFCorePage* p;

    cvtUserToDev (pg, ulx, uly, &x0, &y0);
    cvtUserToDev (pg, lrx, lry, &x1, &y1);
    if (x0 > x1) {
        u = x0;
        x0 = x1;
        x1 = u;
    }
    if (y0 > y1) {
        u = y0;
        y0 = y1;
        y1 = u;
    }
    rx = (double)drawAreaWidth / (double)(x1 - x0);
    ry = (double)drawAreaHeight / (double)(y1 - y0);
    if (rx < ry) {
        newZoom = rx * (dpi / (0.01 * 72));
        sx = (int)(rx * x0);
        t = (drawAreaHeight * (x1 - x0)) / drawAreaWidth;
        sy = (int)(rx * (y0 + y1 - t) / 2);
        if (continuousMode) {
            if ((p = findPage (pg)) && p->w < maxPageW) {
                sx += (int)(0.5 * rx * (maxPageW - p->w));
            }
            u = (pg - 1) * continuousModePageSpacing;
            sy += (int)(rx * (pageY[pg - 1] - u)) + u;
        }
    }
    else {
        newZoom = ry * (dpi / (0.01 * 72));
        t = (drawAreaWidth * (y1 - y0)) / drawAreaHeight;
        sx = (int)(ry * (x0 + x1 - t) / 2);
        sy = (int)(ry * y0);
        if (continuousMode) {
            if ((p = findPage (pg)) && p->w < maxPageW) {
                sx += (int)(0.5 * rx * (maxPageW - p->w));
            }
            u = (pg - 1) * continuousModePageSpacing;
            sy += (int)(ry * (pageY[pg - 1] - u)) + u;
        }
    }
    update (pg, sx, sy, newZoom, rotate, false, false, false);
}

void PDFCore::zoomCentered (double zoomA) {
    int sx, sy, rot, hAdjust, vAdjust, i;
    double dpi1, dpi2, pageW, pageH;
    PDFCorePage* page;

    if (zoomA == zoomPage) {
        if (continuousMode) {
            pageW = (rotate == 90 || rotate == 270) ? maxUnscaledPageH
                                                    : maxUnscaledPageW;
            pageH = (rotate == 90 || rotate == 270) ? maxUnscaledPageW
                                                    : maxUnscaledPageH;
            dpi1 = 72.0 * (double)drawAreaWidth / pageW;
            dpi2 = 72.0 * (double)(drawAreaHeight - continuousModePageSpacing) /
                   pageH;
            if (dpi2 < dpi1) { dpi1 = dpi2; }
        }
        else {
            // in single-page mode, sx=sy=0 -- so dpi1 is irrelevant
            dpi1 = dpi;
        }
        sx = 0;
    }
    else if (zoomA == zoomWidth) {
        if (continuousMode) {
            pageW = (rotate == 90 || rotate == 270) ? maxUnscaledPageH
                                                    : maxUnscaledPageW;
        }
        else {
            rot = rotate + doc->getPageRotate (topPage);
            if (rot >= 360) { rot -= 360; }
            else if (rot < 0) {
                rot += 360;
            }
            pageW = (rot == 90 || rot == 270) ? doc->getPageCropHeight (topPage)
                                              : doc->getPageCropWidth (topPage);
        }
        dpi1 = 72.0 * (double)drawAreaWidth / pageW;
        sx = 0;
    }
    else if (zoomA <= 0) {
        return;
    }
    else {
        dpi1 = 72.0 * zoomA / 100.0;
        if ((page = (PDFCorePage*)pages->get (0)) && page->xDest > 0) {
            hAdjust = page->xDest;
        }
        else {
            hAdjust = 0;
        }
        sx = (int)((scrollX - hAdjust + drawAreaWidth / 2) * (dpi1 / dpi)) -
             drawAreaWidth / 2;
        if (sx < 0) { sx = 0; }
    }

    if (continuousMode) {
        // we can't just multiply scrollY by dpi1/dpi -- the rounding
        // errors add up (because the pageY values are integers) -- so
        // we compute the pageY values at the new zoom level instead
        sy = 0;
        for (i = 1; i < topPage; ++i) {
            rot = rotate + doc->getPageRotate (i);
            if (rot >= 360) { rot -= 360; }
            else if (rot < 0) {
                rot += 360;
            }
            if (rot == 90 || rot == 270) {
                sy += (int)((doc->getPageCropWidth (i) * dpi1) / 72 + 0.5);
            }
            else {
                sy += (int)((doc->getPageCropHeight (i) * dpi1) / 72 + 0.5);
            }
        }
        vAdjust = (topPage - 1) * continuousModePageSpacing;
        sy =
            sy +
            (int)((scrollY - pageY[topPage - 1] + drawAreaHeight / 2) * (dpi1 / dpi)) +
            vAdjust - drawAreaHeight / 2;
    }
    else {
        sy = (int)((scrollY + drawAreaHeight / 2) * (dpi1 / dpi)) -
             drawAreaHeight / 2;
    }

    update (topPage, sx, sy, zoomA, rotate, false, false, false);
}

// Zoom so that the current page(s) fill the window width.  Maintain
// the vertical center.
void PDFCore::zoomToCurrentWidth () {
    double w, maxW, dpi1;
    int sx, sy, vAdjust, rot, i;

    // compute the maximum page width of visible pages
    rot = rotate + doc->getPageRotate (topPage);
    if (rot >= 360) { rot -= 360; }
    else if (rot < 0) {
        rot += 360;
    }
    if (rot == 90 || rot == 270) { maxW = doc->getPageCropHeight (topPage); }
    else {
        maxW = doc->getPageCropWidth (topPage);
    }
    if (continuousMode) {
        for (i = topPage + 1;
             i < doc->getNumPages () && pageY[i - 1] < scrollY + drawAreaHeight;
             ++i) {
            rot = rotate + doc->getPageRotate (i);
            if (rot >= 360) { rot -= 360; }
            else if (rot < 0) {
                rot += 360;
            }
            if (rot == 90 || rot == 270) { w = doc->getPageCropHeight (i); }
            else {
                w = doc->getPageCropWidth (i);
            }
            if (w > maxW) { maxW = w; }
        }
    }

    // compute the resolution
    dpi1 = (drawAreaWidth / maxW) * 72;

    // compute the horizontal scroll position
    if (continuousMode) {
        sx = ((int)(maxPageW * dpi1 / dpi) - drawAreaWidth) / 2;
    }
    else {
        sx = 0;
    }

    // compute the vertical scroll position
    if (continuousMode) {
        // we can't just multiply scrollY by dpi1/dpi -- the rounding
        // errors add up (because the pageY values are integers) -- so
        // we compute the pageY values at the new zoom level instead
        sy = 0;
        for (i = 1; i < topPage; ++i) {
            rot = rotate + doc->getPageRotate (i);
            if (rot >= 360) { rot -= 360; }
            else if (rot < 0) {
                rot += 360;
            }
            if (rot == 90 || rot == 270) {
                sy += (int)((doc->getPageCropWidth (i) * dpi1) / 72 + 0.5);
            }
            else {
                sy += (int)((doc->getPageCropHeight (i) * dpi1) / 72 + 0.5);
            }
        }
        vAdjust = (topPage - 1) * continuousModePageSpacing;
        sy =
            sy +
            (int)((scrollY - pageY[topPage - 1] + drawAreaHeight / 2) * (dpi1 / dpi)) +
            vAdjust - drawAreaHeight / 2;
    }
    else {
        sy = (int)((scrollY + drawAreaHeight / 2) * (dpi1 / dpi)) -
             drawAreaHeight / 2;
    }

    update (topPage, sx, sy, (dpi1 * 100) / 72, rotate, false, false, false);
}

void PDFCore::setContinuousMode (bool cm) {
    if (continuousMode != cm) {
        continuousMode = cm;
        update (topPage, scrollX, -1, zoom, rotate, true, false, true);
    }
}

void PDFCore::setSelectionColor (SplashColor color) {
    splashColorCopy (selectXorColor, color);
    splashColorXor (selectXorColor, paperColor);
}

void PDFCore::setSelection (
    int newSelectPage,
    int newSelectULX, int newSelectULY, int newSelectLRX, int newSelectLRY) {
    int x0, y0, x1, y1, py;
    bool haveSel, newHaveSel;
    bool needRedraw, needScroll;
    bool moveLeft, moveRight, moveTop, moveBottom;
    PDFCorePage* page;

    haveSel = selectULX != selectLRX && selectULY != selectLRY;
    newHaveSel = newSelectULX != newSelectLRX && newSelectULY != newSelectLRY;

    // erase old selection on off-screen bitmap
    needRedraw = false;
    if (haveSel) {
        xorRectangle (
            selectPage, selectULX, selectULY, selectLRX, selectLRY,
            new SplashSolidColor (selectXorColor));
        needRedraw = true;
    }

    // draw new selection on off-screen bitmap
    if (newHaveSel) {
        xorRectangle (
            newSelectPage, newSelectULX, newSelectULY, newSelectLRX,
            newSelectLRY, new SplashSolidColor (selectXorColor));
        needRedraw = true;
    }

    // check which edges moved
    if (!haveSel || newSelectPage != selectPage) {
        moveLeft = moveTop = moveRight = moveBottom = true;
    }
    else {
        moveLeft = newSelectULX != selectULX;
        moveTop = newSelectULY != selectULY;
        moveRight = newSelectLRX != selectLRX;
        moveBottom = newSelectLRY != selectLRY;
    }

    // redraw currently visible part of bitmap
    if (needRedraw) {
        if (!haveSel) {
            page = findPage (newSelectPage);
            x0 = newSelectULX;
            y0 = newSelectULY;
            x1 = newSelectLRX;
            y1 = newSelectLRY;
            redrawWindow (
                page->xDest + x0, page->yDest + y0, x1 - x0 + 1, y1 - y0 + 1,
                false);
        }
        else if (!newHaveSel) {
            if ((page = findPage (selectPage))) {
                x0 = selectULX;
                y0 = selectULY;
                x1 = selectLRX;
                y1 = selectLRY;
                redrawWindow (
                    page->xDest + x0, page->yDest + y0, x1 - x0 + 1,
                    y1 - y0 + 1, false);
            }
        }
        else {
            page = findPage (newSelectPage);
            if (moveLeft) {
                x0 = newSelectULX < selectULX ? newSelectULX : selectULX;
                y0 = newSelectULY < selectULY ? newSelectULY : selectULY;
                x1 = newSelectULX > selectULX ? newSelectULX : selectULX;
                y1 = newSelectLRY > selectLRY ? newSelectLRY : selectLRY;
                redrawWindow (
                    page->xDest + x0, page->yDest + y0, x1 - x0 + 1,
                    y1 - y0 + 1, false);
            }
            if (moveRight) {
                x0 = newSelectLRX < selectLRX ? newSelectLRX : selectLRX;
                y0 = newSelectULY < selectULY ? newSelectULY : selectULY;
                x1 = newSelectLRX > selectLRX ? newSelectLRX : selectLRX;
                y1 = newSelectLRY > selectLRY ? newSelectLRY : selectLRY;
                redrawWindow (
                    page->xDest + x0, page->yDest + y0, x1 - x0 + 1,
                    y1 - y0 + 1, false);
            }
            if (moveTop) {
                x0 = newSelectULX < selectULX ? newSelectULX : selectULX;
                y0 = newSelectULY < selectULY ? newSelectULY : selectULY;
                x1 = newSelectLRX > selectLRX ? newSelectLRX : selectLRX;
                y1 = newSelectULY > selectULY ? newSelectULY : selectULY;
                redrawWindow (
                    page->xDest + x0, page->yDest + y0, x1 - x0 + 1,
                    y1 - y0 + 1, false);
            }
            if (moveBottom) {
                x0 = newSelectULX < selectULX ? newSelectULX : selectULX;
                y0 = newSelectLRY < selectLRY ? newSelectLRY : selectLRY;
                x1 = newSelectLRX > selectLRX ? newSelectLRX : selectLRX;
                y1 = newSelectLRY > selectLRY ? newSelectLRY : selectLRY;
                redrawWindow (
                    page->xDest + x0, page->yDest + y0, x1 - x0 + 1,
                    y1 - y0 + 1, false);
            }
        }
    }

    // switch to new selection coords
    selectPage = newSelectPage;
    selectULX = newSelectULX;
    selectULY = newSelectULY;
    selectLRX = newSelectLRX;
    selectLRY = newSelectLRY;

    // scroll if necessary
    if (newHaveSel) {
        page = findPage (selectPage);
        needScroll = false;
        x0 = scrollX;
        y0 = scrollY;
        if (moveLeft && page->xDest + selectULX < 0) {
            x0 += page->xDest + selectULX;
            needScroll = true;
        }
        else if (moveRight && page->xDest + selectLRX >= drawAreaWidth) {
            x0 += page->xDest + selectLRX - drawAreaWidth;
            needScroll = true;
        }
        else if (moveLeft && page->xDest + selectULX >= drawAreaWidth) {
            x0 += page->xDest + selectULX - drawAreaWidth;
            needScroll = true;
        }
        else if (moveRight && page->xDest + selectLRX < 0) {
            x0 += page->xDest + selectLRX;
            needScroll = true;
        }
        py = continuousMode ? pageY[selectPage - 1] : 0;
        if (moveTop && py + selectULY < y0) {
            y0 = py + selectULY;
            needScroll = true;
        }
        else if (moveBottom && py + selectLRY >= y0 + drawAreaHeight) {
            y0 = py + selectLRY - drawAreaHeight;
            needScroll = true;
        }
        else if (moveTop && py + selectULY >= y0 + drawAreaHeight) {
            y0 = py + selectULY - drawAreaHeight;
            needScroll = true;
        }
        else if (moveBottom && py + selectLRY < y0) {
            y0 = py + selectLRY;
            needScroll = true;
        }
        if (needScroll) { scrollTo (x0, y0); }
    }
}

void PDFCore::moveSelection (int pg, int x, int y) {
    int newSelectULX, newSelectULY, newSelectLRX, newSelectLRY;

    // don't allow selections to span multiple pages
    if (pg != selectPage) { return; }

    // move appropriate edges of selection
    if (lastDragLeft) {
        if (x < selectLRX) {
            newSelectULX = x;
            newSelectLRX = selectLRX;
        }
        else {
            newSelectULX = selectLRX;
            newSelectLRX = x;
            lastDragLeft = false;
        }
    }
    else {
        if (x > selectULX) {
            newSelectULX = selectULX;
            newSelectLRX = x;
        }
        else {
            newSelectULX = x;
            newSelectLRX = selectULX;
            lastDragLeft = true;
        }
    }
    if (lastDragTop) {
        if (y < selectLRY) {
            newSelectULY = y;
            newSelectLRY = selectLRY;
        }
        else {
            newSelectULY = selectLRY;
            newSelectLRY = y;
            lastDragTop = false;
        }
    }
    else {
        if (y > selectULY) {
            newSelectULY = selectULY;
            newSelectLRY = y;
        }
        else {
            newSelectULY = y;
            newSelectLRY = selectULY;
            lastDragTop = true;
        }
    }

    // redraw the selection
    setSelection (
        selectPage, newSelectULX, newSelectULY, newSelectLRX, newSelectLRY);
}

void PDFCore::xorRectangle (
    int pg, int x0, int y0, int x1, int y1, SplashPattern* pattern,
    PDFCoreTile* oneTile) {

    // TODO: retarded interface
    PDFCorePage* page;
    PDFCoreTile* tile;

    SplashCoord xx0, yy0, xx1, yy1;
    int xi, yi, wi, hi, i;

    if ((page = findPage (pg))) {
        for (i = 0; i < page->tiles->getLength (); ++i) {
            tile = (PDFCoreTile*)page->tiles->get (i);
            if (!oneTile || tile == oneTile) {
                Splash splash (tile->bitmap, false);

                if (pattern) {
                    splash.setFillPattern (pattern->copy ());
                }

                xx0 = (SplashCoord) (x0 - tile->xMin);
                yy0 = (SplashCoord) (y0 - tile->yMin);
                xx1 = (SplashCoord) (x1 - tile->xMin);
                yy1 = (SplashCoord) (y1 - tile->yMin);

                SplashPath path;

                path.moveTo (xx0, yy0);
                path.lineTo (xx1, yy0);
                path.lineTo (xx1, yy1);
                path.lineTo (xx0, yy1);
                path.close ();

                if (pattern) {
                    splash.xorFill (&path, true);
                }
                else {
                    splash.stroke (&path);
                }

                xi = x0 - tile->xMin;
                wi = x1 - x0;
                if (xi < 0) {
                    wi += xi;
                    xi = 0;
                }
                if (xi + wi > tile->bitmap->getWidth ()) {
                    wi = tile->bitmap->getWidth () - xi;
                }
                yi = y0 - tile->yMin;
                hi = y1 - y0;
                if (yi < 0) {
                    hi += yi;
                    yi = 0;
                }
                if (yi + hi > tile->bitmap->getHeight ()) {
                    hi = tile->bitmap->getHeight () - yi;
                }
                updateTileData (tile, xi, yi, wi, hi, true);
            }
        }
    }

    if (pattern) {
        delete pattern;
    }
}

bool PDFCore::getSelection (
    int* pg, double* ulx, double* uly, double* lrx, double* lry) {
    if (selectULX == selectLRX || selectULY == selectLRY) { return false; }
    *pg = selectPage;
    cvtDevToUser (selectPage, selectULX, selectULY, ulx, uly);
    cvtDevToUser (selectPage, selectLRX, selectLRY, lrx, lry);
    return true;
}

GString* PDFCore::extractText (
    int pg, double xMin, double yMin, double xMax, double yMax) {
    PDFCorePage* page;
    TextOutputControl textOutCtrl;
    TextOutputDev* textOut;
    int x0, y0, x1, y1, t;
    GString* s;

    if (!doc->okToCopy ()) { return NULL; }
    if ((page = findPage (pg))) {
        cvtUserToDev (pg, xMin, yMin, &x0, &y0);
        cvtUserToDev (pg, xMax, yMax, &x1, &y1);
        if (x0 > x1) {
            t = x0;
            x0 = x1;
            x1 = t;
        }

        if (y0 > y1) {
            t = y0;
            y0 = y1;
            y1 = t;
        }
        s = page->text->getText (xpdf::bbox_t{
                double (x0), double (y0), double (x1), double (y1) });
    }
    else {
        textOutCtrl.mode = textOutPhysLayout;
        textOut = new TextOutputDev (NULL, &textOutCtrl, false);
        if (textOut->isOk ()) {
            doc->displayPage (
                textOut, pg, dpi, dpi, rotate, false, true, false);
            textOut->cvtUserToDev (xMin, yMin, &x0, &y0);
            textOut->cvtUserToDev (xMax, yMax, &x1, &y1);
            if (x0 > x1) {
                t = x0;
                x0 = x1;
                x1 = t;
            }
            if (y0 > y1) {
                t = y0;
                y0 = y1;
                y1 = t;
            }
            s = textOut->getText (xpdf::bbox_t{
                double (x0), double (y0), double (x1), double (y1) });
        }
        else {
            s = new GString ();
        }
        delete textOut;
    }
    return s;
}

bool PDFCore::find (
    char* s, bool caseSensitive, bool next, bool backward, bool wholeWord,
    bool onePageOnly) {
    Unicode* u;
    int len, i;
    bool ret;

    // convert to Unicode
    len = (int)strlen (s);
    u = (Unicode*)calloc (len, sizeof (Unicode));
    for (i = 0; i < len; ++i) { u[i] = (Unicode) (s[i] & 0xff); }

    ret = findU (u, len, caseSensitive, next, backward, wholeWord, onePageOnly);

    free (u);
    return ret;
}

bool PDFCore::findU (
    Unicode* u, int len, bool caseSensitive, bool next, bool backward,
    bool wholeWord, bool onePageOnly) {
    TextOutputControl textOutCtrl;
    PDFCorePage* page;
    int pg;
    bool startAtTop, startAtLast, stopAtLast;

    // check for zero-length string
    if (len == 0) { return false; }

    setBusyCursor (true);

    // search current page starting at previous result, current
    // selection, or top/bottom of page
    startAtTop = startAtLast = false;
    xpdf::bbox_t box{ };
    pg = topPage;
    if (next) { startAtLast = true; }
    else if (selectULX != selectLRX && selectULY != selectLRY) {
        pg = selectPage;
        if (backward) {
            box.arr [0] = selectULX - 1;
            box.arr [1] = selectULY - 1;
        }
        else {
            box.arr [0] = selectULX + 1;
            box.arr [1] = selectULY + 1;
        }
    }
    else {
        startAtTop = true;
    }
    if (!(page = findPage (pg))) {
        displayPage (pg, zoom, rotate, true, false);
        page = findPage (pg);
    }
    if (page->text->findText (
            u, len, startAtTop, true, startAtLast, false,
            caseSensitive, backward, wholeWord,
            box)) {
        goto found;
    }

    if (!onePageOnly) {
        // search following/previous pages
        textOutCtrl.mode = textOutPhysLayout;
        TextOutputDev textOut (NULL, &textOutCtrl, false);

        if (!textOut.isOk ()) {
            goto notFound;
        }

        for (pg = backward ? pg - 1 : pg + 1;
             backward ? pg >= 1 : pg <= doc->getNumPages ();
             pg += backward ? -1 : 1) {
            doc->displayPage (&textOut, pg, 72, 72, 0, false, true, false);

            if (textOut.findText (
                    u, len, true, true, false, false,
                    caseSensitive, backward, wholeWord,
                    box)) {
                goto foundPage;
            }
        }

        // search previous/following pages
        for (pg = backward ? doc->getNumPages () : 1;
             backward ? pg > topPage : pg < topPage; pg += backward ? -1 : 1) {
            doc->displayPage (&textOut, pg, 72, 72, 0, false, true, false);
            if (textOut.findText (
                    u, len, true, true, false, false,
                    caseSensitive, backward, wholeWord,
                    box)) {
                goto foundPage;
            }
        }
    }

    // search current page ending at previous result, current selection,
    // or bottom/top of page
    if (!startAtTop) {
        box = { };

        if (next) {
            stopAtLast = true;
        }
        else {
            stopAtLast = false;
            box.arr [2] = selectLRX;
            box.arr [3] = selectLRY;
        }

        if (page->text->findText (
                u, len, true, false, false, stopAtLast, caseSensitive,
                backward, wholeWord, box)) {
            goto found;
        }
    }

    // not found
notFound:
    setBusyCursor (false);
    return false;

    // found on a different page
foundPage:
    update (
        pg, scrollX, continuousMode ? -1 : 0, zoom, rotate, false, true,
        true);

    page = findPage (pg);

    if (!page->text->findText (
            u, len, true, true, false, false,
            caseSensitive, backward, wholeWord,
            box)) {
        // this can happen if coalescing is bad
        goto notFound;
    }

    // found: change the selection
found:
    setSelection (pg, xpdf::to< int > (xpdf::ceil (box)));
    setBusyCursor (false);

    return true;
}

bool PDFCore::cvtWindowToUser (
    int xw, int yw, int* pg, double* xu, double* yu) {
    PDFCorePage* page;
    PDFCoreTile* tile;
    int i;

    for (i = 0; i < pages->getLength (); ++i) {
        page = (PDFCorePage*)pages->get (i);
        if (xw >= page->xDest && xw < page->xDest + page->w &&
            yw >= page->yDest && yw < page->yDest + page->h) {
            if (page->tiles->getLength () == 0) { break; }
            tile = (PDFCoreTile*)page->tiles->get (0);
            *pg = page->page;
            xw -= tile->xDest;
            yw -= tile->yDest;
            *xu = tile->ictm[0] * xw + tile->ictm[2] * yw + tile->ictm[4];
            *yu = tile->ictm[1] * xw + tile->ictm[3] * yw + tile->ictm[5];
            return true;
        }
    }
    *pg = 0;
    *xu = *yu = 0;
    return false;
}

bool PDFCore::cvtWindowToDev (int xw, int yw, int* pg, int* xd, int* yd) {
    PDFCorePage* page;
    int i;

    for (i = 0; i < pages->getLength (); ++i) {
        page = (PDFCorePage*)pages->get (i);
        if (xw >= page->xDest && xw < page->xDest + page->w &&
            yw >= page->yDest && yw < page->yDest + page->h) {
            *pg = page->page;
            *xd = xw - page->xDest;
            *yd = yw - page->yDest;
            return true;
        }
    }
    *pg = 0;
    *xd = *yd = 0;
    return false;
}

void PDFCore::cvtUserToWindow (int pg, double xu, double yu, int* xw, int* yw) {
    PDFCorePage* page;
    PDFCoreTile* tile;

    if ((page = findPage (pg)) && page->tiles->getLength () > 0) {
        tile = (PDFCoreTile*)page->tiles->get (0);
    }
    else if (curTile && curPage->page == pg) {
        tile = curTile;
    }
    else {
        tile = NULL;
    }
    if (tile) {
        *xw = tile->xDest +
              (int)(tile->ctm[0] * xu + tile->ctm[2] * yu + tile->ctm[4] + 0.5);
        *yw = tile->yDest +
              (int)(tile->ctm[1] * xu + tile->ctm[3] * yu + tile->ctm[5] + 0.5);
    }
    else {
        // this should never happen
        *xw = *yw = 0;
    }
}

void PDFCore::cvtUserToDev (int pg, double xu, double yu, int* xd, int* yd) {
    PDFCorePage* page;
    PDFCoreTile* tile;
    double ctm[6];

    if ((page = findPage (pg)) && page->tiles->getLength () > 0) {
        tile = (PDFCoreTile*)page->tiles->get (0);
    }
    else if (curTile && curPage->page == pg) {
        tile = curTile;
    }
    else {
        tile = NULL;
    }
    if (tile) {
        *xd =
            (int)(tile->xMin + tile->ctm[0] * xu + tile->ctm[2] * yu + tile->ctm[4] + 0.5);
        *yd =
            (int)(tile->yMin + tile->ctm[1] * xu + tile->ctm[3] * yu + tile->ctm[5] + 0.5);
    }
    else {
        doc->getCatalog ()->getPage (pg)->getDefaultCTM (
            ctm, dpi, dpi, rotate, false, out->upsideDown ());
        *xd = (int)(ctm[0] * xu + ctm[2] * yu + ctm[4] + 0.5);
        *yd = (int)(ctm[1] * xu + ctm[3] * yu + ctm[5] + 0.5);
    }
}

void PDFCore::cvtDevToWindow (int pg, int xd, int yd, int* xw, int* yw) {
    PDFCorePage* page;

    if ((page = findPage (pg))) {
        *xw = page->xDest + xd;
        *yw = page->yDest + yd;
    }
    else {
        // this should never happen
        *xw = *yw = 0;
    }
}

void PDFCore::cvtDevToUser (int pg, int xd, int yd, double* xu, double* yu) {
    PDFCorePage* page;
    PDFCoreTile* tile;

    if ((page = findPage (pg)) && page->tiles->getLength () > 0) {
        tile = (PDFCoreTile*)page->tiles->get (0);
    }
    else if (curTile && curPage->page == pg) {
        tile = curTile;
    }
    else {
        tile = NULL;
    }
    if (tile) {
        xd -= tile->xMin;
        yd -= tile->yMin;
        *xu = tile->ictm[0] * xd + tile->ictm[2] * yd + tile->ictm[4];
        *yu = tile->ictm[1] * xd + tile->ictm[3] * yd + tile->ictm[5];
    }
    else {
        // this should never happen
        *xu = *yu = 0;
    }
}

void PDFCore::setReverseVideo (bool reverseVideoA) {
    out->setReverseVideo (reverseVideoA);
    update (topPage, scrollX, scrollY, zoom, rotate, true, false, false);
}

LinkAction* PDFCore::findLink (int pg, double x, double y) {
    PDFCorePage* page;

    if ((page = findPage (pg))) {
        return page->links ? page->links->find (x, y) : (LinkAction*)NULL;
    }
    return NULL;
}

PDFCorePage* PDFCore::findPage (int pg) {
    PDFCorePage* page;
    int i;

    for (i = 0; i < pages->getLength (); ++i) {
        page = (PDFCorePage*)pages->get (i);
        if (page->page == pg) { return page; }
    }
    return NULL;
}

void PDFCore::redrawCbk (
    void* data, int x0, int y0, int x1, int y1, bool composited) {
    PDFCore* core = (PDFCore*)data;

    core->curTile->bitmap = core->out->getBitmap ();

    // the default CTM is set by the Gfx constructor; tile->ctm is
    // needed by the coordinate conversion functions (which may be
    // called during redraw)
    memcpy (core->curTile->ctm, core->out->getDefCTM (), 6 * sizeof (double));
    memcpy (core->curTile->ictm, core->out->getDefICTM (), 6 * sizeof (double));

    // the bitmap created by Gfx and SplashOutputDev can be a slightly
    // different size due to rounding errors
    if (x1 >= core->curTile->xMax - core->curTile->xMin) {
        x1 = core->curTile->xMax - core->curTile->xMin - 1;
    }
    if (y1 >= core->curTile->yMax - core->curTile->yMin) {
        y1 = core->curTile->yMax - core->curTile->yMin - 1;
    }

    core->clippedRedrawRect (
        core->curTile, x0, y0, core->curTile->xDest + x0,
        core->curTile->yDest + y0, x1 - x0 + 1, y1 - y0 + 1, 0, 0,
        core->drawAreaWidth, core->drawAreaHeight, true, composited);
}

void PDFCore::redrawWindow (
    int x, int y, int width, int height, bool needUpdate) {
    PDFCorePage* page;
    PDFCoreTile* tile;
    int xDest, yDest, w, i, j;

    if (pages->getLength () == 0) {
        redrawRect (NULL, 0, 0, x, y, width, height, true);
        return;
    }

    for (i = 0; i < pages->getLength (); ++i) {
        page = (PDFCorePage*)pages->get (i);
        for (j = 0; j < page->tiles->getLength (); ++j) {
            tile = (PDFCoreTile*)page->tiles->get (j);
            if (tile->edges & pdfCoreTileTopEdge) {
                if (tile->edges & pdfCoreTileLeftEdge) { xDest = 0; }
                else {
                    xDest = tile->xDest;
                }
                if (tile->edges & pdfCoreTileRightEdge) {
                    w = drawAreaWidth - xDest;
                }
                else {
                    w = tile->xDest + (tile->xMax - tile->xMin) - xDest;
                }
                clippedRedrawRect (
                    NULL, 0, 0, xDest, 0, w, tile->yDest, x, y, width, height,
                    false);
            }
            if (tile->edges & pdfCoreTileBottomEdge) {
                if (tile->edges & pdfCoreTileLeftEdge) { xDest = 0; }
                else {
                    xDest = tile->xDest;
                }
                if (tile->edges & pdfCoreTileRightEdge) {
                    w = drawAreaWidth - xDest;
                }
                else {
                    w = tile->xDest + (tile->xMax - tile->xMin) - xDest;
                }
                yDest = tile->yDest + (tile->yMax - tile->yMin);
                clippedRedrawRect (
                    NULL, 0, 0, xDest, yDest, w, drawAreaHeight - yDest, x, y,
                    width, height, false);
            }
            else if (
                (tile->edges & pdfCoreTileBottomSpace) &&
                i + 1 < pages->getLength ()) {
                if (tile->edges & pdfCoreTileLeftEdge) { xDest = 0; }
                else {
                    xDest = tile->xDest;
                }
                if (tile->edges & pdfCoreTileRightEdge) {
                    w = drawAreaWidth - xDest;
                }
                else {
                    w = tile->xDest + (tile->xMax - tile->xMin) - xDest;
                }
                yDest = tile->yDest + (tile->yMax - tile->yMin);
                clippedRedrawRect (
                    NULL, 0, 0, xDest, yDest, w,
                    ((PDFCorePage*)pages->get (i + 1))->yDest - yDest, x, y,
                    width, height, false);
            }
            if (tile->edges & pdfCoreTileLeftEdge) {
                clippedRedrawRect (
                    NULL, 0, 0, 0, tile->yDest, tile->xDest,
                    tile->yMax - tile->yMin, x, y, width, height, false);
            }
            if (tile->edges & pdfCoreTileRightEdge) {
                xDest = tile->xDest + (tile->xMax - tile->xMin);
                clippedRedrawRect (
                    NULL, 0, 0, xDest, tile->yDest, drawAreaWidth - xDest,
                    tile->yMax - tile->yMin, x, y, width, height, false);
            }
            clippedRedrawRect (
                tile, 0, 0, tile->xDest, tile->yDest, tile->bitmap->getWidth (),
                tile->bitmap->getHeight (), x, y, width, height, needUpdate);
        }
    }
}

PDFCoreTile* PDFCore::newTile (int xDestA, int yDestA) {
    return new PDFCoreTile (xDestA, yDestA);
}

void PDFCore::updateTileData (
    PDFCoreTile* tileA, int xSrc, int ySrc, int width, int height,
    bool composited) {}

void PDFCore::clippedRedrawRect (
    PDFCoreTile* tile, int xSrc, int ySrc, int xDest, int yDest, int width,
    int height, int xClip, int yClip, int wClip, int hClip, bool needUpdate,
    bool composited) {
    if (tile && needUpdate) {
        updateTileData (tile, xSrc, ySrc, width, height, composited);
    }
    if (xDest < xClip) {
        xSrc += xClip - xDest;
        width -= xClip - xDest;
        xDest = xClip;
    }
    if (xDest + width > xClip + wClip) { width = xClip + wClip - xDest; }
    if (yDest < yClip) {
        ySrc += yClip - yDest;
        height -= yClip - yDest;
        yDest = yClip;
    }
    if (yDest + height > yClip + hClip) { height = yClip + hClip - yDest; }
    if (width > 0 && height > 0) {
        redrawRect (tile, xSrc, ySrc, xDest, yDest, width, height, composited);
    }
}

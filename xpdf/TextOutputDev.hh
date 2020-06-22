// -*- mode: c++; -*-
// Copyright 1997-2014 Glyph & Cog, LLC

#ifndef XPDF_XPDF_TEXTOUTPUTDEV_HH
#define XPDF_XPDF_TEXTOUTPUTDEV_HH

#include <defs.hh>

#include <xpdf/bbox.hh>
#include <xpdf/OutputDev.hh>
#include <xpdf/TextOutput.hh>
#include <xpdf/TextOutputFunc.hh>
#include <xpdf/TextOutputControl.hh>

struct TextOutputDev : public OutputDev
{
    //
    // If `physLayoutA' is true, the original physical layout of the text is
    // maintained.  If `rawOrder' is true, the text is kept in content stream
    // order.
    //
    TextOutputDev(TextOutputControl);

    //---- get info about output device

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    virtual bool upsideDown() { return true; }

    // Does this device use drawChar() or drawString()?
    virtual bool useDrawChar() { return true; }

    // Does this device use beginType3Char/endType3Char?  Otherwise,
    // text in Type 3 fonts will be drawn with drawChar/drawString.
    virtual bool interpretType3Chars() { return false; }

    // Does this device need non-text content?
    virtual bool needNonText() { return false; }

    // Does this device require incCharCount to be called for text on
    // non-shown layers?
    virtual bool needCharCount() { return true; }

    //----- initialization and control

    virtual void startPage(int pageNum, GfxState *state);
    virtual void endPage() { }

    //----- save/restore graphics state
    virtual void restoreState(GfxState *state);

    //----- update text state
    virtual void updateFont(GfxState *state);

    //----- text drawing
    virtual void beginString(GfxState *, GString *) { }
    virtual void endString(GfxState *) { }

    virtual void drawChar(GfxState *state, double x, double y, double dx,
                          double dy, double originX, double originY, CharCode c,
                          int nBytes, Unicode *u, int uLen);

    virtual void incCharCount(int nChars);

    virtual void beginActualText(GfxState *state, Unicode *u, int uLen);
    virtual void endActualText(GfxState *state);

    //----- special access

    //
    // See TextPage::findText
    //
    bool findText(Unicode *s, int len, bool startAtTop, bool stopAtBottom,
                  bool startAtLast, bool stopAtLast, bool caseSensitive,
                  bool backward, bool wholeWord, xpdf::bbox_t &);

    //
    // See TextPage::getText
    //
    GString *getText(const xpdf::bbox_t &);

    // Returns the TextPage object for the last rasterized page,
    // transferring ownership to the caller.
    TextPagePtr takeText();

private:
    TextPagePtr       text; // text for the current page
    TextOutputControl control; // formatting parameters
};

#endif // XPDF_XPDF_TEXTOUTPUTDEV_HH

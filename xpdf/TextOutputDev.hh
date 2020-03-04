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

struct TextOutputDev : public OutputDev {
    // Open a text output file.  If <fileName> is NULL, no file is
    // written (this is useful, e.g., for searching text).  If
    // <physLayoutA> is true, the original physical layout of the text
    // is maintained.  If <rawOrder> is true, the text is kept in
    // content stream order.
    TextOutputDev (const char*, TextOutputControl*, bool);

    // Create a TextOutputDev which will write to a generic stream.  If
    // <physLayoutA> is true, the original physical layout of the text
    // is maintained.  If <rawOrder> is true, the text is kept in
    // content stream order.
    TextOutputDev (TextOutputFunc, void*, TextOutputControl*);

    // Destructor.
    virtual ~TextOutputDev ();

    // Check if file was successfully created.
    virtual bool isOk () { return ok; }

    //---- get info about output device

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    virtual bool upsideDown () { return true; }

    // Does this device use drawChar() or drawString()?
    virtual bool useDrawChar () { return true; }

    // Does this device use beginType3Char/endType3Char?  Otherwise,
    // text in Type 3 fonts will be drawn with drawChar/drawString.
    virtual bool interpretType3Chars () { return false; }

    // Does this device need non-text content?
    virtual bool needNonText () { return false; }

    // Does this device require incCharCount to be called for text on
    // non-shown layers?
    virtual bool needCharCount () { return true; }

    //----- initialization and control

    // Start a page.
    virtual void startPage (int pageNum, GfxState* state);

    // End a page.
    virtual void endPage ();

    //----- save/restore graphics state
    virtual void restoreState (GfxState* state);

    //----- update text state
    virtual void updateFont (GfxState* state);

    //----- text drawing
    virtual void beginString (GfxState* state, GString* s);
    virtual void endString (GfxState* state);
    virtual void drawChar (
        GfxState* state, double x, double y, double dx, double dy,
        double originX, double originY, CharCode c, int nBytes, Unicode* u,
        int uLen);
    virtual void incCharCount (int nChars);
    virtual void beginActualText (GfxState* state, Unicode* u, int uLen);
    virtual void endActualText (GfxState* state);

    //----- special access

    // Find a string.  If <startAtTop> is true, starts looking at the
    // top of the page; else if <startAtLast> is true, starts looking
    // immediately after the last find result; else starts looking at
    // <xMin>,<yMin>.  If <stopAtBottom> is true, stops looking at the
    // bottom of the page; else if <stopAtLast> is true, stops looking
    // just before the last find result; else stops looking at
    // <xMax>,<yMax>.
    bool findText (
        Unicode* s, int len,
        bool startAtTop, bool stopAtBottom, bool startAtLast, bool stopAtLast,
        bool caseSensitive, bool backward, bool wholeWord,
        xpdf::bbox_t&);

    // Get the text which is inside the specified rectangle.
    GString*
    getText (const xpdf::bbox_t&);

    // Build a flat word list, in content stream order (if
    // this->rawOrder is true), physical layout order (if
    // this->physLayout is true and this->rawOrder is false), or reading
    // order (if both flags are false).
    TextWords makeWordList ();

    // Returns the TextPage object for the last rasterized page,
    // transferring ownership to the caller.
    TextPagePtr takeText ();

private:
    TextOutputFunc outputFunc; // output function
    void* outputStream;        // output stream
    bool needClose;           // need to close the output file?
                               //   (only if outputStream is a FILE*)
    TextPagePtr text;          // text for the current page
    TextOutputControl control; // formatting parameters
    bool ok;                  // set up ok?
};

#endif // XPDF_XPDF_TEXTOUTPUTDEV_HH

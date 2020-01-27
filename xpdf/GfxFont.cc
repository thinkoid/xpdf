// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>

#include <goo/memory.hh>

#include <fofi/FoFiIdentifier.hh>
#include <fofi/FoFiTrueType.hh>
#include <fofi/FoFiType1.hh>
#include <fofi/FoFiType1C.hh>

#include <xpdf/array.hh>
#include <xpdf/BuiltinFontTables.hh>
#include <xpdf/CMap.hh>
#include <xpdf/CharCodeToUnicode.hh>
#include <xpdf/dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/FontEncodingTables.hh>
#include <xpdf/GfxFont.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>

//------------------------------------------------------------------------

struct Base14FontMapEntry {
    const char* altName;
    const char* base14Name;
};

static Base14FontMapEntry base14FontMap[] = {
    { "Arial", "Helvetica" },
    { "Arial,Bold", "Helvetica-Bold" },
    { "Arial,BoldItalic", "Helvetica-BoldOblique" },
    { "Arial,Italic", "Helvetica-Oblique" },
    { "Arial-Bold", "Helvetica-Bold" },
    { "Arial-BoldItalic", "Helvetica-BoldOblique" },
    { "Arial-BoldItalicMT", "Helvetica-BoldOblique" },
    { "Arial-BoldMT", "Helvetica-Bold" },
    { "Arial-Italic", "Helvetica-Oblique" },
    { "Arial-ItalicMT", "Helvetica-Oblique" },
    { "ArialMT", "Helvetica" },
    { "Courier", "Courier" },
    { "Courier,Bold", "Courier-Bold" },
    { "Courier,BoldItalic", "Courier-BoldOblique" },
    { "Courier,Italic", "Courier-Oblique" },
    { "Courier-Bold", "Courier-Bold" },
    { "Courier-BoldOblique", "Courier-BoldOblique" },
    { "Courier-Oblique", "Courier-Oblique" },
    { "CourierNew", "Courier" },
    { "CourierNew,Bold", "Courier-Bold" },
    { "CourierNew,BoldItalic", "Courier-BoldOblique" },
    { "CourierNew,Italic", "Courier-Oblique" },
    { "CourierNew-Bold", "Courier-Bold" },
    { "CourierNew-BoldItalic", "Courier-BoldOblique" },
    { "CourierNew-Italic", "Courier-Oblique" },
    { "CourierNewPS-BoldItalicMT", "Courier-BoldOblique" },
    { "CourierNewPS-BoldMT", "Courier-Bold" },
    { "CourierNewPS-ItalicMT", "Courier-Oblique" },
    { "CourierNewPSMT", "Courier" },
    { "Helvetica", "Helvetica" },
    { "Helvetica,Bold", "Helvetica-Bold" },
    { "Helvetica,BoldItalic", "Helvetica-BoldOblique" },
    { "Helvetica,Italic", "Helvetica-Oblique" },
    { "Helvetica-Bold", "Helvetica-Bold" },
    { "Helvetica-BoldItalic", "Helvetica-BoldOblique" },
    { "Helvetica-BoldOblique", "Helvetica-BoldOblique" },
    { "Helvetica-Italic", "Helvetica-Oblique" },
    { "Helvetica-Oblique", "Helvetica-Oblique" },
    { "Symbol", "Symbol" },
    { "Symbol,Bold", "Symbol" },
    { "Symbol,BoldItalic", "Symbol" },
    { "Symbol,Italic", "Symbol" },
    { "Times-Bold", "Times-Bold" },
    { "Times-BoldItalic", "Times-BoldItalic" },
    { "Times-Italic", "Times-Italic" },
    { "Times-Roman", "Times-Roman" },
    { "TimesNewRoman", "Times-Roman" },
    { "TimesNewRoman,Bold", "Times-Bold" },
    { "TimesNewRoman,BoldItalic", "Times-BoldItalic" },
    { "TimesNewRoman,Italic", "Times-Italic" },
    { "TimesNewRoman-Bold", "Times-Bold" },
    { "TimesNewRoman-BoldItalic", "Times-BoldItalic" },
    { "TimesNewRoman-Italic", "Times-Italic" },
    { "TimesNewRomanPS", "Times-Roman" },
    { "TimesNewRomanPS-Bold", "Times-Bold" },
    { "TimesNewRomanPS-BoldItalic", "Times-BoldItalic" },
    { "TimesNewRomanPS-BoldItalicMT", "Times-BoldItalic" },
    { "TimesNewRomanPS-BoldMT", "Times-Bold" },
    { "TimesNewRomanPS-Italic", "Times-Italic" },
    { "TimesNewRomanPS-ItalicMT", "Times-Italic" },
    { "TimesNewRomanPSMT", "Times-Roman" },
    { "TimesNewRomanPSMT,Bold", "Times-Bold" },
    { "TimesNewRomanPSMT,BoldItalic", "Times-BoldItalic" },
    { "TimesNewRomanPSMT,Italic", "Times-Italic" },
    { "ZapfDingbats", "ZapfDingbats" }
};

//------------------------------------------------------------------------

// index: {fixed:0, sans-serif:4, serif:8} + bold*2 + italic
// NB: must be in same order as psSubstFonts in PSOutputDev.cc
static const char* base14SubstFonts[14] = {
    "Courier", "Courier-Oblique", "Courier-Bold", "Courier-BoldOblique",
    "Helvetica", "Helvetica-Oblique", "Helvetica-Bold", "Helvetica-BoldOblique",
    "Times-Roman", "Times-Italic", "Times-Bold", "Times-BoldItalic",
    // the last two are never used for substitution
    "Symbol", "ZapfDingbats"
};

//------------------------------------------------------------------------

static int readFromStream (void* data) { return ((Stream*)data)->getChar (); }

//------------------------------------------------------------------------
// GfxFontLoc
//------------------------------------------------------------------------

GfxFontLoc::GfxFontLoc () {
    path = NULL;
    fontNum = 0;
    oblique = 0;
    encoding = NULL;
    substIdx = -1;
}

GfxFontLoc::~GfxFontLoc () {
    if (path) { delete path; }
    if (encoding) { delete encoding; }
}

//------------------------------------------------------------------------
// GfxFont
//------------------------------------------------------------------------

GfxFont* GfxFont::makeFont (XRef* xref, const char* tagA, Ref idA, Dict* fontDict) {
    GString* nameA;
    Ref embFontIDA;
    GfxFontType typeA;
    GfxFont* font;
    Object obj1;

    // get base font name
    nameA = NULL;
    obj1 = resolve ((*fontDict) ["BaseFont"]);
    if (obj1.is_name ()) { nameA = new GString (obj1.as_name ()); }
    else if (obj1.is_string ()) {
        nameA = obj1.as_string ()->copy ();
    }

    // get embedded font ID and font type
    typeA = getFontType (xref, fontDict, &embFontIDA);

    // create the font object
    font = NULL;
    if (typeA < fontCIDType0) {
        font = new Gfx8BitFont (
            xref, tagA, idA, nameA, typeA, embFontIDA, fontDict);
    }
    else {
        font = new GfxCIDFont (
            xref, tagA, idA, nameA, typeA, embFontIDA, fontDict);
    }

    return font;
}

GfxFont::GfxFont (
    const char* tagA, Ref idA, GString* nameA, GfxFontType typeA, Ref embFontIDA) {
    ok = false;
    tag = new GString (tagA);
    id = idA;
    name = nameA;
    type = typeA;
    embFontID = embFontIDA;
    embFontName = NULL;
}

GfxFont::~GfxFont () {
    delete tag;
    if (name) { delete name; }
    if (embFontName) { delete embFontName; }
}

// This function extracts three pieces of information:
// 1. the "expected" font type, i.e., the font type implied by
//    Font.Subtype, DescendantFont.Subtype, and
//    FontDescriptor.FontFile3.Subtype
// 2. the embedded font object ID
// 3. the actual font type - determined by examining the embedded font
//    if there is one, otherwise equal to the expected font type
// If the expected and actual font types don't match, a warning
// message is printed.  The expected font type is not used for
// anything else.
GfxFontType GfxFont::getFontType (XRef* xref, Dict* fontDict, Ref* embID) {
    GfxFontType t, expectedType;
    FoFiIdentifierType fft;
    Dict* fontDict2;
    Object subtype, fontDesc, obj1, obj2, obj3, obj4;
    bool isType0, err;

    t = fontUnknownType;
    embID->num = embID->gen = -1;
    err = false;

    subtype = resolve ((*fontDict) ["Subtype"]);
    expectedType = fontUnknownType;
    isType0 = false;
    if (subtype.is_name ("Type1") || subtype.is_name ("MMType1")) {
        expectedType = fontType1;
    }
    else if (subtype.is_name ("Type1C")) {
        expectedType = fontType1C;
    }
    else if (subtype.is_name ("Type3")) {
        expectedType = fontType3;
    }
    else if (subtype.is_name ("TrueType")) {
        expectedType = fontTrueType;
    }
    else if (subtype.is_name ("Type0")) {
        isType0 = true;
    }
    else {
        error (
            errSyntaxWarning, -1, "Unknown font type: '{0:s}'",
            subtype.is_name () ? subtype.as_name () : "???");
    }

    fontDict2 = fontDict;
    if ((obj1 = resolve ((*fontDict) ["DescendantFonts"])).is_array ()) {
        if (obj1.as_array ().size () == 0) {
            error (errSyntaxWarning, -1, "Empty DescendantFonts array in font");
            obj2 = { };
        }
        else if ((obj2 = resolve (obj1 [0UL])).is_dict ()) {
            if (!isType0) {
                error (
                    errSyntaxWarning, -1,
                    "Non-CID font with DescendantFonts array");
            }
            fontDict2 = obj2.as_dict_ptr ();
            subtype = resolve ((*fontDict2) ["Subtype"]);
            if (subtype.is_name ("CIDFontType0")) {
                if (isType0) { expectedType = fontCIDType0; }
            }
            else if (subtype.is_name ("CIDFontType2")) {
                if (isType0) { expectedType = fontCIDType2; }
            }
        }
    }
    else {
        obj2 = { };
    }

    if ((fontDesc = resolve ((*fontDict2) ["FontDescriptor"])).is_dict ()) {
        if ((obj3 = fontDesc.as_dict ()["FontFile"]).is_ref ()) {
            *embID = obj3.as_ref ();
            if (expectedType != fontType1) { err = true; }
        }
        if (embID->num == -1 &&
            (obj3 = fontDesc.as_dict ()["FontFile2"]).is_ref ()) {
            *embID = obj3.as_ref ();
            if (isType0) { expectedType = fontCIDType2; }
            else if (expectedType != fontTrueType) {
                err = true;
            }
        }
        if (embID->num == -1 &&
            (obj3 = fontDesc.as_dict ()["FontFile3"]).is_ref ()) {
            *embID = obj3.as_ref ();
            if ((obj4 = resolve (obj3)).is_stream ()) {
                subtype = resolve ((*obj4.streamGetDict ()) ["Subtype"]);
                if (subtype.is_name ("Type1")) {
                    if (expectedType != fontType1) {
                        err = true;
                        expectedType = isType0 ? fontCIDType0 : fontType1;
                    }
                }
                else if (subtype.is_name ("Type1C")) {
                    if (expectedType == fontType1) {
                        expectedType = fontType1C;
                    }
                    else if (expectedType != fontType1C) {
                        err = true;
                        expectedType = isType0 ? fontCIDType0C : fontType1C;
                    }
                }
                else if (subtype.is_name ("TrueType")) {
                    if (expectedType != fontTrueType) {
                        err = true;
                        expectedType = isType0 ? fontCIDType2 : fontTrueType;
                    }
                }
                else if (subtype.is_name ("CIDFontType0C")) {
                    if (expectedType == fontCIDType0) {
                        expectedType = fontCIDType0C;
                    }
                    else {
                        err = true;
                        expectedType = isType0 ? fontCIDType0C : fontType1C;
                    }
                }
                else if (subtype.is_name ("OpenType")) {
                    if (expectedType == fontTrueType) {
                        expectedType = fontTrueTypeOT;
                    }
                    else if (expectedType == fontType1) {
                        expectedType = fontType1COT;
                    }
                    else if (expectedType == fontCIDType0) {
                        expectedType = fontCIDType0COT;
                    }
                    else if (expectedType == fontCIDType2) {
                        expectedType = fontCIDType2OT;
                    }
                    else {
                        err = true;
                    }
                }
                else {
                    error (
                        errSyntaxError, -1, "Unknown font type '{0:s}'",
                        subtype.is_name () ? subtype.as_name () : "???");
                }
            }
        }
    }

    t = fontUnknownType;
    if (embID->num >= 0) {
        obj3 = xpdf::make_ref_obj (embID->num, embID->gen, xref);
        obj4 = resolve (obj3);
        if (obj4.is_stream ()) {
            obj4.streamReset ();
            fft = FoFiIdentifier::identifyStream (
                &readFromStream, obj4.as_stream ());
            obj4.streamClose ();
            switch (fft) {
            case fofiIdType1PFA:
            case fofiIdType1PFB: t = fontType1; break;
            case fofiIdCFF8Bit: t = isType0 ? fontCIDType0C : fontType1C; break;
            case fofiIdCFFCID: t = fontCIDType0C; break;
            case fofiIdTrueType:
            case fofiIdTrueTypeCollection:
                t = isType0 ? fontCIDType2 : fontTrueType;
                break;
            case fofiIdOpenTypeCFF8Bit:
                t = isType0 ? fontCIDType0COT : fontType1COT;
                break;
            case fofiIdOpenTypeCFFCID: t = fontCIDType0COT; break;
            default:
                error (errSyntaxError, -1, "Embedded font file may be invalid");
                break;
            }
        }
    }

    if (t == fontUnknownType) { t = expectedType; }

    if (t != expectedType) { err = true; }

    if (err) {
        error (
            errSyntaxWarning, -1,
            "Mismatch between font type and embedded font file");
    }


    return t;
}

void GfxFont::readFontDescriptor (XRef* xref, Dict* fontDict) {
    Object obj1, obj2, obj3, obj4;
    double t;
    int i;

    // assume Times-Roman by default (for substitution purposes)
    flags = fontSerif;

    missingWidth = 0;

    if ((obj1 = resolve ((*fontDict) ["FontDescriptor"])).is_dict ()) {
        // get flags
        if ((obj2 = resolve (obj1.as_dict ()["Flags"])).is_int ()) {
            flags = obj2.as_int ();
        }

        // get name
        *&obj2 = resolve (obj1.as_dict ()["FontName"]);
        if (obj2.is_name ()) { embFontName = new GString (obj2.as_name ()); }

        // look for MissingWidth
        *&obj2 = resolve (obj1.as_dict ()["MissingWidth"]);
        if (obj2.is_num ()) { missingWidth = obj2.as_num (); }

        // get Ascent and Descent
        *&obj2 = resolve (obj1.as_dict ()["Ascent"]);
        if (obj2.is_num ()) {
            t = 0.001 * obj2.as_num ();
            // some broken font descriptors specify a negative ascent
            if (t < 0) { t = -t; }
            // some broken font descriptors set ascent and descent to 0;
            // others set it to ridiculous values (e.g., 32768)
            if (t != 0 && t < 1.9) { ascent = t; }
        }
        *&obj2 = resolve (obj1.as_dict ()["Descent"]);
        if (obj2.is_num ()) {
            t = 0.001 * obj2.as_num ();
            // some broken font descriptors specify a positive descent
            if (t > 0) { t = -t; }
            // some broken font descriptors set ascent and descent to 0
            if (t != 0 && t > -1.9) { descent = t; }
        }

        // font FontBBox
        if ((obj2 = resolve (obj1.as_dict ()["FontBBox"])).is_array ()) {
            for (i = 0; i < 4 && i < obj2.as_array ().size (); ++i) {
                if ((obj3 = resolve (obj2 [i])).is_num ()) {
                    fontBBox[i] = 0.001 * obj3.as_num ();
                }
            }
        }
    }
}

CharCodeToUnicode*
GfxFont::readToUnicodeCMap (Dict* fontDict, int nBits, CharCodeToUnicode* ctu) {
    GString* buf;
    Object obj1;
    char buf2[4096];
    int n;

    if (!(obj1 = resolve ((*fontDict) ["ToUnicode"])).is_stream ()) {
        return NULL;
    }
    buf = new GString ();
    obj1.streamReset ();
    while ((n = obj1.streamGetBlock (buf2, sizeof (buf2))) > 0) {
        buf->append (buf2, n);
    }
    obj1.streamClose ();
    if (ctu) { ctu->mergeCMap (buf, nBits); }
    else {
        ctu = CharCodeToUnicode::parseCMap (buf, nBits);
    }
    delete buf;
    return ctu;
}

GfxFontLoc* GfxFont::locateFont (XRef* xref, bool ps) {
    GfxFontLoc* fontLoc;
    SysFontType sysFontType;
    GString *path, *base14Name, *substName;
    PSFontParam16* psFont16;
    Object refObj, embFontObj;
    int substIdx, fontNum;
    double oblique;
    bool embed;

    if (type == fontType3) { return NULL; }

    //----- embedded font
    if (embFontID.num >= 0) {
        embed = true;
        refObj = xpdf::make_ref_obj (embFontID.num, embFontID.gen, xref);
        embFontObj = resolve (refObj);
        if (!embFontObj.is_stream ()) {
            error (errSyntaxError, -1, "Embedded font object is wrong type");
            embed = false;
        }
        if (embed) {
            if (ps) {
                switch (type) {
                case fontType1:
                case fontType1C:
                case fontType1COT:
                    embed = globalParams->getPSEmbedType1 ();
                    break;
                case fontTrueType:
                case fontTrueTypeOT:
                    embed = globalParams->getPSEmbedTrueType ();
                    break;
                case fontCIDType0C:
                case fontCIDType0COT:
                    embed = globalParams->getPSEmbedCIDPostScript ();
                    break;
                case fontCIDType2:
                case fontCIDType2OT:
                    embed = globalParams->getPSEmbedCIDTrueType ();
                    break;
                default: break;
                }
            }
            if (embed) {
                fontLoc = new GfxFontLoc ();
                fontLoc->locType = gfxFontLocEmbedded;
                fontLoc->fontType = type;
                fontLoc->embFontID = embFontID;
                return fontLoc;
            }
        }
    }

    //----- PS passthrough
    if (ps && name && !isCIDFont () && globalParams->getPSFontPassthrough ()) {
        fontLoc = new GfxFontLoc ();
        fontLoc->locType = gfxFontLocResident;
        fontLoc->fontType = fontType1;
        fontLoc->path = name->copy ();
        return fontLoc;
    }

    //----- external font file (fontFile, fontDir)
    if (name && (path = globalParams->findFontFile (name))) {
        if ((fontLoc = getExternalFont (path, 0, 0, isCIDFont ()))) {
            return fontLoc;
        }
    }

    //----- PS resident Base-14 font
    if (ps && !isCIDFont () && ((Gfx8BitFont*)this)->base14) {
        fontLoc = new GfxFontLoc ();
        fontLoc->locType = gfxFontLocResident;
        fontLoc->fontType = fontType1;
        fontLoc->path = new GString (((Gfx8BitFont*)this)->base14->base14Name);
        return fontLoc;
    }

    //----- external font file for Base-14 font
    if (!ps && !isCIDFont () && ((Gfx8BitFont*)this)->base14) {
        base14Name = new GString (((Gfx8BitFont*)this)->base14->base14Name);
        path =
            globalParams->findBase14FontFile (base14Name, &fontNum, &oblique);
        delete base14Name;
        if (path &&
            (fontLoc = getExternalFont (path, fontNum, oblique, false))) {
            return fontLoc;
        }
    }

    //----- system font
    if (name && (path = globalParams->findSystemFontFile (
                     name, &sysFontType, &fontNum))) {
        if (isCIDFont ()) {
            if (sysFontType == sysFontTTF || sysFontType == sysFontTTC) {
                fontLoc = new GfxFontLoc ();
                fontLoc->locType = gfxFontLocExternal;
                fontLoc->fontType = fontCIDType2;
                fontLoc->path = path;
                fontLoc->fontNum = fontNum;
                return fontLoc;
            }
        }
        else {
            if (sysFontType == sysFontTTF || sysFontType == sysFontTTC) {
                fontLoc = new GfxFontLoc ();
                fontLoc->locType = gfxFontLocExternal;
                fontLoc->fontType = fontTrueType;
                fontLoc->path = path;
                fontLoc->fontNum = fontNum;
                return fontLoc;
            }
            else if (sysFontType == sysFontPFA || sysFontType == sysFontPFB) {
                fontLoc = new GfxFontLoc ();
                fontLoc->locType = gfxFontLocExternal;
                fontLoc->fontType = fontType1;
                fontLoc->path = path;
                return fontLoc;
            }
        }
        delete path;
    }

    if (!isCIDFont ()) {
        //----- 8-bit PS resident font
        if (ps) {
            if (name && (path = globalParams->getPSResidentFont (name))) {
                fontLoc = new GfxFontLoc ();
                fontLoc->locType = gfxFontLocResident;
                fontLoc->fontType = fontType1;
                fontLoc->path = path;
                return fontLoc;
            }
        }

        //----- 8-bit font substitution
        if (flags & fontFixedWidth) { substIdx = 0; }
        else if (flags & fontSerif) {
            substIdx = 8;
        }
        else {
            substIdx = 4;
        }
        if (isBold ()) { substIdx += 2; }
        if (isItalic ()) { substIdx += 1; }
        substName = new GString (base14SubstFonts[substIdx]);
        if (ps) {
            error (
                errSyntaxWarning, -1, "Substituting font '{0:s}' for '{1:t}'",
                base14SubstFonts[substIdx], name);
            fontLoc = new GfxFontLoc ();
            fontLoc->locType = gfxFontLocResident;
            fontLoc->fontType = fontType1;
            fontLoc->path = substName;
            fontLoc->substIdx = substIdx;
            return fontLoc;
        }
        else {
            path = globalParams->findBase14FontFile (
                substName, &fontNum, &oblique);
            delete substName;
            if (path) {
                if ((fontLoc =
                         getExternalFont (path, fontNum, oblique, false))) {
                    error (
                        errSyntaxWarning, -1,
                        "Substituting font '{0:s}' for '{1:t}'",
                        base14SubstFonts[substIdx], name);
                    fontLoc->substIdx = substIdx;
                    return fontLoc;
                }
            }
        }

        // failed to find a substitute font
        return NULL;
    }

    //----- 16-bit PS resident font
    if (ps && name &&
        ((psFont16 = globalParams->getPSResidentFont16 (
              name, ((GfxCIDFont*)this)->getWMode ())))) {
        fontLoc = new GfxFontLoc ();
        fontLoc->locType = gfxFontLocResident;
        fontLoc->fontType = fontCIDType0; // this is not used
        fontLoc->path = psFont16->psFontName->copy ();
        fontLoc->encoding = psFont16->encoding->copy ();
        fontLoc->wMode = psFont16->wMode;
        return fontLoc;
    }
    if (ps && ((psFont16 = globalParams->getPSResidentFontCC (
                    ((GfxCIDFont*)this)->getCollection (),
                    ((GfxCIDFont*)this)->getWMode ())))) {
        error (
            errSyntaxWarning, -1, "Substituting font '{0:t}' for '{1:t}'",
            psFont16->psFontName, name);
        fontLoc = new GfxFontLoc ();
        fontLoc->locType = gfxFontLocResident;
        fontLoc->fontType = fontCIDType0; // this is not used
        fontLoc->path = psFont16->psFontName->copy ();
        fontLoc->encoding = psFont16->encoding->copy ();
        fontLoc->wMode = psFont16->wMode;
        return fontLoc;
    }

    //----- CID font substitution
    if ((path = globalParams->findCCFontFile (
             ((GfxCIDFont*)this)->getCollection ()))) {
        if ((fontLoc = getExternalFont (path, 0, 0, true))) {
            error (
                errSyntaxWarning, -1, "Substituting font '{0:t}' for '{1:t}'",
                fontLoc->path, name);
            return fontLoc;
        }
    }

    // failed to find a substitute font
    return NULL;
}

GfxFontLoc* GfxFont::locateBase14Font (GString* base14Name) {
    GString* path;
    int fontNum;
    double oblique;

    path = globalParams->findBase14FontFile (base14Name, &fontNum, &oblique);
    if (!path) { return NULL; }
    return getExternalFont (path, fontNum, oblique, false);
}

GfxFontLoc* GfxFont::getExternalFont (
    GString* path, int fontNum, double oblique, bool cid) {
    FoFiIdentifierType fft;
    GfxFontType fontType;
    GfxFontLoc* fontLoc;

    fft = FoFiIdentifier::identifyFile (path->c_str ());
    switch (fft) {
    case fofiIdType1PFA:
    case fofiIdType1PFB: fontType = fontType1; break;
    case fofiIdCFF8Bit: fontType = fontType1C; break;
    case fofiIdCFFCID: fontType = fontCIDType0C; break;
    case fofiIdTrueType:
    case fofiIdTrueTypeCollection:
        fontType = cid ? fontCIDType2 : fontTrueType;
        break;
    case fofiIdOpenTypeCFF8Bit: fontType = fontType1COT; break;
    case fofiIdOpenTypeCFFCID: fontType = fontCIDType0COT; break;
    case fofiIdDfont: fontType = cid ? fontCIDType2 : fontTrueType; break;
    case fofiIdUnknown:
    case fofiIdError:
    default: fontType = fontUnknownType; break;
    }
    if (fontType == fontUnknownType ||
        (cid ? (fontType < fontCIDType0) : (fontType >= fontCIDType0))) {
        delete path;
        return NULL;
    }
    fontLoc = new GfxFontLoc ();
    fontLoc->locType = gfxFontLocExternal;
    fontLoc->fontType = fontType;
    fontLoc->path = path;
    fontLoc->fontNum = fontNum;
    fontLoc->oblique = oblique;
    return fontLoc;
}

char* GfxFont::readEmbFontFile (XRef* xref, int* len) {
    char* buf;
    Object obj1, obj2;
    Stream* str;
    int size, n;

    obj1 = xpdf::make_ref_obj (embFontID.num, embFontID.gen, xref);
    obj2 = resolve (obj1);
    if (!obj2.is_stream ()) {
        error (errSyntaxError, -1, "Embedded font file is not a stream");
        embFontID.num = -1;
        return NULL;
    }
    str = obj2.as_stream ();

    size = 0;
    buf = NULL;
    str->reset ();
    do {
        if (size > INT_MAX - 4096) {
            error (errSyntaxError, -1, "Embedded font file is too large");
            break;
        }
        buf = (char*)realloc (buf, size + 4096);
        n = str->getBlock (buf + size, 4096);
        size += n;
    } while (n == 4096);
    *len = size;
    str->close ();


    return buf;
}

//------------------------------------------------------------------------
// Gfx8BitFont
//------------------------------------------------------------------------

Gfx8BitFont::Gfx8BitFont (
    XRef* xref, const char* tagA, Ref idA, GString* nameA, GfxFontType typeA,
    Ref embFontIDA, Dict* fontDict)
    : GfxFont (tagA, idA, nameA, typeA, embFontIDA) {
    GString* name2;
    BuiltinFont* builtinFont;
    const char** baseEnc;
    bool baseEncFromFontFile;
    char* buf;
    int len;
    FoFiType1* ffT1;
    FoFiType1C* ffT1C;
    int code, code2;
    char* charName;
    bool missing, hex;
    Unicode toUnicode[256];
    CharCodeToUnicode *utu, *ctu2;
    Unicode uBuf[8];
    double mul;
    int firstChar, lastChar;
    unsigned short w;
    Object obj1, obj2, obj3;
    int n, i, a, b, m;

    ctu = NULL;

    // do font name substitution for various aliases of the Base 14 font
    // names
    base14 = NULL;
    if (name) {
        name2 = name->copy ();
        i = 0;
        while (i < name2->getLength ()) {
            if (name2->getChar (i) == ' ') { name2->del (i); }
            else {
                ++i;
            }
        }
        a = 0;
        b = sizeof (base14FontMap) / sizeof (Base14FontMapEntry);
        // invariant: base14FontMap[a].altName <= name2 < base14FontMap[b].altName
        while (b - a > 1) {
            m = (a + b) / 2;
            if (name2->cmp (base14FontMap[m].altName) >= 0) { a = m; }
            else {
                b = m;
            }
        }
        if (!name2->cmp (base14FontMap[a].altName)) {
            base14 = &base14FontMap[a];
        }
        delete name2;
    }

    // is it a built-in font?
    builtinFont = NULL;
    if (base14) {
        for (i = 0; i < nBuiltinFonts; ++i) {
            if (!strcmp (base14->base14Name, builtinFonts[i].name)) {
                builtinFont = &builtinFonts[i];
                break;
            }
        }
    }

    // default ascent/descent values
    if (builtinFont) {
        ascent = 0.001 * builtinFont->ascent;
        descent = 0.001 * builtinFont->descent;
        fontBBox[0] = 0.001 * builtinFont->bbox[0];
        fontBBox[1] = 0.001 * builtinFont->bbox[1];
        fontBBox[2] = 0.001 * builtinFont->bbox[2];
        fontBBox[3] = 0.001 * builtinFont->bbox[3];
    }
    else {
        ascent = 0.75;
        descent = -0.25;
        fontBBox[0] = fontBBox[1] = fontBBox[2] = fontBBox[3] = 0;
    }

    // get info from font descriptor
    readFontDescriptor (xref, fontDict);

    // for non-embedded fonts, don't trust the ascent/descent/bbox
    // values from the font descriptor
    if (builtinFont && embFontID.num < 0) {
        ascent = 0.001 * builtinFont->ascent;
        descent = 0.001 * builtinFont->descent;
        fontBBox[0] = 0.001 * builtinFont->bbox[0];
        fontBBox[1] = 0.001 * builtinFont->bbox[1];
        fontBBox[2] = 0.001 * builtinFont->bbox[2];
        fontBBox[3] = 0.001 * builtinFont->bbox[3];
    }

    // get font matrix
    fontMat[0] = fontMat[3] = 1;
    fontMat[1] = fontMat[2] = fontMat[4] = fontMat[5] = 0;
    if ((obj1 = resolve ((*fontDict) ["FontMatrix"])).is_array ()) {
        for (i = 0; i < 6 && i < obj1.as_array ().size (); ++i) {
            if ((obj2 = resolve (obj1 [i])).is_num ()) {
                fontMat[i] = obj2.as_num ();
            }
        }
    }

    // get Type 3 bounding box, font definition, and resources
    if (type == fontType3) {
        if ((obj1 = resolve ((*fontDict) ["FontBBox"])).is_array ()) {
            for (i = 0; i < 4 && i < obj1.as_array ().size (); ++i) {
                if ((obj2 = resolve (obj1 [i])).is_num ()) {
                    fontBBox[i] = obj2.as_num ();
                }
            }
        }
        if (!(charProcs = resolve ((*fontDict) ["CharProcs"])).is_dict ()) {
            error (
                errSyntaxError, -1,
                "Missing or invalid CharProcs dictionary in Type 3 font");
        }
        if (!(resources = resolve ((*fontDict) ["Resources"])).is_dict ()) {
        }
    }

    //----- build the font encoding -----

    // Encodings start with a base encoding, which can come from
    // (in order of priority):
    //   1. FontDict.Encoding or FontDict.Encoding.BaseEncoding
    //        - MacRoman / MacExpert / WinAnsi / Standard
    //   2. embedded or external font file
    //   3. default:
    //        - builtin --> builtin encoding
    //        - TrueType --> WinAnsiEncoding
    //        - others --> StandardEncoding
    // and then add a list of differences (if any) from
    // FontDict.Encoding.Differences.

    // check FontDict for base encoding
    hasEncoding = false;
    usesMacRomanEnc = false;
    baseEnc = NULL;
    baseEncFromFontFile = false;
    obj1 = resolve ((*fontDict) ["Encoding"]);
    if (obj1.is_dict ()) {
        *&obj2 = resolve (obj1.as_dict ()["BaseEncoding"]);
        if (obj2.is_name ("MacRomanEncoding")) {
            hasEncoding = true;
            usesMacRomanEnc = true;
            baseEnc = macRomanEncoding;
        }
        else if (obj2.is_name ("MacExpertEncoding")) {
            hasEncoding = true;
            baseEnc = macExpertEncoding;
        }
        else if (obj2.is_name ("WinAnsiEncoding")) {
            hasEncoding = true;
            baseEnc = winAnsiEncoding;
        }
    }
    else if (obj1.is_name ("MacRomanEncoding")) {
        hasEncoding = true;
        usesMacRomanEnc = true;
        baseEnc = macRomanEncoding;
    }
    else if (obj1.is_name ("MacExpertEncoding")) {
        hasEncoding = true;
        baseEnc = macExpertEncoding;
    }
    else if (obj1.is_name ("WinAnsiEncoding")) {
        hasEncoding = true;
        baseEnc = winAnsiEncoding;
    }

    // check embedded font file for base encoding
    // (only for Type 1 fonts - trying to get an encoding out of a
    // TrueType font is a losing proposition)
    ffT1 = NULL;
    ffT1C = NULL;
    buf = NULL;
    if (type == fontType1 && embFontID.num >= 0) {
        if ((buf = readEmbFontFile (xref, &len))) {
            if ((ffT1 = FoFiType1::make (buf, len))) {
                if (ffT1->getName ()) {
                    if (embFontName) { delete embFontName; }
                    embFontName = new GString (ffT1->getName ());
                }
                if (!baseEnc) {
                    baseEnc = (const char**)ffT1->getEncoding ();
                    baseEncFromFontFile = true;
                }
            }
            free (buf);
        }
    }
    else if (type == fontType1C && embFontID.num >= 0) {
        if ((buf = readEmbFontFile (xref, &len))) {
            if ((ffT1C = FoFiType1C::make (buf, len))) {
                if (ffT1C->getName ()) {
                    if (embFontName) { delete embFontName; }
                    embFontName = new GString (ffT1C->getName ());
                }
                if (!baseEnc) {
                    baseEnc = (const char**)ffT1C->getEncoding ();
                    baseEncFromFontFile = true;
                }
            }
            free (buf);
        }
    }

    // get default base encoding
    if (!baseEnc) {
        if (builtinFont && embFontID.num < 0) {
            baseEnc = builtinFont->defaultBaseEnc;
            hasEncoding = true;
        }
        else if (type == fontTrueType) {
            baseEnc = winAnsiEncoding;
        }
        else {
            baseEnc = standardEncoding;
        }
    }

    // copy the base encoding
    for (i = 0; i < 256; ++i) {
        enc[i] = (char*)baseEnc[i];
        if ((encFree[i] = baseEncFromFontFile) && enc[i]) {
            enc[i] = strdup (baseEnc[i]);
        }
    }

    // some Type 1C font files have empty encodings, which can break the
    // T1C->T1 conversion (since the 'seac' operator depends on having
    // the accents in the encoding), so we fill in any gaps from
    // StandardEncoding
    if (type == fontType1C && embFontID.num >= 0 && baseEncFromFontFile) {
        for (i = 0; i < 256; ++i) {
            if (!enc[i] && standardEncoding[i]) {
                enc[i] = (char*)standardEncoding[i];
                encFree[i] = false;
            }
        }
    }

    // merge differences into encoding
    if (obj1.is_dict ()) {
        *&obj2 = resolve (obj1.as_dict ()["Differences"]);
        if (obj2.is_array ()) {
            hasEncoding = true;
            code = 0;
            for (i = 0; i < obj2.as_array ().size (); ++i) {
                obj3 = resolve (obj2 [i]);
                if (obj3.is_int ()) { code = obj3.as_int (); }
                else if (obj3.is_name ()) {
                    if (code >= 0 && code < 256) {
                        if (encFree[code]) { free (enc[code]); }
                        enc[code] = strdup (obj3.as_name ());
                        encFree[code] = true;
                    }
                    ++code;
                }
                else {
                    error (
                        errSyntaxError, -1,
                        "Wrong type in font encoding resource differences "
                        "({0:s})",
                        obj3.getTypeName ());
                }
            }
        }
    }
    if (ffT1) { delete ffT1; }
    if (ffT1C) { delete ffT1C; }

    //----- build the mapping to Unicode -----

    // pass 1: use the name-to-Unicode mapping table
    missing = hex = false;
    for (code = 0; code < 256; ++code) {
        if ((charName = enc[code])) {
            if (!(toUnicode[code] =
                      globalParams->mapNameToUnicode (charName)) &&
                strcmp (charName, ".notdef")) {
                // if it wasn't in the name-to-Unicode table, check for a
                // name that looks like 'Axx' or 'xx', where 'A' is any letter
                // and 'xx' is two hex digits
                if ((strlen (charName) == 3 && isalpha (charName[0]) &&
                     isxdigit (charName[1]) && isxdigit (charName[2]) &&
                     ((charName[1] >= 'a' && charName[1] <= 'f') ||
                      (charName[1] >= 'A' && charName[1] <= 'F') ||
                      (charName[2] >= 'a' && charName[2] <= 'f') ||
                      (charName[2] >= 'A' && charName[2] <= 'F'))) ||
                    (strlen (charName) == 2 && isxdigit (charName[0]) &&
                     isxdigit (charName[1]) &&
                     ((charName[0] >= 'a' && charName[0] <= 'f') ||
                      (charName[0] >= 'A' && charName[0] <= 'F') ||
                      (charName[1] >= 'a' && charName[1] <= 'f') ||
                      (charName[1] >= 'A' && charName[1] <= 'F')))) {
                    hex = true;
                }
                missing = true;
            }
        }
        else {
            toUnicode[code] = 0;
        }
    }

    // pass 2: try to fill in the missing chars, looking for names of
    // any of the following forms:
    // - 'xx'
    // - 'Axx'
    // - 'nn'
    // - 'Ann'
    // - 'ABnn'
    // - 'unixxxx' (possibly followed by garbage - some Arabic files
    //             use 'uni0628.medi', etc.)
    // where 'A' and 'B' are any letters, 'xx' is two hex digits, 'xxxx'
    // is four hex digits, and 'nn' is 2-4 decimal digits
    if (missing && globalParams->getMapNumericCharNames ()) {
        for (code = 0; code < 256; ++code) {
            if ((charName = enc[code]) && !toUnicode[code] &&
                strcmp (charName, ".notdef")) {
                n = (int)strlen (charName);
                code2 = -1;
                if (hex && n == 3 && isalpha (charName[0]) &&
                    isxdigit (charName[1]) && isxdigit (charName[2])) {
                    sscanf (charName + 1, "%x", &code2);
                }
                else if (
                    hex && n == 2 && isxdigit (charName[0]) &&
                    isxdigit (charName[1])) {
                    sscanf (charName, "%x", &code2);
                }
                else if (
                    !hex && n >= 2 && n <= 4 && isdigit (charName[0]) &&
                    isdigit (charName[1])) {
                    code2 = atoi (charName);
                }
                else if (
                    n >= 3 && n <= 5 && isdigit (charName[1]) &&
                    isdigit (charName[2])) {
                    code2 = atoi (charName + 1);
                }
                else if (
                    n >= 4 && n <= 6 && isdigit (charName[2]) &&
                    isdigit (charName[3])) {
                    code2 = atoi (charName + 2);
                }
                else if (
                    n >= 7 && charName[0] == 'u' && charName[1] == 'n' &&
                    charName[2] == 'i' && isxdigit (charName[3]) &&
                    isxdigit (charName[4]) && isxdigit (charName[5]) &&
                    isxdigit (charName[6])) {
                    sscanf (charName + 3, "%x", &code2);
                }
                if (code2 >= 0 && code2 <= 0xffff) {
                    toUnicode[code] = (Unicode)code2;
                }
            }
        }

        // if the 'mapUnknownCharNames' flag is set, do a simple pass-through
        // mapping for unknown character names
    }
    else if (missing && globalParams->getMapUnknownCharNames ()) {
        for (code = 0; code < 256; ++code) {
            if (!toUnicode[code]) { toUnicode[code] = code; }
        }
    }

    // construct the char code -> Unicode mapping object
    ctu = CharCodeToUnicode::make8BitToUnicode (toUnicode);

    // merge in a ToUnicode CMap, if there is one -- this overwrites
    // existing entries in ctu, i.e., the ToUnicode CMap takes
    // precedence, but the other encoding info is allowed to fill in any
    // holes
    readToUnicodeCMap (fontDict, 8, ctu);

    // look for a Unicode-to-Unicode mapping
    if (name && (utu = globalParams->getUnicodeToUnicode (name))) {
        for (i = 0; i < 256; ++i) { toUnicode[i] = 0; }
        ctu2 = CharCodeToUnicode::make8BitToUnicode (toUnicode);
        for (i = 0; i < 256; ++i) {
            n = ctu->mapToUnicode ((CharCode)i, uBuf, 8);
            if (n >= 1) {
                n = utu->mapToUnicode ((CharCode)uBuf[0], uBuf, 8);
                if (n >= 1) { ctu2->setMapping ((CharCode)i, uBuf, n); }
            }
        }
        utu->decRefCnt ();
        delete ctu;
        ctu = ctu2;
    }

    //----- get the character widths -----

    // initialize all widths
    for (code = 0; code < 256; ++code) { widths[code] = missingWidth * 0.001; }

    // use widths from font dict, if present
    obj1 = resolve ((*fontDict) ["FirstChar"]);
    firstChar = obj1.is_int () ? obj1.as_int () : 0;
    if (firstChar < 0 || firstChar > 255) { firstChar = 0; }
    obj1 = resolve ((*fontDict) ["LastChar"]);
    lastChar = obj1.is_int () ? obj1.as_int () : 255;
    if (lastChar < 0 || lastChar > 255) { lastChar = 255; }
    mul = (type == fontType3) ? fontMat[0] : 0.001;
    obj1 = resolve ((*fontDict) ["Widths"]);
    if (obj1.is_array ()) {
        flags |= fontFixedWidth;
        if (obj1.as_array ().size () < lastChar - firstChar + 1) {
            lastChar = firstChar + obj1.as_array ().size () - 1;
        }
        for (code = firstChar; code <= lastChar; ++code) {
            obj2 = resolve (obj1 [code - firstChar]);
            if (obj2.is_num ()) {
                widths[code] = obj2.as_num () * mul;
                if (fabs (widths[code] - widths[firstChar]) > 0.00001) {
                    flags &= ~fontFixedWidth;
                }
            }
        }

        // use widths from built-in font
    }
    else if (builtinFont) {
        // this is a kludge for broken PDF files that encode char 32
        // as .notdef
        if (builtinFont->widths->getWidth ("space", &w)) {
            widths[32] = 0.001 * w;
        }
        for (code = 0; code < 256; ++code) {
            if (enc[code] && builtinFont->widths->getWidth (enc[code], &w)) {
                widths[code] = 0.001 * w;
            }
        }

        // couldn't find widths -- use defaults
    }
    else {
        // this is technically an error -- the Widths entry is required
        // for all but the Base-14 fonts -- but certain PDF generators
        // apparently don't include widths for Arial and TimesNewRoman
        if (isFixedWidth ()) { i = 0; }
        else if (isSerif ()) {
            i = 8;
        }
        else {
            i = 4;
        }
        if (isBold ()) { i += 2; }
        if (isItalic ()) { i += 1; }
        builtinFont = builtinFontSubst[i];
        // this is a kludge for broken PDF files that encode char 32
        // as .notdef
        if (builtinFont->widths->getWidth ("space", &w)) {
            widths[32] = 0.001 * w;
        }
        for (code = 0; code < 256; ++code) {
            if (enc[code] && builtinFont->widths->getWidth (enc[code], &w)) {
                widths[code] = 0.001 * w;
            }
        }
    }

    ok = true;
}

Gfx8BitFont::~Gfx8BitFont () {
    int i;

    for (i = 0; i < 256; ++i) {
        if (encFree[i] && enc[i]) { free (enc[i]); }
    }
    ctu->decRefCnt ();
}

int Gfx8BitFont::getNextChar (
    const char* s, int len, CharCode* code, Unicode* u, int uSize, int* uLen,
    double* dx, double* dy, double* ox, double* oy) {
    CharCode c;

    *code = c = (CharCode) (*s & 0xff);
    *uLen = ctu->mapToUnicode (c, u, uSize);
    *dx = widths[c];
    *dy = *ox = *oy = 0;
    return 1;
}

CharCodeToUnicode* Gfx8BitFont::getToUnicode () {
    ctu->incRefCnt ();
    return ctu;
}

int* Gfx8BitFont::getCodeToGIDMap (FoFiTrueType* ff) {
    int* map;
    int cmapPlatform, cmapEncoding;
    int unicodeCmap, macRomanCmap, msSymbolCmap, cmap;
    bool useMacRoman, useUnicode;
    char* charName;
    Unicode u;
    int code, i, n;

    map = (int*)calloc (256, sizeof (int));
    for (i = 0; i < 256; ++i) { map[i] = 0; }

    // To match up with the Adobe-defined behaviour, we choose a cmap
    // like this:
    // 1. If the PDF font has an encoding:
    //    1a. If the PDF font specified MacRomanEncoding and the
    //        TrueType font has a Macintosh Roman cmap, use it, and
    //        reverse map the char names through MacRomanEncoding to
    //        get char codes.
    //    1b. If the PDF font is not symbolic or the PDF font is not
    //        embedded, and the TrueType font has a Microsoft Unicode
    //        cmap or a non-Microsoft Unicode cmap, use it, and use the
    //        Unicode indexes, not the char codes.
    //    1c. If the PDF font is symbolic and the TrueType font has a
    //        Microsoft Symbol cmap, use it, and use char codes
    //        directly (possibly with an offset of 0xf000).
    //    1d. If the TrueType font has a Macintosh Roman cmap, use it,
    //        as in case 1a.
    // 2. If the PDF font does not have an encoding or the PDF font is
    //    symbolic:
    //    2a. If the TrueType font has a Macintosh Roman cmap, use it,
    //        and use char codes directly (possibly with an offset of
    //        0xf000).
    //    2b. If the TrueType font has a Microsoft Symbol cmap, use it,
    //        and use char codes directly (possible with an offset of
    //        0xf000).
    // 3. If none of these rules apply, use the first cmap and hope for
    //    the best (this shouldn't happen).
    unicodeCmap = macRomanCmap = msSymbolCmap = -1;
    for (i = 0; i < ff->getNumCmaps (); ++i) {
        cmapPlatform = ff->getCmapPlatform (i);
        cmapEncoding = ff->getCmapEncoding (i);
        if ((cmapPlatform == 3 && cmapEncoding == 1) || cmapPlatform == 0) {
            unicodeCmap = i;
        }
        else if (cmapPlatform == 1 && cmapEncoding == 0) {
            macRomanCmap = i;
        }
        else if (cmapPlatform == 3 && cmapEncoding == 0) {
            msSymbolCmap = i;
        }
    }
    cmap = 0;
    useMacRoman = false;
    useUnicode = false;
    if (hasEncoding) {
        if (usesMacRomanEnc && macRomanCmap >= 0) {
            cmap = macRomanCmap;
            useMacRoman = true;
        }
        else if (
            (!(flags & fontSymbolic) || embFontID.num < 0) &&
            unicodeCmap >= 0) {
            cmap = unicodeCmap;
            useUnicode = true;
        }
        else if ((flags & fontSymbolic) && msSymbolCmap >= 0) {
            cmap = msSymbolCmap;
        }
        else if ((flags & fontSymbolic) && macRomanCmap >= 0) {
            cmap = macRomanCmap;
        }
        else if (macRomanCmap >= 0) {
            cmap = macRomanCmap;
            useMacRoman = true;
        }
    }
    else {
        if (msSymbolCmap >= 0) { cmap = msSymbolCmap; }
        else if (macRomanCmap >= 0) {
            cmap = macRomanCmap;
        }
    }

    // reverse map the char names through MacRomanEncoding, then map the
    // char codes through the cmap
    if (useMacRoman) {
        for (i = 0; i < 256; ++i) {
            if ((charName = enc[i])) {
                if ((code = globalParams->getMacRomanCharCode (charName))) {
                    map[i] = ff->mapCodeToGID (cmap, code);
                }
            }
            else {
                map[i] = -1;
            }
        }

        // map Unicode through the cmap
    }
    else if (useUnicode) {
        for (i = 0; i < 256; ++i) {
            if (((charName = enc[i]) &&
                 (u = globalParams->mapNameToUnicode (charName))) ||
                (n = ctu->mapToUnicode ((CharCode)i, &u, 1))) {
                map[i] = ff->mapCodeToGID (cmap, u);
            }
            else {
                map[i] = -1;
            }
        }

        // map the char codes through the cmap, possibly with an offset of
        // 0xf000
    }
    else {
        for (i = 0; i < 256; ++i) {
            if (!(map[i] = ff->mapCodeToGID (cmap, i))) {
                map[i] = ff->mapCodeToGID (cmap, 0xf000 + i);
            }
        }
    }

    // try the TrueType 'post' table to handle any unmapped characters
    for (i = 0; i < 256; ++i) {
        if (map[i] <= 0 && (charName = enc[i])) {
            map[i] = ff->mapNameToGID (charName);
        }
    }

    return map;
}

Dict* Gfx8BitFont::getCharProcs () {
    return charProcs.is_dict () ? charProcs.as_dict_ptr () : (Dict*)NULL;
}

Object* Gfx8BitFont::getCharProc (int code, Object* proc) {
    if (enc[code] && charProcs.is_dict ()) {
        *proc = resolve (charProcs.as_dict ()[enc[code]]);
    }
    else {
        *proc = { };
    }
    return proc;
}

Object* Gfx8BitFont::getCharProcNF (int code, Object* proc) {
    if (enc[code] && charProcs.is_dict ()) {
        *proc = charProcs.as_dict ()[enc[code]];
    }
    else {
        *proc = { };
    }
    return proc;
}

Dict* Gfx8BitFont::getResources () {
    return resources.is_dict () ? resources.as_dict_ptr () : (Dict*)NULL;
}

//------------------------------------------------------------------------
// GfxCIDFont
//------------------------------------------------------------------------

struct cmpWidthExcepFunctor {
    bool operator() (
        const GfxFontCIDWidthExcep& w1, const GfxFontCIDWidthExcep& w2) {
        return w1.first < w2.first;
    }
};

struct cmpWidthExcepVFunctor {
    bool operator() (
        const GfxFontCIDWidthExcepV& w1, const GfxFontCIDWidthExcepV& w2) {
        return w1.first < w2.first;
    }
};

GfxCIDFont::GfxCIDFont (
    XRef* xref, const char* tagA, Ref idA, GString* nameA, GfxFontType typeA,
    Ref embFontIDA, Dict* fontDict)
    : GfxFont (tagA, idA, nameA, typeA, embFontIDA) {
    Dict* desFontDict;
    Object desFontDictObj;
    Object obj1, obj2, obj3, obj4, obj5, obj6;
    CharCodeToUnicode* utu;
    CharCode c;
    Unicode uBuf[8];
    int c1, c2;
    int excepsSize, i, j, k, n;

    ascent = 0.95;
    descent = -0.35;
    fontBBox[0] = fontBBox[1] = fontBBox[2] = fontBBox[3] = 0;
    collection = NULL;
    cMap = NULL;
    ctu = NULL;
    ctuUsesCharCode = true;
    widths.defWidth = 1.0;
    widths.defHeight = -1.0;
    widths.defVY = 0.880;
    widths.exceps = NULL;
    widths.nExceps = 0;
    widths.excepsV = NULL;
    widths.nExcepsV = 0;
    cidToGID = NULL;
    cidToGIDLen = 0;

    // get the descendant font
    if (!(obj1 = resolve ((*fontDict) ["DescendantFonts"])).is_array () ||
        obj1.as_array ().size () == 0) {
        error (
            errSyntaxError, -1,
            "Missing or empty DescendantFonts entry in Type 0 font");
        goto err1;
    }
    if (!(desFontDictObj = resolve (obj1 [0UL])).is_dict ()) {
        error (errSyntaxError, -1, "Bad descendant font in Type 0 font");
        goto err2;
    }
    desFontDict = desFontDictObj.as_dict_ptr ();

    // get info from font descriptor
    readFontDescriptor (xref, desFontDict);

    //----- encoding info -----

    // char collection
    if (!(obj1 = resolve ((*desFontDict) ["CIDSystemInfo"])).is_dict ()) {
        error (
            errSyntaxError, -1,
            "Missing CIDSystemInfo dictionary in Type 0 descendant font");
        goto err2;
    }
    *&obj2 = resolve (obj1.as_dict ()["Registry"]);
    *&obj3 = resolve (obj1.as_dict ()["Ordering"]);
    if (!obj2.is_string () || !obj3.is_string ()) {
        error (
            errSyntaxError, -1,
            "Invalid CIDSystemInfo dictionary in Type 0 descendant font");
        goto err3;
    }
    collection =
        obj2.as_string ()->copy ()->append ('-')->append (obj3.as_string ());

    // look for a ToUnicode CMap
    if (!(ctu = readToUnicodeCMap (fontDict, 16, NULL))) {
        ctuUsesCharCode = false;

        // use an identity mapping for the "Adobe-Identity" and
        // "Adobe-UCS" collections
        if (!collection->cmp ("Adobe-Identity") ||
            !collection->cmp ("Adobe-UCS")) {
            ctu = CharCodeToUnicode::makeIdentityMapping ();

            // look for a user-supplied .cidToUnicode file
        }
        else if (!(ctu = globalParams->getCIDToUnicode (collection))) {
            error (
                errSyntaxError, -1, "Unknown character collection '{0:t}'",
                collection);
        }
    }

    // look for a Unicode-to-Unicode mapping
    if (name && (utu = globalParams->getUnicodeToUnicode (name))) {
        if (ctu) {
            for (c = 0; c < ctu->getLength (); ++c) {
                n = ctu->mapToUnicode (c, uBuf, 8);
                if (n >= 1) {
                    n = utu->mapToUnicode ((CharCode)uBuf[0], uBuf, 8);
                    if (n >= 1) { ctu->setMapping (c, uBuf, n); }
                }
            }
            utu->decRefCnt ();
        }
        else {
            ctu = utu;
        }
    }

    // encoding (i.e., CMap)
    if ((obj1 = resolve ((*fontDict) ["Encoding"])).is_null ()) {
        error (errSyntaxError, -1, "Missing Encoding entry in Type 0 font");
        goto err2;
    }
    if (!(cMap = CMap::parse (NULL, collection, &obj1))) { goto err2; }

    // CIDToGIDMap
    // (the PDF spec only allows these for TrueType fonts, but Acrobat
    // apparently also allows them for OpenType CFF fonts)
    if (type == fontCIDType2 || type == fontCIDType0COT) {
        obj1 = resolve ((*desFontDict) ["CIDToGIDMap"]);
        if (obj1.is_stream ()) {
            cidToGIDLen = 0;
            i = 64;
            cidToGID = (int*)calloc (i, sizeof (int));
            obj1.streamReset ();
            while ((c1 = obj1.streamGetChar ()) != EOF &&
                   (c2 = obj1.streamGetChar ()) != EOF) {
                if (cidToGIDLen == i) {
                    i *= 2;
                    cidToGID = (int*)reallocarray (cidToGID, i, sizeof (int));
                }
                cidToGID[cidToGIDLen++] = (c1 << 8) + c2;
            }
            obj1.streamClose ();
        }
        else if (!obj1.is_name ("Identity") && !obj1.is_null ()) {
            error (errSyntaxError, -1, "Invalid CIDToGIDMap entry in CID font");
        }
    }

    //----- character metrics -----

    // default char width
    if ((obj1 = resolve ((*desFontDict) ["DW"])).is_int ()) {
        widths.defWidth = obj1.as_int () * 0.001;
    }

    // char width exceptions
    if ((obj1 = resolve ((*desFontDict) ["W"])).is_array ()) {
        excepsSize = 0;
        i = 0;
        while (i + 1 < obj1.as_array ().size ()) {
            obj2 = resolve (obj1 [i]);
            obj3 = resolve (obj1 [i + 1]);
            if (obj2.is_int () && obj3.is_int () &&
                i + 2 < obj1.as_array ().size ()) {
                if ((obj4 = resolve (obj1 [i + 2])).is_num ()) {
                    if (widths.nExceps == excepsSize) {
                        excepsSize += 16;
                        widths.exceps = (GfxFontCIDWidthExcep*)reallocarray (
                            widths.exceps, excepsSize,
                            sizeof (GfxFontCIDWidthExcep));
                    }
                    widths.exceps[widths.nExceps].first = obj2.as_int ();
                    widths.exceps[widths.nExceps].last = obj3.as_int ();
                    widths.exceps[widths.nExceps].width =
                        obj4.as_num () * 0.001;
                    ++widths.nExceps;
                }
                else {
                    error (
                        errSyntaxError, -1, "Bad widths array in Type 0 font");
                }
                i += 3;
            }
            else if (obj2.is_int () && obj3.is_array ()) {
                if (widths.nExceps + obj3.as_array ().size () > excepsSize) {
                    excepsSize =
                        (widths.nExceps + obj3.as_array ().size () + 15) & ~15;
                    widths.exceps = (GfxFontCIDWidthExcep*)reallocarray (
                        widths.exceps, excepsSize,
                        sizeof (GfxFontCIDWidthExcep));
                }
                j = obj2.as_int ();
                for (k = 0; k < obj3.as_array ().size (); ++k) {
                    if ((obj4 = resolve (obj3 [k])).is_num ()) {
                        widths.exceps[widths.nExceps].first = j;
                        widths.exceps[widths.nExceps].last = j;
                        widths.exceps[widths.nExceps].width =
                            obj4.as_num () * 0.001;
                        ++j;
                        ++widths.nExceps;
                    }
                    else {
                        error (
                            errSyntaxError, -1,
                            "Bad widths array in Type 0 font");
                    }
                }
                i += 2;
            }
            else {
                error (errSyntaxError, -1, "Bad widths array in Type 0 font");
                ++i;
            }
        }
        std::sort (
            widths.exceps, widths.exceps + widths.nExceps,
            cmpWidthExcepFunctor ());
    }

    // default metrics for vertical font
    if ((obj1 = resolve ((*desFontDict) ["DW2"])).is_array () &&
        obj1.as_array ().size () == 2) {
        if ((obj2 = resolve (obj1 [0UL])).is_num ()) {
            widths.defVY = obj2.as_num () * 0.001;
        }
        if ((obj2 = resolve (obj1 [1])).is_num ()) {
            widths.defHeight = obj2.as_num () * 0.001;
        }
    }

    // char metric exceptions for vertical font
    if ((obj1 = resolve ((*desFontDict) ["W2"])).is_array ()) {
        excepsSize = 0;
        i = 0;
        while (i + 1 < obj1.as_array ().size ()) {
            obj2 = resolve (obj1 [i]);
            obj3 = resolve (obj1 [i + 1]);
            if (obj2.is_int () && obj3.is_int () &&
                i + 4 < obj1.as_array ().size ()) {
                if ((obj4 = resolve (obj1 [i + 2])).is_num () &&
                    (obj5 = resolve (obj1 [i + 3])).is_num () &&
                    (obj6 = resolve (obj1 [i + 4])).is_num ()) {
                    if (widths.nExcepsV == excepsSize) {
                        excepsSize += 16;
                        widths.excepsV = (GfxFontCIDWidthExcepV*)reallocarray (
                            widths.excepsV, excepsSize,
                            sizeof (GfxFontCIDWidthExcepV));
                    }
                    widths.excepsV[widths.nExcepsV].first = obj2.as_int ();
                    widths.excepsV[widths.nExcepsV].last = obj3.as_int ();
                    widths.excepsV[widths.nExcepsV].height =
                        obj4.as_num () * 0.001;
                    widths.excepsV[widths.nExcepsV].vx = obj5.as_num () * 0.001;
                    widths.excepsV[widths.nExcepsV].vy = obj6.as_num () * 0.001;
                    ++widths.nExcepsV;
                }
                else {
                    error (
                        errSyntaxError, -1,
                        "Bad widths (W2) array in Type 0 font");
                }
                i += 5;
            }
            else if (obj2.is_int () && obj3.is_array ()) {
                if (widths.nExcepsV + obj3.as_array ().size () / 3 > excepsSize) {
                    excepsSize =
                        (widths.nExcepsV + obj3.as_array ().size () / 3 + 15) &
                        ~15;
                    widths.excepsV = (GfxFontCIDWidthExcepV*)reallocarray (
                        widths.excepsV, excepsSize,
                        sizeof (GfxFontCIDWidthExcepV));
                }
                j = obj2.as_int ();
                for (k = 0; k < obj3.as_array ().size (); k += 3) {
                    if ((obj4 = resolve (obj3 [k])).is_num () &&
                        (obj5 = resolve (obj3 [k + 1])).is_num () &&
                        (obj6 = resolve (obj3 [k + 2])).is_num ()) {
                        widths.excepsV[widths.nExcepsV].first = j;
                        widths.excepsV[widths.nExcepsV].last = j;
                        widths.excepsV[widths.nExcepsV].height =
                            obj4.as_num () * 0.001;
                        widths.excepsV[widths.nExcepsV].vx =
                            obj5.as_num () * 0.001;
                        widths.excepsV[widths.nExcepsV].vy =
                            obj6.as_num () * 0.001;
                        ++j;
                        ++widths.nExcepsV;
                    }
                    else {
                        error (
                            errSyntaxError, -1,
                            "Bad widths (W2) array in Type 0 font");
                    }
                }
                i += 2;
            }
            else {
                error (
                    errSyntaxError, -1, "Bad widths (W2) array in Type 0 font");
                ++i;
            }
        }
        std::sort (
            widths.excepsV, widths.excepsV + widths.nExcepsV,
            cmpWidthExcepVFunctor ());
    }

    ok = true;
    return;

err3:
err2:
err1:
    error (errSyntaxError, -1, "Failed to parse font object for '{0:t}'", name);
}

GfxCIDFont::~GfxCIDFont () {
    if (collection) { delete collection; }
    if (cMap) { cMap->decRefCnt (); }
    if (ctu) { ctu->decRefCnt (); }
    free (widths.exceps);
    free (widths.excepsV);
    if (cidToGID) { free (cidToGID); }
}

int GfxCIDFont::getNextChar (
    const char* s, int len, CharCode* code, Unicode* u, int uSize, int* uLen,
    double* dx, double* dy, double* ox, double* oy) {
    CID cid;
    CharCode c;
    double w, h, vx, vy;
    int n, a, b, m;

    if (!cMap) {
        *code = 0;
        *uLen = 0;
        *dx = *dy = 0;
        return 1;
    }

    *code = (CharCode) (cid = cMap->getCID (s, len, &c, &n));
    if (ctu) {
        *uLen = ctu->mapToUnicode (ctuUsesCharCode ? c : cid, u, uSize);
    }
    else {
        *uLen = 0;
    }
    if (!*uLen && uSize >= 1 && globalParams->getMapUnknownCharNames ()) {
        u[0] = *code;
        *uLen = 1;
    }

    // horizontal
    if (cMap->getWMode () == 0) {
        w = widths.defWidth;
        h = vx = vy = 0;
        if (widths.nExceps > 0 && cid >= widths.exceps[0].first) {
            a = 0;
            b = widths.nExceps;
            // invariant: widths.exceps[a].first <= cid < widths.exceps[b].first
            while (b - a > 1) {
                m = (a + b) / 2;
                if (widths.exceps[m].first <= cid) { a = m; }
                else {
                    b = m;
                }
            }
            if (cid <= widths.exceps[a].last) { w = widths.exceps[a].width; }
        }

        // vertical
    }
    else {
        w = 0;
        h = widths.defHeight;
        vx = widths.defWidth / 2;
        vy = widths.defVY;
        if (widths.nExcepsV > 0 && cid >= widths.excepsV[0].first) {
            a = 0;
            b = widths.nExcepsV;
            // invariant: widths.excepsV[a].first <= cid < widths.excepsV[b].first
            while (b - a > 1) {
                m = (a + b) / 2;
                if (widths.excepsV[m].last <= cid) { a = m; }
                else {
                    b = m;
                }
            }
            if (cid <= widths.excepsV[a].last) {
                h = widths.excepsV[a].height;
                vx = widths.excepsV[a].vx;
                vy = widths.excepsV[a].vy;
            }
        }
    }

    *dx = w;
    *dy = h;
    *ox = vx;
    *oy = vy;

    return n;
}

int GfxCIDFont::getWMode () { return cMap ? cMap->getWMode () : 0; }

CharCodeToUnicode* GfxCIDFont::getToUnicode () {
    if (ctu) { ctu->incRefCnt (); }
    return ctu;
}

GString* GfxCIDFont::getCollection () {
    return cMap ? cMap->getCollection () : (GString*)NULL;
}

//------------------------------------------------------------------------
// GfxFontDict
//------------------------------------------------------------------------

GfxFontDict::GfxFontDict (XRef* xref, Ref* fontDictRef, Dict* fontDict) {
    int i;
    Object obj1, obj2;
    Ref r;

    numFonts = fontDict->size ();
    fonts = (GfxFont**)calloc (numFonts, sizeof (GfxFont*));
    for (i = 0; i < numFonts; ++i) {
        fontDict->getValNF (i, &obj1);
        obj2 = resolve (obj1);
        if (obj2.is_dict ()) {
            if (obj1.is_ref ()) { r = obj1.as_ref (); }
            else {
                // no indirect reference for this font, so invent a unique one
                // (legal generation numbers are five digits, so any 6-digit
                // number would be safe)
                r.num = i;
                if (fontDictRef) { r.gen = 100000 + fontDictRef->num; }
                else {
                    r.gen = 999999;
                }
            }
            fonts[i] = GfxFont::makeFont (
                xref, fontDict->key_at (i).c_str (), r, obj2.as_dict_ptr ());
            if (fonts[i] && !fonts[i]->isOk ()) {
                delete fonts[i];
                fonts[i] = NULL;
            }
        }
        else {
            error (errSyntaxError, -1, "font resource is not a dictionary");
            fonts[i] = NULL;
        }
    }
}

GfxFontDict::~GfxFontDict () {
    int i;

    for (i = 0; i < numFonts; ++i) {
        if (fonts[i]) { delete fonts[i]; }
    }
    free (fonts);
}

GfxFont* GfxFontDict::lookup (const char* tag) {
    int i;

    for (i = 0; i < numFonts; ++i) {
        if (fonts[i] && fonts[i]->matches (tag)) { return fonts[i]; }
    }
    return NULL;
}

GfxFont* GfxFontDict::lookupByRef (Ref ref) {
    int i;

    for (i = 0; i < numFonts; ++i) {
        if (fonts[i] && fonts[i]->getID ()->num == ref.num &&
            fonts[i]->getID ()->gen == ref.gen) {
            return fonts[i];
        }
    }
    return NULL;
}

// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <cstring>
#include <cstdio>
#include <cctype>

#include <paper.h>

#include <utils/string.hh>
#include <utils/GList.hh>
#include <utils/GHash.hh>
#include <utils/gfile.hh>

#include <fofi/FoFiIdentifier.hh>

#include <xpdf/Error.hh>
#include <xpdf/NameToCharCode.hh>
#include <xpdf/CharCodeToUnicode.hh>
#include <xpdf/UnicodeMap.hh>
#include <xpdf/CMap.hh>
#include <xpdf/BuiltinFontTables.hh>
#include <xpdf/FontEncodingTables.hh>

#include <xpdf/GlobalParams.hh>

#include "NameToUnicodeTable.cc"
#include "UnicodeMapTables.cc"
#include "UTF8.cc"

//------------------------------------------------------------------------

#define cidToUnicodeCacheSize 4
#define unicodeToUnicodeCacheSize 4

//------------------------------------------------------------------------

static struct {
    const char* name;
    const char* t1FileName;
    const char* ttFileName;
    const char* macFileName; // may be .dfont, .ttf, or .ttc
    const char* macFontName; // font name inside .dfont or .ttc
    const char* obliqueFont; // name of font to oblique
    double obliqueFactor;    // oblique sheer factor
} displayFontTab[] = {
    { "Courier", "n022003l.pfb", "cour.ttf", "Courier", "Courier", NULL, 0 },
    { "Courier-Bold", "n022004l.pfb", "courbd.ttf", "Courier", "Courier Bold",
      NULL, 0 },
    { "Courier-BoldOblique", "n022024l.pfb", "courbi.ttf", "Courier",
      "Courier Bold Oblique", "Courier-Bold", 0.212557 },
    { "Courier-Oblique", "n022023l.pfb", "couri.ttf", "Courier",
      "Courier Oblique", "Courier", 0.212557 },
    { "Helvetica", "n019003l.pfb", "arial.ttf", "Helvetica", "Helvetica", NULL,
      0 },
    { "Helvetica-Bold", "n019004l.pfb", "arialbd.ttf", "Helvetica",
      "Helvetica-Bold", NULL, 0 },
    { "Helvetica-BoldOblique", "n019024l.pfb", "arialbi.ttf", "Helvetica",
      "Helvetica Bold Oblique", "Helvetica-Bold", 0.212557 },
    { "Helvetica-Oblique", "n019023l.pfb", "ariali.ttf", "Helvetica",
      "Helvetica Oblique", "Helvetica", 0.212557 },
    { "Symbol", "s050000l.pfb", NULL, "Symbol", "Symbol", NULL, 0 },
    { "Times-Bold", "n021004l.pfb", "timesbd.ttf", "Times", "Times-Bold", NULL,
      0 },
    { "Times-BoldItalic", "n021024l.pfb", "timesbi.ttf", "Times",
      "Times-BoldItalic", NULL, 0 },
    { "Times-Italic", "n021023l.pfb", "timesi.ttf", "Times", "Times-Italic",
      NULL, 0 },
    { "Times-Roman", "n021003l.pfb", "times.ttf", "Times", "Times-Roman", NULL,
      0 },
    { "ZapfDingbats", "d050000l.pfb", NULL, "ZapfDingbats", "Zapf Dingbats",
      NULL, 0 },
    { }
};

static const char* displayFontDirs[] = {
    "/usr/share/ghostscript/fonts",   "/usr/local/share/ghostscript/fonts",
    "/usr/share/fonts/default/Type1", "/usr/share/fonts/default/ghostscript",
    "/usr/share/fonts/type1/gsfonts", NULL
};

struct Base14FontInfo {
    Base14FontInfo (GString* fileNameA, int fontNumA, double obliqueA) {
        fileName = fileNameA;
        fontNum = fontNumA;
        oblique = obliqueA;
    }
    ~Base14FontInfo () { delete fileName; }
    GString* fileName;
    int fontNum;
    double oblique;
};

//------------------------------------------------------------------------

GlobalParams* globalParams = NULL;

//------------------------------------------------------------------------
// PSFontParam16
//------------------------------------------------------------------------

PSFontParam16::PSFontParam16 (
    GString* nameA, int wModeA, GString* psFontNameA, GString* encodingA) {
    name = nameA;
    wMode = wModeA;
    psFontName = psFontNameA;
    encoding = encodingA;
}

PSFontParam16::~PSFontParam16 () {
    delete name;
    delete psFontName;
    delete encoding;
}

//------------------------------------------------------------------------
// SysFontInfo
//------------------------------------------------------------------------

class SysFontInfo {
public:
    GString* name;
    bool bold;
    bool italic;
    GString* path;
    SysFontType type;
    int fontNum; // for TrueType collections

    SysFontInfo (
        GString* nameA, bool boldA, bool italicA, GString* pathA,
        SysFontType typeA, int fontNumA);
    ~SysFontInfo ();
    bool match (SysFontInfo* fi);
    bool match (GString* nameA, bool boldA, bool italicA);
};

SysFontInfo::SysFontInfo (
    GString* nameA, bool boldA, bool italicA, GString* pathA,
    SysFontType typeA, int fontNumA) {
    name = nameA;
    bold = boldA;
    italic = italicA;
    path = pathA;
    type = typeA;
    fontNum = fontNumA;
}

SysFontInfo::~SysFontInfo () {
    delete name;
    delete path;
}

bool SysFontInfo::match (SysFontInfo* fi) {
    return !strcasecmp (name->c_str (), fi->name->c_str ()) &&
           bold == fi->bold && italic == fi->italic;
}

bool SysFontInfo::match (GString* nameA, bool boldA, bool italicA) {
    return !strcasecmp (name->c_str (), nameA->c_str ()) &&
           bold == boldA && italic == italicA;
}

//------------------------------------------------------------------------
// SysFontList
//------------------------------------------------------------------------

class SysFontList {
public:
    SysFontList ();
    ~SysFontList ();
    SysFontInfo* find (GString* name);

private:
    GList* fonts; // [SysFontInfo]
};

SysFontList::SysFontList () { fonts = new GList (); }

SysFontList::~SysFontList () { deleteGList (fonts, SysFontInfo); }

SysFontInfo* SysFontList::find (GString* name) {
    GString* name2;
    bool bold, italic;
    SysFontInfo* fi;
    char c;
    int n, i;

    name2 = name->copy ();

    // remove space, comma, dash chars
    i = 0;
    while (i < name2->getLength ()) {
        c = (*name2) [i];
        if (c == ' ' || c == ',' || c == '-') { name2->del (i); }
        else {
            ++i;
        }
    }
    n = name2->getLength ();

    // font names like "Arial-BoldMT,Bold" are occasionally used,
    // so run this loop twice
    bold = italic = false;
    for (i = 0; i < 2; ++i) {
        // remove trailing "MT" (Foo-MT, Foo-BoldMT, etc.)
        if (n > 2 && !strcmp (name2->c_str () + n - 2, "MT")) {
            name2->del (n - 2, 2);
            n -= 2;
        }

        // look for "Regular"
        if (n > 7 && !strcmp (name2->c_str () + n - 7, "Regular")) {
            name2->del (n - 7, 7);
            n -= 7;
        }

        // look for "Italic"
        if (n > 6 && !strcmp (name2->c_str () + n - 6, "Italic")) {
            name2->del (n - 6, 6);
            italic = true;
            n -= 6;
        }

        // look for "Bold"
        if (n > 4 && !strcmp (name2->c_str () + n - 4, "Bold")) {
            name2->del (n - 4, 4);
            bold = true;
            n -= 4;
        }
    }

    // remove trailing "PS"
    if (n > 2 && !strcmp (name2->c_str () + n - 2, "PS")) {
        name2->del (n - 2, 2);
        n -= 2;
    }

    // remove trailing "IdentityH"
    if (n > 9 && !strcmp (name2->c_str () + n - 9, "IdentityH")) {
        name2->del (n - 9, 9);
        n -= 9;
    }

    // search for the font
    fi = NULL;
    for (i = 0; i < fonts->getLength (); ++i) {
        fi = (SysFontInfo*)fonts->get (i);
        if (fi->match (name2, bold, italic)) { break; }
        fi = NULL;
    }
    if (!fi && bold) {
        // try ignoring the bold flag
        for (i = 0; i < fonts->getLength (); ++i) {
            fi = (SysFontInfo*)fonts->get (i);
            if (fi->match (name2, false, italic)) { break; }
            fi = NULL;
        }
    }
    if (!fi && (bold || italic)) {
        // try ignoring the bold and italic flags
        for (i = 0; i < fonts->getLength (); ++i) {
            fi = (SysFontInfo*)fonts->get (i);
            if (fi->match (name2, false, false)) { break; }
            fi = NULL;
        }
    }

    delete name2;
    return fi;
}

//------------------------------------------------------------------------
// KeyBinding
//------------------------------------------------------------------------

KeyBinding::KeyBinding (int codeA, int modsA, int contextA, const char* cmd0) {
    code = codeA;
    mods = modsA;
    context = contextA;
    cmds = new GList ();
    cmds->append (new GString (cmd0));
}

KeyBinding::KeyBinding (
    int codeA, int modsA, int contextA, const char* cmd0, const char* cmd1) {
    code = codeA;
    mods = modsA;
    context = contextA;
    cmds = new GList ();
    cmds->append (new GString (cmd0));
    cmds->append (new GString (cmd1));
}

KeyBinding::KeyBinding (int codeA, int modsA, int contextA, GList* cmdsA) {
    code = codeA;
    mods = modsA;
    context = contextA;
    cmds = cmdsA;
}

KeyBinding::~KeyBinding () { deleteGList (cmds, GString); }

//------------------------------------------------------------------------
// parsing
//------------------------------------------------------------------------

GlobalParams::GlobalParams (const char* cfgFileName) {
    UnicodeMap* map;
    GString* fileName;
    FILE* f;
    int i;

    initBuiltinFontTables ();

    // scan the encoding in reverse because we want the lowest-numbered
    // index for each char name ('space' is encoded twice)
    macRomanReverseMap = new NameToCharCode ();
    for (i = 255; i >= 0; --i) {
        if (macRomanEncoding[i]) {
            macRomanReverseMap->add (macRomanEncoding[i], (CharCode)i);
        }
    }

    baseDir = appendToPath (getHomeDir (), ".xpdf");
    nameToUnicode = new NameToCharCode ();
    cidToUnicodes = new GHash (true);
    unicodeToUnicodes = new GHash (true);
    residentUnicodeMaps = new GHash ();
    unicodeMaps = new GHash (true);
    cMapDirs = new GHash (true);
    toUnicodeDirs = new GList ();
    fontFiles = new GHash (true);
    fontDirs = new GList ();
    ccFontFiles = new GHash (true);
    base14SysFonts = new GHash (true);
    sysFonts = new SysFontList ();

    char* paperName;
    const struct paper* paperType;
    paperinit ();
    if ((paperName = systempapername ())) {
        paperType = paperinfo (paperName);
        psPaperWidth = (int)paperpswidth (paperType);
        psPaperHeight = (int)paperpsheight (paperType);
    }
    else {
        error (
            errConfig, -1, "No paper information available - using defaults");

        psPaperWidth = XPDF_PAPER_WIDTH;
        psPaperHeight = XPDF_PAPER_HEIGHT;
    }
    paperdone ();

    psImageableLLX = psImageableLLY = 0;
    psImageableURX = psPaperWidth;
    psImageableURY = psPaperHeight;
    psCrop = true;
    psUseCropBoxAsPage = false;
    psExpandSmaller = false;
    psShrinkLarger = true;
    psCenter = true;
    psDuplex = false;
    psLevel = psLevel2;
    psFile = NULL;
    psResidentFonts = new GHash (true);
    psResidentFonts16 = new GList ();
    psResidentFontsCC = new GList ();
    psEmbedType1 = true;
    psEmbedTrueType = true;
    psEmbedCIDPostScript = true;
    psEmbedCIDTrueType = true;
    psFontPassthrough = false;
    psPreload = false;
    psOPI = false;
    psASCIIHex = false;
    psLZW = true;
    psUncompressPreloadedImages = false;
    psMinLineWidth = 0;
    psRasterResolution = 300;
    psRasterMono = false;
    psRasterSliceSize = 20000000;
    psAlwaysRasterize = false;
    textEncoding = new GString ("Latin1");
    textEOL = eolUnix;
    textPageBreaks = true;
    textKeepTinyChars = true;
    initialZoom = new GString ("125");
    continuousView = false;
    enableFreeType = true;
    disableFreeTypeHinting = false;
    antialias = true;
    vectorAntialias = true;
    antialiasPrinting = false;
    strokeAdjust = true;
    screenType = screenUnset;
    screenSize = -1;
    screenDotRadius = -1;
    screenGamma = 1.0;
    screenBlackThreshold = 0.0;
    screenWhiteThreshold = 1.0;
    minLineWidth = 0.0;
    drawAnnotations = true;
    overprintPreview = false;
    launchCommand = NULL;
    urlCommand = NULL;
    movieCommand = NULL;
    mapNumericCharNames = true;
    mapUnknownCharNames = false;
    mapExtTrueTypeFontsViaUnicode = true;
    enableXFA = true;
    createDefaultKeyBindings ();
    printCommands = false;
    errQuiet = false;

    cidToUnicodeCache = new CharCodeToUnicodeCache (cidToUnicodeCacheSize);
    unicodeToUnicodeCache =
        new CharCodeToUnicodeCache (unicodeToUnicodeCacheSize);
    unicodeMapCache = new UnicodeMapCache ();
    cMapCache = new CMapCache ();

    // set up the initial nameToUnicode table
    for (i = 0; nameToUnicodeTab[i].name; ++i) {
        nameToUnicode->add (nameToUnicodeTab[i].name, nameToUnicodeTab[i].u);
    }

    // set up the residentUnicodeMaps table
    map = new UnicodeMap (
        "Latin1", false, latin1UnicodeMapRanges, latin1UnicodeMapLen);
    residentUnicodeMaps->add (map->getEncodingName (), map);
    map = new UnicodeMap (
        "ASCII7", false, ascii7UnicodeMapRanges, ascii7UnicodeMapLen);
    residentUnicodeMaps->add (map->getEncodingName (), map);
    map = new UnicodeMap (
        "Symbol", false, symbolUnicodeMapRanges, symbolUnicodeMapLen);
    residentUnicodeMaps->add (map->getEncodingName (), map);
    map = new UnicodeMap (
        "ZapfDingbats", false, zapfDingbatsUnicodeMapRanges,
        zapfDingbatsUnicodeMapLen);
    residentUnicodeMaps->add (map->getEncodingName (), map);
    map = new UnicodeMap ("UTF-8", true, &mapUTF8);
    residentUnicodeMaps->add (map->getEncodingName (), map);
    map = new UnicodeMap ("UCS-2", true, &mapUCS2);
    residentUnicodeMaps->add (map->getEncodingName (), map);

    // look for a user config file, then a system-wide config file
    f = NULL;
    fileName = NULL;

    if (cfgFileName && cfgFileName[0]) {
        fileName = new GString (cfgFileName);
        if (!(f = fopen (fileName->c_str (), "r"))) { delete fileName; }
    }

    if (!f) {
        fileName = appendToPath (getHomeDir (), XPDF_XPDFRC);

        if (!(f = fopen (fileName->c_str (), "r"))) {
            delete fileName;
        }
    }

    if (!f) {
        fileName = new GString (XPDF_SYSTEM_XPDFRC);

        if (!(f = fopen (fileName->c_str (), "r"))) { delete fileName; }
    }

    if (f) {
        parseFile (fileName, f);
        delete fileName;
        fclose (f);
    }
}

void GlobalParams::createDefaultKeyBindings () {
    keyBindings = new GList ();

    //----- mouse buttons
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMousePress1, xpdfKeyModNone, xpdfKeyContextAny,
        "startSelection"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMouseRelease1, xpdfKeyModNone, xpdfKeyContextAny,
        "endSelection", "followLinkNoSel"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMousePress2, xpdfKeyModNone, xpdfKeyContextAny, "startPan"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMouseRelease2, xpdfKeyModNone, xpdfKeyContextAny, "endPan"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMousePress3, xpdfKeyModNone, xpdfKeyContextAny,
        "postPopupMenu"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMousePress4, xpdfKeyModNone, xpdfKeyContextAny,
        "scrollUpPrevPage(16)"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMousePress5, xpdfKeyModNone, xpdfKeyContextAny,
        "scrollDownNextPage(16)"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMousePress6, xpdfKeyModNone, xpdfKeyContextAny,
        "scrollLeft(16)"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeMousePress7, xpdfKeyModNone, xpdfKeyContextAny,
        "scrollRight(16)"));

    //----- keys
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeHome, xpdfKeyModCtrl, xpdfKeyContextAny, "gotoPage(1)"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeHome, xpdfKeyModNone, xpdfKeyContextAny, "scrollToTopLeft"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeEnd, xpdfKeyModCtrl, xpdfKeyContextAny, "gotoLastPage"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeEnd, xpdfKeyModNone, xpdfKeyContextAny,
        "scrollToBottomRight"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodePgUp, xpdfKeyModNone, xpdfKeyContextAny, "pageUp"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeBackspace, xpdfKeyModNone, xpdfKeyContextAny, "pageUp"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeDelete, xpdfKeyModNone, xpdfKeyContextAny, "pageUp"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodePgDn, xpdfKeyModNone, xpdfKeyContextAny, "pageDown"));
    keyBindings->append (
        new KeyBinding (' ', xpdfKeyModNone, xpdfKeyContextAny, "pageDown"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeLeft, xpdfKeyModNone, xpdfKeyContextAny, "scrollLeft(16)"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeRight, xpdfKeyModNone, xpdfKeyContextAny,
        "scrollRight(16)"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeUp, xpdfKeyModNone, xpdfKeyContextAny, "scrollUp(16)"));
    keyBindings->append (new KeyBinding (
        xpdfKeyCodeDown, xpdfKeyModNone, xpdfKeyContextAny, "scrollDown(16)"));
    keyBindings->append (
        new KeyBinding ('o', xpdfKeyModNone, xpdfKeyContextAny, "open"));
    keyBindings->append (
        new KeyBinding ('O', xpdfKeyModNone, xpdfKeyContextAny, "open"));
    keyBindings->append (
        new KeyBinding ('r', xpdfKeyModNone, xpdfKeyContextAny, "reload"));
    keyBindings->append (
        new KeyBinding ('R', xpdfKeyModNone, xpdfKeyContextAny, "reload"));
    keyBindings->append (
        new KeyBinding ('f', xpdfKeyModNone, xpdfKeyContextAny, "find"));
    keyBindings->append (
        new KeyBinding ('F', xpdfKeyModNone, xpdfKeyContextAny, "find"));
    keyBindings->append (
        new KeyBinding ('f', xpdfKeyModCtrl, xpdfKeyContextAny, "find"));
    keyBindings->append (
        new KeyBinding ('g', xpdfKeyModCtrl, xpdfKeyContextAny, "findNext"));
    keyBindings->append (
        new KeyBinding ('p', xpdfKeyModCtrl, xpdfKeyContextAny, "print"));
    keyBindings->append (new KeyBinding (
        'n', xpdfKeyModNone, xpdfKeyContextScrLockOff, "nextPage"));
    keyBindings->append (new KeyBinding (
        'N', xpdfKeyModNone, xpdfKeyContextScrLockOff, "nextPage"));
    keyBindings->append (new KeyBinding (
        'n', xpdfKeyModNone, xpdfKeyContextScrLockOn, "nextPageNoScroll"));
    keyBindings->append (new KeyBinding (
        'N', xpdfKeyModNone, xpdfKeyContextScrLockOn, "nextPageNoScroll"));
    keyBindings->append (new KeyBinding (
        'p', xpdfKeyModNone, xpdfKeyContextScrLockOff, "prevPage"));
    keyBindings->append (new KeyBinding (
        'P', xpdfKeyModNone, xpdfKeyContextScrLockOff, "prevPage"));
    keyBindings->append (new KeyBinding (
        'p', xpdfKeyModNone, xpdfKeyContextScrLockOn, "prevPageNoScroll"));
    keyBindings->append (new KeyBinding (
        'P', xpdfKeyModNone, xpdfKeyContextScrLockOn, "prevPageNoScroll"));
    keyBindings->append (
        new KeyBinding ('v', xpdfKeyModNone, xpdfKeyContextAny, "goForward"));
    keyBindings->append (
        new KeyBinding ('b', xpdfKeyModNone, xpdfKeyContextAny, "goBackward"));
    keyBindings->append (new KeyBinding (
        'g', xpdfKeyModNone, xpdfKeyContextAny, "focusToPageNum"));
    keyBindings->append (new KeyBinding (
        '0', xpdfKeyModNone, xpdfKeyContextAny, "zoomPercent(125)"));
    keyBindings->append (
        new KeyBinding ('+', xpdfKeyModNone, xpdfKeyContextAny, "zoomIn"));
    keyBindings->append (
        new KeyBinding ('-', xpdfKeyModNone, xpdfKeyContextAny, "zoomOut"));
    keyBindings->append (
        new KeyBinding ('z', xpdfKeyModNone, xpdfKeyContextAny, "zoomFitPage"));
    keyBindings->append (new KeyBinding (
        'w', xpdfKeyModNone, xpdfKeyContextAny, "zoomFitWidth"));
    keyBindings->append (new KeyBinding (
        'f', xpdfKeyModAlt, xpdfKeyContextAny, "toggleFullScreenMode"));
    keyBindings->append (
        new KeyBinding ('l', xpdfKeyModCtrl, xpdfKeyContextAny, "redraw"));
    keyBindings->append (new KeyBinding (
        'w', xpdfKeyModCtrl, xpdfKeyContextAny, "closeWindowOrQuit"));
    keyBindings->append (
        new KeyBinding ('?', xpdfKeyModNone, xpdfKeyContextAny, "about"));
    keyBindings->append (
        new KeyBinding ('q', xpdfKeyModNone, xpdfKeyContextAny, "quit"));
    keyBindings->append (
        new KeyBinding ('Q', xpdfKeyModNone, xpdfKeyContextAny, "quit"));
}

void GlobalParams::parseFile (GString* fileName, FILE* f) {
    int line;
    char buf[512];

    line = 1;
    while (getLine (buf, sizeof (buf) - 1, f)) {
        parseLine (buf, fileName, line);
        ++line;
    }
}

void GlobalParams::parseLine (char* buf, GString* fileName, int line) {
    GList* tokens;
    GString *cmd, *incFile;
    char *p1, *p2;
    FILE* f2;

    // break the line into tokens
    tokens = new GList ();
    p1 = buf;
    while (*p1) {
        for (; *p1 && isspace (*p1); ++p1)
            ;
        if (!*p1) { break; }
        if (*p1 == '"' || *p1 == '\'') {
            for (p2 = p1 + 1; *p2 && *p2 != *p1; ++p2)
                ;
            ++p1;
        }
        else {
            for (p2 = p1 + 1; *p2 && !isspace (*p2); ++p2)
                ;
        }
        tokens->append (new GString (p1, (int)(p2 - p1)));
        p1 = *p2 ? p2 + 1 : p2;
    }

    // parse the line
    if (tokens->getLength () > 0 &&
        ((GString*)tokens->get (0))->front () != '#') {
        cmd = (GString*)tokens->get (0);
        if (!cmd->cmp ("include")) {
            if (tokens->getLength () == 2) {
                incFile = (GString*)tokens->get (1);
                if ((f2 = openFile (incFile->c_str (), "r"))) {
                    parseFile (incFile, f2);
                    fclose (f2);
                }
                else {
                    error (
                        errConfig, -1,
                        "Couldn't find included config file: '{0:t}' "
                        "({1:t}:{2:d})",
                        incFile, fileName, line);
                }
            }
            else {
                error (
                    errConfig, -1,
                    "Bad 'include' config file command ({0:t}:{1:d})", fileName,
                    line);
            }
        }
        else if (!cmd->cmp ("nameToUnicode")) {
            parseNameToUnicode (tokens, fileName, line);
        }
        else if (!cmd->cmp ("cidToUnicode")) {
            parseCIDToUnicode (tokens, fileName, line);
        }
        else if (!cmd->cmp ("unicodeToUnicode")) {
            parseUnicodeToUnicode (tokens, fileName, line);
        }
        else if (!cmd->cmp ("unicodeMap")) {
            parseUnicodeMap (tokens, fileName, line);
        }
        else if (!cmd->cmp ("cMapDir")) {
            parseCMapDir (tokens, fileName, line);
        }
        else if (!cmd->cmp ("toUnicodeDir")) {
            parseToUnicodeDir (tokens, fileName, line);
        }
        else if (!cmd->cmp ("fontFile")) {
            parseFontFile (tokens, fileName, line);
        }
        else if (!cmd->cmp ("fontDir")) {
            parseFontDir (tokens, fileName, line);
        }
        else if (!cmd->cmp ("fontFileCC")) {
            parseFontFileCC (tokens, fileName, line);
        }
        else if (!cmd->cmp ("psFile")) {
            parsePSFile (tokens, fileName, line);
        }
        else if (!cmd->cmp ("psPaperSize")) {
            parsePSPaperSize (tokens, fileName, line);
        }
        else if (!cmd->cmp ("psImageableArea")) {
            parsePSImageableArea (tokens, fileName, line);
        }
        else if (!cmd->cmp ("psCrop")) {
            parseYesNo ("psCrop", &psCrop, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psUseCropBoxAsPage")) {
            parseYesNo (
                "psUseCropBoxAsPage", &psUseCropBoxAsPage, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("psExpandSmaller")) {
            parseYesNo (
                "psExpandSmaller", &psExpandSmaller, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psShrinkLarger")) {
            parseYesNo (
                "psShrinkLarger", &psShrinkLarger, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psCenter")) {
            parseYesNo ("psCenter", &psCenter, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psDuplex")) {
            parseYesNo ("psDuplex", &psDuplex, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psLevel")) {
            parsePSLevel (tokens, fileName, line);
        }
        else if (!cmd->cmp ("psResidentFont")) {
            parsePSResidentFont (tokens, fileName, line);
        }
        else if (!cmd->cmp ("psResidentFont16")) {
            parsePSResidentFont16 (tokens, fileName, line);
        }
        else if (!cmd->cmp ("psResidentFontCC")) {
            parsePSResidentFontCC (tokens, fileName, line);
        }
        else if (!cmd->cmp ("psEmbedType1Fonts")) {
            parseYesNo ("psEmbedType1", &psEmbedType1, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psEmbedTrueTypeFonts")) {
            parseYesNo (
                "psEmbedTrueType", &psEmbedTrueType, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psEmbedCIDPostScriptFonts")) {
            parseYesNo (
                "psEmbedCIDPostScript", &psEmbedCIDPostScript, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("psEmbedCIDTrueTypeFonts")) {
            parseYesNo (
                "psEmbedCIDTrueType", &psEmbedCIDTrueType, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("psFontPassthrough")) {
            parseYesNo (
                "psFontPassthrough", &psFontPassthrough, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("psPreload")) {
            parseYesNo ("psPreload", &psPreload, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psOPI")) {
            parseYesNo ("psOPI", &psOPI, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psASCIIHex")) {
            parseYesNo ("psASCIIHex", &psASCIIHex, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psLZW")) {
            parseYesNo ("psLZW", &psLZW, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psUncompressPreloadedImages")) {
            parseYesNo (
                "psUncompressPreloadedImages", &psUncompressPreloadedImages,
                tokens, fileName, line);
        }
        else if (!cmd->cmp ("psMinLineWidth")) {
            parseFloat (
                "psMinLineWidth", &psMinLineWidth, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psRasterResolution")) {
            parseFloat (
                "psRasterResolution", &psRasterResolution, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("psRasterMono")) {
            parseYesNo ("psRasterMono", &psRasterMono, tokens, fileName, line);
        }
        else if (!cmd->cmp ("psRasterSliceSize")) {
            parseInteger (
                "psRasterSliceSize", &psRasterSliceSize, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("psAlwaysRasterize")) {
            parseYesNo (
                "psAlwaysRasterize", &psAlwaysRasterize, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("textEncoding")) {
            parseTextEncoding (tokens, fileName, line);
        }
        else if (!cmd->cmp ("textEOL")) {
            parseTextEOL (tokens, fileName, line);
        }
        else if (!cmd->cmp ("textPageBreaks")) {
            parseYesNo (
                "textPageBreaks", &textPageBreaks, tokens, fileName, line);
        }
        else if (!cmd->cmp ("textKeepTinyChars")) {
            parseYesNo (
                "textKeepTinyChars", &textKeepTinyChars, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("initialZoom")) {
            parseInitialZoom (tokens, fileName, line);
        }
        else if (!cmd->cmp ("continuousView")) {
            parseYesNo (
                "continuousView", &continuousView, tokens, fileName, line);
        }
        else if (!cmd->cmp ("enableFreeType")) {
            parseYesNo (
                "enableFreeType", &enableFreeType, tokens, fileName, line);
        }
        else if (!cmd->cmp ("disableFreeTypeHinting")) {
            parseYesNo (
                "disableFreeTypeHinting", &disableFreeTypeHinting, tokens,
                fileName, line);
        }
        else if (!cmd->cmp ("antialias")) {
            parseYesNo ("antialias", &antialias, tokens, fileName, line);
        }
        else if (!cmd->cmp ("vectorAntialias")) {
            parseYesNo (
                "vectorAntialias", &vectorAntialias, tokens, fileName, line);
        }
        else if (!cmd->cmp ("antialiasPrinting")) {
            parseYesNo (
                "antialiasPrinting", &antialiasPrinting, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("strokeAdjust")) {
            parseYesNo ("strokeAdjust", &strokeAdjust, tokens, fileName, line);
        }
        else if (!cmd->cmp ("screenType")) {
            parseScreenType (tokens, fileName, line);
        }
        else if (!cmd->cmp ("screenSize")) {
            parseInteger ("screenSize", &screenSize, tokens, fileName, line);
        }
        else if (!cmd->cmp ("screenDotRadius")) {
            parseInteger (
                "screenDotRadius", &screenDotRadius, tokens, fileName, line);
        }
        else if (!cmd->cmp ("screenGamma")) {
            parseFloat ("screenGamma", &screenGamma, tokens, fileName, line);
        }
        else if (!cmd->cmp ("screenBlackThreshold")) {
            parseFloat (
                "screenBlackThreshold", &screenBlackThreshold, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("screenWhiteThreshold")) {
            parseFloat (
                "screenWhiteThreshold", &screenWhiteThreshold, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("minLineWidth")) {
            parseFloat ("minLineWidth", &minLineWidth, tokens, fileName, line);
        }
        else if (!cmd->cmp ("drawAnnotations")) {
            parseYesNo (
                "drawAnnotations", &drawAnnotations, tokens, fileName, line);
        }
        else if (!cmd->cmp ("overprintPreview")) {
            parseYesNo (
                "overprintPreview", &overprintPreview, tokens, fileName, line);
        }
        else if (!cmd->cmp ("launchCommand")) {
            parseCommand (
                "launchCommand", &launchCommand, tokens, fileName, line);
        }
        else if (!cmd->cmp ("urlCommand")) {
            parseCommand ("urlCommand", &urlCommand, tokens, fileName, line);
        }
        else if (!cmd->cmp ("movieCommand")) {
            parseCommand (
                "movieCommand", &movieCommand, tokens, fileName, line);
        }
        else if (!cmd->cmp ("mapNumericCharNames")) {
            parseYesNo (
                "mapNumericCharNames", &mapNumericCharNames, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("mapUnknownCharNames")) {
            parseYesNo (
                "mapUnknownCharNames", &mapUnknownCharNames, tokens, fileName,
                line);
        }
        else if (!cmd->cmp ("mapExtTrueTypeFontsViaUnicode")) {
            parseYesNo (
                "mapExtTrueTypeFontsViaUnicode", &mapExtTrueTypeFontsViaUnicode,
                tokens, fileName, line);
        }
        else if (!cmd->cmp ("enableXFA")) {
            parseYesNo ("enableXFA", &enableXFA, tokens, fileName, line);
        }
        else if (!cmd->cmp ("bind")) {
            parseBind (tokens, fileName, line);
        }
        else if (!cmd->cmp ("unbind")) {
            parseUnbind (tokens, fileName, line);
        }
        else if (!cmd->cmp ("printCommands")) {
            parseYesNo (
                "printCommands", &printCommands, tokens, fileName, line);
        }
        else if (!cmd->cmp ("errQuiet")) {
            parseYesNo ("errQuiet", &errQuiet, tokens, fileName, line);
        }
        else {
            error (
                errConfig, -1,
                "Unknown config file command '{0:t}' ({1:t}:{2:d})", cmd,
                fileName, line);
            if (!cmd->cmp ("displayFontX") ||
                !cmd->cmp ("displayNamedCIDFontX") ||
                !cmd->cmp ("displayCIDFontX")) {
                error (errConfig, -1, "Xpdf no longer supports X fonts");
            }
            else if (!cmd->cmp ("enableT1lib")) {
                error (errConfig, -1, "Xpdf no longer uses t1lib");
            }
            else if (
                !cmd->cmp ("t1libControl") || !cmd->cmp ("freetypeControl")) {
                error (
                    errConfig, -1,
                    "The t1libControl and freetypeControl options have been "
                    "replaced by the enableT1lib, enableFreeType, and "
                    "antialias options");
            }
            else if (!cmd->cmp ("fontpath") || !cmd->cmp ("fontmap")) {
                error (
                    errConfig, -1,
                    "The config file format has changed since Xpdf 0.9x");
            }
        }
    }

    deleteGList (tokens, GString);
}

void GlobalParams::parseNameToUnicode (
    GList* tokens, GString* fileName, int line) {
    GString* name;
    char *tok1, *tok2;
    FILE* f;
    char buf[256];
    int line2;
    Unicode u;

    if (tokens->getLength () != 2) {
        error (
            errConfig, -1,
            "Bad 'nameToUnicode' config file command ({0:t}:{1:d})", fileName,
            line);
        return;
    }
    name = (GString*)tokens->get (1);
    if (!(f = openFile (name->c_str (), "r"))) {
        error (
            errConfig, -1, "Couldn't open 'nameToUnicode' file '{0:t}'", name);
        return;
    }
    line2 = 1;
    while (getLine (buf, sizeof (buf), f)) {
        tok1 = strtok (buf, " \t\r\n");
        tok2 = strtok (NULL, " \t\r\n");
        if (tok1 && tok2) {
            sscanf (tok1, "%x", &u);
            nameToUnicode->add (tok2, u);
        }
        else {
            error (
                errConfig, -1, "Bad line in 'nameToUnicode' file ({0:t}:{1:d})",
                name, line2);
        }
        ++line2;
    }
    fclose (f);
}

void GlobalParams::parseCIDToUnicode (
    GList* tokens, GString* fileName, int line) {
    GString *collection, *name, *old;

    if (tokens->getLength () != 3) {
        error (
            errConfig, -1,
            "Bad 'cidToUnicode' config file command ({0:t}:{1:d})", fileName,
            line);
        return;
    }
    collection = (GString*)tokens->get (1);
    name = (GString*)tokens->get (2);
    if ((old = (GString*)cidToUnicodes->remove (collection))) { delete old; }
    cidToUnicodes->add (collection->copy (), name->copy ());
}

void GlobalParams::parseUnicodeToUnicode (
    GList* tokens, GString* fileName, int line) {
    GString *font, *file, *old;

    if (tokens->getLength () != 3) {
        error (
            errConfig, -1,
            "Bad 'unicodeToUnicode' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    font = (GString*)tokens->get (1);
    file = (GString*)tokens->get (2);
    if ((old = (GString*)unicodeToUnicodes->remove (font))) { delete old; }
    unicodeToUnicodes->add (font->copy (), file->copy ());
}

void GlobalParams::parseUnicodeMap (
    GList* tokens, GString* fileName, int line) {
    GString *encodingName, *name, *old;

    if (tokens->getLength () != 3) {
        error (
            errConfig, -1, "Bad 'unicodeMap' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    encodingName = (GString*)tokens->get (1);
    name = (GString*)tokens->get (2);
    if ((old = (GString*)unicodeMaps->remove (encodingName))) { delete old; }
    unicodeMaps->add (encodingName->copy (), name->copy ());
}

void GlobalParams::parseCMapDir (GList* tokens, GString* fileName, int line) {
    GString *collection, *dir;
    GList* list;

    if (tokens->getLength () != 3) {
        error (
            errConfig, -1, "Bad 'cMapDir' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    collection = (GString*)tokens->get (1);
    dir = (GString*)tokens->get (2);
    if (!(list = (GList*)cMapDirs->lookup (collection))) {
        list = new GList ();
        cMapDirs->add (collection->copy (), list);
    }
    list->append (dir->copy ());
}

void GlobalParams::parseToUnicodeDir (
    GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 2) {
        error (
            errConfig, -1,
            "Bad 'toUnicodeDir' config file command ({0:t}:{1:d})", fileName,
            line);
        return;
    }
    toUnicodeDirs->append (((GString*)tokens->get (1))->copy ());
}

void GlobalParams::parseFontFile (GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 3) {
        error (
            errConfig, -1, "Bad 'fontFile' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    fontFiles->add (
        ((GString*)tokens->get (1))->copy (),
        ((GString*)tokens->get (2))->copy ());
}

void GlobalParams::parseFontDir (GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad 'fontDir' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    fontDirs->append (((GString*)tokens->get (1))->copy ());
}

void GlobalParams::parseFontFileCC (
    GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 3) {
        error (
            errConfig, -1, "Bad 'fontFileCC' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    ccFontFiles->add (
        ((GString*)tokens->get (1))->copy (),
        ((GString*)tokens->get (2))->copy ());
}

void GlobalParams::parsePSFile (GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad 'psFile' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    if (psFile) { delete psFile; }
    psFile = ((GString*)tokens->get (1))->copy ();
}

void GlobalParams::parsePSPaperSize (
    GList* tokens, GString* fileName, int line) {
    GString* tok;

    if (tokens->getLength () == 2) {
        tok = (GString*)tokens->get (1);
        if (!setPSPaperSize (tok->c_str ())) {
            error (
                errConfig, -1,
                "Bad 'psPaperSize' config file command ({0:s}:{1:d})", fileName,
                line);
        }
    }
    else if (tokens->getLength () == 3) {
        tok = (GString*)tokens->get (1);
        psPaperWidth = atoi (tok->c_str ());
        tok = (GString*)tokens->get (2);
        psPaperHeight = atoi (tok->c_str ());
        psImageableLLX = psImageableLLY = 0;
        psImageableURX = psPaperWidth;
        psImageableURY = psPaperHeight;
    }
    else {
        error (
            errConfig, -1,
            "Bad 'psPaperSize' config file command ({0:t}:{1:d})", fileName,
            line);
    }
}

void GlobalParams::parsePSImageableArea (
    GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 5) {
        error (
            errConfig, -1,
            "Bad 'psImageableArea' config file command ({0:t}:{1:d})", fileName,
            line);
        return;
    }
    psImageableLLX = atoi (((GString*)tokens->get (1))->c_str ());
    psImageableLLY = atoi (((GString*)tokens->get (2))->c_str ());
    psImageableURX = atoi (((GString*)tokens->get (3))->c_str ());
    psImageableURY = atoi (((GString*)tokens->get (4))->c_str ());
}

void GlobalParams::parsePSLevel (GList* tokens, GString* fileName, int line) {
    GString* tok;

    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad 'psLevel' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    tok = (GString*)tokens->get (1);
    if (!tok->cmp ("level1")) { psLevel = psLevel1; }
    else if (!tok->cmp ("level1sep")) {
        psLevel = psLevel1Sep;
    }
    else if (!tok->cmp ("level2")) {
        psLevel = psLevel2;
    }
    else if (!tok->cmp ("level2sep")) {
        psLevel = psLevel2Sep;
    }
    else if (!tok->cmp ("level3")) {
        psLevel = psLevel3;
    }
    else if (!tok->cmp ("level3Sep")) {
        psLevel = psLevel3Sep;
    }
    else {
        error (
            errConfig, -1, "Bad 'psLevel' config file command ({0:t}:{1:d})",
            fileName, line);
    }
}

void GlobalParams::parsePSResidentFont (
    GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 3) {
        error (
            errConfig, -1,
            "Bad 'psResidentFont' config file command ({0:t}:{1:d})", fileName,
            line);
        return;
    }
    psResidentFonts->add (
        ((GString*)tokens->get (1))->copy (),
        ((GString*)tokens->get (2))->copy ());
}

void GlobalParams::parsePSResidentFont16 (
    GList* tokens, GString* fileName, int line) {
    PSFontParam16* param;
    int wMode;
    GString* tok;

    if (tokens->getLength () != 5) {
        error (
            errConfig, -1,
            "Bad 'psResidentFont16' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    tok = (GString*)tokens->get (2);
    if (!tok->cmp ("H")) { wMode = 0; }
    else if (!tok->cmp ("V")) {
        wMode = 1;
    }
    else {
        error (
            errConfig, -1,
            "Bad wMode in psResidentFont16 config file command ({1:t}:{2:d})",
            fileName, line);
        return;
    }
    param = new PSFontParam16 (
        ((GString*)tokens->get (1))->copy (), wMode,
        ((GString*)tokens->get (3))->copy (),
        ((GString*)tokens->get (4))->copy ());
    psResidentFonts16->append (param);
}

void GlobalParams::parsePSResidentFontCC (
    GList* tokens, GString* fileName, int line) {
    PSFontParam16* param;
    int wMode;
    GString* tok;

    if (tokens->getLength () != 5) {
        error (
            errConfig, -1,
            "Bad 'psResidentFontCC' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    tok = (GString*)tokens->get (2);
    if (!tok->cmp ("H")) { wMode = 0; }
    else if (!tok->cmp ("V")) {
        wMode = 1;
    }
    else {
        error (
            errConfig, -1,
            "Bad wMode in psResidentFontCC config file command ({1:t}:{2:d})",
            fileName, line);
        return;
    }
    param = new PSFontParam16 (
        ((GString*)tokens->get (1))->copy (), wMode,
        ((GString*)tokens->get (3))->copy (),
        ((GString*)tokens->get (4))->copy ());
    psResidentFontsCC->append (param);
}

void GlobalParams::parseTextEncoding (
    GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 2) {
        error (
            errConfig, -1,
            "Bad 'textEncoding' config file command ({0:s}:{1:d})", fileName,
            line);
        return;
    }
    delete textEncoding;
    textEncoding = ((GString*)tokens->get (1))->copy ();
}

void GlobalParams::parseTextEOL (GList* tokens, GString* fileName, int line) {
    GString* tok;

    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad 'textEOL' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    tok = (GString*)tokens->get (1);
    if (!tok->cmp ("unix")) { textEOL = eolUnix; }
    else if (!tok->cmp ("dos")) {
        textEOL = eolDOS;
    }
    else if (!tok->cmp ("mac")) {
        textEOL = eolMac;
    }
    else {
        error (
            errConfig, -1, "Bad 'textEOL' config file command ({0:t}:{1:d})",
            fileName, line);
    }
}

void GlobalParams::parseInitialZoom (
    GList* tokens, GString* fileName, int line) {
    if (tokens->getLength () != 2) {
        error (
            errConfig, -1,
            "Bad 'initialZoom' config file command ({0:t}:{1:d})", fileName,
            line);
        return;
    }
    delete initialZoom;
    initialZoom = ((GString*)tokens->get (1))->copy ();
}

void GlobalParams::parseScreenType (
    GList* tokens, GString* fileName, int line) {
    GString* tok;

    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad 'screenType' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    tok = (GString*)tokens->get (1);
    if (!tok->cmp ("dispersed")) { screenType = screenDispersed; }
    else if (!tok->cmp ("clustered")) {
        screenType = screenClustered;
    }
    else if (!tok->cmp ("stochasticClustered")) {
        screenType = screenStochasticClustered;
    }
    else {
        error (
            errConfig, -1, "Bad 'screenType' config file command ({0:t}:{1:d})",
            fileName, line);
    }
}

void GlobalParams::parseBind (GList* tokens, GString* fileName, int line) {
    KeyBinding* binding;
    GList* cmds;
    int code, mods, context, i;

    if (tokens->getLength () < 4) {
        error (
            errConfig, -1, "Bad 'bind' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    if (!parseKey (
            (GString*)tokens->get (1), (GString*)tokens->get (2), &code, &mods,
            &context, "bind", tokens, fileName, line)) {
        return;
    }
    for (i = 0; i < keyBindings->getLength (); ++i) {
        binding = (KeyBinding*)keyBindings->get (i);
        if (binding->code == code && binding->mods == mods &&
            binding->context == context) {
            delete (KeyBinding*)keyBindings->del (i);
            break;
        }
    }
    cmds = new GList ();
    for (i = 3; i < tokens->getLength (); ++i) {
        cmds->append (((GString*)tokens->get (i))->copy ());
    }
    keyBindings->append (new KeyBinding (code, mods, context, cmds));
}

void GlobalParams::parseUnbind (GList* tokens, GString* fileName, int line) {
    KeyBinding* binding;
    int code, mods, context, i;

    if (tokens->getLength () != 3) {
        error (
            errConfig, -1, "Bad 'unbind' config file command ({0:t}:{1:d})",
            fileName, line);
        return;
    }
    if (!parseKey (
            (GString*)tokens->get (1), (GString*)tokens->get (2), &code, &mods,
            &context, "unbind", tokens, fileName, line)) {
        return;
    }
    for (i = 0; i < keyBindings->getLength (); ++i) {
        binding = (KeyBinding*)keyBindings->get (i);
        if (binding->code == code && binding->mods == mods &&
            binding->context == context) {
            delete (KeyBinding*)keyBindings->del (i);
            break;
        }
    }
}

bool GlobalParams::parseKey (
    GString* modKeyStr, GString* contextStr, int* code, int* mods, int* context,
    const char* cmdName, GList* tokens, GString* fileName, int line) {
    int btn;

    *mods = xpdfKeyModNone;
    const char* p0 = modKeyStr->c_str ();

    while (1) {
        if (!strncmp (p0, "shift-", 6)) {
            *mods |= xpdfKeyModShift;
            p0 += 6;
        }
        else if (!strncmp (p0, "ctrl-", 5)) {
            *mods |= xpdfKeyModCtrl;
            p0 += 5;
        }
        else if (!strncmp (p0, "alt-", 4)) {
            *mods |= xpdfKeyModAlt;
            p0 += 4;
        }
        else {
            break;
        }
    }

    if (!strcmp (p0, "space")) { *code = ' '; }
    else if (!strcmp (p0, "tab")) {
        *code = xpdfKeyCodeTab;
    }
    else if (!strcmp (p0, "return")) {
        *code = xpdfKeyCodeReturn;
    }
    else if (!strcmp (p0, "enter")) {
        *code = xpdfKeyCodeEnter;
    }
    else if (!strcmp (p0, "backspace")) {
        *code = xpdfKeyCodeBackspace;
    }
    else if (!strcmp (p0, "insert")) {
        *code = xpdfKeyCodeInsert;
    }
    else if (!strcmp (p0, "delete")) {
        *code = xpdfKeyCodeDelete;
    }
    else if (!strcmp (p0, "home")) {
        *code = xpdfKeyCodeHome;
    }
    else if (!strcmp (p0, "end")) {
        *code = xpdfKeyCodeEnd;
    }
    else if (!strcmp (p0, "pgup")) {
        *code = xpdfKeyCodePgUp;
    }
    else if (!strcmp (p0, "pgdn")) {
        *code = xpdfKeyCodePgDn;
    }
    else if (!strcmp (p0, "left")) {
        *code = xpdfKeyCodeLeft;
    }
    else if (!strcmp (p0, "right")) {
        *code = xpdfKeyCodeRight;
    }
    else if (!strcmp (p0, "up")) {
        *code = xpdfKeyCodeUp;
    }
    else if (!strcmp (p0, "down")) {
        *code = xpdfKeyCodeDown;
    }
    else if (p0[0] == 'f' && p0[1] >= '1' && p0[1] <= '9' && !p0[2]) {
        *code = xpdfKeyCodeF1 + (p0[1] - '1');
    }
    else if (
        p0[0] == 'f' &&
        ((p0[1] >= '1' && p0[1] <= '2' && p0[2] >= '0' && p0[2] <= '9') ||
         (p0[1] == '3' && p0[2] >= '0' && p0[2] <= '5')) &&
        !p0[3]) {
        *code = xpdfKeyCodeF1 + 10 * (p0[1] - '0') + (p0[2] - '0') - 1;
    }
    else if (
        !strncmp (p0, "mousePress", 10) && p0[10] >= '0' && p0[10] <= '9' &&
        (!p0[11] || (p0[11] >= '0' && p0[11] <= '9' && !p0[12])) &&
        (btn = atoi (p0 + 10)) >= 1 && btn <= 32) {
        *code = xpdfKeyCodeMousePress1 + btn - 1;
    }
    else if (
        !strncmp (p0, "mouseRelease", 12) && p0[12] >= '0' && p0[12] <= '9' &&
        (!p0[13] || (p0[13] >= '0' && p0[13] <= '9' && !p0[14])) &&
        (btn = atoi (p0 + 12)) >= 1 && btn <= 32) {
        *code = xpdfKeyCodeMouseRelease1 + btn - 1;
    }
    else if (*p0 >= 0x20 && *p0 <= 0x7e && !p0[1]) {
        *code = (int)*p0;
    }
    else {
        error (
            errConfig, -1,
            "Bad key/modifier in '{0:s}' config file command ({1:t}:{2:d})",
            cmdName, fileName, line);
        return false;
    }

    p0 = contextStr->c_str ();
    if (!strcmp (p0, "any")) { *context = xpdfKeyContextAny; }
    else {
        *context = xpdfKeyContextAny;
        while (1) {
            if (!strncmp (p0, "fullScreen", 10)) {
                *context |= xpdfKeyContextFullScreen;
                p0 += 10;
            }
            else if (!strncmp (p0, "window", 6)) {
                *context |= xpdfKeyContextWindow;
                p0 += 6;
            }
            else if (!strncmp (p0, "continuous", 10)) {
                *context |= xpdfKeyContextContinuous;
                p0 += 10;
            }
            else if (!strncmp (p0, "singlePage", 10)) {
                *context |= xpdfKeyContextSinglePage;
                p0 += 10;
            }
            else if (!strncmp (p0, "overLink", 8)) {
                *context |= xpdfKeyContextOverLink;
                p0 += 8;
            }
            else if (!strncmp (p0, "offLink", 7)) {
                *context |= xpdfKeyContextOffLink;
                p0 += 7;
            }
            else if (!strncmp (p0, "outline", 7)) {
                *context |= xpdfKeyContextOutline;
                p0 += 7;
            }
            else if (!strncmp (p0, "mainWin", 7)) {
                *context |= xpdfKeyContextMainWin;
                p0 += 7;
            }
            else if (!strncmp (p0, "scrLockOn", 9)) {
                *context |= xpdfKeyContextScrLockOn;
                p0 += 9;
            }
            else if (!strncmp (p0, "scrLockOff", 10)) {
                *context |= xpdfKeyContextScrLockOff;
                p0 += 10;
            }
            else {
                error (
                    errConfig, -1,
                    "Bad context in '{0:s}' config file command ({1:t}:{2:d})",
                    cmdName, fileName, line);
                return false;
            }
            if (!*p0) { break; }
            if (*p0 != ',') {
                error (
                    errConfig, -1,
                    "Bad context in '{0:s}' config file command ({1:t}:{2:d})",
                    cmdName, fileName, line);
                return false;
            }
            ++p0;
        }
    }

    return true;
}

void GlobalParams::parseCommand (
    const char* cmdName, GString** val, GList* tokens, GString* fileName,
    int line) {
    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
            cmdName, fileName, line);
        return;
    }
    if (*val) { delete *val; }
    *val = ((GString*)tokens->get (1))->copy ();
}

void GlobalParams::parseYesNo (
    const char* cmdName, bool* flag, GList* tokens, GString* fileName,
    int line) {
    GString* tok;

    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
            cmdName, fileName, line);
        return;
    }
    tok = (GString*)tokens->get (1);
    if (!parseYesNo2 (tok->c_str (), flag)) {
        error (
            errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
            cmdName, fileName, line);
    }
}

bool GlobalParams::parseYesNo2 (const char* token, bool* flag) {
    if (!strcmp (token, "yes")) { *flag = true; }
    else if (!strcmp (token, "no")) {
        *flag = false;
    }
    else {
        return false;
    }
    return true;
}

void GlobalParams::parseInteger (
    const char* cmdName, int* val, GList* tokens, GString* fileName, int line) {
    GString* tok;
    int i;

    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
            cmdName, fileName, line);
        return;
    }
    tok = (GString*)tokens->get (1);
    if (tok->getLength () == 0) {
        error (
            errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
            cmdName, fileName, line);
        return;
    }
    if (tok->front () == '-') { i = 1; }
    else {
        i = 0;
    }
    for (; i < tok->getLength (); ++i) {
        if ((*tok) [i] < '0' || (*tok) [i] > '9') {
            error (
                errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
                cmdName, fileName, line);
            return;
        }
    }
    *val = atoi (tok->c_str ());
}

void GlobalParams::parseFloat (
    const char* cmdName, double* val, GList* tokens, GString* fileName,
    int line) {
    GString* tok;
    int i;

    if (tokens->getLength () != 2) {
        error (
            errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
            cmdName, fileName, line);
        return;
    }
    tok = (GString*)tokens->get (1);
    if (tok->getLength () == 0) {
        error (
            errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
            cmdName, fileName, line);
        return;
    }
    if (tok->front () == '-') { i = 1; }
    else {
        i = 0;
    }
    for (; i < tok->getLength (); ++i) {
        if (!(((*tok) [i] >= '0' && (*tok) [i] <= '9') ||
              (*tok) [i] == '.')) {
            error (
                errConfig, -1, "Bad '{0:s}' config file command ({1:t}:{2:d})",
                cmdName, fileName, line);
            return;
        }
    }
    *val = atof (tok->c_str ());
}

GlobalParams::~GlobalParams () {
    GHashIter* iter;
    GString* key;
    GList* list;

    freeBuiltinFontTables ();

    delete macRomanReverseMap;

    delete baseDir;
    delete nameToUnicode;
    deleteGHash (cidToUnicodes, GString);
    deleteGHash (unicodeToUnicodes, GString);
    deleteGHash (residentUnicodeMaps, UnicodeMap);
    deleteGHash (unicodeMaps, GString);
    deleteGList (toUnicodeDirs, GString);
    deleteGHash (fontFiles, GString);
    deleteGList (fontDirs, GString);
    deleteGHash (ccFontFiles, GString);
    deleteGHash (base14SysFonts, Base14FontInfo);
    delete sysFonts;
    if (psFile) { delete psFile; }
    deleteGHash (psResidentFonts, GString);
    deleteGList (psResidentFonts16, PSFontParam16);
    deleteGList (psResidentFontsCC, PSFontParam16);
    delete textEncoding;
    delete initialZoom;
    if (launchCommand) { delete launchCommand; }
    if (urlCommand) { delete urlCommand; }
    if (movieCommand) { delete movieCommand; }
    deleteGList (keyBindings, KeyBinding);

    cMapDirs->startIter (&iter);
    while (cMapDirs->getNext (&iter, &key, (void**)&list)) {
        deleteGList (list, GString);
    }
    delete cMapDirs;

    delete cidToUnicodeCache;
    delete unicodeToUnicodeCache;
    delete unicodeMapCache;
    delete cMapCache;
}

//------------------------------------------------------------------------

void GlobalParams::setBaseDir (char* dir) {
    delete baseDir;
    baseDir = new GString (dir);
}

void GlobalParams::setupBaseFonts (char* dir) {
    GString* fontName;
    GString* fileName;
    int fontNum;
    const char* s;
    Base14FontInfo* base14;
    FILE* f;
    int i, j;

    for (i = 0; displayFontTab[i].name; ++i) {
        if (fontFiles->lookup (displayFontTab[i].name)) { continue; }
        fontName = new GString (displayFontTab[i].name);
        fileName = NULL;
        fontNum = 0;
        if (dir) {
            fileName =
                appendToPath (new GString (dir), displayFontTab[i].t1FileName);
            if ((f = fopen (fileName->c_str (), "rb"))) { fclose (f); }
            else {
                delete fileName;
                fileName = NULL;
            }
        }
        // On Linux, this checks the "standard" ghostscript font
        // directories.  On Windows, it checks the "standard" system font
        // directories (because SHGetSpecialFolderPath(CSIDL_FONTS)
        // doesn't work on Win 2k Server or Win2003 Server, or with older
        // versions of shell32.dll).
        s = displayFontTab[i].t1FileName;
        if (!fileName && s) {
            for (j = 0; !fileName && displayFontDirs[j]; ++j) {
                fileName = appendToPath (new GString (displayFontDirs[j]), s);
                if ((f = fopen (fileName->c_str (), "rb"))) { fclose (f); }
                else {
                    delete fileName;
                    fileName = NULL;
                }
            }
        }
        if (!fileName) {
            delete fontName;
            continue;
        }
        base14SysFonts->add (
            fontName, new Base14FontInfo (fileName, fontNum, 0));
    }
    for (i = 0; displayFontTab[i].name; ++i) {
        if (!base14SysFonts->lookup (displayFontTab[i].name) &&
            !fontFiles->lookup (displayFontTab[i].name)) {
            if (displayFontTab[i].obliqueFont &&
                ((base14 = (Base14FontInfo*)base14SysFonts->lookup (
                      displayFontTab[i].obliqueFont)))) {
                base14SysFonts->add (
                    new GString (displayFontTab[i].name),
                    new Base14FontInfo (
                        base14->fileName->copy (), base14->fontNum,
                        displayFontTab[i].obliqueFactor));
            }
            else {
                error (
                    errConfig, -1, "No display font for '{0:s}'",
                    displayFontTab[i].name);
            }
        }
    }
}

//------------------------------------------------------------------------
// accessors
//------------------------------------------------------------------------

CharCode GlobalParams::getMacRomanCharCode (char* charName) {
    // no need to lock - macRomanReverseMap is constant
    return macRomanReverseMap->lookup (charName);
}

GString* GlobalParams::getBaseDir () {
    GString* s;

    s = baseDir->copy ();
    return s;
}

Unicode GlobalParams::mapNameToUnicode (const char* charName) {
    // no need to lock - nameToUnicode is constant
    return nameToUnicode->lookup (charName);
}

UnicodeMap* GlobalParams::getResidentUnicodeMap (GString* encodingName) {
    UnicodeMap* map;

    map = (UnicodeMap*)residentUnicodeMaps->lookup (encodingName);
    if (map) { map->incRefCnt (); }
    return map;
}

FILE* GlobalParams::getUnicodeMapFile (GString* encodingName) {
    GString* fileName;
    FILE* f;

    if ((fileName = (GString*)unicodeMaps->lookup (encodingName))) {
        f = openFile (fileName->c_str (), "r");
    }
    else {
        f = NULL;
    }
    return f;
}

FILE* GlobalParams::findCMapFile (GString* collection, GString* cMapName) {
    GList* list;
    GString* dir;
    GString* fileName;
    FILE* f;
    int i;

    if (!(list = (GList*)cMapDirs->lookup (collection))) { return NULL; }
    for (i = 0; i < list->getLength (); ++i) {
        dir = (GString*)list->get (i);
        fileName = appendToPath (dir->copy (), cMapName->c_str ());
        f = openFile (fileName->c_str (), "r");
        delete fileName;
        if (f) { return f; }
    }
    return NULL;
}

FILE* GlobalParams::findToUnicodeFile (GString* name) {
    GString *dir, *fileName;
    FILE* f;
    int i;

    for (i = 0; i < toUnicodeDirs->getLength (); ++i) {
        dir = (GString*)toUnicodeDirs->get (i);
        fileName = appendToPath (dir->copy (), name->c_str ());
        f = openFile (fileName->c_str (), "r");
        delete fileName;
        if (f) { return f; }
    }
    return NULL;
}

GString* GlobalParams::findFontFile (GString* fontName) {
    static const char* exts[] = { ".pfa", ".pfb", ".ttf", ".ttc" };
    GString *path, *dir;
    const char* ext;
    FILE* f;
    int i, j;

    if ((path = (GString*)fontFiles->lookup (fontName))) {
        path = path->copy ();
        return path;
    }
    for (i = 0; i < fontDirs->getLength (); ++i) {
        dir = (GString*)fontDirs->get (i);
        for (j = 0; j < (int)(sizeof (exts) / sizeof (exts[0])); ++j) {
            ext = exts[j];
            path = appendToPath (dir->copy (), fontName->c_str ());
            path->append (ext);
            if ((f = openFile (path->c_str (), "rb"))) {
                fclose (f);
                return path;
            }
            delete path;
        }
    }
    return NULL;
}

GString* GlobalParams::findBase14FontFile (
    GString* fontName, int* fontNum, double* oblique) {
    Base14FontInfo* fi;
    GString* path;

    if ((fi = (Base14FontInfo*)base14SysFonts->lookup (fontName))) {
        path = fi->fileName->copy ();
        *fontNum = fi->fontNum;
        *oblique = fi->oblique;
        return path;
    }
    *fontNum = 0;
    *oblique = 0;
    return findFontFile (fontName);
}

GString* GlobalParams::findSystemFontFile (
    GString* fontName, SysFontType* type, int* fontNum) {
    SysFontInfo* fi;
    GString* path;

    path = NULL;
    if ((fi = sysFonts->find (fontName))) {
        path = fi->path->copy ();
        *type = fi->type;
        *fontNum = fi->fontNum;
    }
    return path;
}

GString* GlobalParams::findCCFontFile (GString* collection) {
    GString* path;

    if ((path = (GString*)ccFontFiles->lookup (collection))) {
        path = path->copy ();
    }
    return path;
}

GString* GlobalParams::getPSFile () {
    GString* s;

    s = psFile ? psFile->copy () : (GString*)NULL;
    return s;
}

int GlobalParams::getPSPaperWidth () {
    int w;

    w = psPaperWidth;
    return w;
}

int GlobalParams::getPSPaperHeight () {
    int h;

    h = psPaperHeight;
    return h;
}

void GlobalParams::getPSImageableArea (int* llx, int* lly, int* urx, int* ury) {
    *llx = psImageableLLX;
    *lly = psImageableLLY;
    *urx = psImageableURX;
    *ury = psImageableURY;
}

bool GlobalParams::getPSCrop () {
    bool f;

    f = psCrop;
    return f;
}

bool GlobalParams::getPSUseCropBoxAsPage () {
    bool f;

    f = psUseCropBoxAsPage;
    return f;
}

bool GlobalParams::getPSExpandSmaller () {
    bool f;

    f = psExpandSmaller;
    return f;
}

bool GlobalParams::getPSShrinkLarger () {
    bool f;

    f = psShrinkLarger;
    return f;
}

bool GlobalParams::getPSCenter () {
    bool f;

    f = psCenter;
    return f;
}

bool GlobalParams::getPSDuplex () {
    bool d;

    d = psDuplex;
    return d;
}

PSLevel GlobalParams::getPSLevel () {
    PSLevel level;

    level = psLevel;
    return level;
}

GString* GlobalParams::getPSResidentFont (GString* fontName) {
    GString* psName;

    if ((psName = (GString*)psResidentFonts->lookup (fontName))) {
        psName = psName->copy ();
    }
    return psName;
}

GList* GlobalParams::getPSResidentFonts () {
    GList* names;
    GHashIter* iter;
    GString* name;
    GString* psName;

    names = new GList ();
    psResidentFonts->startIter (&iter);
    while (psResidentFonts->getNext (&iter, &name, (void**)&psName)) {
        names->append (psName->copy ());
    }
    return names;
}

PSFontParam16*
GlobalParams::getPSResidentFont16 (GString* fontName, int wMode) {
    PSFontParam16* p;
    int i;

    p = NULL;
    for (i = 0; i < psResidentFonts16->getLength (); ++i) {
        p = (PSFontParam16*)psResidentFonts16->get (i);
        if (!(p->name->cmp (fontName)) && p->wMode == wMode) { break; }
        p = NULL;
    }
    return p;
}

PSFontParam16*
GlobalParams::getPSResidentFontCC (GString* collection, int wMode) {
    PSFontParam16* p;
    int i;

    p = NULL;
    for (i = 0; i < psResidentFontsCC->getLength (); ++i) {
        p = (PSFontParam16*)psResidentFontsCC->get (i);
        if (!(p->name->cmp (collection)) && p->wMode == wMode) { break; }
        p = NULL;
    }
    return p;
}

bool GlobalParams::getPSEmbedType1 () {
    bool e;

    e = psEmbedType1;
    return e;
}

bool GlobalParams::getPSEmbedTrueType () {
    bool e;

    e = psEmbedTrueType;
    return e;
}

bool GlobalParams::getPSEmbedCIDPostScript () {
    bool e;

    e = psEmbedCIDPostScript;
    return e;
}

bool GlobalParams::getPSEmbedCIDTrueType () {
    bool e;

    e = psEmbedCIDTrueType;
    return e;
}

bool GlobalParams::getPSFontPassthrough () {
    bool e;

    e = psFontPassthrough;
    return e;
}

bool GlobalParams::getPSPreload () {
    bool preload;

    preload = psPreload;
    return preload;
}

bool GlobalParams::getPSOPI () {
    bool opi;

    opi = psOPI;
    return opi;
}

bool GlobalParams::getPSASCIIHex () {
    bool ah;

    ah = psASCIIHex;
    return ah;
}

bool GlobalParams::getPSLZW () {
    bool ah;

    ah = psLZW;
    return ah;
}

bool GlobalParams::getPSUncompressPreloadedImages () {
    bool ah;

    ah = psUncompressPreloadedImages;
    return ah;
}

double GlobalParams::getPSMinLineWidth () {
    double w;

    w = psMinLineWidth;
    return w;
}

double GlobalParams::getPSRasterResolution () {
    double res;

    res = psRasterResolution;
    return res;
}

bool GlobalParams::getPSRasterMono () {
    bool mono;

    mono = psRasterMono;
    return mono;
}

int GlobalParams::getPSRasterSliceSize () {
    int slice;

    slice = psRasterSliceSize;
    return slice;
}

bool GlobalParams::getPSAlwaysRasterize () {
    bool rast;

    rast = psAlwaysRasterize;
    return rast;
}

GString* GlobalParams::getTextEncodingName () {
    GString* s;

    s = textEncoding->copy ();
    return s;
}

EndOfLineKind GlobalParams::getTextEOL () {
    EndOfLineKind eol;

    eol = textEOL;
    return eol;
}

bool GlobalParams::getTextPageBreaks () {
    bool pageBreaks;

    pageBreaks = textPageBreaks;
    return pageBreaks;
}

bool GlobalParams::getTextKeepTinyChars () {
    bool tiny;

    tiny = textKeepTinyChars;
    return tiny;
}

GString* GlobalParams::getInitialZoom () {
    GString* s;

    s = initialZoom->copy ();
    return s;
}

bool GlobalParams::getContinuousView () {
    bool f;

    f = continuousView;
    return f;
}

bool GlobalParams::getEnableFreeType () {
    bool f;

    f = enableFreeType;
    return f;
}

bool GlobalParams::getDisableFreeTypeHinting () {
    bool f;

    f = disableFreeTypeHinting;
    return f;
}

bool GlobalParams::getAntialias () {
    bool f;

    f = antialias;
    return f;
}

bool GlobalParams::getVectorAntialias () {
    bool f;

    f = vectorAntialias;
    return f;
}

bool GlobalParams::getAntialiasPrinting () {
    bool f;

    f = antialiasPrinting;
    return f;
}

bool GlobalParams::getStrokeAdjust () {
    bool f;

    f = strokeAdjust;
    return f;
}

ScreenType GlobalParams::getScreenType () {
    ScreenType t;

    t = screenType;
    return t;
}

int GlobalParams::getScreenSize () {
    int size;

    size = screenSize;
    return size;
}

int GlobalParams::getScreenDotRadius () {
    int r;

    r = screenDotRadius;
    return r;
}

double GlobalParams::getScreenGamma () {
    double gamma;

    gamma = screenGamma;
    return gamma;
}

double GlobalParams::getScreenBlackThreshold () {
    double thresh;

    thresh = screenBlackThreshold;
    return thresh;
}

double GlobalParams::getScreenWhiteThreshold () {
    double thresh;

    thresh = screenWhiteThreshold;
    return thresh;
}

double GlobalParams::getMinLineWidth () {
    double w;

    w = minLineWidth;
    return w;
}

bool GlobalParams::getDrawAnnotations () {
    bool draw;

    draw = drawAnnotations;
    return draw;
}

bool GlobalParams::getMapNumericCharNames () {
    bool map;

    map = mapNumericCharNames;
    return map;
}

bool GlobalParams::getMapUnknownCharNames () {
    bool map;

    map = mapUnknownCharNames;
    return map;
}

bool GlobalParams::getMapExtTrueTypeFontsViaUnicode () {
    bool map;

    map = mapExtTrueTypeFontsViaUnicode;
    return map;
}

bool GlobalParams::getEnableXFA () {
    bool enable;

    enable = enableXFA;
    return enable;
}

GList* GlobalParams::getKeyBinding (int code, int mods, int context) {
    KeyBinding* binding;
    GList* cmds;
    int modMask;
    int i, j;

    cmds = NULL;
    // for ASCII chars, ignore the shift modifier
    modMask = code <= 0xff ? ~xpdfKeyModShift : ~0;
    for (i = 0; i < keyBindings->getLength (); ++i) {
        binding = (KeyBinding*)keyBindings->get (i);
        if (binding->code == code &&
            (binding->mods & modMask) == (mods & modMask) &&
            (~binding->context | context) == ~0) {
            cmds = new GList ();
            for (j = 0; j < binding->cmds->getLength (); ++j) {
                cmds->append (((GString*)binding->cmds->get (j))->copy ());
            }
            break;
        }
    }
    return cmds;
}

bool GlobalParams::getPrintCommands () {
    bool p;

    p = printCommands;
    return p;
}

bool GlobalParams::getErrQuiet () {
    // no locking -- this function may get called from inside a locked
    // section
    return errQuiet;
}

CharCodeToUnicode* GlobalParams::getCIDToUnicode (GString* collection) {
    GString* fileName;
    CharCodeToUnicode* ctu;

    if (!(ctu = cidToUnicodeCache->getCharCodeToUnicode (collection))) {
        if ((fileName = (GString*)cidToUnicodes->lookup (collection)) &&
            (ctu =
                 CharCodeToUnicode::parseCIDToUnicode (fileName, collection))) {
            cidToUnicodeCache->add (ctu);
        }
    }
    return ctu;
}

CharCodeToUnicode* GlobalParams::getUnicodeToUnicode (GString* fontName) {
    CharCodeToUnicode* ctu;
    GHashIter* iter;
    GString *fontPattern, *fileName;

    fileName = NULL;
    unicodeToUnicodes->startIter (&iter);
    while (
        unicodeToUnicodes->getNext (&iter, &fontPattern, (void**)&fileName)) {
        if (strstr (fontName->c_str (), fontPattern->c_str ())) {
            unicodeToUnicodes->killIter (&iter);
            break;
        }
        fileName = NULL;
    }
    if (fileName) {
        if (!(ctu = unicodeToUnicodeCache->getCharCodeToUnicode (fileName))) {
            if ((ctu = CharCodeToUnicode::parseUnicodeToUnicode (fileName))) {
                unicodeToUnicodeCache->add (ctu);
            }
        }
    }
    else {
        ctu = NULL;
    }
    return ctu;
}

UnicodeMap* GlobalParams::getUnicodeMap (GString* encodingName) {
    return getUnicodeMap2 (encodingName);
}

UnicodeMap* GlobalParams::getUnicodeMap2 (GString* encodingName) {
    UnicodeMap* map;

    if (!(map = getResidentUnicodeMap (encodingName))) {
        map = unicodeMapCache->getUnicodeMap (encodingName);
    }
    return map;
}

CMap* GlobalParams::getCMap (GString* collection, GString* cMapName) {
    CMap* cMap;

    cMap = cMapCache->getCMap (collection, cMapName);
    return cMap;
}

UnicodeMap* GlobalParams::getTextEncoding () {
    return getUnicodeMap2 (textEncoding);
}

//------------------------------------------------------------------------
// functions to set parameters
//------------------------------------------------------------------------

void GlobalParams::addFontFile (GString* fontName, GString* path) {
    fontFiles->add (fontName, path);
}

void GlobalParams::setPSFile (char* file) {
    if (psFile) { delete psFile; }
    psFile = new GString (file);
}

bool GlobalParams::setPSPaperSize (const char* size) {
    if (!strcmp (size, "match")) { psPaperWidth = psPaperHeight = -1; }
    else if (!strcmp (size, "letter")) {
        psPaperWidth = 612;
        psPaperHeight = 792;
    }
    else if (!strcmp (size, "legal")) {
        psPaperWidth = 612;
        psPaperHeight = 1008;
    }
    else if (!strcmp (size, "A4")) {
        psPaperWidth = 595;
        psPaperHeight = 842;
    }
    else if (!strcmp (size, "A3")) {
        psPaperWidth = 842;
        psPaperHeight = 1190;
    }
    else {
        return false;
    }
    psImageableLLX = psImageableLLY = 0;
    psImageableURX = psPaperWidth;
    psImageableURY = psPaperHeight;
    return true;
}

void GlobalParams::setPSPaperWidth (int width) {
    psPaperWidth = width;
    psImageableLLX = 0;
    psImageableURX = psPaperWidth;
}

void GlobalParams::setPSPaperHeight (int height) {
    psPaperHeight = height;
    psImageableLLY = 0;
    psImageableURY = psPaperHeight;
}

void GlobalParams::setPSImageableArea (int llx, int lly, int urx, int ury) {
    psImageableLLX = llx;
    psImageableLLY = lly;
    psImageableURX = urx;
    psImageableURY = ury;
}

void GlobalParams::setPSCrop (bool crop) { psCrop = crop; }

void GlobalParams::setPSUseCropBoxAsPage (bool crop) {
    psUseCropBoxAsPage = crop;
}

void GlobalParams::setPSExpandSmaller (bool expand) {
    psExpandSmaller = expand;
}

void GlobalParams::setPSShrinkLarger (bool shrink) { psShrinkLarger = shrink; }

void GlobalParams::setPSCenter (bool center) { psCenter = center; }

void GlobalParams::setPSDuplex (bool duplex) { psDuplex = duplex; }

void GlobalParams::setPSLevel (PSLevel level) { psLevel = level; }

void GlobalParams::setPSEmbedType1 (bool embed) { psEmbedType1 = embed; }

void GlobalParams::setPSEmbedTrueType (bool embed) { psEmbedTrueType = embed; }

void GlobalParams::setPSEmbedCIDPostScript (bool embed) {
    psEmbedCIDPostScript = embed;
}

void GlobalParams::setPSEmbedCIDTrueType (bool embed) {
    psEmbedCIDTrueType = embed;
}

void GlobalParams::setPSFontPassthrough (bool passthrough) {
    psFontPassthrough = passthrough;
}

void GlobalParams::setPSPreload (bool preload) { psPreload = preload; }

void GlobalParams::setPSOPI (bool opi) { psOPI = opi; }

void GlobalParams::setPSASCIIHex (bool hex) { psASCIIHex = hex; }

void GlobalParams::setTextEncoding (const char* encodingName) {
    delete textEncoding;
    textEncoding = new GString (encodingName);
}

bool GlobalParams::setTextEOL (char* s) {
    if (!strcmp (s, "unix")) { textEOL = eolUnix; }
    else if (!strcmp (s, "dos")) {
        textEOL = eolDOS;
    }
    else if (!strcmp (s, "mac")) {
        textEOL = eolMac;
    }
    else {
        return false;
    }
    return true;
}

void GlobalParams::setTextPageBreaks (bool pageBreaks) {
    textPageBreaks = pageBreaks;
}

void GlobalParams::setTextKeepTinyChars (bool keep) {
    textKeepTinyChars = keep;
}

void GlobalParams::setInitialZoom (const char* s) {
    delete initialZoom;
    initialZoom = new GString (s);
}

void GlobalParams::setContinuousView (bool cont) { continuousView = cont; }

bool GlobalParams::setEnableFreeType (char* s) {
    bool ok;

    ok = parseYesNo2 (s, &enableFreeType);
    return ok;
}

bool GlobalParams::setAntialias (char* s) {
    bool ok;

    ok = parseYesNo2 (s, &antialias);
    return ok;
}

bool GlobalParams::setVectorAntialias (char* s) {
    bool ok;

    ok = parseYesNo2 (s, &vectorAntialias);
    return ok;
}

void GlobalParams::setScreenType (ScreenType t) { screenType = t; }

void GlobalParams::setScreenSize (int size) { screenSize = size; }

void GlobalParams::setScreenDotRadius (int r) { screenDotRadius = r; }

void GlobalParams::setScreenGamma (double gamma) { screenGamma = gamma; }

void GlobalParams::setScreenBlackThreshold (double thresh) {
    screenBlackThreshold = thresh;
}

void GlobalParams::setScreenWhiteThreshold (double thresh) {
    screenWhiteThreshold = thresh;
}

void GlobalParams::setMapNumericCharNames (bool map) {
    mapNumericCharNames = map;
}

void GlobalParams::setMapUnknownCharNames (bool map) {
    mapUnknownCharNames = map;
}

void GlobalParams::setMapExtTrueTypeFontsViaUnicode (bool map) {
    mapExtTrueTypeFontsViaUnicode = map;
}

void GlobalParams::setEnableXFA (bool enable) { enableXFA = enable; }

void GlobalParams::setPrintCommands (bool printCommandsA) {
    printCommands = printCommandsA;
}

void GlobalParams::setErrQuiet (bool errQuietA) { errQuiet = errQuietA; }

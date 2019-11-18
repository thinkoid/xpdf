//========================================================================
//
// GlobalParams.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef GLOBALPARAMS_H
#define GLOBALPARAMS_H

#include <defs.hh>

#include <cstdio>
#include <xpdf/CharTypes.hh>

class GString;
class GList;
class GHash;
class NameToCharCode;
class CharCodeToUnicode;
class CharCodeToUnicodeCache;
class UnicodeMap;
class UnicodeMapCache;
class CMap;
class CMapCache;
struct XpdfSecurityHandler;
class GlobalParams;
class SysFontList;

//------------------------------------------------------------------------

// The global parameters object.
extern GlobalParams* globalParams;

//------------------------------------------------------------------------

enum SysFontType { sysFontPFA, sysFontPFB, sysFontTTF, sysFontTTC };

//------------------------------------------------------------------------

class PSFontParam16 {
public:
    GString* name;       // PDF font name for psResidentFont16;
                         //   char collection name for psResidentFontCC
    int wMode;           // writing mode (0=horiz, 1=vert)
    GString* psFontName; // PostScript font name
    GString* encoding;   // encoding

    PSFontParam16 (
        GString* nameA, int wModeA, GString* psFontNameA, GString* encodingA);
    ~PSFontParam16 ();
};

//------------------------------------------------------------------------

enum PSLevel {
    psLevel1,
    psLevel1Sep,
    psLevel2,
    psLevel2Sep,
    psLevel3,
    psLevel3Sep
};

//------------------------------------------------------------------------

enum EndOfLineKind {
    eolUnix, // LF
    eolDOS,  // CR+LF
    eolMac   // CR
};

//------------------------------------------------------------------------

enum ScreenType {
    screenUnset,
    screenDispersed,
    screenClustered,
    screenStochasticClustered
};

//------------------------------------------------------------------------

class KeyBinding {
public:
    int code;    // 0x20 .. 0xfe = ASCII,
                 //   >=0x10000 = special keys, mouse buttons,
                 //   etc. (xpdfKeyCode* symbols)
    int mods;    // modifiers (xpdfKeyMod* symbols, or-ed
                 //   together)
    int context; // context (xpdfKeyContext* symbols, or-ed
                 //   together)
    GList* cmds; // list of commands [GString]

    KeyBinding (int codeA, int modsA, int contextA, const char* cmd0);
    KeyBinding (
        int codeA, int modsA, int contextA, const char* cmd0, const char* cmd1);
    KeyBinding (int codeA, int modsA, int contextA, GList* cmdsA);
    ~KeyBinding ();
};

#define xpdfKeyCodeTab 0x1000
#define xpdfKeyCodeReturn 0x1001
#define xpdfKeyCodeEnter 0x1002
#define xpdfKeyCodeBackspace 0x1003
#define xpdfKeyCodeInsert 0x1004
#define xpdfKeyCodeDelete 0x1005
#define xpdfKeyCodeHome 0x1006
#define xpdfKeyCodeEnd 0x1007
#define xpdfKeyCodePgUp 0x1008
#define xpdfKeyCodePgDn 0x1009
#define xpdfKeyCodeLeft 0x100a
#define xpdfKeyCodeRight 0x100b
#define xpdfKeyCodeUp 0x100c
#define xpdfKeyCodeDown 0x100d
#define xpdfKeyCodeF1 0x1100
#define xpdfKeyCodeF35 0x1122
#define xpdfKeyCodeMousePress1 0x2001
#define xpdfKeyCodeMousePress2 0x2002
#define xpdfKeyCodeMousePress3 0x2003
#define xpdfKeyCodeMousePress4 0x2004
#define xpdfKeyCodeMousePress5 0x2005
#define xpdfKeyCodeMousePress6 0x2006
#define xpdfKeyCodeMousePress7 0x2007
// ...
#define xpdfKeyCodeMousePress32 0x2020
#define xpdfKeyCodeMouseRelease1 0x2101
#define xpdfKeyCodeMouseRelease2 0x2102
#define xpdfKeyCodeMouseRelease3 0x2103
#define xpdfKeyCodeMouseRelease4 0x2104
#define xpdfKeyCodeMouseRelease5 0x2105
#define xpdfKeyCodeMouseRelease6 0x2106
#define xpdfKeyCodeMouseRelease7 0x2107
// ...
#define xpdfKeyCodeMouseRelease32 0x2120
#define xpdfKeyModNone 0
#define xpdfKeyModShift (1 << 0)
#define xpdfKeyModCtrl (1 << 1)
#define xpdfKeyModAlt (1 << 2)
#define xpdfKeyContextAny 0
#define xpdfKeyContextFullScreen (1 << 0)
#define xpdfKeyContextWindow (2 << 0)
#define xpdfKeyContextContinuous (1 << 2)
#define xpdfKeyContextSinglePage (2 << 2)
#define xpdfKeyContextOverLink (1 << 4)
#define xpdfKeyContextOffLink (2 << 4)
#define xpdfKeyContextOutline (1 << 6)
#define xpdfKeyContextMainWin (2 << 6)
#define xpdfKeyContextScrLockOn (1 << 8)
#define xpdfKeyContextScrLockOff (2 << 8)

//------------------------------------------------------------------------

class GlobalParams {
public:
    // Initialize the global parameters by attempting to read a config
    // file.
    GlobalParams (const char* cfgFileName);

    ~GlobalParams ();

    void setBaseDir (char* dir);
    void setupBaseFonts (char* dir);

    void parseLine (char* buf, GString* fileName, int line);

    //----- accessors

    CharCode getMacRomanCharCode (char* charName);

    GString* getBaseDir ();
    Unicode mapNameToUnicode (const char* charName);
    UnicodeMap* getResidentUnicodeMap (GString* encodingName);
    FILE* getUnicodeMapFile (GString* encodingName);
    FILE* findCMapFile (GString* collection, GString* cMapName);
    FILE* findToUnicodeFile (GString* name);
    GString* findFontFile (GString* fontName);
    GString*
    findBase14FontFile (GString* fontName, int* fontNum, double* oblique);
    GString*
    findSystemFontFile (GString* fontName, SysFontType* type, int* fontNum);
    GString* findCCFontFile (GString* collection);
    GString* getPSFile ();
    int getPSPaperWidth ();
    int getPSPaperHeight ();
    void getPSImageableArea (int* llx, int* lly, int* urx, int* ury);
    bool getPSDuplex ();
    bool getPSCrop ();
    bool getPSUseCropBoxAsPage ();
    bool getPSExpandSmaller ();
    bool getPSShrinkLarger ();
    bool getPSCenter ();
    PSLevel getPSLevel ();
    GString* getPSResidentFont (GString* fontName);
    GList* getPSResidentFonts ();
    PSFontParam16* getPSResidentFont16 (GString* fontName, int wMode);
    PSFontParam16* getPSResidentFontCC (GString* collection, int wMode);
    bool getPSEmbedType1 ();
    bool getPSEmbedTrueType ();
    bool getPSEmbedCIDPostScript ();
    bool getPSEmbedCIDTrueType ();
    bool getPSFontPassthrough ();
    bool getPSPreload ();
    bool getPSOPI ();
    bool getPSASCIIHex ();
    bool getPSLZW ();
    bool getPSUncompressPreloadedImages ();
    double getPSMinLineWidth ();
    double getPSRasterResolution ();
    bool getPSRasterMono ();
    int getPSRasterSliceSize ();
    bool getPSAlwaysRasterize ();
    GString* getTextEncodingName ();
    EndOfLineKind getTextEOL ();
    bool getTextPageBreaks ();
    bool getTextKeepTinyChars ();
    GString* getInitialZoom ();
    bool getContinuousView ();
    bool getEnableFreeType ();
    bool getDisableFreeTypeHinting ();
    bool getAntialias ();
    bool getVectorAntialias ();
    bool getAntialiasPrinting ();
    bool getStrokeAdjust ();
    ScreenType getScreenType ();
    int getScreenSize ();
    int getScreenDotRadius ();
    double getScreenGamma ();
    double getScreenBlackThreshold ();
    double getScreenWhiteThreshold ();
    double getMinLineWidth ();
    bool getDrawAnnotations ();
    bool getOverprintPreview () { return overprintPreview; }
    GString* getLaunchCommand () { return launchCommand; }
    GString* getURLCommand () { return urlCommand; }
    GString* getMovieCommand () { return movieCommand; }
    bool getMapNumericCharNames ();
    bool getMapUnknownCharNames ();
    bool getMapExtTrueTypeFontsViaUnicode ();
    bool getEnableXFA ();
    GList* getKeyBinding (int code, int mods, int context);
    bool getPrintCommands ();
    bool getErrQuiet ();

    CharCodeToUnicode* getCIDToUnicode (GString* collection);
    CharCodeToUnicode* getUnicodeToUnicode (GString* fontName);
    UnicodeMap* getUnicodeMap (GString* encodingName);
    CMap* getCMap (GString* collection, GString* cMapName);
    UnicodeMap* getTextEncoding ();

    //----- functions to set parameters

    void addFontFile (GString* fontName, GString* path);
    void setPSFile (char* file);
    bool setPSPaperSize (const char* size);
    void setPSPaperWidth (int width);
    void setPSPaperHeight (int height);
    void setPSImageableArea (int llx, int lly, int urx, int ury);
    void setPSDuplex (bool duplex);
    void setPSCrop (bool crop);
    void setPSUseCropBoxAsPage (bool crop);
    void setPSExpandSmaller (bool expand);
    void setPSShrinkLarger (bool shrink);
    void setPSCenter (bool center);
    void setPSLevel (PSLevel level);
    void setPSEmbedType1 (bool embed);
    void setPSEmbedTrueType (bool embed);
    void setPSEmbedCIDPostScript (bool embed);
    void setPSEmbedCIDTrueType (bool embed);
    void setPSFontPassthrough (bool passthrough);
    void setPSPreload (bool preload);
    void setPSOPI (bool opi);
    void setPSASCIIHex (bool hex);
    void setTextEncoding (const char* encodingName);
    bool setTextEOL (char* s);
    void setTextPageBreaks (bool pageBreaks);
    void setTextKeepTinyChars (bool keep);
    void setInitialZoom (const char* s);
    void setContinuousView (bool cont);
    bool setEnableFreeType (char* s);
    bool setAntialias (char* s);
    bool setVectorAntialias (char* s);
    void setScreenType (ScreenType t);
    void setScreenSize (int size);
    void setScreenDotRadius (int r);
    void setScreenGamma (double gamma);
    void setScreenBlackThreshold (double thresh);
    void setScreenWhiteThreshold (double thresh);
    void setMapNumericCharNames (bool map);
    void setMapUnknownCharNames (bool map);
    void setMapExtTrueTypeFontsViaUnicode (bool map);
    void setEnableXFA (bool enable);
    void setPrintCommands (bool printCommandsA);
    void setErrQuiet (bool errQuietA);

    //----- security handlers

    void addSecurityHandler (XpdfSecurityHandler* handler);
    XpdfSecurityHandler* getSecurityHandler (char* name);

private:
    void createDefaultKeyBindings ();
    void parseFile (GString* fileName, FILE* f);
    void parseNameToUnicode (GList* tokens, GString* fileName, int line);
    void parseCIDToUnicode (GList* tokens, GString* fileName, int line);
    void parseUnicodeToUnicode (GList* tokens, GString* fileName, int line);
    void parseUnicodeMap (GList* tokens, GString* fileName, int line);
    void parseCMapDir (GList* tokens, GString* fileName, int line);
    void parseToUnicodeDir (GList* tokens, GString* fileName, int line);
    void parseFontFile (GList* tokens, GString* fileName, int line);
    void parseFontDir (GList* tokens, GString* fileName, int line);
    void parseFontFileCC (GList* tokens, GString* fileName, int line);
    void parsePSFile (GList* tokens, GString* fileName, int line);
    void parsePSPaperSize (GList* tokens, GString* fileName, int line);
    void parsePSImageableArea (GList* tokens, GString* fileName, int line);
    void parsePSLevel (GList* tokens, GString* fileName, int line);
    void parsePSResidentFont (GList* tokens, GString* fileName, int line);
    void parsePSResidentFont16 (GList* tokens, GString* fileName, int line);
    void parsePSResidentFontCC (GList* tokens, GString* fileName, int line);
    void parseTextEncoding (GList* tokens, GString* fileName, int line);
    void parseTextEOL (GList* tokens, GString* fileName, int line);
    void parseInitialZoom (GList* tokens, GString* fileName, int line);
    void parseScreenType (GList* tokens, GString* fileName, int line);
    void parseBind (GList* tokens, GString* fileName, int line);
    void parseUnbind (GList* tokens, GString* fileName, int line);
    bool parseKey (
        GString* modKeyStr, GString* contextStr, int* code, int* mods,
        int* context, const char* cmdName, GList* tokens, GString* fileName,
        int line);
    void parseCommand (
        const char* cmdName, GString** val, GList* tokens, GString* fileName,
        int line);
    void parseYesNo (
        const char* cmdName, bool* flag, GList* tokens, GString* fileName,
        int line);
    bool parseYesNo2 (const char* token, bool* flag);
    void parseInteger (
        const char* cmdName, int* val, GList* tokens, GString* fileName,
        int line);
    void parseFloat (
        const char* cmdName, double* val, GList* tokens, GString* fileName,
        int line);
    UnicodeMap* getUnicodeMap2 (GString* encodingName);
#ifdef ENABLE_PLUGINS
    bool loadPlugin (char* type, char* name);
#endif

    //----- static tables

    NameToCharCode*         // mapping from char name to
        macRomanReverseMap; //   MacRomanEncoding index

    //----- user-modifiable settings

    GString* baseDir; // base directory - for plugins, etc.
    NameToCharCode*   // mapping from char name to Unicode
        nameToUnicode;
    GHash* cidToUnicodes;       // files for mappings from char collections
                                //   to Unicode, indexed by collection name
                                //   [GString]
    GHash* unicodeToUnicodes;   // files for Unicode-to-Unicode mappings,
                                //   indexed by font name pattern [GString]
    GHash* residentUnicodeMaps; // mappings from Unicode to char codes,
                                //   indexed by encoding name [UnicodeMap]
    GHash* unicodeMaps;         // files for mappings from Unicode to char
                                //   codes, indexed by encoding name [GString]
    GHash* cMapDirs;            // list of CMap dirs, indexed by collection
                                //   name [GList[GString]]
    GList* toUnicodeDirs;       // list of ToUnicode CMap dirs [GString]
    GHash* fontFiles;           // font files: font name mapped to path
                                //   [GString]
    GList* fontDirs;            // list of font dirs [GString]
    GHash* ccFontFiles;         // character collection font files:
                                //   collection name  mapped to path [GString]
    GHash* base14SysFonts;      // Base-14 system font files: font name
                                //   mapped to path [Base14FontInfo]
    SysFontList* sysFonts;      // system fonts
    GString* psFile;            // PostScript file or command (for xpdf)
    int psPaperWidth;           // paper size, in PostScript points, for
    int psPaperHeight;          //   PostScript output
    int psImageableLLX,         // imageable area, in PostScript points,
        psImageableLLY,         //   for PostScript output
        psImageableURX, psImageableURY;
    bool psCrop;               // crop PS output to CropBox
    bool psUseCropBoxAsPage;   // use CropBox as page size
    bool psExpandSmaller;      // expand smaller pages to fill paper
    bool psShrinkLarger;       // shrink larger pages to fit paper
    bool psCenter;             // center pages on the paper
    bool psDuplex;             // enable duplexing in PostScript?
    PSLevel psLevel;            // PostScript level to generate
    GHash* psResidentFonts;     // 8-bit fonts resident in printer:
                                //   PDF font name mapped to PS font name
                                //   [GString]
    GList* psResidentFonts16;   // 16-bit fonts resident in printer:
                                //   PDF font name mapped to font info
                                //   [PSFontParam16]
    GList* psResidentFontsCC;   // 16-bit character collection fonts
                                //   resident in printer: collection name
                                //   mapped to font info [PSFontParam16]
    bool psEmbedType1;         // embed Type 1 fonts?
    bool psEmbedTrueType;      // embed TrueType fonts?
    bool psEmbedCIDPostScript; // embed CID PostScript fonts?
    bool psEmbedCIDTrueType;   // embed CID TrueType fonts?
    bool psFontPassthrough;    // pass all fonts through as-is?
    bool psPreload;            // preload PostScript images and forms into
                                //   memory
    bool psOPI;                // generate PostScript OPI comments?
    bool psASCIIHex;           // use ASCIIHex instead of ASCII85?
    bool psLZW;                // false to use RLE instead of LZW
    bool psUncompressPreloadedImages; // uncompress all preloaded images
    double psMinLineWidth;        // minimum line width for PostScript output
    double psRasterResolution;    // PostScript rasterization resolution (dpi)
    bool psRasterMono;           // true to do PostScript rasterization
                                  //   in monochrome (gray); false to do it
                                  //   in color (RGB/CMYK)
    int psRasterSliceSize;        // maximum size (pixels) of PostScript
                                  //   rasterization slice
    bool psAlwaysRasterize;      // force PostScript rasterization
    GString* textEncoding;        // encoding (unicodeMap) to use for text
                                  //   output
    EndOfLineKind textEOL;        // type of EOL marker to use for text
                                  //   output
    bool textPageBreaks;         // insert end-of-page markers?
    bool textKeepTinyChars;      // keep all characters in text output
    GString* initialZoom;         // initial zoom level
    bool continuousView;         // continuous view mode
    bool enableFreeType;         // FreeType enable flag
    bool disableFreeTypeHinting; // FreeType hinting disable flag
    bool antialias;              // font anti-aliasing enable flag
    bool vectorAntialias;        // vector anti-aliasing enable flag
    bool antialiasPrinting;      // allow anti-aliasing when printing
    bool strokeAdjust;           // stroke adjustment enable flag
    ScreenType screenType;        // halftone screen type
    int screenSize;               // screen matrix size
    int screenDotRadius;          // screen dot radius
    double screenGamma;           // screen gamma correction
    double screenBlackThreshold;  // screen black clamping threshold
    double screenWhiteThreshold;  // screen white clamping threshold
    double minLineWidth;          // minimum line width
    bool drawAnnotations;        // draw annotations or not
    bool overprintPreview;       // enable overprint preview
    GString* launchCommand;       // command executed for 'launch' links
    GString* urlCommand;          // command executed for URL links
    GString* movieCommand;        // command executed for movie annotations
    bool mapNumericCharNames;    // map numeric char names (from font subsets)?
    bool mapUnknownCharNames;    // map unknown char names?
    bool mapExtTrueTypeFontsViaUnicode; // map char codes to GID via Unicode
                                         //   for external TrueType fonts?
    bool enableXFA;                     // enable XFA form rendering
    GList* keyBindings;  // key & mouse button bindings [KeyBinding]
    bool printCommands; // print the drawing commands
    bool errQuiet;      // suppress error messages?

    CharCodeToUnicodeCache* cidToUnicodeCache;
    CharCodeToUnicodeCache* unicodeToUnicodeCache;
    UnicodeMapCache* unicodeMapCache;
    CMapCache* cMapCache;

#ifdef ENABLE_PLUGINS
    GList* plugins;          // list of plugins [Plugin]
    GList* securityHandlers; // list of loaded security handlers
        //   [XpdfSecurityHandler]
#endif
};

#endif

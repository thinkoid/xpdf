// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <unistd.h>

#include <iostream>

#include <utils/string.hh>
#include <utils/path.hh>

#include <fofi/FoFiTrueType.hh>
#include <fofi/FoFiType1C.hh>

#include <splash/SplashFTFontFile.hh>
#include <splash/SplashFTFontEngine.hh>

#include FT_MODULE_H

#ifdef FT_CFF_DRIVER_H
#include FT_CFF_DRIVER_H
#endif

static void gstringWrite(void *stream, const char *data, int len)
{
    ((GString *)stream)->append(data, len);
}

//------------------------------------------------------------------------
// SplashFTFontEngine
//------------------------------------------------------------------------

SplashFTFontEngine::SplashFTFontEngine(bool aaA, unsigned flagsA, FT_Library libA)
{
    FT_Int major, minor, patch;

    aa = aaA;
    flags = flagsA;
    lib = libA;

    // as of FT 2.1.8, CID fonts are indexed by CID instead of GID
    FT_Library_Version(lib, &major, &minor, &patch);
    useCIDs = major > 2 ||
              (major == 2 && (minor > 1 || (minor == 1 && patch > 7)));
}

SplashFTFontEngine *SplashFTFontEngine::init(bool aaA, unsigned flagsA)
{
    FT_Library libA;

    if (FT_Init_FreeType(&libA)) {
        return NULL;
    }
    return new SplashFTFontEngine(aaA, flagsA, libA);
}

SplashFTFontEngine::~SplashFTFontEngine()
{
    FT_Done_FreeType(lib);
}

SplashFontFile *SplashFTFontEngine::loadType1Font(SplashFontFileID *idA,
                                                  GString *         fontBuf,
                                                  const char **     enc)
{
    return SplashFTFontFile::loadType1Font(this, idA, fontBuf, enc, true);
}

SplashFontFile *SplashFTFontEngine::loadType1CFont(SplashFontFileID *idA,
                                                   GString *         fontBuf,
                                                   const char **     enc)
{
    return SplashFTFontFile::loadType1Font(this, idA, fontBuf, enc, false);
}

SplashFontFile *SplashFTFontEngine::loadOpenTypeT1CFont(SplashFontFileID *idA,
                                                        GString *         fontBuf,
                                                        const char **     enc)
{
    FoFiTrueType *  ff;
    GString *       fontBuf2;
    SplashFontFile *ret;

    if (!(ff = FoFiTrueType::make(fontBuf->c_str(), fontBuf->getLength(), 0,
                                  true))) {
        return NULL;
    }
    if (ff->isHeadlessCFF()) {
        fontBuf2 = new GString();
        ff->convertToType1(NULL, enc, false, &gstringWrite, fontBuf2);
        delete ff;
        ret = SplashFTFontFile::loadType1Font(this, idA, fontBuf2, enc, false);
        if (ret) {
            delete fontBuf;
        } else {
            delete fontBuf2;
        }
    } else {
        delete ff;
        ret = SplashFTFontFile::loadType1Font(this, idA, fontBuf, enc, false);
    }
    return ret;
}

SplashFontFile *SplashFTFontEngine::loadCIDFont(SplashFontFileID *idA,
                                                GString *         fontBuf)
{
    FoFiType1C *    ff;
    int *           cidToGIDMap;
    int             nCIDs;
    SplashFontFile *ret;

    // check for a CFF font
    if (useCIDs) {
        cidToGIDMap = NULL;
        nCIDs = 0;
    } else if ((ff = FoFiType1C::make(fontBuf->c_str(), fontBuf->getLength()))) {
        cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
        delete ff;
    } else {
        cidToGIDMap = NULL;
        nCIDs = 0;
    }
    ret = SplashFTFontFile::loadCIDFont(this, idA, fontBuf, cidToGIDMap, nCIDs);
    if (!ret) {
        free(cidToGIDMap);
    }
    return ret;
}

SplashFontFile *SplashFTFontEngine::loadOpenTypeCFFFont(SplashFontFileID *idA,
                                                        GString *         fontBuf,
                                                        int *codeToGID,
                                                        int  codeToGIDLen)
{
    FoFiTrueType *  ff;
    GString *       fontBuf2;
    char *          cffStart;
    int             cffLength;
    int *           cidToGIDMap;
    int             nCIDs;
    SplashFontFile *ret;

    if (!(ff = FoFiTrueType::make(fontBuf->c_str(), fontBuf->getLength(), 0,
                                  true))) {
        return NULL;
    }
    cidToGIDMap = NULL;
    nCIDs = 0;
    if (ff->isHeadlessCFF()) {
        if (!ff->getCFFBlock(&cffStart, &cffLength)) {
            return NULL;
        }
        fontBuf2 = new GString(cffStart, cffLength);
        if (!useCIDs) {
            cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
        }
        ret = SplashFTFontFile::loadCIDFont(this, idA, fontBuf2, cidToGIDMap,
                                            nCIDs);
        if (ret) {
            delete fontBuf;
        } else {
            delete fontBuf2;
        }
    } else {
        if (!codeToGID && !useCIDs && ff->isOpenTypeCFF()) {
            cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
        }
        ret = SplashFTFontFile::loadCIDFont(this, idA, fontBuf,
                                            codeToGID ? codeToGID : cidToGIDMap,
                                            codeToGID ? codeToGIDLen : nCIDs);
    }
    delete ff;
    if (!ret) {
        free(cidToGIDMap);
    }
    return ret;
}

SplashFontFile *SplashFTFontEngine::loadTrueTypeFont(SplashFontFileID *idA,
                                                     GString *         fontBuf,
                                                     int fontNum, int *codeToGID,
                                                     int codeToGIDLen)
{
    FoFiTrueType *  ff;
    GString *       fontBuf2;
    SplashFontFile *ret;

    if (!(ff = FoFiTrueType::make(fontBuf->c_str(), fontBuf->getLength(),
                                  fontNum))) {
        return NULL;
    }
    fontBuf2 = new GString;
    ff->writeTTF(&gstringWrite, fontBuf2);
    delete ff;
    ret = SplashFTFontFile::loadTrueTypeFont(this, idA, fontBuf2, 0, codeToGID,
                                             codeToGIDLen);
    if (ret) {
        delete fontBuf;
    } else {
        delete fontBuf2;
    }

    return ret;
}

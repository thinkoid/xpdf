// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <unistd.h>

#include <iostream>

#include <utils/string.hh>
#include <utils/gfile.hh>

#include <fofi/FoFiTrueType.hh>
#include <fofi/FoFiType1C.hh>

#include <splash/SplashFTFontFile.hh>
#include <splash/SplashFTFontEngine.hh>

#include FT_MODULE_H

#ifdef FT_CFF_DRIVER_H
#include FT_CFF_DRIVER_H
#endif

//------------------------------------------------------------------------

static void fileWrite(void *stream, const char *data, int len)
{
    fwrite(data, 1, len, (FILE *)stream);
}

#if LOAD_FONTS_FROM_MEM
static void gstringWrite(void *stream, const char *data, int len)
{
    ((GString *)stream)->append(data, len);
}
#endif

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
#if LOAD_FONTS_FROM_MEM
                                                  GString *fontBuf,
#else
                                                  const char *fileName,
                                                  bool        deleteFile,
#endif
                                                  const char **enc)
{
    return SplashFTFontFile::loadType1Font(this, idA,
#if LOAD_FONTS_FROM_MEM
                                           fontBuf,
#else
                                           fileName, deleteFile,
#endif
                                           enc, true);
}

SplashFontFile *SplashFTFontEngine::loadType1CFont(SplashFontFileID *idA,
#if LOAD_FONTS_FROM_MEM
                                                   GString *fontBuf,
#else
                                                   const char *fileName,
                                                   bool        deleteFile,
#endif
                                                   const char **enc)
{
    return SplashFTFontFile::loadType1Font(this, idA,
#if LOAD_FONTS_FROM_MEM
                                           fontBuf,
#else
                                           fileName, deleteFile,
#endif
                                           enc, false);
}

SplashFontFile *SplashFTFontEngine::loadOpenTypeT1CFont(SplashFontFileID *idA,
#if LOAD_FONTS_FROM_MEM
                                                        GString *fontBuf,
#else
                                                        const char *fileName,
                                                        bool        deleteFile,
#endif
                                                        const char **enc)
{
    FoFiTrueType *ff;
#if LOAD_FONTS_FROM_MEM
    GString *fontBuf2;
#else
    GString *tmpFileName = 0;
    FILE *   tmpFile = 0;
#endif
    SplashFontFile *ret;

#if LOAD_FONTS_FROM_MEM
    if (!(ff = FoFiTrueType::make(fontBuf->c_str(), fontBuf->getLength(), 0,
                                  true))) {
#else
    if (!(ff = FoFiTrueType::load(fileName, 0, true))) {
#endif
        return NULL;
    }
    if (ff->isHeadlessCFF()) {
#if LOAD_FONTS_FROM_MEM
        fontBuf2 = new GString();
        ff->convertToType1(NULL, enc, false, &gstringWrite, fontBuf2);
        delete ff;
        ret = SplashFTFontFile::loadType1Font(this, idA, fontBuf2, enc, false);
        if (ret) {
            delete fontBuf;
        } else {
            delete fontBuf2;
        }
#else
        if (!openTempFile(&tmpFileName, &tmpFile, "wb")) {
            delete ff;
            return NULL;
        }
        ff->convertToType1(NULL, enc, false, &fileWrite, tmpFile);
        delete ff;
        fclose(tmpFile);
        ret = SplashFTFontFile::loadType1Font(this, idA, tmpFileName->c_str(),
                                              true, enc, false);
        if (ret) {
            if (deleteFile) {
                unlink(fileName);
            }
        } else {
            unlink(tmpFileName->c_str());
        }
        delete tmpFileName;
#endif
    } else {
        delete ff;
        ret = SplashFTFontFile::loadType1Font(this, idA,
#if LOAD_FONTS_FROM_MEM
                                              fontBuf,
#else
                                              fileName, deleteFile,
#endif
                                              enc, false);
    }
    return ret;
}

SplashFontFile *SplashFTFontEngine::loadCIDFont(SplashFontFileID *idA,
#if LOAD_FONTS_FROM_MEM
                                                GString *fontBuf
#else
                                                const char *fileName,
                                                bool        deleteFile
#endif
)
{
    FoFiType1C *    ff;
    int *           cidToGIDMap;
    int             nCIDs;
    SplashFontFile *ret;

    // check for a CFF font
    if (useCIDs) {
        cidToGIDMap = NULL;
        nCIDs = 0;
#if LOAD_FONTS_FROM_MEM
    } else if ((ff = FoFiType1C::make(fontBuf->c_str(), fontBuf->getLength()))) {
#else
    } else if ((ff = FoFiType1C::load(fileName))) {
#endif
        cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
        delete ff;
    } else {
        cidToGIDMap = NULL;
        nCIDs = 0;
    }
    ret = SplashFTFontFile::loadCIDFont(this, idA,
#if LOAD_FONTS_FROM_MEM
                                        fontBuf,
#else
                                        fileName, deleteFile,
#endif
                                        cidToGIDMap, nCIDs);
    if (!ret) {
        free(cidToGIDMap);
    }
    return ret;
}

SplashFontFile *SplashFTFontEngine::loadOpenTypeCFFFont(SplashFontFileID *idA,
#if LOAD_FONTS_FROM_MEM
                                                        GString *fontBuf,
#else
                                                        const char *fileName,
                                                        bool        deleteFile,
#endif
                                                        int *codeToGID,
                                                        int  codeToGIDLen)
{
    FoFiTrueType *ff;
#if LOAD_FONTS_FROM_MEM
    GString *fontBuf2;
#else
    GString *tmpFileName = 0;
    FILE *   tmpFile = 0;
#endif
    char *          cffStart;
    int             cffLength;
    int *           cidToGIDMap;
    int             nCIDs;
    SplashFontFile *ret;

#if LOAD_FONTS_FROM_MEM
    if (!(ff = FoFiTrueType::make(fontBuf->c_str(), fontBuf->getLength(), 0,
                                  true))) {
#else
    if (!(ff = FoFiTrueType::load(fileName, 0, true))) {
#endif
        return NULL;
    }
    cidToGIDMap = NULL;
    nCIDs = 0;
    if (ff->isHeadlessCFF()) {
        if (!ff->getCFFBlock(&cffStart, &cffLength)) {
            return NULL;
        }
#if LOAD_FONTS_FROM_MEM
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
#else
        if (!openTempFile(&tmpFileName, &tmpFile, "wb")) {
            delete ff;
            return NULL;
        }
        fwrite(cffStart, 1, cffLength, tmpFile);
        fclose(tmpFile);
        if (!useCIDs) {
            cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
        }
        ret = SplashFTFontFile::loadCIDFont(this, idA, tmpFileName->c_str(), true,
                                            cidToGIDMap, nCIDs);
        if (ret) {
            if (deleteFile) {
                unlink(fileName);
            }
        } else {
            unlink(tmpFileName->c_str());
        }
        delete tmpFileName;
#endif
    } else {
        if (!codeToGID && !useCIDs && ff->isOpenTypeCFF()) {
            cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
        }
        ret = SplashFTFontFile::loadCIDFont(this, idA,
#if LOAD_FONTS_FROM_MEM
                                            fontBuf,
#else
                                            fileName, deleteFile,
#endif
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
#if LOAD_FONTS_FROM_MEM
                                                     GString *fontBuf,
#else
                                                     const char *fileName,
                                                     bool        deleteFile,
#endif
                                                     int fontNum, int *codeToGID,
                                                     int codeToGIDLen)
{
    FoFiTrueType *ff;
#if LOAD_FONTS_FROM_MEM
    GString *fontBuf2;
#else
    GString *tmpFileName = 0;
    FILE *   tmpFile = 0;
#endif
    SplashFontFile *ret;

#if LOAD_FONTS_FROM_MEM
    if (!(ff = FoFiTrueType::make(fontBuf->c_str(), fontBuf->getLength(),
                                  fontNum))) {
#else
    if (!(ff = FoFiTrueType::load(fileName, fontNum))) {
#endif
        return NULL;
    }
#if LOAD_FONTS_FROM_MEM
    fontBuf2 = new GString;
    ff->writeTTF(&gstringWrite, fontBuf2);
#else
    if (!openTempFile(&tmpFileName, &tmpFile, "wb")) {
        delete ff;
        return NULL;
    }
    ff->writeTTF(&fileWrite, tmpFile);
    fclose(tmpFile);
#endif
    delete ff;
    ret = SplashFTFontFile::loadTrueTypeFont(this, idA,
#if LOAD_FONTS_FROM_MEM
                                             fontBuf2,
#else
                                             tmpFileName->c_str(), true,
#endif
                                             0, codeToGID, codeToGIDLen);
#if LOAD_FONTS_FROM_MEM
    if (ret) {
        delete fontBuf;
    } else {
        delete fontBuf2;
    }
#else
    if (ret) {
        if (deleteFile) {
            unlink(fileName);
        }
    } else {
        unlink(tmpFileName->c_str());
    }
    delete tmpFileName;
#endif
    return ret;
}

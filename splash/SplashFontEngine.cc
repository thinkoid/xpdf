// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdlib>
#include <cstdio>
#include <unistd.h>

#include <utils/string.hh>

#include <splash/SplashMath.hh>
#include <splash/SplashFTFontEngine.hh>
#include <splash/SplashFontFile.hh>
#include <splash/SplashFontFileID.hh>
#include <splash/SplashFont.hh>
#include <splash/SplashFontEngine.hh>

//------------------------------------------------------------------------
// SplashFontEngine
//------------------------------------------------------------------------

SplashFontEngine::SplashFontEngine(bool enableFreeType, unsigned freeTypeFlags,
                                   bool aa)
{
    int i;

    for (i = 0; i < splashFontCacheSize; ++i) {
        fontCache[i] = NULL;
    }

    if (enableFreeType) {
        ftEngine = SplashFTFontEngine::init(aa, freeTypeFlags);
    } else {
        ftEngine = NULL;
    }
}

SplashFontEngine::~SplashFontEngine()
{
    int i;

    for (i = 0; i < splashFontCacheSize; ++i) {
        if (fontCache[i]) {
            delete fontCache[i];
        }
    }

    if (ftEngine) {
        delete ftEngine;
    }
}

SplashFontFile *SplashFontEngine::getFontFile(SplashFontFileID *id)
{
    SplashFontFile *fontFile;
    int             i;

    for (i = 0; i < splashFontCacheSize; ++i) {
        if (fontCache[i]) {
            fontFile = fontCache[i]->getFontFile();
            if (fontFile && fontFile->getID()->matches(id)) {
                return fontFile;
            }
        }
    }
    return NULL;
}

SplashFontFile *SplashFontEngine::loadType1Font(SplashFontFileID *idA,
                                                GString *         fontBuf,
                                                const char **     enc)
{
    SplashFontFile *fontFile;

    fontFile = NULL;
    if (!fontFile && ftEngine) {
        fontFile = ftEngine->loadType1Font(idA, fontBuf, enc);
    }

    return fontFile;
}

SplashFontFile *SplashFontEngine::loadType1CFont(SplashFontFileID *idA,
                                                 GString *         fontBuf,
                                                 const char **     enc)
{
    SplashFontFile *fontFile;

    fontFile = NULL;
    if (!fontFile && ftEngine) {
        fontFile = ftEngine->loadType1CFont(idA, fontBuf, enc);
    }

    return fontFile;
}

SplashFontFile *SplashFontEngine::loadOpenTypeT1CFont(SplashFontFileID *idA,
                                                      GString *         fontBuf,
                                                      const char **     enc)
{
    SplashFontFile *fontFile;

    fontFile = NULL;
    if (!fontFile && ftEngine) {
        fontFile = ftEngine->loadOpenTypeT1CFont(idA, fontBuf, enc);
    }

    return fontFile;
}

SplashFontFile *SplashFontEngine::loadCIDFont(SplashFontFileID *idA,
                                              GString *         fontBuf)
{
    SplashFontFile *fontFile;

    fontFile = NULL;
    if (!fontFile && ftEngine) {
        fontFile = ftEngine->loadCIDFont(idA, fontBuf);
    }

    return fontFile;
}

SplashFontFile *SplashFontEngine::loadOpenTypeCFFFont(SplashFontFileID *idA,
                                                      GString *         fontBuf,
                                                      int *             codeToGID,
                                                      int codeToGIDLen)
{
    SplashFontFile *fontFile;

    fontFile = NULL;
    if (!fontFile && ftEngine) {
        fontFile =
            ftEngine->loadOpenTypeCFFFont(idA, fontBuf, codeToGID, codeToGIDLen);
    }

    return fontFile;
}

SplashFontFile *SplashFontEngine::loadTrueTypeFont(SplashFontFileID *idA,
                                                   GString *fontBuf, int fontNum,
                                                   int *       codeToGID,
                                                   int         codeToGIDLen,
                                                   const char *fontName)
{
    SplashFontFile *fontFile;

    fontFile = NULL;
    if (!fontFile && ftEngine) {
        fontFile = ftEngine->loadTrueTypeFont(idA, fontBuf, fontNum, codeToGID,
                                              codeToGIDLen);
    }

    if (!fontFile) {
        free(codeToGID);
    }

    return fontFile;
}

SplashFont *SplashFontEngine::getFont(SplashFontFile *fontFile,
                                      SplashCoord *textMat, SplashCoord *ctm)
{
    SplashCoord mat[4];
    SplashFont *font;
    int         i, j;

    mat[0] = textMat[0] * ctm[0] + textMat[1] * ctm[2];
    mat[1] = -(textMat[0] * ctm[1] + textMat[1] * ctm[3]);
    mat[2] = textMat[2] * ctm[0] + textMat[3] * ctm[2];
    mat[3] = -(textMat[2] * ctm[1] + textMat[3] * ctm[3]);
    if (!splashCheckDet(mat[0], mat[1], mat[2], mat[3], 0.01)) {
        // avoid a singular (or close-to-singular) matrix
        mat[0] = 0.01;
        mat[1] = 0;
        mat[2] = 0;
        mat[3] = 0.01;
    }

    font = fontCache[0];
    if (font && font->matches(fontFile, mat, textMat)) {
        return font;
    }
    for (i = 1; i < splashFontCacheSize; ++i) {
        font = fontCache[i];
        if (font && font->matches(fontFile, mat, textMat)) {
            for (j = i; j > 0; --j) {
                fontCache[j] = fontCache[j - 1];
            }
            fontCache[0] = font;
            return font;
        }
    }
    font = fontFile->makeFont(mat, textMat);
    if (fontCache[splashFontCacheSize - 1]) {
        delete fontCache[splashFontCacheSize - 1];
    }
    for (j = splashFontCacheSize - 1; j > 0; --j) {
        fontCache[j] = fontCache[j - 1];
    }
    fontCache[0] = font;
    return font;
}

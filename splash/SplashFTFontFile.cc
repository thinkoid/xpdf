// -*- mode: c++; -*-
// Copyright 2003-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <utils/string.hh>
#include <splash/SplashFTFontEngine.hh>
#include <splash/SplashFTFont.hh>
#include <splash/SplashFTFontFile.hh>

//------------------------------------------------------------------------
// SplashFTFontFile
//------------------------------------------------------------------------

SplashFontFile *SplashFTFontFile::loadType1Font(SplashFTFontEngine *engineA,
                                                SplashFontFileID *  idA,
#if LOAD_FONTS_FROM_MEM
                                                GString *fontBufA,
#else
                                                const char *fileNameA,
                                                bool        deleteFileA,
#endif
                                                const char **encA,
                                                bool         useLightHintingA)
{
    FT_Face     faceA;
    int *       codeToGIDA;
    const char *name;
    int         i;

#if LOAD_FONTS_FROM_MEM
    if (FT_New_Memory_Face(engineA->lib, (FT_Byte *)fontBufA->c_str(),
                           fontBufA->getLength(), 0, &faceA)) {
#else
    if (FT_New_Face(engineA->lib, fileNameA, 0, &faceA)) {
#endif
        return NULL;
    }
    codeToGIDA = (int *)calloc(256, sizeof(int));
    for (i = 0; i < 256; ++i) {
        codeToGIDA[i] = 0;
        if ((name = encA[i])) {
            codeToGIDA[i] = (int)FT_Get_Name_Index(faceA, (char *)name);
        }
    }

    return new SplashFTFontFile(engineA, idA,
#if LOAD_FONTS_FROM_MEM
                                fontBufA,
#else
                                fileNameA, deleteFileA,
#endif
                                faceA, codeToGIDA, 256, false, useLightHintingA);
}

SplashFontFile *SplashFTFontFile::loadCIDFont(SplashFTFontEngine *engineA,
                                              SplashFontFileID *  idA,
#if LOAD_FONTS_FROM_MEM
                                              GString *fontBufA,
#else
                                              const char *fileNameA,
                                              bool        deleteFileA,
#endif
                                              int *codeToGIDA, int codeToGIDLenA)
{
    FT_Face faceA;

#if LOAD_FONTS_FROM_MEM
    if (FT_New_Memory_Face(engineA->lib, (FT_Byte *)fontBufA->c_str(),
                           fontBufA->getLength(), 0, &faceA)) {
#else
    if (FT_New_Face(engineA->lib, fileNameA, 0, &faceA)) {
#endif
        return NULL;
    }

    return new SplashFTFontFile(engineA, idA,
#if LOAD_FONTS_FROM_MEM
                                fontBufA,
#else
                                fileNameA, deleteFileA,
#endif
                                faceA, codeToGIDA, codeToGIDLenA, false, false);
}

SplashFontFile *SplashFTFontFile::loadTrueTypeFont(SplashFTFontEngine *engineA,
                                                   SplashFontFileID *  idA,
#if LOAD_FONTS_FROM_MEM
                                                   GString *fontBufA,
#else
                                                   const char *fileNameA,
                                                   bool        deleteFileA,
#endif
                                                   int fontNum, int *codeToGIDA,
                                                   int codeToGIDLenA)
{
    FT_Face faceA;

#if LOAD_FONTS_FROM_MEM
    if (FT_New_Memory_Face(engineA->lib, (FT_Byte *)fontBufA->c_str(),
                           fontBufA->getLength(), fontNum, &faceA)) {
#else
    if (FT_New_Face(engineA->lib, fileNameA, fontNum, &faceA)) {
#endif
        return NULL;
    }

    return new SplashFTFontFile(engineA, idA,
#if LOAD_FONTS_FROM_MEM
                                fontBufA,
#else
                                fileNameA, deleteFileA,
#endif
                                faceA, codeToGIDA, codeToGIDLenA, true, false);
}

SplashFTFontFile::SplashFTFontFile(SplashFTFontEngine *engineA,
                                   SplashFontFileID *  idA,
#if LOAD_FONTS_FROM_MEM
                                   GString *fontBufA,
#else
                                   const char *fileNameA, bool deleteFileA,
#endif
                                   FT_Face faceA, int *codeToGIDA,
                                   int codeToGIDLenA, bool trueTypeA,
                                   bool useLightHintingA)
    :
#if LOAD_FONTS_FROM_MEM
    SplashFontFile(idA, fontBufA)
#else
    SplashFontFile(idA, fileNameA, deleteFileA)
#endif
{
    engine = engineA;
    face = faceA;
    codeToGID = codeToGIDA;
    codeToGIDLen = codeToGIDLenA;
    trueType = trueTypeA;
    useLightHinting = useLightHintingA;
}

SplashFTFontFile::~SplashFTFontFile()
{
    if (face) {
        FT_Done_Face(face);
    }
    if (codeToGID) {
        free(codeToGID);
    }
}

SplashFont *SplashFTFontFile::makeFont(SplashCoord *mat, SplashCoord *textMat)
{
    SplashFont *font;

    font = new SplashFTFont(this, mat, textMat);
    font->initCache();
    return font;
}

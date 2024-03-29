// -*- mode: c++; -*-
// Copyright 1999-2003 Glyph & Cog, LLC

#ifndef XPDF_FOFI_FOFITRUETYPE_HH
#define XPDF_FOFI_FOFITRUETYPE_HH

#include <defs.hh>

#include <vector>

#include <fofi/FoFiBase.hh>

class GHash;
class GString;

struct TrueTypeTable;
struct TrueTypeCmap;

//------------------------------------------------------------------------
// FoFiTrueType
//------------------------------------------------------------------------

class FoFiTrueType : public FoFiBase
{
public:
    // Create a FoFiTrueType object from a memory buffer.  If
    // <allowHeadlessCFF> is true, OpenType CFF fonts without the 'head'
    // table are permitted -- this is useful when calling the convert*
    // functions.
    static FoFiTrueType *make(const char *fileA, int lenA, int fontNum,
                              bool allowHeadlessCFF = false);

    // Create a FoFiTrueType object from a file on disk.  If
    // <allowHeadlessCFF> is true, OpenType CFF fonts without the 'head'
    // table are permitted -- this is useful when calling the convert*
    // functions.
    static FoFiTrueType *load(const char *fileName, int fontNum,
                              bool allowHeadlessCFF = false);

    virtual ~FoFiTrueType();

    // Returns true if this an OpenType font containing CFF data, false
    // if it's a TrueType font (or OpenType font with TrueType data).
    bool isOpenTypeCFF() { return openTypeCFF; }

    // Returns true if this is an OpenType CFF font that is missing the
    // 'head' table.  This is a violation of the OpenType spec, but the
    // embedded CFF font can be usable for some purposes (e.g., the
    // convert* functions).
    bool isHeadlessCFF() { return headlessCFF; }

    // Return the number of cmaps defined by this font.
    int getNumCmaps();

    // Return the platform ID of the <i>th cmap.
    int getCmapPlatform(int i);

    // Return the encoding ID of the <i>th cmap.
    int getCmapEncoding(int i);

    // Return the index of the cmap for <platform>, <encoding>.  Returns
    // -1 if there is no corresponding cmap.
    int findCmap(int platform, int encoding);

    // Return the GID corresponding to <c> according to the <i>th cmap.
    int mapCodeToGID(int i, int c);

    // Returns the GID corresponding to <name> according to the post
    // table.  Returns 0 if there is no mapping for <name> or if the
    // font does not have a post table.
    int mapNameToGID(char *name);

    // Return the mapping from CIDs to GIDs, and return the number of
    // CIDs in *<nCIDs>.  This is only useful for CID fonts.  (Only
    // useful for OpenType CFF fonts.)
    int *getCIDToGIDMap(int *nCIDs);

    // Returns the least restrictive embedding licensing right (as
    // defined by the TrueType spec):
    // * 4: OS/2 table is missing or invalid
    // * 3: installable embedding
    // * 2: editable embedding
    // * 1: preview & print embedding
    // * 0: restricted license embedding
    int getEmbeddingRights();

    // Return the font matrix as an array of six numbers.  (Only useful
    // for OpenType CFF fonts.)
    void getFontMatrix(double *mat);

    // Convert to a Type 42 font, suitable for embedding in a PostScript
    // file.  <psName> will be used as the PostScript font name (so we
    // don't need to depend on the 'name' table in the font).  The
    // <encoding> array specifies the mapping from char codes to names.
    // If <encoding> is NULL, the encoding is unknown or undefined.  The
    // <codeToGID> array specifies the mapping from char codes to GIDs.
    // (Not useful for OpenType CFF fonts.)
    void convertToType42(const char *psName, char **encoding, int *codeToGID,
                         FoFiOutputFunc outputFunc, void *outputStream);

    // Convert to a Type 1 font, suitable for embedding in a PostScript
    // file.  This is only useful with 8-bit fonts.  If <newEncoding> is
    // not NULL, it will be used in place of the encoding in the Type 1C
    // font.  If <ascii> is true the eexec section will be hex-encoded,
    // otherwise it will be left as binary data.  If <psName> is
    // non-NULL, it will be used as the PostScript font name.  (Only
    // useful for OpenType CFF fonts.)
    void convertToType1(const char *psName, const char **newEncoding, bool ascii,
                        FoFiOutputFunc outputFunc, void *outputStream);

    // Convert to a Type 2 CIDFont, suitable for embedding in a
    // PostScript file.  <psName> will be used as the PostScript font
    // name (so we don't need to depend on the 'name' table in the
    // font).  The <cidMap> array maps CIDs to GIDs; it has <nCIDs>
    // entries.  (Not useful for OpenType CFF fonts.)
    void convertToCIDType2(const char *psName, int *cidMap, int nCIDs,
                           bool needVerticalMetrics, FoFiOutputFunc outputFunc,
                           void *outputStream);

    // Convert to a Type 0 CIDFont, suitable for embedding in a
    // PostScript file.  <psName> will be used as the PostScript font
    // name.  (Only useful for OpenType CFF fonts.)
    void convertToCIDType0(const char *psName, int *cidMap, int nCIDs,
                           FoFiOutputFunc outputFunc, void *outputStream);

    // Convert to a Type 0 (but non-CID) composite font, suitable for
    // embedding in a PostScript file.  <psName> will be used as the
    // PostScript font name (so we don't need to depend on the 'name'
    // table in the font).  The <cidMap> array maps CIDs to GIDs; it has
    // <nCIDs> entries.  (Not useful for OpenType CFF fonts.)
    void convertToType0(const char *psName, int *cidMap, int nCIDs,
                        bool needVerticalMetrics, FoFiOutputFunc outputFunc,
                        void *outputStream);

    // Convert to a Type 0 (but non-CID) composite font, suitable for
    // embedding in a PostScript file.  <psName> will be used as the
    // PostScript font name.  (Only useful for OpenType CFF fonts.)
    void convertToType0(const char *psName, int *cidMap, int nCIDs,
                        FoFiOutputFunc outputFunc, void *outputStream);

    // Write a clean TTF file, filling in missing tables and correcting
    // various other errors.  If <name> is non-NULL, the font is renamed
    // to <name>.  If <codeToGID> is non-NULL, the font is re-encoded,
    // using a Windows Unicode cmap.  If <name> is NULL and the font is
    // complete and correct, it will be written unmodified.  (Not useful
    // for OpenType CFF fonts.)
    void writeTTF(FoFiOutputFunc outputFunc, void *outputStream,
                  char *name = NULL, int *codeToGID = NULL);

    // Returns a pointer to the CFF font embedded in this OpenType font.
    // If successful, sets *<start> and *<length>, and returns true.
    // Otherwise returns false.  (Only useful for OpenType CFF fonts).
    bool getCFFBlock(char **start, int *length);

private:
    FoFiTrueType(const char *fileA, int lenA, bool freeFileDataA, int fontNum,
                 bool isDfont, bool allowHeadlessCFF);
    void cvtEncoding(char **encoding, FoFiOutputFunc outputFunc,
                     void *outputStream);
    void cvtCharStrings(char **encoding, int *codeToGID,
                        FoFiOutputFunc outputFunc, void *outputStream);
    void cvtSfnts(FoFiOutputFunc outputFunc, void *outputStream, GString *name,
                  bool needVerticalMetrics, int *maxUsedGlyph);

    void dumpString(const unsigned char *, size_t, FoFiOutputFunc, void *);

    void dumpString(const std::vector< char > &xs, FoFiOutputFunc pfun,
                    void *pstream)
    {
        return dumpString(reinterpret_cast< const unsigned char * >(xs.data()),
                          xs.size(), pfun, pstream);
    }

    unsigned computeTableChecksum(const unsigned char *, size_t) const;

    unsigned checksum_of(const std::vector< char > &xs) const
    {
        return computeTableChecksum(
            reinterpret_cast< const unsigned char * >(xs.data()), xs.size());
    }

    void parse(int fontNum, bool allowHeadlessCFF);
    void parseTTC(int fontNum, int *pos);
    void parseDfont(int fontNum, int *offset, int *pos);
    void readPostTable();
    int  seekTable(const char *tag);

    TrueTypeTable *tables;
    int            nTables;
    TrueTypeCmap * cmaps;
    int            nCmaps;
    int            nGlyphs;
    int            locaFmt;
    int            bbox[4];
    GHash *        nameToGID;
    bool           openTypeCFF;
    bool           headlessCFF;
    bool           isDfont;

    bool parsedOk;
};

#endif // XPDF_FOFI_FOFITRUETYPE_HH

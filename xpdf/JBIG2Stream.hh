//========================================================================
//
// JBIG2Stream.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef JBIG2STREAM_H
#define JBIG2STREAM_H

#include <defs.hh>

#include <xpdf/Object.hh>
#include <xpdf/Stream.hh>

class GList;
class JBIG2Segment;
class JBIG2Bitmap;
class JArithmeticDecoder;
class JArithmeticDecoderStats;
class JBIG2HuffmanDecoder;
struct JBIG2HuffmanTable;
class JBIG2MMRDecoder;

//------------------------------------------------------------------------

class JBIG2Stream : public FilterStream {
public:
    JBIG2Stream (Stream* strA, Object* globalsStreamA);
    virtual ~JBIG2Stream ();
    virtual StreamKind getKind () { return strJBIG2; }
    virtual void reset ();
    virtual void close ();
    virtual int getChar ();
    virtual int lookChar ();
    virtual int getBlock (char* blk, int size);
    virtual GString* getPSFilter (int psLevel, const char* indent);
    virtual bool isBinary (bool last = true);

private:
    void readSegments ();
    bool readSymbolDictSeg (
        unsigned segNum, unsigned length, unsigned* refSegs, unsigned nRefSegs);
    void readTextRegionSeg (
        unsigned segNum, bool imm, bool lossless, unsigned length, unsigned* refSegs,
        unsigned nRefSegs);
    JBIG2Bitmap* readTextRegion (
        bool huff, bool refine, int w, int h, unsigned numInstances,
        unsigned logStrips, int numSyms, JBIG2HuffmanTable* symCodeTab,
        unsigned symCodeLen, JBIG2Bitmap** syms, unsigned defPixel, unsigned combOp,
        unsigned transposed, unsigned refCorner, int sOffset,
        JBIG2HuffmanTable* huffFSTable, JBIG2HuffmanTable* huffDSTable,
        JBIG2HuffmanTable* huffDTTable, JBIG2HuffmanTable* huffRDWTable,
        JBIG2HuffmanTable* huffRDHTable, JBIG2HuffmanTable* huffRDXTable,
        JBIG2HuffmanTable* huffRDYTable, JBIG2HuffmanTable* huffRSizeTable,
        unsigned templ, int* atx, int* aty);
    void readPatternDictSeg (unsigned segNum, unsigned length);
    void readHalftoneRegionSeg (
        unsigned segNum, bool imm, bool lossless, unsigned length, unsigned* refSegs,
        unsigned nRefSegs);
    void readGenericRegionSeg (
        unsigned segNum, bool imm, bool lossless, unsigned length);
    void
    mmrAddPixels (int a1, int blackPixels, int* codingLine, int* a0i, int w);
    void
    mmrAddPixelsNeg (int a1, int blackPixels, int* codingLine, int* a0i, int w);
    JBIG2Bitmap* readGenericBitmap (
        bool mmr, int w, int h, int templ, bool tpgdOn, bool useSkip,
        JBIG2Bitmap* skip, int* atx, int* aty, int mmrDataLength);
    void readGenericRefinementRegionSeg (
        unsigned segNum, bool imm, bool lossless, unsigned length, unsigned* refSegs,
        unsigned nRefSegs);
    JBIG2Bitmap* readGenericRefinementRegion (
        int w, int h, int templ, bool tpgrOn, JBIG2Bitmap* refBitmap,
        int refDX, int refDY, int* atx, int* aty);
    void readPageInfoSeg (unsigned length);
    void readEndOfStripeSeg (unsigned length);
    void readProfilesSeg (unsigned length);
    void readCodeTableSeg (unsigned segNum, unsigned length);
    void readExtensionSeg (unsigned length);
    JBIG2Segment* findSegment (unsigned segNum);
    void discardSegment (unsigned segNum);
    void resetGenericStats (unsigned templ, JArithmeticDecoderStats* prevStats);
    void resetRefinementStats (unsigned templ, JArithmeticDecoderStats* prevStats);
    void resetIntStats (int symCodeLen);
    bool readUByte (unsigned* x);
    bool readByte (int* x);
    bool readUWord (unsigned* x);
    bool readULong (unsigned* x);
    bool readLong (int* x);

    Object globalsStream;
    unsigned pageW, pageH, curPageH;
    unsigned pageDefPixel;
    JBIG2Bitmap* pageBitmap;
    unsigned defCombOp;
    GList* segments;       // [JBIG2Segment]
    GList* globalSegments; // [JBIG2Segment]
    Stream* curStr;
    unsigned char* dataPtr;
    unsigned char* dataEnd;
    unsigned byteCounter;

    JArithmeticDecoder* arithDecoder;
    JArithmeticDecoderStats* genericRegionStats;
    JArithmeticDecoderStats* refinementRegionStats;
    JArithmeticDecoderStats* iadhStats;
    JArithmeticDecoderStats* iadwStats;
    JArithmeticDecoderStats* iaexStats;
    JArithmeticDecoderStats* iaaiStats;
    JArithmeticDecoderStats* iadtStats;
    JArithmeticDecoderStats* iaitStats;
    JArithmeticDecoderStats* iafsStats;
    JArithmeticDecoderStats* iadsStats;
    JArithmeticDecoderStats* iardxStats;
    JArithmeticDecoderStats* iardyStats;
    JArithmeticDecoderStats* iardwStats;
    JArithmeticDecoderStats* iardhStats;
    JArithmeticDecoderStats* iariStats;
    JArithmeticDecoderStats* iaidStats;
    JBIG2HuffmanDecoder* huffDecoder;
    JBIG2MMRDecoder* mmrDecoder;
};

#endif

// -*- mode: c++; -*-
// Copyright 2002-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_JPXSTREAM_HH
#define XPDF_XPDF_JPXSTREAM_HH

#include <defs.hh>

#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>

class JArithmeticDecoder;
class JArithmeticDecoderStats;

//------------------------------------------------------------------------

enum JPXColorSpaceType {
    jpxCSBiLevel = 0,
    jpxCSYCbCr1 = 1,
    jpxCSYCbCr2 = 3,
    jpxCSYCBCr3 = 4,
    jpxCSPhotoYCC = 9,
    jpxCSCMY = 11,
    jpxCSCMYK = 12,
    jpxCSYCCK = 13,
    jpxCSCIELab = 14,
    jpxCSsRGB = 16,
    jpxCSGrayscale = 17,
    jpxCSBiLevel2 = 18,
    jpxCSCIEJab = 19,
    jpxCSCISesRGB = 20,
    jpxCSROMMRGB = 21,
    jpxCSsRGBYCbCr = 22,
    jpxCSYPbPr1125 = 23,
    jpxCSYPbPr1250 = 24
};

struct JPXColorSpecCIELab
{
    unsigned rl, ol, ra, oa, rb, ob, il;
};

struct JPXColorSpecEnumerated
{
    JPXColorSpaceType type; // color space type
    union
    {
        JPXColorSpecCIELab cieLab;
    };
};

struct JPXColorSpec
{
    unsigned meth; // method
    int      prec; // precedence
    union
    {
        JPXColorSpecEnumerated enumerated;
    };
};

//------------------------------------------------------------------------

struct JPXPalette
{
    unsigned  nEntries; // number of entries in the palette
    unsigned  nComps; // number of components in each entry
    unsigned *bpc; // bits per component, for each component
    int *     c; // color data:
        //   c[i*nComps+j] = entry i, component j
};

//------------------------------------------------------------------------

struct JPXCompMap
{
    unsigned  nChannels; // number of channels
    unsigned *comp; // codestream components mapped to each channel
    unsigned *type; // 0 for direct use, 1 for palette mapping
    unsigned *pComp; // palette components to use
};

//------------------------------------------------------------------------

struct JPXChannelDefn
{
    unsigned  nChannels; // number of channels
    unsigned *idx; // channel indexes
    unsigned *type; // channel types
    unsigned *assoc; // channel associations
};

//------------------------------------------------------------------------

struct JPXTagTreeNode
{
    bool     finished; // true if this node is finished
    unsigned val; // current value
};

//------------------------------------------------------------------------

struct JPXCodeBlock
{
    //----- size
    unsigned x0, y0, x1, y1; // bounds

    //----- persistent state
    bool seen; // true if this code-block has already
        //   been seen
    unsigned lBlock; // base number of bits used for pkt data length
    unsigned nextPass; // next coding pass

    //---- info from first packet
    unsigned nZeroBitPlanes; // number of zero bit planes

    //----- info for the current packet
    unsigned included; // code-block inclusion in this packet:
        //   0=not included, 1=included
    unsigned  nCodingPasses; // number of coding passes in this pkt
    unsigned *dataLen; // data lengths (one per codeword segment)
    unsigned  dataLenSize; // size of the dataLen array

    //----- coefficient data
    int *          coeffs;
    char *         touched; // coefficient 'touched' flags
    unsigned short len; // coefficient length
    JArithmeticDecoder // arithmetic decoder
        *arithDecoder;
    JArithmeticDecoderStats // arithmetic decoder stats
        *stats;
};

//------------------------------------------------------------------------

struct JPXSubband
{
    //----- computed
    unsigned x0, y0, x1, y1; // bounds
    unsigned nXCBs, nYCBs; // number of code-blocks in the x and y
        //   directions

    //----- tag trees
    unsigned        maxTTLevel; // max tag tree level
    JPXTagTreeNode *inclusion; // inclusion tag tree for each subband
    JPXTagTreeNode *zeroBitPlane; // zero-bit plane tag tree for each
        //   subband

    //----- children
    JPXCodeBlock *cbs; // the code-blocks (len = nXCBs * nYCBs)
};

//------------------------------------------------------------------------

struct JPXPrecinct
{
    //----- computed
    unsigned x0, y0, x1, y1; // bounds of the precinct

    //----- children
    JPXSubband *subbands; // the subbands
};

//------------------------------------------------------------------------

struct JPXResLevel
{
    //----- from the COD and COC segments (main and tile)
    unsigned precinctWidth; // log2(precinct width)
    unsigned precinctHeight; // log2(precinct height)

    //----- computed
    unsigned x0, y0, x1, y1; // bounds of the tile-comp (for this res level)
    unsigned bx0[3], by0[3], // subband bounds
        bx1[3], by1[3];

    //---- children
    JPXPrecinct *precincts; // the precincts
};

//------------------------------------------------------------------------

struct JPXTileComp
{
    //----- from the SIZ segment
    bool     sgned; // 1 for signed, 0 for unsigned
    unsigned prec; // precision, in bits
    unsigned hSep; // horizontal separation of samples
    unsigned vSep; // vertical separation of samples

    //----- from the COD and COC segments (main and tile)
    unsigned style; // coding style parameter (Scod / Scoc)
    unsigned nDecompLevels; // number of decomposition levels
    unsigned codeBlockW; // log2(code-block width)
    unsigned codeBlockH; // log2(code-block height)
    unsigned codeBlockStyle; // code-block style
    unsigned transform; // wavelet transformation

    //----- from the QCD and QCC segments (main and tile)
    unsigned  quantStyle; // quantization style
    unsigned *quantSteps; // quantization step size for each subband
    unsigned  nQuantSteps; // number of entries in quantSteps

    //----- computed
    unsigned x0, y0, x1, y1; // bounds of the tile-comp, in ref coords
    unsigned w, h; // data size = {x1 - x0, y1 - y0} >> reduction
    unsigned cbW; // code-block width
    unsigned cbH; // code-block height

    //----- image data
    int *data; // the decoded image data
    int *buf; // intermediate buffer for the inverse
        //   transform

    //----- children
    JPXResLevel *resLevels; // the resolution levels
        //   (len = nDecompLevels + 1)
};

//------------------------------------------------------------------------

struct JPXTile
{
    bool init;

    //----- from the COD segments (main and tile)
    unsigned progOrder; // progression order
    unsigned nLayers; // number of layers
    unsigned multiComp; // multiple component transformation

    //----- computed
    unsigned x0, y0, x1, y1; // bounds of the tile, in ref coords
    unsigned maxNDecompLevels; // max number of decomposition levels used
        //   in any component in this tile

    //----- progression order loop counters
    unsigned comp; //   component
    unsigned res; //   resolution level
    unsigned precinct; //   precinct
    unsigned layer; //   layer

    //----- tile part info
    unsigned nextTilePart; // next expected tile-part

    //----- children
    JPXTileComp *tileComps; // the tile-components (len = JPXImage.nComps)
};

//------------------------------------------------------------------------

struct JPXImage
{
    //----- from the SIZ segment
    unsigned xSize, ySize; // size of reference grid
    unsigned xOffset, yOffset; // image offset
    unsigned xTileSize, yTileSize; // size of tiles
    unsigned xTileOffset, // offset of first tile
        yTileOffset;
    unsigned xSizeR, ySizeR; // size of reference grid >> reduction
    unsigned xOffsetR, yOffsetR; // image offset >> reduction
    unsigned xTileSizeR, yTileSizeR; // size of tiles >> reduction
    unsigned xTileOffsetR, // offset of first tile >> reduction
        yTileOffsetR;
    unsigned nComps; // number of components

    //----- computed
    unsigned nXTiles; // number of tiles in x direction
    unsigned nYTiles; // number of tiles in y direction

    //----- children
    JPXTile *tiles; // the tiles (len = nXTiles * nYTiles)
};

//------------------------------------------------------------------------

enum JPXDecodeResult { jpxDecodeOk, jpxDecodeNonFatalError, jpxDecodeFatalError };

//------------------------------------------------------------------------

class JPXStream : public FilterStream
{
public:
    JPXStream(Stream *strA);
    virtual ~JPXStream();

    const std::type_info &type() const override { return typeid(*this); }

    virtual void       reset();
    virtual void       close();
    virtual int        get();
    virtual int        peek();
    virtual GString *  getPSFilter(int psLevel, const char *indent);
    virtual bool       isBinary(bool last = true);
    virtual void       getImageParams(int *                 bitsPerComponent,
                                      StreamColorSpaceMode *csMode);
    void reduceResolution(int reductionA) { reduction = reductionA; }

private:
    void fillReadBuf();
    void getImageParams2(int *bitsPerComponent, StreamColorSpaceMode *csMode);
    JPXDecodeResult readBoxes();
    bool            readColorSpecBox(unsigned dataLen);
    JPXDecodeResult readCodestream(unsigned len);
    bool            readTilePart();
    bool            readTilePartData(unsigned tileIdx, unsigned tilePartLen,
                                     bool tilePartToEOC);
    bool     readCodeBlockData(JPXTileComp *tileComp, JPXResLevel *resLevel,
                               JPXPrecinct *precinct, JPXSubband *subband,
                               unsigned res, unsigned sb, JPXCodeBlock *cb);
    void     inverseTransform(JPXTileComp *tileComp);
    void     inverseTransformLevel(JPXTileComp *tileComp, unsigned r,
                                   JPXResLevel *resLevel);
    void     inverseTransform1D(JPXTileComp *tileComp, int *data, unsigned offset,
                                unsigned n);
    bool     inverseMultiCompAndDC(JPXTile *tile);
    bool     readBoxHdr(unsigned *boxType, unsigned *boxLen, unsigned *dataLen);
    int      readMarkerHdr(int *segType, unsigned *segLen);
    bool     readUByte(unsigned *x);
    bool     readByte(int *x);
    bool     readUWord(unsigned *x);
    bool     readULong(unsigned *x);
    bool     readNBytes(int nBytes, bool signd, int *x);
    void     startBitBuf(unsigned byteCountA);
    bool     readBits(int nBits, unsigned *x);
    void     skipSOP();
    void     skipEPH();
    unsigned finishBitBuf();

    BufStream *bufStr; // buffered stream (for lookahead)

    unsigned  nComps; // number of components
    unsigned *bpc; // bits per component, for each component
    unsigned  width, height; // image size
    int       reduction; // log2(reduction in resolution)
    bool      haveImgHdr; // set if a JP2/JPX image header has been
        //   found
    JPXColorSpec   cs; // color specification
    bool           haveCS; // set if a color spec has been found
    JPXPalette     palette; // the palette
    bool           havePalette; // set if a palette has been found
    JPXCompMap     compMap; // the component mapping
    bool           haveCompMap; // set if a component mapping has been found
    JPXChannelDefn channelDefn; // channel definition
    bool           haveChannelDefn; // set if a channel defn has been found

    JPXImage img; // JPEG2000 decoder data
    unsigned bitBuf; // buffer for bit reads
    int      bitBufLen; // number of bits in bitBuf
    bool     bitBufSkip; // true if next bit should be skipped
        //   (for bit stuffing)
    unsigned byteCount; // number of available bytes left

    unsigned curX, curY, curComp; // current position for peek/get
    unsigned readBuf; // read buffer
    unsigned readBufLen; // number of valid bits in readBuf
};

#endif // XPDF_XPDF_JPXSTREAM_HH

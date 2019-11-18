//========================================================================
//
// Stream.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef STREAM_H
#define STREAM_H

#include <defs.hh>

#include <cstdio>
#include <goo/gfile.hh>
#include <xpdf/Object.hh>

class BaseStream;

//------------------------------------------------------------------------

enum StreamKind {
    strFile,
    strASCIIHex,
    strASCII85,
    strLZW,
    strRunLength,
    strCCITTFax,
    strDCT,
    strFlate,
    strJBIG2,
    strJPX,
    strWeird // internal-use stream types
};

enum StreamColorSpaceMode {
    streamCSNone,
    streamCSDeviceGray,
    streamCSDeviceRGB,
    streamCSDeviceCMYK
};

//------------------------------------------------------------------------

// This is in Stream.h instead of Decrypt.h to avoid really annoying
// include file dependency loops.
enum CryptAlgorithm { cryptRC4, cryptAES, cryptAES256 };

//------------------------------------------------------------------------
// Stream (base class)
//------------------------------------------------------------------------

class Stream {
public:
    // Constructor.
    Stream ();

    // Destructor.
    virtual ~Stream ();

    // Reference counting.
    int incRef () { return ++ref; }
    int decRef () { return --ref; }

    // Get kind of stream.
    virtual StreamKind getKind () = 0;

    // Reset stream to beginning.
    virtual void reset () = 0;

    // Close down the stream.
    virtual void close ();

    // Get next char from stream.
    virtual int getChar () = 0;

    // Peek at next char in stream.
    virtual int lookChar () = 0;

    // Get next char from stream without using the predictor.
    // This is only used by StreamPredictor.
    virtual int getRawChar ();

    // Get exactly <size> bytes from stream.  Returns the number of
    // bytes read -- the returned count will be less than <size> at EOF.
    virtual int getBlock (char* blk, int size);

    // Get next line from stream.
    virtual char* getLine (char* buf, int size);

    // Discard the next <n> bytes from stream.  Returns the number of
    // bytes discarded, which will be less than <n> only if EOF is
    // reached.
    virtual unsigned discardChars (unsigned n);

    // Get current position in file.
    virtual GFileOffset getPos () = 0;

    // Go to a position in the stream.  If <dir> is negative, the
    // position is from the end of the file; otherwise the position is
    // from the start of the file.
    virtual void setPos (GFileOffset pos, int dir = 0) = 0;

    // Get PostScript command for the filter(s).
    virtual GString* getPSFilter (int psLevel, const char* indent);

    // Does this stream type potentially contain non-printable chars?
    virtual bool isBinary (bool last = true) = 0;

    // Get the BaseStream of this stream.
    virtual BaseStream* getBaseStream () = 0;

    // Get the stream after the last decoder (this may be a BaseStream
    // or a DecryptStream).
    virtual Stream* getUndecodedStream () = 0;

    // Get the dictionary associated with this stream.
    virtual Dict* getDict () = 0;

    // Is this an encoding filter?
    virtual bool isEncoder () { return false; }

    // Get image parameters which are defined by the stream contents.
    virtual void
    getImageParams (int* bitsPerComponent, StreamColorSpaceMode* csMode) {}

    // Return the next stream in the "stack".
    virtual Stream* getNextStream () { return NULL; }

    // Add filters to this stream according to the parameters in <dict>.
    // Returns the new stream.
    Stream* addFilters (Object* dict, int recursion = 0);

private:
    Stream* makeFilter (char* name, Stream* str, Object* params, int recursion);

    int ref; // reference count
};

//------------------------------------------------------------------------
// BaseStream
//
// This is the base class for all streams that read directly from a file.
//------------------------------------------------------------------------

class BaseStream : public Stream {
public:
    BaseStream (Object* dictA);
    virtual ~BaseStream ();
    virtual Stream* makeSubStream (
        GFileOffset start, bool limited, GFileOffset length, Object* dict) = 0;
    virtual void setPos (GFileOffset pos, int dir = 0) = 0;
    virtual bool isBinary (bool last = true) { return last; }
    virtual BaseStream* getBaseStream () { return this; }
    virtual Stream* getUndecodedStream () { return this; }
    virtual Dict* getDict () { return dict.getDict (); }
    virtual GString* getFileName () { return NULL; }

    // Get/set position of first byte of stream within the file.
    virtual GFileOffset getStart () = 0;
    virtual void moveStart (int delta) = 0;

private:
    Object dict;
};

//------------------------------------------------------------------------
// FilterStream
//
// This is the base class for all streams that filter another stream.
//------------------------------------------------------------------------

class FilterStream : public Stream {
public:
    FilterStream (Stream* strA);
    virtual ~FilterStream ();
    virtual void close ();
    virtual GFileOffset getPos () { return str->getPos (); }
    virtual void setPos (GFileOffset pos, int dir = 0);
    virtual BaseStream* getBaseStream () { return str->getBaseStream (); }
    virtual Stream* getUndecodedStream () { return str->getUndecodedStream (); }
    virtual Dict* getDict () { return str->getDict (); }
    virtual Stream* getNextStream () { return str; }

protected:
    Stream* str;
};

//------------------------------------------------------------------------
// ImageStream
//------------------------------------------------------------------------

class ImageStream {
public:
    // Create an image stream object for an image with the specified
    // parameters.  Note that these are the actual image parameters,
    // which may be different from the predictor parameters.
    ImageStream (Stream* strA, int widthA, int nCompsA, int nBitsA);

    ~ImageStream ();

    // Reset the stream.
    void reset ();

    // Close down the stream.
    void close ();

    // Gets the next pixel from the stream.  <pix> should be able to hold
    // at least nComps elements.  Returns false at end of file.
    bool getPixel (unsigned char* pix);

    // Returns a pointer to the next line of pixels.  Returns NULL at
    // end of file.
    unsigned char* getLine ();

    // Skip an entire line from the image.
    void skipLine ();

private:
    Stream* str;       // base stream
    int width;         // pixels per line
    int nComps;        // components per pixel
    int nBits;         // bits per component
    int nVals;         // components per line
    int inputLineSize; // input line buffer size
    char* inputLine;   // input line buffer
    unsigned char* imgLine;   // line buffer
    int imgIdx;        // current index in imgLine
};

//------------------------------------------------------------------------
// StreamPredictor
//------------------------------------------------------------------------

class StreamPredictor {
public:
    // Create a predictor object.  Note that the parameters are for the
    // predictor, and may not match the actual image parameters.
    StreamPredictor (
        Stream* strA, int predictorA, int widthA, int nCompsA, int nBitsA);

    ~StreamPredictor ();

    bool isOk () { return ok; }

    void reset ();

    int lookChar ();
    int getChar ();
    int getBlock (char* blk, int size);

private:
    bool getNextLine ();

    Stream* str;      // base stream
    int predictor;    // predictor
    int width;        // pixels per line
    int nComps;       // components per pixel
    int nBits;        // bits per component
    int nVals;        // components per line
    int pixBytes;     // bytes per pixel
    int rowBytes;     // bytes per line
    unsigned char* predLine; // line buffer
    int predIdx;      // current index in predLine
    bool ok;
};

//------------------------------------------------------------------------
// FileStream
//------------------------------------------------------------------------

#define fileStreamBufSize 256

class FileStream : public BaseStream {
public:
    FileStream (
        FILE* fA, GFileOffset startA, bool limitedA, GFileOffset lengthA,
        Object* dictA);
    virtual ~FileStream ();
    virtual Stream* makeSubStream (
        GFileOffset startA, bool limitedA, GFileOffset lengthA, Object* dictA);
    virtual StreamKind getKind () { return strFile; }
    virtual void reset ();
    virtual void close ();
    virtual int getChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr++ & 0xff);
    }
    virtual int lookChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr & 0xff);
    }
    virtual int getBlock (char* blk, int size);
    virtual GFileOffset getPos () { return bufPos + (int)(bufPtr - buf); }
    virtual void setPos (GFileOffset pos, int dir = 0);
    virtual GFileOffset getStart () { return start; }
    virtual void moveStart (int delta);

private:
    bool fillBuf ();

    FILE* f;
    GFileOffset start;
    bool limited;
    GFileOffset length;
    char buf[fileStreamBufSize];
    char* bufPtr;
    char* bufEnd;
    GFileOffset bufPos;
    GFileOffset savePos;
    bool saved;
};

//------------------------------------------------------------------------
// MemStream
//------------------------------------------------------------------------

class MemStream : public BaseStream {
public:
    MemStream (const char* bufA, unsigned startA, unsigned lengthA, Object* dictA);
    virtual ~MemStream ();
    virtual Stream* makeSubStream (
        GFileOffset start, bool limited, GFileOffset lengthA, Object* dictA);
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset ();
    virtual void close ();
    virtual int getChar () {
        return (bufPtr < bufEnd) ? (*bufPtr++ & 0xff) : EOF;
    }
    virtual int lookChar () {
        return (bufPtr < bufEnd) ? (*bufPtr & 0xff) : EOF;
    }
    virtual int getBlock (char* blk, int size);
    virtual GFileOffset getPos () { return (GFileOffset) (bufPtr - buf); }
    virtual void setPos (GFileOffset pos, int dir = 0);
    virtual GFileOffset getStart () { return start; }
    virtual void moveStart (int delta);

private:
    const char* buf;
    unsigned start;
    unsigned length;
    const char* bufEnd;
    const char* bufPtr;
    bool needFree;
};

//------------------------------------------------------------------------
// EmbedStream
//
// This is a special stream type used for embedded streams (inline
// images).  It reads directly from the base stream -- after the
// EmbedStream is deleted, reads from the base stream will proceed where
// the BaseStream left off.  Note that this is very different behavior
// that creating a new FileStream (using makeSubStream).
//------------------------------------------------------------------------

class EmbedStream : public BaseStream {
public:
    EmbedStream (
        Stream* strA, Object* dictA, bool limitedA, GFileOffset lengthA);
    virtual ~EmbedStream ();
    virtual Stream* makeSubStream (
        GFileOffset start, bool limitedA, GFileOffset lengthA, Object* dictA);
    virtual StreamKind getKind () { return str->getKind (); }
    virtual void reset () {}
    virtual int getChar ();
    virtual int lookChar ();
    virtual int getBlock (char* blk, int size);
    virtual GFileOffset getPos () { return str->getPos (); }
    virtual void setPos (GFileOffset pos, int dir = 0);
    virtual GFileOffset getStart ();
    virtual void moveStart (int delta);

private:
    Stream* str;
    bool limited;
    GFileOffset length;
};

//------------------------------------------------------------------------
// ASCIIHexStream
//------------------------------------------------------------------------

class ASCIIHexStream : public FilterStream {
public:
    ASCIIHexStream (Stream* strA);
    virtual ~ASCIIHexStream ();
    virtual StreamKind getKind () { return strASCIIHex; }
    virtual void reset ();
    virtual int getChar () {
        int c = lookChar ();
        buf = EOF;
        return c;
    }
    virtual int lookChar ();
    virtual GString* getPSFilter (int psLevel, const char* indent);
    virtual bool isBinary (bool last = true);

private:
    int buf;
    bool eof;
};

//------------------------------------------------------------------------
// ASCII85Stream
//------------------------------------------------------------------------

class ASCII85Stream : public FilterStream {
public:
    ASCII85Stream (Stream* strA);
    virtual ~ASCII85Stream ();
    virtual StreamKind getKind () { return strASCII85; }
    virtual void reset ();
    virtual int getChar () {
        int ch = lookChar ();
        ++index;
        return ch;
    }
    virtual int lookChar ();
    virtual GString* getPSFilter (int psLevel, const char* indent);
    virtual bool isBinary (bool last = true);

private:
    int c[5];
    int b[4];
    int index, n;
    bool eof;
};

//------------------------------------------------------------------------
// LZWStream
//------------------------------------------------------------------------

class LZWStream : public FilterStream {
public:
    LZWStream (
        Stream* strA, int predictor, int columns, int colors, int bits,
        int earlyA);
    virtual ~LZWStream ();
    virtual StreamKind getKind () { return strLZW; }
    virtual void reset ();
    virtual int getChar ();
    virtual int lookChar ();
    virtual int getRawChar ();
    virtual int getBlock (char* blk, int size);
    virtual GString* getPSFilter (int psLevel, const char* indent);
    virtual bool isBinary (bool last = true);

private:
    StreamPredictor* pred; // predictor
    int early;             // early parameter
    bool eof;             // true if at eof
    int inputBuf;          // input buffer
    int inputBits;         // number of bits in input buffer
    struct {               // decoding table
        int length;
        int head;
        unsigned char tail;
    } table[4097];
    int nextCode;        // next code to be used
    int nextBits;        // number of bits in next code word
    int prevCode;        // previous code used in stream
    int newChar;         // next char to be added to table
    unsigned char seqBuf[4097]; // buffer for current sequence
    int seqLength;       // length of current sequence
    int seqIndex;        // index into current sequence
    bool first;         // first code after a table clear

    bool processNextCode ();
    void clearTable ();
    int getCode ();
};

//------------------------------------------------------------------------
// RunLengthStream
//------------------------------------------------------------------------

class RunLengthStream : public FilterStream {
public:
    RunLengthStream (Stream* strA);
    virtual ~RunLengthStream ();
    virtual StreamKind getKind () { return strRunLength; }
    virtual void reset ();
    virtual int getChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr++ & 0xff);
    }
    virtual int lookChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr & 0xff);
    }
    virtual int getBlock (char* blk, int size);
    virtual GString* getPSFilter (int psLevel, const char* indent);
    virtual bool isBinary (bool last = true);

private:
    char buf[128]; // buffer
    char* bufPtr;  // next char to read
    char* bufEnd;  // end of buffer
    bool eof;

    bool fillBuf ();
};

//------------------------------------------------------------------------
// CCITTFaxStream
//------------------------------------------------------------------------

struct CCITTCodeTable;

class CCITTFaxStream : public FilterStream {
public:
    CCITTFaxStream (
        Stream* strA, int encodingA, bool endOfLineA, bool byteAlignA,
        int columnsA, int rowsA, bool endOfBlockA, bool blackA);
    virtual ~CCITTFaxStream ();
    virtual StreamKind getKind () { return strCCITTFax; }
    virtual void reset ();
    virtual int getChar () {
        int c = lookChar ();
        buf = EOF;
        return c;
    }
    virtual int lookChar ();
    virtual GString* getPSFilter (int psLevel, const char* indent);
    virtual bool isBinary (bool last = true);

private:
    int encoding;     // 'K' parameter
    bool endOfLine;  // 'EndOfLine' parameter
    bool byteAlign;  // 'EncodedByteAlign' parameter
    int columns;      // 'Columns' parameter
    int rows;         // 'Rows' parameter
    bool endOfBlock; // 'EndOfBlock' parameter
    bool black;      // 'BlackIs1' parameter
    bool eof;        // true if at eof
    bool nextLine2D; // true if next line uses 2D encoding
    int row;          // current row
    unsigned inputBuf;   // input buffer
    int inputBits;    // number of bits in input buffer
    int* codingLine;  // coding line changing elements
    int* refLine;     // reference line changing elements
    int a0i;          // index into codingLine
    bool err;        // error on current line
    int outputBits;   // remaining ouput bits
    int buf;          // character buffer

    void addPixels (int a1, int blackPixels);
    void addPixelsNeg (int a1, int blackPixels);
    short getTwoDimCode ();
    short getWhiteCode ();
    short getBlackCode ();
    short lookBits (int n);
    void eatBits (int n) {
        if ((inputBits -= n) < 0) inputBits = 0;
    }
};

//------------------------------------------------------------------------
// DCTStream
//------------------------------------------------------------------------

// DCT component info
struct DCTCompInfo {
    int id;               // component ID
    int hSample, vSample; // horiz/vert sampling resolutions
    int quantTable;       // quantization table number
    int prevDC;           // DC coefficient accumulator
};

struct DCTScanInfo {
    bool comp[4];             // comp[i] is set if component i is
                               //   included in this scan
    int numComps;              // number of components in the scan
    int dcHuffTable[4];        // DC Huffman table numbers
    int acHuffTable[4];        // AC Huffman table numbers
    int firstCoeff, lastCoeff; // first and last DCT coefficient
    int ah, al;                // successive approximation parameters
};

// DCT Huffman decoding table
struct DCTHuffTable {
    unsigned char firstSym[17];   // first symbol for this bit length
    unsigned short firstCode[17]; // first code for this bit length
    unsigned short numCodes[17];  // number of codes of this bit length
    unsigned char sym[256];       // symbols
};

class DCTStream : public FilterStream {
public:
    DCTStream (Stream* strA, bool colorXformA);
    virtual ~DCTStream ();
    virtual StreamKind getKind () { return strDCT; }
    virtual void reset ();
    virtual void close ();
    virtual int getChar ();
    virtual int lookChar ();
    virtual GString* getPSFilter (int psLevel, const char* indent);
    virtual bool isBinary (bool last = true);
    Stream* getRawStream () { return str; }

private:
    bool progressive;            // set if in progressive mode
    bool interleaved;            // set if in interleaved mode
    int width, height;            // image size
    int mcuWidth, mcuHeight;      // size of min coding unit, in data units
    int bufWidth, bufHeight;      // frameBuf size
    DCTCompInfo compInfo[4];      // info for each component
    DCTScanInfo scanInfo;         // info for the current scan
    int numComps;                 // number of components in image
    int colorXform;               // color transform: -1 = unspecified
                                  //                   0 = none
                                  //                   1 = YUV/YUVK -> RGB/CMYK
    bool gotJFIFMarker;          // set if APP0 JFIF marker was present
    bool gotAdobeMarker;         // set if APP14 Adobe marker was present
    int restartInterval;          // restart interval, in MCUs
    unsigned short quantTables[4][64];   // quantization tables
    int numQuantTables;           // number of quantization tables
    DCTHuffTable dcHuffTables[4]; // DC Huffman tables
    DCTHuffTable acHuffTables[4]; // AC Huffman tables
    int numDCHuffTables;          // number of DC Huffman tables
    int numACHuffTables;          // number of AC Huffman tables
    unsigned char* rowBuf;
    unsigned char* rowBufPtr; // current position within rowBuf
    unsigned char* rowBufEnd; // end of valid data in rowBuf
    int* frameBuf[4];  // buffer for frame (progressive mode)
    int comp, x, y;    // current position within image/MCU
    int restartCtr;    // MCUs left until restart
    int restartMarker; // next restart marker
    int eobRun;        // number of EOBs left in the current run
    int inputBuf;      // input buffer for variable length codes
    int inputBits;     // number of valid bits in input buffer

    void restart ();
    bool readMCURow ();
    void readScan ();
    bool readDataUnit (
        DCTHuffTable* dcHuffTable, DCTHuffTable* acHuffTable, int* prevDC,
        int data[64]);
    bool readProgressiveDataUnit (
        DCTHuffTable* dcHuffTable, DCTHuffTable* acHuffTable, int* prevDC,
        int data[64]);
    void decodeImage ();
    void
    transformDataUnit (unsigned short* quantTable, int dataIn[64], unsigned char dataOut[64]);
    int readHuffSym (DCTHuffTable* table);
    int readAmp (int size);
    int readBit ();
    bool readHeader ();
    bool readBaselineSOF ();
    bool readProgressiveSOF ();
    bool readScanInfo ();
    bool readQuantTables ();
    bool readHuffmanTables ();
    bool readRestartInterval ();
    bool readJFIFMarker ();
    bool readAdobeMarker ();
    bool readTrailer ();
    int readMarker ();
    int read16 ();
};

//------------------------------------------------------------------------
// FlateStream
//------------------------------------------------------------------------

#define flateWindow 32768 // buffer size
#define flateMask (flateWindow - 1)
#define flateMaxHuffman 15      // max Huffman code length
#define flateMaxCodeLenCodes 19 // max # code length codes
#define flateMaxLitCodes 288    // max # literal codes
#define flateMaxDistCodes 30    // max # distance codes

// Huffman code table entry
struct FlateCode {
    unsigned short len; // code length, in bits
    unsigned short val; // value represented by this code
};

struct FlateHuffmanTab {
    FlateCode* codes;
    int maxLen;
};

// Decoding info for length and distance code words
struct FlateDecode {
    int bits;  // # extra bits
    int first; // first length/distance
};

class FlateStream : public FilterStream {
public:
    FlateStream (
        Stream* strA, int predictor, int columns, int colors, int bits);
    virtual ~FlateStream ();
    virtual StreamKind getKind () { return strFlate; }
    virtual void reset ();
    virtual int getChar ();
    virtual int lookChar ();
    virtual int getRawChar ();
    virtual int getBlock (char* blk, int size);
    virtual GString* getPSFilter (int psLevel, const char* indent);
    virtual bool isBinary (bool last = true);

private:
    StreamPredictor* pred;   // predictor
    unsigned char buf[flateWindow]; // output data buffer
    int index;               // current index into output buffer
    int remain;              // number valid bytes in output buffer
    int codeBuf;             // input buffer
    int codeSize;            // number of bits in input buffer
    int                      // literal and distance code lengths
        codeLengths[flateMaxLitCodes + flateMaxDistCodes];
    FlateHuffmanTab litCodeTab;  // literal code table
    FlateHuffmanTab distCodeTab; // distance code table
    bool compressedBlock;       // set if reading a compressed block
    int blockLen;                // remaining length of uncompressed block
    bool endOfBlock;            // set when end of block is reached
    bool eof;                   // set when end of stream is reached

    static int // code length code reordering
        codeLenCodeMap[flateMaxCodeLenCodes];
    static FlateDecode // length decoding info
        lengthDecode[flateMaxLitCodes - 257];
    static FlateDecode // distance decoding info
        distDecode[flateMaxDistCodes];
    static FlateHuffmanTab // fixed literal code table
        fixedLitCodeTab;
    static FlateHuffmanTab // fixed distance code table
        fixedDistCodeTab;

    void readSome ();
    bool startBlock ();
    void loadFixedCodes ();
    bool readDynamicCodes ();
    void compHuffmanCodes (int* lengths, int n, FlateHuffmanTab* tab);
    int getHuffmanCodeWord (FlateHuffmanTab* tab);
    int getCodeWord (int bits);
};

//------------------------------------------------------------------------
// EOFStream
//------------------------------------------------------------------------

class EOFStream : public FilterStream {
public:
    EOFStream (Stream* strA);
    virtual ~EOFStream ();
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset () {}
    virtual int getChar () { return EOF; }
    virtual int lookChar () { return EOF; }
    virtual int getBlock (char* blk, int size) { return 0; }
    virtual GString* getPSFilter (int psLevel, const char* indent) {
        return NULL;
    }
    virtual bool isBinary (bool last = true) { return false; }
};

//------------------------------------------------------------------------
// BufStream
//------------------------------------------------------------------------

class BufStream : public FilterStream {
public:
    BufStream (Stream* strA, int bufSizeA);
    virtual ~BufStream ();
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset ();
    virtual int getChar ();
    virtual int lookChar ();
    virtual GString* getPSFilter (int psLevel, const char* indent) {
        return NULL;
    }
    virtual bool isBinary (bool last = true);

    int lookChar (int idx);

private:
    int* buf;
    int bufSize;
};

//------------------------------------------------------------------------
// FixedLengthEncoder
//------------------------------------------------------------------------

class FixedLengthEncoder : public FilterStream {
public:
    FixedLengthEncoder (Stream* strA, int lengthA);
    ~FixedLengthEncoder ();
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset ();
    virtual int getChar ();
    virtual int lookChar ();
    virtual GString* getPSFilter (int psLevel, const char* indent) {
        return NULL;
    }
    virtual bool isBinary (bool last = true);
    virtual bool isEncoder () { return true; }

private:
    int length;
    int count;
};

//------------------------------------------------------------------------
// ASCIIHexEncoder
//------------------------------------------------------------------------

class ASCIIHexEncoder : public FilterStream {
public:
    ASCIIHexEncoder (Stream* strA);
    virtual ~ASCIIHexEncoder ();
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset ();
    virtual int getChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr++ & 0xff);
    }
    virtual int lookChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr & 0xff);
    }
    virtual GString* getPSFilter (int psLevel, const char* indent) {
        return NULL;
    }
    virtual bool isBinary (bool last = true) { return false; }
    virtual bool isEncoder () { return true; }

private:
    char buf[4];
    char* bufPtr;
    char* bufEnd;
    int lineLen;
    bool eof;

    bool fillBuf ();
};

//------------------------------------------------------------------------
// ASCII85Encoder
//------------------------------------------------------------------------

class ASCII85Encoder : public FilterStream {
public:
    ASCII85Encoder (Stream* strA);
    virtual ~ASCII85Encoder ();
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset ();
    virtual int getChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr++ & 0xff);
    }
    virtual int lookChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr & 0xff);
    }
    virtual GString* getPSFilter (int psLevel, const char* indent) {
        return NULL;
    }
    virtual bool isBinary (bool last = true) { return false; }
    virtual bool isEncoder () { return true; }

private:
    char buf[8];
    char* bufPtr;
    char* bufEnd;
    int lineLen;
    bool eof;

    bool fillBuf ();
};

//------------------------------------------------------------------------
// RunLengthEncoder
//------------------------------------------------------------------------

class RunLengthEncoder : public FilterStream {
public:
    RunLengthEncoder (Stream* strA);
    virtual ~RunLengthEncoder ();
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset ();
    virtual int getChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr++ & 0xff);
    }
    virtual int lookChar () {
        return (bufPtr >= bufEnd && !fillBuf ()) ? EOF : (*bufPtr & 0xff);
    }
    virtual GString* getPSFilter (int psLevel, const char* indent) {
        return NULL;
    }
    virtual bool isBinary (bool last = true) { return true; }
    virtual bool isEncoder () { return true; }

private:
    char buf[131];
    char* bufPtr;
    char* bufEnd;
    char* nextEnd;
    bool eof;

    bool fillBuf ();
};

//------------------------------------------------------------------------
// LZWEncoder
//------------------------------------------------------------------------

struct LZWEncoderNode {
    int byte;
    LZWEncoderNode* next;     // next sibling
    LZWEncoderNode* children; // first child
};

class LZWEncoder : public FilterStream {
public:
    LZWEncoder (Stream* strA);
    virtual ~LZWEncoder ();
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset ();
    virtual int getChar ();
    virtual int lookChar ();
    virtual GString* getPSFilter (int psLevel, const char* indent) {
        return NULL;
    }
    virtual bool isBinary (bool last = true) { return true; }
    virtual bool isEncoder () { return true; }

private:
    LZWEncoderNode table[4096];
    int nextSeq;
    int codeLen;
    unsigned char inBuf[4096];
    int inBufLen;
    int outBuf;
    int outBufLen;
    bool needEOD;

    void fillBuf ();
};

#endif

//========================================================================
//
// JArithmeticDecoder.h
//
// Arithmetic decoder used by the JBIG2 and JPEG2000 decoders.
//
// Copyright 2002-2004 Glyph & Cog, LLC
//
//========================================================================

#ifndef JARITHMETICDECODER_H
#define JARITHMETICDECODER_H

#include <defs.hh>


class Stream;

//------------------------------------------------------------------------
// JArithmeticDecoderStats
//------------------------------------------------------------------------

class JArithmeticDecoderStats {
public:
    JArithmeticDecoderStats (int contextSizeA);
    ~JArithmeticDecoderStats ();
    JArithmeticDecoderStats* copy ();
    void reset ();
    int getContextSize () { return contextSize; }
    void copyFrom (JArithmeticDecoderStats* stats);
    void setEntry (unsigned cx, int i, int mps);

private:
    unsigned char* cxTab; // cxTab[cx] = (i[cx] << 1) + mps[cx]
    int contextSize;

    friend class JArithmeticDecoder;
};

//------------------------------------------------------------------------
// JArithmeticDecoder
//------------------------------------------------------------------------

class JArithmeticDecoder {
public:
    JArithmeticDecoder ();
    ~JArithmeticDecoder ();

    void setStream (Stream* strA) {
        str = strA;
        dataLen = 0;
        limitStream = false;
    }
    void setStream (Stream* strA, int dataLenA) {
        str = strA;
        dataLen = dataLenA;
        limitStream = true;
    }

    // Start decoding on a new stream.  This fills the byte buffers and
    // runs INITDEC.
    void start ();

    // Restart decoding on an interrupted stream.  This refills the
    // buffers if needed, but does not run INITDEC.  (This is used in
    // JPEG 2000 streams when codeblock data is split across multiple
    // packets/layers.)
    void restart (int dataLenA);

    // Read any leftover data in the stream.
    void cleanup ();

    // Decode one bit.
    int decodeBit (unsigned context, JArithmeticDecoderStats* stats);

    // Decode eight bits.
    int decodeByte (unsigned context, JArithmeticDecoderStats* stats);

    // Returns false for OOB, otherwise sets *<x> and returns true.
    bool decodeInt (int* x, JArithmeticDecoderStats* stats);

    unsigned decodeIAID (unsigned codeLen, JArithmeticDecoderStats* stats);

    void resetByteCounter () { nBytesRead = 0; }
    unsigned getByteCounter () { return nBytesRead; }

private:
    unsigned readByte ();
    int decodeIntBit (JArithmeticDecoderStats* stats);
    void byteIn ();

    static unsigned qeTab[47];
    static int nmpsTab[47];
    static int nlpsTab[47];
    static int switchTab[47];

    unsigned buf0, buf1;
    unsigned c, a;
    int ct;

    unsigned prev; // for the integer decoder

    Stream* str;
    unsigned nBytesRead;
    int dataLen;
    bool limitStream;
    int readBuf;
};

#endif

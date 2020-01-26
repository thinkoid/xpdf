// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_DECRYPT_HH
#define XPDF_XPDF_DECRYPT_HH

#include <defs.hh>

#include <goo/GString.hh>
#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>

//------------------------------------------------------------------------
// Decrypt
//------------------------------------------------------------------------

class Decrypt {
public:
    // Generate a file key.  The <fileKey> buffer must have space for at
    // least 16 bytes.  Checks <ownerPassword> and then <userPassword>
    // and returns true if either is correct.  Sets <ownerPasswordOk> if
    // the owner password was correct.  Either or both of the passwords
    // may be NULL, which is treated as an empty string.
    static bool makeFileKey (
        int encVersion, int encRevision, int keyLength, GString* ownerKey,
        GString* userKey, GString* ownerEnc, GString* userEnc, int permissions,
        GString* fileID, GString* ownerPassword, GString* userPassword,
        unsigned char* fileKey, bool encryptMetadata, bool* ownerPasswordOk);

private:
    static void r6Hash (
        unsigned char* key, int keyLen, const char* pwd, int pwdLen, const char* userKey);

    static bool makeFileKey2 (
        int encVersion, int encRevision, int keyLength, GString* ownerKey,
        GString* userKey, int permissions, GString* fileID,
        GString* userPassword, unsigned char* fileKey, bool encryptMetadata);
};

//------------------------------------------------------------------------
// DecryptStream
//------------------------------------------------------------------------

struct DecryptRC4State {
    unsigned char state[256];
    unsigned char x, y;
    int buf;
};

struct DecryptAESState {
    unsigned w[44];
    unsigned char state[16];
    unsigned char cbc[16];
    unsigned char buf[16];
    int bufIdx;
};

struct DecryptAES256State {
    unsigned w[60];
    unsigned char state[16];
    unsigned char cbc[16];
    unsigned char buf[16];
    int bufIdx;
};

class DecryptStream : public FilterStream {
public:
    DecryptStream (
        Stream* strA, unsigned char* fileKey, CryptAlgorithm algoA, int keyLength,
        int objNum, int objGen);
    virtual ~DecryptStream ();
    virtual StreamKind getKind () { return strWeird; }
    virtual void reset ();
    virtual int getChar ();
    virtual int lookChar ();
    virtual bool isBinary (bool last);
    virtual Stream* getUndecodedStream () { return this; }

private:
    CryptAlgorithm algo;
    int objKeyLength;
    unsigned char objKey[32];

    union {
        DecryptRC4State rc4;
        DecryptAESState aes;
        DecryptAES256State aes256;
    } state;
};

//------------------------------------------------------------------------

struct MD5State {
    size_t a, b, c, d;
    unsigned char buf[64];
    int bufLen;
    int msgLen;
    unsigned char digest[16];
};

extern void rc4InitKey (unsigned char* key, int keyLen, unsigned char* state);
extern unsigned char rc4DecryptByte (unsigned char* state, unsigned char* x, unsigned char* y, unsigned char c);
void md5Start (MD5State* state);
void md5Append (MD5State* state, unsigned char* data, int dataLen);
void md5Finish (MD5State* state);
extern void md5 (unsigned char* msg, int msgLen, unsigned char* digest);
extern void aesKeyExpansion (
    DecryptAESState* s, unsigned char* objKey, int objKeyLen, bool decrypt);
extern void aesEncryptBlock (DecryptAESState* s, unsigned char* in);
extern void aesDecryptBlock (DecryptAESState* s, unsigned char* in, bool last);

#endif // XPDF_XPDF_DECRYPT_HH

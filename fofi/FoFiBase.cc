//========================================================================
//
// FoFiBase.cc
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <cstdio>
#include <cstdlib>
#include <climits>

#include <fofi/FoFiBase.hh>

//------------------------------------------------------------------------
// FoFiBase
//------------------------------------------------------------------------

FoFiBase::FoFiBase (char* fileA, int lenA, bool freeFileDataA) {
    fileData = file = (unsigned char*)fileA;
    len = lenA;
    freeFileData = freeFileDataA;
}

FoFiBase::~FoFiBase () {
    if (freeFileData) { free (fileData); }
}

char* FoFiBase::readFile (const char* fileName, int* fileLen) {
    FILE* f;
    char* buf;
    int n;

    if (!(f = fopen (fileName, "rb"))) { return NULL; }
    fseek (f, 0, SEEK_END);
    n = (int)ftell (f);
    if (n < 0) {
        fclose (f);
        return NULL;
    }
    fseek (f, 0, SEEK_SET);
    buf = (char*)malloc (n);
    if ((int)fread (buf, 1, n, f) != n) {
        free (buf);
        fclose (f);
        return NULL;
    }
    fclose (f);
    *fileLen = n;
    return buf;
}

int FoFiBase::getS8 (int pos, bool* ok) {
    int x;

    if (pos < 0 || pos >= len) {
        *ok = false;
        return 0;
    }
    x = file[pos];
    if (x & 0x80) { x |= ~0xff; }
    return x;
}

int FoFiBase::getU8 (int pos, bool* ok) {
    if (pos < 0 || pos >= len) {
        *ok = false;
        return 0;
    }
    return file[pos];
}

int FoFiBase::getS16BE (int pos, bool* ok) {
    int x;

    if (pos < 0 || pos > INT_MAX - 1 || pos + 1 >= len) {
        *ok = false;
        return 0;
    }
    x = file[pos];
    x = (x << 8) + file[pos + 1];
    if (x & 0x8000) { x |= ~0xffff; }
    return x;
}

int FoFiBase::getU16BE (int pos, bool* ok) {
    int x;

    if (pos < 0 || pos > INT_MAX - 1 || pos + 1 >= len) {
        *ok = false;
        return 0;
    }
    x = file[pos];
    x = (x << 8) + file[pos + 1];
    return x;
}

int FoFiBase::getS32BE (int pos, bool* ok) {
    int x;

    if (pos < 0 || pos > INT_MAX - 3 || pos + 3 >= len) {
        *ok = false;
        return 0;
    }
    x = file[pos];
    x = (x << 8) + file[pos + 1];
    x = (x << 8) + file[pos + 2];
    x = (x << 8) + file[pos + 3];
    if (x & 0x80000000) { x |= ~0xffffffff; }
    return x;
}

unsigned FoFiBase::getU32BE (int pos, bool* ok) {
    unsigned x;

    if (pos < 0 || pos > INT_MAX - 3 || pos + 3 >= len) {
        *ok = false;
        return 0;
    }
    x = file[pos];
    x = (x << 8) + file[pos + 1];
    x = (x << 8) + file[pos + 2];
    x = (x << 8) + file[pos + 3];
    return x;
}

unsigned FoFiBase::getU32LE (int pos, bool* ok) {
    unsigned x;

    if (pos < 0 || pos > INT_MAX - 3 || pos + 3 >= len) {
        *ok = false;
        return 0;
    }
    x = file[pos + 3];
    x = (x << 8) + file[pos + 2];
    x = (x << 8) + file[pos + 1];
    x = (x << 8) + file[pos];
    return x;
}

unsigned FoFiBase::getUVarBE (int pos, int size, bool* ok) {
    unsigned x;
    int i;

    if (pos < 0 || pos > INT_MAX - size || pos + size > len) {
        *ok = false;
        return 0;
    }
    x = 0;
    for (i = 0; i < size; ++i) { x = (x << 8) + file[pos + i]; }
    return x;
}

bool FoFiBase::checkRegion (int pos, int size) {
    return pos >= 0 && pos + size >= pos && pos + size <= len;
}

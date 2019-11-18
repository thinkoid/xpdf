/*
 * gmem.c
 *
 * Memory routines with out-of-memory checking.
 *
 * Copyright 1996-2003 Glyph & Cog, LLC
 */

#include <defs.hh>

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <goo/gmem.hh>

void* gmalloc (int size) {
    void* p;

    if (size < 0) { gMemError ("Invalid memory allocation size"); }
    if (size == 0) { return NULL; }
    if (!(p = malloc (size))) { gMemError ("Out of memory"); }
    return p;
}

void* grealloc (void* p, int size) {
    void* q;

    if (size < 0) { gMemError ("Invalid memory allocation size"); }
    if (size == 0) {
        if (p) { free (p); }
        return NULL;
    }
    if (p) { q = realloc (p, size); }
    else {
        q = malloc (size);
    }
    if (!q) { gMemError ("Out of memory"); }
    return q;
}

void* gmallocn (int nObjs, int objSize) {
    int n;

    if (nObjs == 0) { return NULL; }
    n = nObjs * objSize;
    if (objSize <= 0 || nObjs < 0 || nObjs >= INT_MAX / objSize) {
        gMemError ("Bogus memory allocation size");
    }
    return gmalloc (n);
}

void* greallocn (void* p, int nObjs, int objSize) {
    int n;

    if (nObjs == 0) {
        if (p) { gfree (p); }
        return NULL;
    }
    n = nObjs * objSize;
    if (objSize <= 0 || nObjs < 0 || nObjs >= INT_MAX / objSize) {
        gMemError ("Bogus memory allocation size");
    }
    return grealloc (p, n);
}

void gfree (void* p) {
    if (p) {
        free (p);
    }
}

void gMemError (const char* msg) {
    fprintf (stderr, "%s\n", msg);
    exit (1);
}

char* copyString (const char* s) {
    char* s1 = (char*)gmalloc ((int)strlen (s) + 1);
    strcpy (s1, s);
    return s1;
}

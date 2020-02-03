// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_UTILS_PARSEARGS_HH
#define XPDF_UTILS_PARSEARGS_HH

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Argument kinds.
 */
typedef enum {
    argFlag,   /* flag (present / not-present) */
               /*   [val: bool *]             */
    argInt,    /* integer arg    */
               /*   [val: int *] */
    argFP,     /* floating point arg */
               /*   [val: double *]  */
    argString, /* string arg      */
               /*   [val: char *] */
    /* dummy entries -- these show up in the usage listing only; */
    /* useful for X args, for example                            */
    argFlagDummy,
    argIntDummy,
    argFPDummy,
    argStringDummy
} ArgKind;

/*
 * Argument descriptor.
 */
typedef struct {
    const char* arg;   /* the command line switch */
    ArgKind kind;      /* kind of arg */
    void* val;         /* place to store value */
    int size;          /* for argString: size of string */
    const char* usage; /* usage string */
} ArgDesc;

/*
 * Parse command line.  Removes all args which are found in the arg
 * descriptor list <args>.  Stops parsing if "--" is found (and removes
 * it).  Returns false if there was an error.
 */
extern bool parseArgs (ArgDesc* args, int* argc, char* argv[]);

/*
 * Print usage message, based on arg descriptor list.
 */
extern void
printUsage (const char* program, const char* otherArgs, ArgDesc* args);

/*
 * Check if a string is a valid integer or floating point number.
 */
extern bool isInt (char* s);
extern bool isFP (char* s);

#ifdef __cplusplus
}
#endif

#endif // XPDF_UTILS_PARSEARGS_HH

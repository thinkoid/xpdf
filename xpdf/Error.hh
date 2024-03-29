// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_ERROR_HH
#define XPDF_XPDF_ERROR_HH

#include <defs.hh>

#include <cstdio>
#include <defs.hh>
#include <utils/path.hh>

enum ErrorCategory {
    errSyntaxWarning, // PDF syntax error which can be worked around;
    //   output will probably be correct
    errSyntaxError, // PDF syntax error which cannot be worked around;
    //   output will probably be incorrect
    errConfig, // error in Xpdf config info (xpdfrc file, etc.)
    errCommandLine, // error in user-supplied parameters, action not
    //   allowed, etc. (only used by command-line tools)
    errIO, // error in file I/O
    errNotAllowed, // action not allowed by PDF permission bits
    errUnimplemented, // unimplemented PDF feature - display will be
    //   incorrect
    errInternal // internal error - malfunction within the Xpdf code
};

extern void setErrorCallback(void (*cbk)(void *data, ErrorCategory category,
                                         int pos, char *msg),
                             void *data);

extern void error(ErrorCategory category, off_t pos, const char *msg, ...);

#endif // XPDF_XPDF_ERROR_HH

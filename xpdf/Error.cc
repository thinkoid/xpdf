// -*- mode: c++; -*-
// Copyright 1996-2013 Glyph & Cog, LLC

#include <defs.hh>

#include <cstdio>
#include <cstddef>
#include <cstdarg>
#include <utils/string.hh>
#include <xpdf/GlobalParams.hh>
#include <xpdf/Error.hh>

static const char *errorCategoryNames[] = {
    "Syntax Warning",        "Syntax Error",  "Config Error",
    "Command Line Error",    "I/O Error",     "Permission Error",
    "Unimplemented Feature", "Internal Error"
};

static void (*errorCbk)(void *data, ErrorCategory category, int pos,
                        const char *msg) = 0;

static void *errorCbkData = 0;

void setErrorCallback(void (*cbk)(void *data, ErrorCategory category, int pos,
                                  const char *msg),
                      void *data)
{
    errorCbk = cbk;
    errorCbkData = data;
}

void error(ErrorCategory category, off_t pos, const char *msg, ...)
{
    va_list  args;
    GString *s, *sanitized;
    char     c;
    int      i;

    // NB: this can be called before the globalParams object is created
    if (!errorCbk && globalParams && globalParams->getErrQuiet()) {
        return;
    }
    va_start(args, msg);
    s = GString::formatv(msg, args);
    va_end(args);

    // remove non-printable characters, just in case they might cause
    // problems for the terminal program
    sanitized = new GString();
    for (i = 0; i < s->getLength(); ++i) {
        c = (*s)[i];
        if (c >= 0x20 && c <= 0x7e) {
            sanitized->append(1UL, c);
        } else {
            sanitized->appendf("<{0:02x}>", c & 0xff);
        }
    }

    if (errorCbk) {
        (*errorCbk)(errorCbkData, category, (int)pos, sanitized->c_str());
    } else {
        if (pos >= 0) {
            fprintf(stderr, "%s (%d): %s\n", errorCategoryNames[category],
                    (int)pos, sanitized->c_str());
        } else {
            fprintf(stderr, "%s: %s\n", errorCategoryNames[category],
                    sanitized->c_str());
        }
        fflush(stderr);
    }

    delete s;
    delete sanitized;
}

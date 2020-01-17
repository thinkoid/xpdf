// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_GOO_GFILE_HH
#define XPDF_GOO_GFILE_HH

#include <cstdio>
#include <cstdlib>
#include <cstddef>

#include <unistd.h>
#include <sys/types.h>

#include <dirent.h>
#define NAMLEN(d) strlen ((d)->d_name)

class GString;

//------------------------------------------------------------------------

// Get home directory path.
extern GString* getHomeDir ();

// Get current directory.
extern GString* getCurrentDir ();

// Append a file name to a path string.  <path> may be an empty
// string, denoting the current directory).  Returns <path>.
extern GString* appendToPath (GString* path, const char* fileName);

// Grab the path from the front of the file name.  If there is no
// directory component in <fileName>, returns an empty string.
extern GString* grabPath (const char* fileName);

// Is this an absolute path or file name?
extern bool isAbsolutePath (const char* path);

// Make this path absolute by prepending current directory (if path is
// relative) or prepending user's directory (if path starts with '~').
extern GString* makePathAbsolute (GString* path);

// Get the modification time for <fileName>.  Returns 0 if there is an
// error.
extern time_t getModTime (const char* fileName);

// Create a temporary file and open it for writing.  If <ext> is not
// NULL, it will be used as the file name extension.  Returns both the
// name and the file pointer.  For security reasons, all writing
// should be done to the returned file pointer; the file may be
// reopened later for reading, but not for writing.  The <mode> string
// should be "w" or "wb".  Returns true on success.
extern bool
openTempFile (GString** name, FILE** f, const char* mode);

// Create a directory.  Returns true on success.
extern bool createDir (char* path, int mode);

// Execute <command>.  Returns true on success.
extern bool executeCommand (char* cmd);

// Open a file.  On Windows, this converts the path from UTF-8 to
// UCS-2 and calls _wfopen (if available).  On other OSes, this simply
// calls fopen.
extern FILE* openFile (const char* path, const char* mode);

// Just like fgets, but handles Unix, Mac, and/or DOS end-of-line
// conventions.
extern char* getLine (char* buf, int size, FILE* f);

typedef off_t GFileOffset;
#define GFILEOFFSET_MAX 0x7fffffffffffffffLL

// Like fseek, but uses a 64-bit file offset if available.
extern int gfseek (FILE* f, GFileOffset offset, int whence);

// Like ftell, but returns a 64-bit file offset if available.
extern GFileOffset gftell (FILE* f);

//------------------------------------------------------------------------
// GDir and GDirEntry
//------------------------------------------------------------------------

class GDirEntry {
public:
    GDirEntry (const char* dirPath, char* nameA, bool doStat);
    ~GDirEntry ();
    GString* getName () { return name; }
    bool isDir () { return dir; }

private:
    GString* name; // dir/file name
    bool dir;     // is it a directory?
};

class GDir {
public:
    GDir (char* name, bool doStatA = true);
    ~GDir ();
    GDirEntry* getNextEntry ();
    void rewind ();

private:
    GString* path; // directory path
    bool doStat;  // call stat() for each entry?
    DIR* dir;      // the DIR structure from opendir()
};

#endif // XPDF_GOO_GFILE_HH

//========================================================================
//
// gfile.h
//
// Miscellaneous file and directory name manipulation.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef GFILE_H
#define GFILE_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <unistd.h>
#include <sys/types.h>

#include <dirent.h>
#define NAMLEN(d) strlen ((d)->d_name)

#include <goo/gtypes.hh>

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
extern GString* grabPath (char* fileName);

// Is this an absolute path or file name?
extern GBool isAbsolutePath (char* path);

// Make this path absolute by prepending current directory (if path is
// relative) or prepending user's directory (if path starts with '~').
extern GString* makePathAbsolute (GString* path);

// Get the modification time for <fileName>.  Returns 0 if there is an
// error.
extern time_t getModTime (char* fileName);

// Create a temporary file and open it for writing.  If <ext> is not
// NULL, it will be used as the file name extension.  Returns both the
// name and the file pointer.  For security reasons, all writing
// should be done to the returned file pointer; the file may be
// reopened later for reading, but not for writing.  The <mode> string
// should be "w" or "wb".  Returns true on success.
extern GBool
openTempFile (GString** name, FILE** f, const char* mode, const char* ext);

// Create a directory.  Returns true on success.
extern GBool createDir (char* path, int mode);

// Execute <command>.  Returns true on success.
extern GBool executeCommand (char* cmd);

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
    GDirEntry (char* dirPath, char* nameA, GBool doStat);
    ~GDirEntry ();
    GString* getName () { return name; }
    GBool isDir () { return dir; }

private:
    GString* name; // dir/file name
    GBool dir;     // is it a directory?
};

class GDir {
public:
    GDir (char* name, GBool doStatA = gTrue);
    ~GDir ();
    GDirEntry* getNextEntry ();
    void rewind ();

private:
    GString* path; // directory path
    GBool doStat;  // call stat() for each entry?
    DIR* dir;      // the DIR structure from opendir()
};

#endif

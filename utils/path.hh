// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_UTILS_PATH_HH
#define XPDF_UTILS_PATH_HH

#include <cstdio>
#include <cstdlib>
#include <cstddef>

#include <unistd.h>
#include <sys/types.h>

#include <dirent.h>
#define NAMLEN(d) strlen((d)->d_name)

#include <filesystem>
namespace fs = std::filesystem;

class GString;

// Get home directory path.
namespace xpdf {

fs::path home_path();
fs::path expand_path(const fs::path &);

inline bool is_absolute_path(const fs::path &path)
{
    return path.is_absolute();
}

fs::path make_temp_path();

} // namespace xpdf

// Get current directory.
extern GString *getCurrentDir();

// Append a file name to a path string.  <path> may be an empty
// string, denoting the current directory).  Returns <path>.
extern GString *appendToPath(GString *path, const char *fileName);

// Grab the path from the front of the file name.  If there is no
// directory component in <fileName>, returns an empty string.
extern GString *grabPath(const char *fileName);

// Get the modification time for <fileName>.  Returns 0 if there is an
// error.
extern time_t getModTime(const char *fileName);

// Create a directory.  Returns true on success.
extern bool createDir(char *path, int mode);

// Execute <command>.  Returns true on success.
extern bool executeCommand(char *cmd);

// Open a file.  On Windows, this converts the path from UTF-8 to
// UCS-2 and calls _wfopen (if available).  On other OSes, this simply
// calls fopen.
extern FILE *openFile(const char *path, const char *mode);

// Just like fgets, but handles Unix, Mac, and/or DOS end-of-line
// conventions.
extern char *getLine(char *buf, int size, FILE *f);

typedef off_t GFileOffset;
#define GFILEOFFSET_MAX 0x7fffffffffffffffLL

// Like fseek, but uses a 64-bit file offset if available.
extern int gfseek(FILE *f, GFileOffset offset, int whence);

// Like ftell, but returns a 64-bit file offset if available.
extern GFileOffset gftell(FILE *f);

//------------------------------------------------------------------------
// GDir and GDirEntry
//------------------------------------------------------------------------

// class GDirEntry
// {
// public:
//     GDirEntry(const char *dirPath, char *nameA, bool doStat);
//     ~GDirEntry();
//     GString *getName() { return name; }
//     bool     isDir() { return dir; }

// private:
//     GString *name; // dir/file name
//     bool     dir; // is it a directory?
// };

// class GDir
// {
// public:
//     GDir(char *name, bool doStatA = true);
//     ~GDir();
//     GDirEntry *getNextEntry();
//     void       rewind();

// private:
//     GString *path; // directory path
//     bool     doStat; // call stat() for each entry?
//     DIR *    dir; // the DIR structure from opendir()
// };

#endif // XPDF_UTILS_PATH_HH

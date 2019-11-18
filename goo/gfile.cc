//========================================================================
//
// gfile.cc
//
// Miscellaneous file and directory name manipulation.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <ctime>
#include <climits>
#include <cstring>

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <goo/GString.hh>
#include <goo/gfile.hh>

#include <string>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

// Some systems don't define this, so just make it something reasonably
// large.
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

//------------------------------------------------------------------------

GString* getHomeDir () {
    char* s;
    struct passwd* pw;
    GString* ret;

    if ((s = getenv ("HOME"))) { ret = new GString (s); }
    else {
        if ((s = getenv ("USER")))
            pw = getpwnam (s);
        else
            pw = getpwuid (getuid ());
        if (pw)
            ret = new GString (pw->pw_dir);
        else
            ret = new GString (".");
    }
    return ret;
}

GString* getCurrentDir () {
    char buf[PATH_MAX + 1];

    if (getcwd (buf, sizeof (buf))) return new GString (buf);
    return new GString ();
}

GString* appendToPath (GString* path, const char* fileName) {
    int i;

    // appending "." does nothing
    if (!strcmp (fileName, ".")) return path;

    // appending ".." goes up one directory
    if (!strcmp (fileName, "..")) {
        for (i = path->getLength () - 2; i >= 0; --i) {
            if (path->getChar (i) == '/') break;
        }
        if (i <= 0) {
            if (path->getChar (0) == '/') {
                path->del (1, path->getLength () - 1);
            }
            else {
                path->clear ();
                path->append ("..");
            }
        }
        else {
            path->del (i, path->getLength () - i);
        }
        return path;
    }

    // otherwise, append "/" and new path component
    if (path->getLength () > 0 && path->getChar (path->getLength () - 1) != '/')
        path->append ('/');
    path->append (fileName);
    return path;
}

GString* grabPath (const char* fileName) {
    return new GString (fs::path (fileName).parent_path ().native ());
}

GBool isAbsolutePath (const char* path) {
    return path [0] == '/';
}

GString* makePathAbsolute (GString* path) {
    struct passwd* pw;
    char buf[PATH_MAX + 1];
    GString* s;
    int n;

    if (path->getChar (0) == '~') {
        if (path->getChar (1) == '/' || path->getLength () == 1) {
            path->del (0, 1);
            s = getHomeDir ();
            path->insert (0, s);
            delete s;
        }
        else {
            const char* p1 = path->c_str () + 1, *p2;
            for (p2 = p1; *p2 && *p2 != '/'; ++p2) ;

            if ((n = p2 - p1) > PATH_MAX)
                n = PATH_MAX;

            strncpy (buf, p1, n);
            buf[n] = '\0';

            if ((pw = getpwnam (buf))) {
                path->del (0, p2 - p1 + 1);
                path->insert (0, pw->pw_dir);
            }
        }
    }
    else if (!isAbsolutePath (path->c_str ())) {
        if (getcwd (buf, sizeof (buf))) {
            path->insert (0, '/');
            path->insert (0, buf);
        }
    }
    return path;
}

time_t getModTime (const char* fileName) {
    struct stat statBuf;

    if (stat (fileName, &statBuf)) { return 0; }
    return statBuf.st_mtime;
}

GBool openTempFile (
    GString** name, FILE** f, const char* mode, const char* ext) {
    assert (f && 0 == f[0]);
    assert (name && 0 == name [0]);

    auto p = fs::temp_directory_path () / fs::unique_path ();

    if (ext && ext [0]) {
        p += ext;
    }

    if (FILE* pf = fopen (p.native ().c_str (), mode)) {
        try {
            name [0] = new GString (p.native ());
            f [0] = pf;
        }
        catch (...) {
            if (pf) {
                fclose (pf);
            }

            return gFalse;
        }
    }

    return gTrue;
}

GBool createDir (char* path, int mode) { return !mkdir (path, mode); }

GBool executeCommand (char* cmd) { return system (cmd) ? gFalse : gTrue; }

FILE* openFile (const char* path, const char* mode) {
    return fopen (path, mode);
}

char* getLine (char* buf, int size, FILE* f) {
    int c, i;

    i = 0;
    while (i < size - 1) {
        if ((c = fgetc (f)) == EOF) { break; }
        buf[i++] = (char)c;
        if (c == '\x0a') { break; }
        if (c == '\x0d') {
            c = fgetc (f);
            if (c == '\x0a' && i < size - 1) { buf[i++] = (char)c; }
            else if (c != EOF) {
                ungetc (c, f);
            }
            break;
        }
    }
    buf[i] = '\0';
    if (i == 0) { return NULL; }
    return buf;
}

int gfseek (FILE* f, GFileOffset offset, int whence) {
    return fseek (f, offset, whence);
}

GFileOffset gftell (FILE* f) { return ftell (f); }

//------------------------------------------------------------------------
// GDir and GDirEntry
//------------------------------------------------------------------------

GDirEntry::GDirEntry (const char* dirPath, char* nameA, GBool doStat) {
    struct stat st;
    GString* s;

    name = new GString (nameA);
    dir = gFalse;

    if (doStat) {
        s = new GString (dirPath);
        appendToPath (s, nameA);
        if (stat (s->c_str (), &st) == 0) dir = S_ISDIR (st.st_mode);
        delete s;
    }
}

GDirEntry::~GDirEntry () { delete name; }

GDir::GDir (char* name, GBool doStatA) {
    path = new GString (name);
    doStat = doStatA;
    dir = opendir (name);
}

GDir::~GDir () {
    delete path;
    if (dir) closedir (dir);
}

GDirEntry* GDir::getNextEntry () {
    GDirEntry* e;

    struct dirent* ent;
    e = NULL;
    if (dir) {
        ent = (struct dirent*)readdir (dir);
        if (ent && !strcmp (ent->d_name, ".")) {
            ent = (struct dirent*)readdir (dir);
        }
        if (ent) {
            e = new GDirEntry (path->c_str (), ent->d_name, doStat);
        }
    }

    return e;
}

void GDir::rewind () {
    if (dir) rewinddir (dir);
}

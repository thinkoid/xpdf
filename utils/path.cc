// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#include <defs.hh>

#include <ctime>
#include <climits>
#include <cstring>

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wordexp.h>

#include <utils/string.hh>
#include <utils/path.hh>

#include <random>
#include <string>

#include <filesystem>
namespace fs = std::filesystem;

// Some systems don't define this, so just make it something reasonably
// large.
#ifndef PATH_MAX
#  define PATH_MAX 1024
#endif

//------------------------------------------------------------------------

namespace xpdf {
namespace detail {

std::string random_string(size_t n)
{
    static auto &arr =
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    thread_local static std::mt19937 rg{std::random_device()()};

    thread_local static std::uniform_int_distribution< size_t > pick(
        0, sizeof arr - 2);

    std::string s;
    s.reserve(n);

    while(n--)
        s += arr[pick(rg)];

    return s;
}

} // namespace detail

fs::path home_path()
{
    if (const char *s = getenv("HOME")) {
        return fs::path(s);
    } else {
        struct passwd *p = 0;

        if (const char *s = getenv("USER"))
            p = getpwnam(s);
        else
            p = getpwuid(getuid());

        return p ? fs::path(p->pw_dir) : fs::path(".");
    }
}

fs::path expand_path(const fs::path &path)
{
    wordexp_t w{ };

    std::unique_ptr< ::wordexp_t, void(*)(::wordexp_t*) > guard(
        &w, ::wordfree);

    int result = wordexp(
        path.c_str(), &w, WRDE_NOCMD | WRDE_SHOWERR | WRDE_UNDEF);

    if (0 == result && 1 == w.we_wordc)
        return fs::path(w.we_wordv[0]);

    return path;
}

fs::path make_temp_path()
{
    return fs::temp_directory_path() / detail::random_string(10);
}

} // namespace xpdf

GString *getCurrentDir()
{
    char buf[PATH_MAX + 1];

    if (getcwd(buf, sizeof(buf)))
        return new GString(buf);
    return new GString();
}

GString *appendToPath(GString *path, const char *fileName)
{
    int i;

    // appending "." does nothing
    if (!strcmp(fileName, "."))
        return path;

    // appending ".." goes up one directory
    if (!strcmp(fileName, "..")) {
        for (i = (*path)[path->size() - 2]; i >= 0; --i) {
            if ((*path)[i] == '/')
                break;
        }
        if (i <= 0) {
            if (path->front() == '/') {
                path->del(1, path->getLength() - 1);
            } else {
                path->clear();
                path->append("..");
            }
        } else {
            path->del(i, path->getLength() - i);
        }
        return path;
    }

    // otherwise, append "/" and new path component
    if (path->getLength() > 0 && path->back() != '/')
        path->append(1UL, '/');

    path->append(fileName);

    return path;
}

GString *grabPath(const char *fileName)
{
    return new GString(fs::path(fileName).parent_path().native());
}

time_t getModTime(const char *fileName)
{
    struct stat statBuf;

    if (stat(fileName, &statBuf)) {
        return 0;
    }
    return statBuf.st_mtime;
}

bool createDir(char *path, int mode)
{
    return !mkdir(path, mode);
}

bool executeCommand(char *cmd)
{
    return system(cmd) ? false : true;
}

FILE *openFile(const char *path, const char *mode)
{
    return fopen(path, mode);
}

char *getLine(char *buf, int size, FILE *f)
{
    int c, i;

    i = 0;
    while (i < size - 1) {
        if ((c = fgetc(f)) == EOF) {
            break;
        }
        buf[i++] = (char)c;
        if (c == '\x0a') {
            break;
        }
        if (c == '\x0d') {
            c = fgetc(f);
            if (c == '\x0a' && i < size - 1) {
                buf[i++] = (char)c;
            } else if (c != EOF) {
                ungetc(c, f);
            }
            break;
        }
    }
    buf[i] = '\0';
    if (i == 0) {
        return NULL;
    }
    return buf;
}

int gfseek(FILE *f, GFileOffset offset, int whence)
{
    return fseek(f, offset, whence);
}

GFileOffset gftell(FILE *f)
{
    return ftell(f);
}

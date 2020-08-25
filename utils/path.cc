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

#include <limits>
#include <random>
#include <string>

#include <filesystem>
namespace fs = std::filesystem;

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

std::time_t last_write_time(const fs::path &path)
{
    using limits_type = std::numeric_limits< std::time_t >;

    struct stat st{ };

    if (0 == stat(path.c_str(), &st))
        return st.st_mtime;

    return (limits_type::min)();
}

} // namespace xpdf

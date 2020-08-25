// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_UTILS_PATH_HH
#define XPDF_UTILS_PATH_HH

#include <ctime>

#include <filesystem>
namespace fs = std::filesystem;

namespace xpdf {

fs::path expand_path(const fs::path &);
std::time_t last_write_time(const fs::path &);

} // namespace xpdf

#endif // XPDF_UTILS_PATH_HH

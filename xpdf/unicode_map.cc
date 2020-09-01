// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC
// Copyright 2020- Thinkoid, LLC

#include <defs.hh>

#include <cstdio>
#include <cstring>

#include <fstream>
#include <vector>

#include <utils/memory.hh>
#include <utils/path.hh>
#include <utils/string.hh>
#include <utils/GList.hh>

#include <xpdf/Error.hh>
#include <xpdf/GlobalParams.hh>

#include <xpdf/unicode_map.hh>
#include <xpdf/unicode_map.inl>

namespace xpdf {
namespace detail {

unicode_range_t
unicode_range_from(const std::string &in, const std::string &out)
{
    auto x = std::stoi(in, 0, 16);
    return { x, x, std::stoi(out, 0, 16), (out.size() + 1) / 2 };
}

unicode_range_t
unicode_range_from(const std::string &beg, const std::string &end,
                   const std::string &out)
{
    return {
        std::stoi(beg, 0, 16), std::stoi(end, 0, 16), std::stoi(out, 0, 16),
        (out.size() + 1) / 2
    };
}

unicode_range_t
unicode_range_from(const std::string &line)
{
    auto tokens = split(line);

    switch (tokens.size()) {
    case 2:
        return unicode_range_from(tokens[0], tokens[1]);

    case 3:
        return unicode_range_from(tokens[0], tokens[1], tokens[2]);

    default:
        throw std::invalid_argument("line format");
    }
}

std::vector< unicode_range_t >
parse_unicode_map(const std::string &name, std::istream &stream)
{
    int lineno = 1;

    std::vector< unicode_range_t > xs;

    for (std::string line; std::getline(stream, line); ++lineno) {
        try {
            xs.push_back(unicode_range_from(line));
        } catch(const std::exception &err) {
            error(errSyntaxError, -1, "encoding %s : line %d : %s",
                  name, lineno, err.what());
        }
    }

    return xs;
}

std::vector< unicode_range_t >
parse_unicode_map(const std::string &name, const fs::path &path)
{
    std::ifstream stream(path.c_str());
    return parse_unicode_map(name, stream);
}

template< typename T, size_t N >
inline auto range_from(const T (&arr)[N])
{
    return std::ranges::subrange(arr, arr + N);
}

} // namespace detail

unicode_latin1_map_t::unicode_latin1_map_t()
    : unicode_range_map_t(detail::range_from(latin1_unicode_range_data))
{
}

unicode_ascii7_map_t::unicode_ascii7_map_t()
    : unicode_range_map_t(detail::range_from(ascii7_unicode_range_data))
{
}

unicode_symbol_map_t::unicode_symbol_map_t()
    : unicode_range_map_t(detail::range_from(symbol_unicode_range_data))
{
}

unicode_dingbats_map_t::unicode_dingbats_map_t()
    : unicode_range_map_t(detail::range_from(dingbats_unicode_range_data))
{
}

unicode_custom_map_t::unicode_custom_map_t(
    const std::string &name, const fs::path &path)
    : name_(name), map_(detail::parse_unicode_map(name, path))
{
}

const char *unicode_utf8_map_t::name() const { return "UTF-8"; }
const char *unicode_ucs2_map_t::name() const { return "UCS-2"; }

const char *  unicode_latin1_map_t::name() const { return "Latin1"; }
const char *  unicode_ascii7_map_t::name() const { return "ASCII7"; }
const char *  unicode_symbol_map_t::name() const { return "Symbol"; }
const char *unicode_dingbats_map_t::name() const { return "ZapfDingbats"; }

bool unicode_utf8_map_t::is_unicode() const { return true; }
bool unicode_ucs2_map_t::is_unicode() const { return true; }

bool   unicode_latin1_map_t::is_unicode() const { return false; }
bool   unicode_ascii7_map_t::is_unicode() const { return false; }
bool   unicode_symbol_map_t::is_unicode() const { return false; }
bool unicode_dingbats_map_t::is_unicode() const { return false; }

int unicode_utf8_map_t::operator()(wchar_t c, char *buf, size_t len) const
{
    if (c <= 0x0000007f) {
        if (len < 1) return 0;

        buf[0] = (char)c;

        return 1;
    } else if (c <= 0x000007ff) {
        if (len < 2) return 0;

        buf[0] = (char)(0xc0 + (c >> 6));
        buf[1] = (char)(0x80 + (c & 0x3f));

        return 2;
    } else if (c <= 0x0000ffff) {
        if (len < 3) return 0;

        buf[0] = (char)(0xe0 + (c >> 12));
        buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[2] = (char)(0x80 + (c & 0x3f));

        return 3;
    } else if (c <= 0x0010ffff) {
        if (len < 4)
            return 0;

        buf[0] = (char)(0xf0 +  (c >> 18));
        buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
        buf[2] = (char)(0x80 + ((c >>  6) & 0x3f));
        buf[3] = (char)(0x80 +  (c        & 0x3f));

        return 4;
    } else {
        return 0;
    }
}

int unicode_ucs2_map_t::operator()(wchar_t c, char *buf, size_t len) const
{
    if (c <= 0xffff) {
        if (len < 2) return 0;

        buf[0] = (char)((c >> 8) & 0xff);
        buf[1] = (char)((c)      & 0xff);

        return 2;
    } else {
        return 0;
    }
}

int unicode_range_map_t::operator()(wchar_t c, char *buf, size_t len) const
{
    for (auto &rng : map_) {
        if (rng.beg <= c && c <= rng.end) {
            if (len < rng.len)
                return 0;

            wchar_t out = rng.beg == rng.end
                ? rng.beg : rng.out + c - rng.beg;

            std::copy(&out, &out + rng.len, buf);

            return rng.len;
        }
    }

    return 0;
}

} // namespace xpdf

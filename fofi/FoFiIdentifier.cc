// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#include <fofi/FoFiIdentifier.hh>

#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>

#include <iostream>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

#include <boost/endian/arithmetic.hpp>
#include <boost/endian/conversion.hpp>
namespace endian = boost::endian;

#include <boost/iostreams/device/mapped_file.hpp>
namespace io = boost::iostreams;

namespace xpdf {
namespace detail {

inline const char* name_of (font_type arg) {
    static const char* arr[] = {
        "Type1 font in PFA format",
        "Type1 font in PFB format",
        "8-bit CFF font",
        "CID CFF font",
        "TrueType font",
        "TrueType collection",
        "OpenType container of 8-bit CFF font",
        "OpenType container of CID-keyed CFF font",
        "(unknown)"
    };

    return arr[arg];
}

template< typename Iterator >
struct iterator_guard_t {
    iterator_guard_t (Iterator& iter)
        : iter (iter), save (iter), restore (true) {}

    ~iterator_guard_t () {
        if (restore) {
            iter = save;
        }
    }

    void release () { restore = false; }

    Iterator &iter, save;
    bool restore;
};

#define ITERATOR_GUARD(x) detail::iterator_guard_t iterator_guard (x)
#define ITERATOR_RELEASE iterator_guard.release ()
#define PARSE_SUCCESS ITERATOR_RELEASE; return true

#define S_(x) std::string (x, sizeof x - 1)

#define ITERATOR_CONDITIONAL(x)                                \
    template< typename T >                                     \
    constexpr auto is_##x##_iterator = std::is_same_v<         \
        typename std::iterator_traits< T >::iterator_category, \
        std::x##_iterator_tag >

ITERATOR_CONDITIONAL (input);
ITERATOR_CONDITIONAL (output);
ITERATOR_CONDITIONAL (forward);
ITERATOR_CONDITIONAL (bidirectional);
ITERATOR_CONDITIONAL (random_access);

#undef ITERATOR_CONDITIONAL

template< typename Iterator, typename T >
typename std::enable_if_t<
    is_input_iterator< Iterator > || is_output_iterator< Iterator > ||
    is_forward_iterator< Iterator > >
advance (Iterator& iter, Iterator last, T dist) {
    assert (dist >= 0);
    for (; dist && iter != last; --dist, ++iter)
        ;
}

template< typename Iterator, typename T >
typename std::enable_if_t< is_bidirectional_iterator< Iterator > >
advance (Iterator& iter, Iterator last, T dist) {
    if (dist > 0) {
        for (; dist && iter != last; --dist, ++iter)
            ;
    }
    else if (dist < 0) {
        for (; dist && iter != last; ++dist, --iter)
            ;
    }
}

template< typename Iterator, typename T >
typename std::enable_if_t<
    is_random_access_iterator< Iterator > && std::is_signed_v< T > >
advance (Iterator& iter, Iterator last, T dist) {
    if (dist > 0) {
        const auto n = std::distance (iter, last);
        if (dist > n)
            iter = last;
        else
            std::advance (iter, dist);
    }
    else if (dist < 0) {
        if (dist < std::distance (iter, last))
            iter = last;
        else
            std::advance (iter, dist);
    }
}

template< typename Iterator, typename T >
typename std::enable_if_t<
    is_random_access_iterator< Iterator > && std::is_unsigned_v< T > >
advance (Iterator& iter, Iterator last, T dist) {
    if (std::distance (iter, last) < 0 ||
        size_t (std::distance (iter, last)) < dist)
        iter = last;
    else
        std::advance (iter, dist);
}

template< typename Iterator, typename T >
bool safe_advance (Iterator& iter, Iterator last, T dist) {
    advance (iter, last, dist);
    return iter != last;
}

template< typename T >
void big_to_native_inplace (T& arg, size_t size) {
    if (size == sizeof arg)
        endian::big_to_native_inplace (arg);
    else {
        T dst = 0;
        char* pdst = reinterpret_cast< char* > (&dst) + size - 1;

        const char* psrc = reinterpret_cast< char* > (&arg);
        const char* pend = psrc + size;

        for (; psrc != pend; ++psrc, --pdst) {
            pdst[0] = psrc[0];
        }

        arg = dst;
    }
}

////////////////////////////////////////////////////////////////////////

template< typename Iterator >
bool literal_char (Iterator& iter, Iterator last, char c) {
    ITERATOR_GUARD (iter);

    if (iter != last) {
        auto x = *iter++;

        if (x == c) {
            PARSE_SUCCESS;
        }
    }

    return false;
}

template< typename Iterator >
bool literal_string (Iterator& iter, Iterator last, const std::string& s) {
    ITERATOR_GUARD (iter);

    auto other = s.begin ();

    for (; iter != last && other != s.end () && *iter == *other;
         ++iter, ++other)
        ;

    if (other == s.end ()) {
        PARSE_SUCCESS;
    }

    return false;
}

template< typename Iterator >
bool literal_int (Iterator& iter, Iterator last, int& i) {
    ITERATOR_GUARD (iter);

    if (iter != last) {
        int x = 0;

        if (!digit (iter, last, x)) return false;

        int val = x;

        for (; iter != last;) {
            if (!digit (iter, last, x)) break;

            val *= 10;
            val += x;
        }

        i = val;

        PARSE_SUCCESS;
    }

    return false;
}

template< typename Iterator, typename T >
bool integral (Iterator& iter, Iterator last, T& attr) {
    ITERATOR_GUARD (iter);

    if (iter != last) {
        int val = 0;

        size_t i = 0;

        for (; i < sizeof attr && iter != last; ++i, ++iter) {
            reinterpret_cast< char* > (&val)[i] = *iter;
        }

        if (i < sizeof attr) return false;

        attr = val;

        PARSE_SUCCESS;
    }

    return false;
}

template< typename Iterator, typename T >
bool sized_integral (Iterator& iter, Iterator last, T& attr, size_t n) {
    assert (n <= sizeof attr);

    ITERATOR_GUARD (iter);

    if (iter != last) {
        int val = 0;

        size_t i = 0;

        for (; i < n && iter != last; ++i, ++iter) {
            reinterpret_cast< char* > (&val)[i] = *iter;
        }

        if (i < n) return false;

        attr = val;

        PARSE_SUCCESS;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////

template< typename Iterator >
typename std::enable_if_t< is_random_access_iterator< Iterator >, bool >
font_identify_pfa (Iterator& iter, Iterator last, font_type& result) {
    if (literal_string (iter, last, "%!PS-AdobeFont-1") ||
        literal_string (iter, last, "%!FontType1")) {
        result = FONT_TYPE1_PFA;
        return true;
    }

    return false;
}

template< typename Iterator >
typename std::enable_if_t< is_random_access_iterator< Iterator >, bool >
font_identify_pfb (Iterator& iter, Iterator last, font_type& result) {
    ITERATOR_GUARD (iter);

    if (literal_string (iter, last, "\x80\x01")) {
        unsigned n = 0;

        if (integral (iter, last, n)) {
            if ((n >= 16 && literal_string (iter, last, "%!PS-AdobeFont-1")) ||
                (n >= 11 && literal_string (iter, last, "%!FontType1"))) {
                result = FONT_TYPE1_PFB;
                PARSE_SUCCESS;
            }
        }
    }

    return false;
}

template< typename Iterator >
typename std::enable_if_t< is_random_access_iterator< Iterator >, bool >
font_identify_ttf (Iterator& iter, Iterator last, font_type& result) {
    if (literal_string (iter, last, std::string ("\x00\x01\x00\x00", 4)) ||
        literal_string (iter, last, "true")) {
        result = FONT_TRUETYPE;
        return true;
    }

    if (literal_string (iter, last, "ttcf")) {
        result = FONT_TRUETYPE_COLLECTION;
        return true;
    }

    return false;
}

template< typename Iterator >
typename std::enable_if_t< is_random_access_iterator< Iterator >, bool >
font_identify_cff (Iterator& iter, Iterator last, font_type& result) {
    ITERATOR_GUARD (iter);

    if (!literal_string (iter, last, std::string ("\x01\x00", 2))) return false;

    {
        unsigned char a = 0, b = 0;

        if (!integral (iter, last, a) || !integral (iter, last, b))
            return false;

        if (b < 1 || 4 < b || a < 4) return false;

        if (!safe_advance (iter, last, int (a) - 4)) return false;
    }

    {
        unsigned short n;

        if (!integral (iter, last, n)) return false;

        endian::big_to_native_inplace (n);

        if (n) {
            unsigned char x;

            if (!integral (iter, last, x)) return false;

            if (!safe_advance (iter, last, n * x)) return false;

            unsigned long y = 0;

            if (!sized_integral (iter, last, y, x)) return false;

            big_to_native_inplace (y, x);

            if (!safe_advance (iter, last, long (y) - 1)) return false;
        }
    }

    {
        unsigned short n = 0;

        if (!integral (iter, last, n) || 0 == n) return false;

        endian::big_to_native_inplace (n);

        unsigned char x = 0;

        if (!integral (iter, last, x)) return false;

        unsigned long y = 0, z = 0;

        if (!sized_integral (iter, last, y, x) ||
            !sized_integral (iter, last, z, x) || y > z)
            return false;

        big_to_native_inplace (y, x);
        big_to_native_inplace (z, x);

        {
            auto end = last;
            last = iter;

            if (!safe_advance (iter, end, (n - 1) * x + y - 1) ||
                !safe_advance (last, end, (n - 1) * x + z - 1))
                return false;
        }

        for (size_t i = 0; i < 3; ++i) {
            unsigned char c = 0;

            if (!integral (iter, last, c)) return false;

            if (c == 0x1c) {
                if (!safe_advance (iter, last, 2)) return false;
            }
            else if (c == 0x1d) {
                if (!safe_advance (iter, last, 4)) return false;
            }
            else if (c >= 0xf7 && c <= 0xfe) {
                if (!safe_advance (iter, last, 1)) return false;
            }
            else if (c < 0x20 || c > 0xf6) {
                result = FONT_CFF_8BIT;
                PARSE_SUCCESS;
            }

            if (iter == last) {
                result = FONT_CFF_8BIT;
                PARSE_SUCCESS;
            }
        }

        unsigned char c = 0;

        if (integral (iter, last, c) && c == 12 && integral (iter, last, c) &&
            c == 30) {
            result = FONT_CFF_CID;
        }
        else {
            result = FONT_CFF_8BIT;
        }

        PARSE_SUCCESS;
    }

    return false;
}

template< typename Iterator >
typename std::enable_if_t< is_random_access_iterator< Iterator >, bool >
font_identify_otf (Iterator& iter, Iterator last, font_type& result) {
    ITERATOR_GUARD (iter);
    const auto save = iter;

    if (literal_string (iter, last, "OTTO")) {
        int short n = 0;

        if (integral (iter, last, n)) {
            endian::big_to_native_inplace (n);
            assert (n >= 0);

            if (!safe_advance (iter, last, 6)) return false;

            for (size_t i = 0; i < size_t (n); ++i) {
                if (literal_string (iter, last, "CFF ")) {
                    if (!safe_advance (iter, last, 4)) break;

                    int off = 0;

                    if (integral (iter, last, off)) {
                        endian::big_to_native_inplace (off);

                        if (off < INT_MAX) {
                            auto iter2 = save;

                            if (!safe_advance (iter2, last, off)) break;

                            if (font_identify_cff (iter2, last, result)) {
                                switch (result) {
                                case FONT_CFF_8BIT:
                                    result = FONT_OPENTYPE_CFF_8BIT;
                                    PARSE_SUCCESS;

                                case FONT_CFF_CID:
                                    result = FONT_OPENTYPE_CFF_CID;
                                    PARSE_SUCCESS;

                                default:
                                    break;
                                }
                            }
                        }
                    }

                    return false;
                }

                if (!safe_advance (iter, last, 16)) break;
            }
        }
    }

    return false;
}

template< typename Iterator >
typename std::enable_if_t< is_random_access_iterator< Iterator >, bool >
font_identify (Iterator& iter, Iterator last, font_type& result) {
    return font_identify_pfa (iter, last, result) ||
           font_identify_pfb (iter, last, result) ||
           font_identify_ttf (iter, last, result) ||
           font_identify_otf (iter, last, result) ||
           font_identify_cff (iter, last, result);
}

} // namespace detail

////////////////////////////////////////////////////////////////////////

bool font_identify_byextension (const char* filepath, font_type& result) {
    if (fs::path (filepath).extension () == ".dfont") {
        return result = FONT_DFONT, true;
    }

    return false;
}

bool font_identify_bycontent (const char* filepath, font_type& result) {
    io::mapped_file_source src (filepath);
    auto iter = src.begin (), last = src.end ();
    return detail::font_identify (iter, last, result);
}

bool font_identify (const char* filepath, font_type& result) {
    return font_identify_byextension (filepath, result) ||
           font_identify_bycontent (filepath, result);
}

bool font_identify (const char* pbuf, size_t n, font_type& result) {
    return detail::font_identify (pbuf, pbuf + n, result);
}

} // namespace xpdf

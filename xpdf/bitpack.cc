// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#include <defs.hh>
#include <xpdf/bitpack.hh>

#include <range/v3/algorithm/transform.hpp>
using namespace ranges;

namespace xpdf {
namespace detail {

#define U(x) unsigned((unsigned char)(x))

std::vector< double > onebit_unpack(const char *psrc, const char *const pend,
                                    size_t n)
{
    ASSERT(psrc && pend && psrc <= pend);

    const size_t distance = std::distance(psrc, pend);
    ASSERT(n <= (distance << 3));

    std::vector< double > xs(n);
    double *              pdst = xs.data();

    for (; n >= 8 && psrc < pend; n -= 8, ++psrc, pdst += 8) {
        pdst[0] = U(psrc[0]) >> 7;
        pdst[1] = (U(psrc[0]) >> 6) & 1;
        pdst[2] = (U(psrc[0]) >> 5) & 1;
        pdst[3] = (U(psrc[0]) >> 4) & 1;
        pdst[4] = (U(psrc[0]) >> 3) & 1;
        pdst[5] = (U(psrc[0]) >> 2) & 1;
        pdst[6] = (U(psrc[0]) >> 1) & 1;
        pdst[7] = (U(psrc[0])) & 1;
    }

    switch (n) {
    case 8:
        ASSERT(0);
    case 7:
        pdst[6] = (U(psrc[0]) >> 1) & 1;
    case 6:
        pdst[5] = (U(psrc[0]) >> 2) & 1;
    case 5:
        pdst[4] = (U(psrc[0]) >> 3) & 1;
    case 4:
        pdst[3] = (U(psrc[0]) >> 4) & 1;
    case 3:
        pdst[2] = (U(psrc[0]) >> 5) & 1;
    case 2:
        pdst[1] = (U(psrc[0]) >> 6) & 1;
    case 1:
        pdst[0] = U(psrc[0]) >> 7;
    default:
        break;
    }

    return xs;
}

std::vector< double > twobit_unpack(const char *psrc, const char *const pend,
                                    size_t n)
{
    ASSERT(psrc && pend && psrc <= pend);

    const size_t distance = std::distance(psrc, pend);
    ASSERT(n <= (distance << 2));

    std::vector< double > xs(n);
    double *              pdst = xs.data();

    static constexpr double m = 1. / 3;

    for (; n >= 4 && psrc < pend; n -= 4, ++psrc, pdst += 4) {
        pdst[0] = m * (U(psrc[0]) >> 6);
        pdst[1] = m * ((U(psrc[0]) >> 4) & 3);
        pdst[2] = m * ((U(psrc[0]) >> 2) & 3);
        pdst[3] = m * ((U(psrc[0])) & 3);
    }

    switch (n) {
    case 4:
        ASSERT(0);
    case 3:
        pdst[2] = m * ((U(psrc[0]) >> 2) & 3);
    case 2:
        pdst[1] = m * ((U(psrc[0]) >> 4) & 3);
    case 1:
        pdst[0] = m * (U(psrc[0]) >> 6);
    default:
        break;
    }

    return xs;
}

std::vector< double > fourbit_unpack(const char *psrc, const char *const pend,
                                     size_t n)
{
    ASSERT(psrc && pend && psrc <= pend);

    const size_t distance = std::distance(psrc, pend);
    ASSERT(n <= (distance << 1));

    std::vector< double > xs(n);
    double *              pdst = xs.data();

    static constexpr double m = 1. / 15;

    for (; n >= 2 && psrc < pend; n -= 2, ++psrc, pdst += 2) {
        pdst[0] = m * (U(psrc[0]) >> 4);
        pdst[1] = m * ((U(psrc[0])) & 15);
    }

    switch (n) {
    case 2:
        ASSERT(0);
    case 1:
        pdst[0] = m * (U(psrc[0]) >> 4);
    default:
        break;
    }

    return xs;
}

std::vector< double > eightbit_unpack(const char *psrc, const char *const pend,
                                      size_t n)
{
    ASSERT(psrc && pend && psrc <= pend);

    const size_t distance = std::distance(psrc, pend);
    ASSERT(n <= distance);

    std::vector< double > xs(n);
    double *              pdst = xs.data();

    static constexpr double m = 1. / 255;
    transform(psrc, psrc + n, pdst, [=](auto x) { return m * U(x); });

    return xs;
}

std::vector< double > twelvebit_unpack(const char *psrc, const char *const pend,
                                       size_t n)
{
    ASSERT(psrc && pend && psrc <= pend);

    const size_t distance = std::distance(psrc, pend);
    ASSERT((((n << 4) - (n << 2) + 11) >> 3) <= distance);

    std::vector< double > xs(n);
    double *              pdst = xs.data();

    static constexpr double m = 1. / (unsigned(1UL << 12) - 1);

    for (; n >= 2 && psrc < pend; n -= 2, psrc += 3, pdst += 2) {
        pdst[0] = m * ((U(psrc[0]) << 4) | (U(psrc[1]) >> 4));
        pdst[1] = m * (((U(psrc[1]) & 0x0F) << 8) | U(psrc[2]));
    }

    switch (n) {
    case 2:
        ASSERT(0);
    case 1:
        pdst[0] = m * ((U(psrc[0]) << 4) | (U(psrc[1]) >> 4));
    default:
        break;
    }

    return xs;
}

std::vector< double > sixteenbit_unpack(const char *psrc, const char *const pend,
                                        size_t n)
{
    ASSERT(psrc && pend && psrc <= pend);

    const size_t distance = std::distance(psrc, pend);
    ASSERT((n << 1) <= distance);

    std::vector< double > xs(n);
    double *              pdst = xs.data();

    static constexpr double m = 1. / (unsigned(1UL << 16) - 1);

    for (; n && psrc < pend; --n, psrc += 2, ++pdst) {
        pdst[0] = m * ((U(psrc[0]) << 8) | U(psrc[1]));
    }

    return xs;
}

std::vector< double > twentyfourbit_unpack(const char *      psrc,
                                           const char *const pend, size_t n)
{
    ASSERT(psrc && pend && psrc <= pend);

    const size_t distance = std::distance(psrc, pend);
    ASSERT((n << 2) - n <= distance);

    std::vector< double > xs(n);
    double *              pdst = xs.data();

    static constexpr double m = 1. / (unsigned(1UL << 24) - 1);

    for (; n && psrc < pend; --n, psrc += 3, ++pdst) {
        pdst[0] = m * ((U(psrc[0]) << 16) | (U(psrc[1]) << 8) | U(psrc[2]));
    }

    return xs;
}

std::vector< double > thirtytwobit_unpack(const char *      psrc,
                                          const char *const pend, size_t n)
{
    ASSERT(psrc && pend && psrc <= pend);

    const size_t distance = std::distance(psrc, pend);
    ASSERT((n << 2) <= distance);

    std::vector< double > xs(n);
    double *              pdst = xs.data();

    static constexpr double m = 1. / unsigned(-1);

    for (; n && psrc < pend; --n, psrc += 4, ++pdst) {
        pdst[0] = m * ((U(psrc[0]) << 24) | (U(psrc[1]) << 16) |
                       (U(psrc[2]) << 8) | U(psrc[3]));
    }

    return xs;
}

} // namespace detail

std::vector< double > unpack(const char *psrc, const char *const pend, size_t n,
                             size_t bps)
{
    using namespace detail;

    switch (bps) {
    case 1:
        return onebit_unpack(psrc, pend, n);
    case 2:
        return twobit_unpack(psrc, pend, n);
    case 4:
        return fourbit_unpack(psrc, pend, n);
    case 8:
        return eightbit_unpack(psrc, pend, n);
    case 12:
        return twelvebit_unpack(psrc, pend, n);
    case 16:
        return sixteenbit_unpack(psrc, pend, n);
    case 24:
        return twentyfourbit_unpack(psrc, pend, n);
    case 32:
        return thirtytwobit_unpack(psrc, pend, n);
    default:
        throw std::runtime_error("invalid sample size");
    }
}

} // namespace xpdf

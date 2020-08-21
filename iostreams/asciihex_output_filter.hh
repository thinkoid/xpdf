// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_IOSTREAMS_ASCIIHEX_OUTPUT_FILTER_HH
#define XPDF_IOSTREAMS_ASCIIHEX_OUTPUT_FILTER_HH

#include <cctype>
#include <boost/iostreams/concepts.hpp>

namespace xpdf {
namespace iostreams {

struct asciihex_output_filter_t : public boost::iostreams::output_filter
{
    template< typename Sink >
    bool put(Sink &dst, int c)
    {
        if (eof_)
            return false;

        static const char *s = "0123456789ABCDEF";

        return
            boost::iostreams::put(dst, s[((unsigned)c & 0xF0) >> 4]) &&
            boost::iostreams::put(dst, s[ (unsigned)c & 0x0F]);
    }

    template< typename Sink >
    void close(Sink &dst)
    {
        boost::iostreams::put(dst, '>');
        eof_ = true;
    }

private:
    bool eof_ = false;
};

} // namespace iostreams
} // namespace xpdf

#endif // XPDF_IOSTREAMS_ASCIIHEX_OUTPUT_FILTER_HH

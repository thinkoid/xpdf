// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_IOSTREAMS_ARRAY_SOURCE_HH
#define XPDF_IOSTREAMS_ARRAY_SOURCE_HH

#include <vector>
#include <boost/iostreams/concepts.hpp>

namespace xpdf {
namespace iostreams {

template< typename T >
struct array_source_t : public boost::iostreams::source
{
    using char_type = T;

    using container_type = std::vector< char_type >;
    using pos_type = std::streampos;

    template< typename Iterator >
    explicit array_source_t(Iterator first, Iterator last)
        : container_(first, last), pos_()
    { }

    std::streamsize read(char* s, std::streamsize n) {
        assert(n >= 0);
        std::streamsize dist = container_.size() - pos_;

        if (0 == (n = (std::min)(n, dist)))
            return -1;

        auto xs = container_.data();

        std::copy(xs + pos_, xs + pos_ + n, s);
        pos_ += n;

        return n;
    }

private:
    container_type container_;
    pos_type pos_;
};

} // namespace iostreams
} // namespace xpdf

#endif // XPDF_IOSTREAMS_ARRAY_SOURCE_HH

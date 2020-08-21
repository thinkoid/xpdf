// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_IOSTREAMS_CONTAINER_SOURCE_HH
#define XPDF_IOSTREAMS_CONTAINER_SOURCE_HH

#include <vector>
#include <boost/iostreams/concepts.hpp>

namespace xpdf {
namespace iostreams {

template< typename Container >
struct container_source_t : public boost::iostreams::source
{
    using container_type = Container;
    using pos_type = typename Container::size_type;

    explicit container_source_t(const container_type &container)
        : container_(container), pos_()
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
    const container_type &container_;
    pos_type pos_;
};

} // namespace iostreams
} // namespace xpdf

#endif // XPDF_IOSTREAMS_CONTAINER_SOURCE_HH

// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_IOSTREAMS_CONTAINER_SINK_HH
#define XPDF_IOSTREAMS_CONTAINER_SINK_HH

#include <boost/iostreams/concepts.hpp>

namespace xpdf {
namespace iostreams {

template< typename Container >
struct container_sink_t : public boost::iostreams::sink
{
    using container_type = Container;
    using char_type = typename Container::value_type;

    explicit container_sink_t(container_type &container) : container_(container) { }

    std::streamsize write(const char* s, std::streamsize n)
    {
        return container_.insert(container_.end(), s, s + n), n;
    }

private:
    container_type &container_;
};

} // namespace iostreams
} // namespace xpdf

#endif // XPDF_IOSTREAMS_CONTAINER_SINK_HH

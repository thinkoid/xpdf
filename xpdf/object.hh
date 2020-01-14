// -*- mode: c++; -*-

#ifndef XPDF_XPDF_OBJECT_HH
#define XPDF_XPDF_OBJECT_HH

#include <defs.hh>

#include <string>
#include <tuple>
#include <variant>
#include <vector>

class XRef;

namespace xpdf {
namespace ast {

struct null_t { };

struct ref_t : std::tuple< int, int > {
    using std::tuple< int, int >::tuple;
    using std::tuple< int, int >::operator=;
};

using string_t = std::string;

struct name_t : string_t {
    using string_t::string_t;
    using string_t::operator=;
};

struct cmd_t : string_t {
    using string_t::string_t;
    using string_t::operator=;
};

struct stream_t : std::vector< char > {
    using std::vector< char >::vector;
    using std::vector< char >::operator=;
};

struct object_t;

struct array_t : std::vector< object_t > {
    using std::vector< object_t >::vector;
    using std::vector< object_t >::operator=;
};

struct dictionary_t : std::vector< std::tuple< std::string, object_t > > {
    using std::vector< std::tuple< std::string, object_t > >::vector;
    using std::vector< std::tuple< std::string, object_t > >::operator=;
};

struct object_t : std::variant<
    null_t, bool, long, double, std::string, name_t, cmd_t,
    array_t, dictionary_t, stream_t > {

    using base_type = std::variant<
        null_t, bool, long, double, std::string, name_t, cmd_t,
        array_t, dictionary_t, stream_t >;

    using base_type::base_type;
    using base_type::operator=;
};

} // namespace ast
} // namespace xpdf

#endif // XPDF_XPDF_OBJECT_HH

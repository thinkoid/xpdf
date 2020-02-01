// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#ifndef XPDF_XPDF_PARSER_AST_HH
#define XPDF_XPDF_PARSER_AST_HH

#include <defs.hh>

#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace xpdf::parser::ast {

struct null_t { };

struct name_t : std::string {
    using base_type = std::string;
    using base_type::operator=;
};

struct string_t : std::string {
    using base_type = std::string;
    using base_type::operator=;
};

struct ref_t {
    int num, gen;
};

struct  array_t;
struct   dict_t;
struct stream_t;

using  array_pointer = std::shared_ptr<  array_t >;
using   dict_pointer = std::shared_ptr<   dict_t >;
using stream_pointer = std::shared_ptr< stream_t >;

using obj_t = std::variant<
    null_t, bool, int, double, name_t, string_t, ref_t,
    array_pointer, dict_pointer, stream_pointer
    >;

#define XPDF_PARSER_AST_DEF(type, ...)                          \
struct type : __VA_ARGS__ {                                     \
    using base_type = __VA_ARGS__;                              \
                                                                \
    using base_type::base_type;                                 \
    using base_type::operator=;                                 \
                                                                \
    type (const base_type& arg) : base_type (arg) { }           \
    type (base_type&& arg) : base_type (std::move (arg)) { }    \
}

XPDF_PARSER_AST_DEF ( array_t, std::vector< obj_t >);
XPDF_PARSER_AST_DEF (  dict_t, std::vector< std::tuple< name_t, obj_t > >);
XPDF_PARSER_AST_DEF (stream_t, std::vector< char >);

#undef XPDF_PARSER_AST_DEF

struct doc_t  {
    std::vector< std::tuple< int, obj_t > > objs;
    dict_t trailer;
};

} // namespace xpdf::parser::ast

#endif // XPDF_XPDF_PARSER_AST_HH

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

struct obj_t;

struct array_t : std::vector< obj_t > {
    using base_type = std::vector< obj_t >;

    using base_type::base_type;
    using base_type::operator=;

    array_t (const base_type& arg) : base_type (arg) { }
    array_t (base_type&& arg) : base_type (std::move (arg)) { }
};

struct dict_t : std::vector< std::tuple< name_t, obj_t > > {
    using base_type = std::vector< std::tuple< name_t, obj_t > >;

    using base_type::base_type;
    using base_type::operator=;
};

struct stream_t : std::vector< char > {
    using base_type = std::vector< char >;

    using base_type::base_type;
    using base_type::operator=;
};

using  array_ptr_t = std::shared_ptr<  array_t >;
using   dict_ptr_t = std::shared_ptr<   dict_t >;
using stream_ptr_t = std::shared_ptr< stream_t >;

struct obj_t : std::variant<
    null_t, bool, int, double, ref_t, std::string,
    array_ptr_t, dict_ptr_t, stream_ptr_t > {

    using base_type = std::variant<
        null_t, bool, int, double, ref_t, std::string,
        array_ptr_t, dict_ptr_t, stream_ptr_t >;

    using base_type::base_type;
    using base_type::operator=;
};

struct doc_t  {
    std::vector< std::tuple< int, obj_t > > objs;
    dict_t trailer;
};

} // namespace xpdf::parser::ast

#endif // XPDF_XPDF_PARSER_AST_HH

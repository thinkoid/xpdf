// -*- mode: c++; -*-
// Copyright 2020 Thinkoid, LLC

#include <defs.hh>

#include <iostream>
#include <fstream>
#include <typeinfo>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <boost/iostreams/device/mapped_file.hpp>
namespace io = boost::iostreams;

#include <xpdf/parser.hh>
namespace ast = xpdf::parser::ast;

template< typename... Ts> struct overload_ : Ts... { using Ts::operator()...; };
template< typename... Ts> overload_(Ts...) -> overload_< Ts... >;

namespace xpdf::parser::ast {

inline bool is_array (const obj_t& arg) {
    return std::visit (overload_ {
        [](const ast::array_pointer&) { return true; },
        [](auto&&) { return false; }
        }, arg);
}

} // namespace xpdf::parser::ast

static void dump (const ast::doc_t& doc, std::ostream& s) {
    namespace ast = xpdf::parser::ast;

    for (const auto& [id, obj] : doc.objs) {
        std::cout << " --> " << id << " : " << ast::is_array (obj) << "\n";
    }
}

static bool parse (const fs::path& filename, ast::doc_t& doc) {
    io::mapped_file_source f (filename.c_str ());

    auto first = f.data (), iter = first;
    const auto last = first + f.size ();

    return xpdf::parse (first, iter, last, doc);
}

int main (int argc, char** argv) {
    if (0 == argv [1] || 0 == argv [1][0])
        return 1;

    fs::path filename (argv [1]);

    if (fs::exists (filename)) {
        ast::doc_t doc;

        if (parse (filename, doc)) {
            if (argv [2] && argv [2][0]) {
                std::ofstream s (
                    argv [2], std::ios_base::binary | std::ios_base::out);

                assert (s);
                s.exceptions (std::ios_base::failbit | std::ios_base::badbit);

                dump (doc, s);
            }
            else {
                dump (doc, std::cout);
            }
        }
        else {
            std::cerr << "failed" << std::endl;
            return 1;
        }
    }

    return 0;
}

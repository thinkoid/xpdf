// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC

#ifndef XPDF_XPDF_DICT_HH
#define XPDF_XPDF_DICT_HH

#include <defs.hh>

#include <string>
#include <tuple>
#include <vector>

#include <xpdf/obj.hh>

namespace xpdf {

struct dict_t : std::vector< std::tuple< std::string, Object > > {
    using base_type = std::vector<
        std::tuple< std::string, Object > >;

    using base_type::base_type;

    using base_type::operator=;
    using base_type::operator[];

    //
    // Same semantics with std::map::operator[]
    //
    xpdf::obj_t& operator[] (const char*);

    bool has_key  (const std::string&) const;
    bool has_type (const std::string&) const;

    //
    // Same semantics with std::map::at
    //
    xpdf::obj_t& at (const char*);
    const xpdf::obj_t& at (const char* s) const {
        return const_cast< dict_t* > (this)->at (s);
    }

    const std::string& key_at (size_t) const;

    obj_t& val_at (size_t);
    const obj_t& val_at (size_t n) const {
        return const_cast< dict_t* > (this)->val_at (n);
    }

    void emplace (const std::string&, obj_t);
};

} // namespace xpdf

using Dict = xpdf::dict_t;

#endif // XPDF_XPDF_DICT_HH

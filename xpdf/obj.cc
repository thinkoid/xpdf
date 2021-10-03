// -*- mode: c++; -*-
// Copyright 1996-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#include <defs.hh>

#include <cstddef>
#include <cstdlib>

#include <xpdf/array.hh>
#include <xpdf/dict.hh>
#include <xpdf/Error.hh>
#include <xpdf/obj.hh>
#include <xpdf/Stream.hh>
#include <xpdf/XRef.hh>

namespace xpdf {

obj_t::obj_t(GString *p) noexcept
    : var_(std::shared_ptr< GString >(p))
{
}

obj_t::obj_t(const char *s)
    : var_(std::make_shared< GString >(s))
{
}

obj_t::obj_t(const std::string &s)
    : var_(std::make_shared< GString >(s))
{
}

obj_t::obj_t(Array *p) noexcept
    : var_(std::shared_ptr< Array >(p))
{
}

obj_t::obj_t(Dict *p) noexcept
    : var_(std::shared_ptr< Dict >(p))
{
}

obj_t::obj_t(StreamBase *p) noexcept
    : var_(std::shared_ptr< StreamBase >(p))
{
}

obj_t &obj_t::operator[](size_t n)
{
    return as_array()[n];
}

//------------------------------------------------------------------------
// Dict accessors.
//------------------------------------------------------------------------

obj_t &obj_t::operator[](const char *s)
{
    return as_dict()[s];
}

obj_t &obj_t::at(const char *s)
{
    return as_dict().at(s);
}

void obj_t::emplace(const std::string &key, obj_t obj)
{
    as_dict().emplace(key, std::move(obj));
}

bool obj_t::has_key(const std::string &s) const
{
    return as_dict().has_key(s);
}

bool obj_t::has_type(const std::string &s) const
{
    return is_dict() && as_dict().has_type(s);
}

const std::string &obj_t::key_at(size_t n) const
{
    return as_dict().key_at(n);
}

obj_t &obj_t::val_at(size_t n)
{
    return as_dict().val_at(n);
}

//------------------------------------------------------------------------
// StreamBase accessors.
//------------------------------------------------------------------------

bool obj_t::streamIs(const char *dictType) const
{
    return as_stream()->as_dict().has_type(dictType);
}

bool obj_t::is_stream(const char *dictType) const
{
    return is_stream() && streamIs(dictType);
}

void obj_t::streamReset()
{
    as_stream()->reset();
}

void obj_t::streamClose()
{
    as_stream()->close();
}

int obj_t::streamGetChar()
{
    return as_stream()->get();
}

int obj_t::streamLookChar()
{
    return as_stream()->peek();
}

int obj_t::streamGetBlock(char *blk, int size)
{
    return as_stream()->readblock(blk, size);
}

char *obj_t::streamGetLine(char *buf, int size)
{
    return as_stream()->readline(buf, size);
}

off_t obj_t::streamGetPos()
{
    return as_stream()->tellg();
}

void obj_t::streamSetPos(off_t pos, int dir)
{
    as_stream()->seekg(pos, dir);
}

Dict *obj_t::streamGetDict()
{
    return &as_stream()->as_dict();
}

//
// More legacy stuff
//
const char *obj_t::getTypeName() const
{
    static const char *arr[] = { "null",    "eof",  "error",  "boolean",
                                 "integer", "real", "string", "name",
                                 "cmd",     "ref",  "array",  "dictionary",
                                 "stream" };

    return arr[var_.index()];
};

void obj_t::print(FILE *) { }

obj_t make_arr_obj()
{
    return obj_t(new Array());
}

obj_t make_dict_obj()
{
    return obj_t(new Dict);
}

obj_t make_dict_obj(Dict *p)
{
    return obj_t(p);
}

obj_t make_stream_obj(StreamBase *p)
{
    return obj_t(p);
}

//
// Attempts to resolve references to the actual PDF objects:
//
obj_t resolve(const obj_t &obj, int recursion /* = 0 */)
{
    if (obj.is_ref()) {
        auto &ref = obj.as_ref();

        if (ref.xref)
            return ref.xref->fetch(ref, recursion);
    }

    return obj;
}

} // namespace xpdf

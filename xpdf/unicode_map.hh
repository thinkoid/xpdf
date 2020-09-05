// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_XPDF_UNICODE_MAP_HH
#define XPDF_XPDF_UNICODE_MAP_HH

#include <defs.hh>

#include <vector>
#include <ranges>

#include <boost/hana/core/tag_of.hpp>
namespace hana = boost::hana;

#include <xpdf/CharTypes.hh>

namespace xpdf {

struct unicode_map_tag;

#define XPDF_UNICODE_MAP_STRUCT_DEF(x)              \
    struct XPDF_CAT(unicode_, XPDF_CAT(x, _map_t))  \
    {                                               \
        using tag = unicode_map_tag;                \
                                                    \
        const char *name() const;                   \
        bool is_unicode() const;                    \
                                                    \
        int operator()(wchar_t, char *, size_t) const;  \
    }

XPDF_UNICODE_MAP_STRUCT_DEF(utf8);
XPDF_UNICODE_MAP_STRUCT_DEF(ucs2);

#undef XPDF_UNICODE_MAP_STRUCT_DEF

struct unicode_mapping_t
{
    //
    // A range of input codes [beg, end] maps to a contiguous sequence of
    // codes starting at out, with each output code taking len bytes:
    //
    wchar_t beg, end, out;
    size_t len;
};

struct unicode_map_base_t
{
    using iterator = const unicode_mapping_t *;
    using range_type = std::ranges::subrange< iterator >;

    unicode_map_base_t(iterator first, iterator last)
        : map_(first, last)
    { }

    unicode_map_base_t(range_type rng)
        : map_(std::move(rng))
    { }

    int operator()(wchar_t, char *, size_t) const;

protected:
    range_type map_;
};

#define XPDF_UNICODE_MAP_STRUCT_DEF(x)                                  \
    struct XPDF_CAT(unicode_, XPDF_CAT(x, _map_t)) : unicode_map_base_t \
    {                                                                   \
        using tag = unicode_map_tag;                                    \
                                                                        \
        explicit XPDF_CAT(unicode_, XPDF_CAT(x, _map_t))();             \
                                                                        \
        const char *name() const;                                       \
        bool is_unicode() const;                                        \
    }

XPDF_UNICODE_MAP_STRUCT_DEF(latin1);
XPDF_UNICODE_MAP_STRUCT_DEF(ascii7);
XPDF_UNICODE_MAP_STRUCT_DEF(symbol);
XPDF_UNICODE_MAP_STRUCT_DEF(dingbats);

#undef XPDF_UNICODE_MAP_STRUCT_DEF

struct unicode_custom_map_t
{
    using tag = unicode_map_tag;

    explicit unicode_custom_map_t(const std::string &, const fs::path &);

    const char *name() const { return name_.c_str(); }
    bool is_unicode() const { return false; }

private:
    std::string name_;
    std::vector< unicode_mapping_t > map_;
};

template< typename T, typename = boost::hana::when< true > >
struct is_unicode_map_type
{
    static constexpr bool value = false;
};

template< typename T >
struct is_unicode_map_type< T, hana::when_valid< typename T::tag > >
{
    static constexpr bool value = std::is_same<
        typename T::tag, unicode_map_tag >::value;
};

struct unicode_map_t
{
private:
    struct concept_t
    {
        virtual ~concept_t() = default;

        virtual bool is_unicode() const = 0;
        virtual int operator()(wchar_t, char *, size_t) const = 0;
    };

    template<
        typename T,
        typename = typename std::enable_if<
            is_unicode_map_type< T >::value, void >::type >
    struct model_t final : concept_t
    {
        using value_type = T;

        model_t(value_type value) : value(std::move(value)) { }

        virtual bool is_unicode() const override {
            return value.is_unicode();
        }

        virtual int
        operator()(wchar_t c, char *buf, size_t len) const override {
            return value(c, buf, len);
        }

    private:
        value_type value;
    };

    std::shared_ptr< const concept_t > self_;

public:
    explicit unicode_map_t() : self_() { }

    template< typename T >
    unicode_map_t(T value)
        : self_(std::make_shared< model_t< T > >(value))
    { }

    operator bool() const { return bool(self_); }

    bool is_unicode() const { return self_->is_unicode(); }

    int operator()(wchar_t c, char *buf, size_t len) const {
        return self_->operator()(c, buf, len);
    }
};

} // namespace xpdf

#endif // XPDF_XPDF_UNICODE_MAP_HH

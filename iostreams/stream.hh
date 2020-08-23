// -*- mode: c++; -*-
// Copyright 2020- Thinkoid, LLC

#ifndef XPDF_IOSTREAMS_STREAM_HH
#define XPDF_IOSTREAMS_STREAM_HH

#include <defs.hh>

#include <memory>

namespace xpdf {
namespace iostreams {

struct stream_t
{
private:
    struct concept_t
    {
        virtual ~concept_t() = default;

        virtual const std::type_info &type() const = 0;

        virtual       concept_t &self()       = 0;
        virtual const concept_t &self() const = 0;
    };

    template< typename T >
    struct model_t final : concept_t
    {
        using value_type = T;

        model_t(value_type value)
            : value(std::move(value))
        {
        }

        const std::type_info &type() const override { return typeid(value_type); }

        virtual       concept_t &self()       override { return *this; }
        virtual const concept_t &self() const override { return *this; }

        value_type value;
    };

    std::shared_ptr< concept_t > self_;

public:
    template< typename T >
    stream_t(T&& value)
        : self_(std::make_shared< model_t< T > >(std::forward< T >(value)))
    { }

    const std::type_info &type() const { return self_->type(); }

    concept_t &      self()       { return self_->self(); }
    const concept_t &self() const { return self_->self(); }

    template< typename T > friend       T &as(stream_t &);
    template< typename T > friend const T &as(const stream_t &);

    template< typename T > friend bool is(const stream_t &);
};

template< typename T >
/* friend */ inline T &as(stream_t &obj)
{
    using namespace std;
    using value_type = remove_const_t< remove_pointer_t< T > >;

    if (typeid(value_type) != obj.type())
        throw bad_cast();

    return static_cast< stream_t::model_t< value_type > & >(obj.self()).value;
}

template< typename T >
/* friend */ inline const T &as(const stream_t &obj)
{
    using namespace std;
    using value_type = remove_const_t< remove_pointer_t< T > >;

    if (typeid(value_type) != obj.type())
        throw bad_cast();

    return static_cast< const stream_t::model_t< value_type > & >(obj.self()).value;
}

template< typename T >
/* friend */ inline bool is(const stream_t &obj)
{
    using value_type = std::remove_const_t< std::remove_pointer_t< T > >;
    return typeid(value_type) == obj.type();
}

} // namespace iostreams
} // namespace xpdf

#endif // XPDF_IOSTREAMS_STREAM_HH

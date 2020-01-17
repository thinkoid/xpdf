// -*- mode: c++; -*-
// Copyright 2019-2020 Thinkoid, LLC.

#ifndef XPDF_XPDF_ARRAYITERATOR_HH
#define XPDF_XPDF_ARRAYITERATOR_HH

#include <defs.hh>

#include <boost/iterator/iterator_facade.hpp>
#include <range/v3/view/subrange.hpp>

#include <xpdf/xpdf.hh>
#include <xpdf/object.hh>

namespace xpdf {

template< typename T >
struct array_cursor_t {
    array_cursor_t () : p_ (), n_ (), i_ (), cache_ (), valid_ () { }

    array_cursor_t (Object* p, size_t n) noexcept
        : p_ (p), n_ (n), i_ (), cache_ (), valid_ ()
        { }

    struct mixin : ranges::basic_mixin< array_cursor_t > {
        mixin () = default;
        using ranges::basic_mixin< array_cursor_t >::basic_mixin;
        mixin (Object* p, size_t n) : mixin (array_cursor_t (p, n)) { }
    };

    auto read () const noexcept {
        ASSERT (p_);

        if (!valid_) {
            fetch ();
        }

        return cache_;
    }

    void next () noexcept {
        ASSERT (p_);
        ASSERT (i_ < n_);
        valid_ = false;
        ++i_;
    }

    void prev () noexcept {
        ASSERT (p_);
        ASSERT (i_);
        valid_ = false;
        --i_;
    }

    void advance (std::ptrdiff_t off) {
        ASSERT (p_);
        ASSERT (i_ + off <= n_);
        valid_ = false;
        i_ += off;
    }

    std::ptrdiff_t distance_to (array_cursor_t const& other) const {
        return p_
            ? (other.p_ ? other.i_ - i_ : n_ - i_)
            : (other.p_ ? other.i_ - other.n_ : 0);
    }

    bool equal (array_cursor_t const& other) const {
        return p_
            ? (other.p_ ? p_ == other.p_ && i_ == other.i_ : i_ == n_)
            : (!other.p_ || other.i_ == other.n_);
    }

private:
    void fetch () const {
        ASSERT (p_);
        ASSERT (n_ && i_ < n_);
        cache_ = array_get< T > (*p_, i_);
        valid_ = true;
    }

private:
    Object* p_;
    size_t n_, i_;

    mutable T cache_;
    mutable bool valid_;
};

template< typename T >
using array_iterator_t = ranges::basic_iterator< array_cursor_t< T > >;

template< typename T >
inline auto make_array_subrange (Object* pobj) {
    ASSERT (pobj && pobj->isArray ());
    return ranges::make_subrange (
        array_iterator_t< T > (pobj, size_t (pobj->arrayGetLength ())),
        array_iterator_t< T > ());
}

template< typename T, size_t... I >
inline auto make_tuple (T&& rng, std::index_sequence< I... >) {
    return std::make_tuple (rng [I]...);
}

} // namespace xpdf

#endif // XPDF_XPDF_ARRAYITERATOR_HH

#ifndef XPDF_XPDF_ARRAYITERATOR_HH
#define XPDF_XPDF_ARRAYITERATOR_HH

#include <defs.hh>

#include <boost/iterator/iterator_facade.hpp>
#include <range/v3/view/subrange.hpp>

#include <xpdf/xpdf.hh>

namespace xpdf {

template< typename T >
struct array_iterator_t : boost::iterator_facade<
    array_iterator_t< T >, T, boost::random_access_traversal_tag > {

    array_iterator_t () : p_ (), n_ (), i_ () { }

    array_iterator_t (Object* p) : p_ (p), n_ (), i_ () {
        ASSERT (p_ && is< Array > (*p_));
        n_ = p_->arrayGetLength ();
    }

private:
    friend class boost::iterator_core_access;

    void increment () {
        ASSERT (p_);
        ASSERT (i_ < n_);
        valid_ = false;
        ++i_;
    }

    void decrement () {
        ASSERT (p_);
        ASSERT (i_);
        valid_ = false;
        --i_;
    }

    bool equal (array_iterator_t< T > const& other) const {
        return p_
            ? (other.p_ ? p_ == other.p_ && i_ == other.i_ : i_ == n_)
            : (!other.p_ || other.i_ == other.n_);
    }

    T& dereference () const {
        ASSERT (p_);

        if (!valid_) {
            fetch ();
        }

        return cache_;
    }

    std::ptrdiff_t distance_to (array_iterator_t< T > const& other) const {
        return p_
            ? (other.p_ ? other.i_ - i_ : n_ - i_)
            : (other.p_ ? other.i_ - other.n_ : 0);
    }

private:
    void fetch () {
        ASSERT (p_);
        ASSERT (n_ && i_ < n_);
        cache_ = as< T > (p_);
        valid_ = true;
    }

private:
    Object* p_;
    size_t n_, i_;

    T cache_;
    bool valid_;
};

template< typename T >
inline auto make_array_subrange (Object* pobj) {
    return ranges::make_subrange (
        array_iterator_t< T >{ pobj }, array_iterator_t< T >{ });
}

} // namespace xpdf

#endif // XPDF_XPDF_ARRAYITERATOR_HH

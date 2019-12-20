// -*- mode: c++; -*-

#ifndef XPDF_XPDF_XPDF_HH
#define XPDF_XPDF_XPDF_HH

#include <defs.hh>

#include <memory>
#include <tuple>
#include <vector>

#include <xpdf/Array.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Object.hh>

#include <boost/format.hpp>
using fmt = boost::format;

namespace xpdf {

struct object_free_t {
    void operator() (Object* p) const { if (p) p->free (); }
};

struct stream_free_t {
    void operator() (Stream* p) const { if (p) p->close (); }
};

#define XPDF_GUARD(type, x, d) \
    std::unique_ptr< type, std::function< void(type*) > > \
    XPDF_CAT (guard_, __LINE__) (x, d)

#define OBJECT_GUARD(x) XPDF_GUARD(Object, x, xpdf::object_free_t{ })
#define STREAM_GUARD(x) XPDF_GUARD(Stream, x, xpdf::stream_free_t{ })

template< typename T, typename ... U >
inline bool contains (T&& t, U&& ... u) { return ((t == u) || ...); }

template< typename T > bool is (Object&);

template<> inline bool is<    int > (Object& obj) { return obj.isInt (); }
template<> inline bool is< double > (Object& obj) { return obj.isNum (); }

template< typename T > T get (Object&);

template<> inline    int get<    int > (Object& obj) { return obj.getInt (); }
template<> inline double get< double > (Object& obj) { return obj.getNum (); }

template< typename T >
inline T as (Object& obj) {
    if (!is< T > (obj)) {
        throw std::runtime_error ("mismatched object type");
    }

    return get< T > (obj);
}

template< typename T >
inline T as (Dict& dict, const char* s) {
    Object obj;

    if (0 == dict.lookup (s, &obj)) {
        throw std::runtime_error (fmt ("missing key %1%") % s);
    }

    OBJECT_GUARD (&obj);
    return as< T > (obj);
}

template< typename T >
inline T array_get (Object& arr, size_t i) {
    Object tmp;

    if (0 == arr.arrayGet (i, &tmp)) {
        throw std::runtime_error ((fmt ("invalid array or index %1%") % i).str ());
    }

    OBJECT_GUARD (&tmp);
    return as< T > (tmp);
}

template< typename T >
inline auto as_array_of (Object& src) {
    std::vector< T > xs;

    for (size_t i = 0, n = src.arrayGetLength (); i < n; ++i) {
        xs.emplace_back (array_get< T > (src, i));
    }

    return xs;
}

template< >
inline auto as_array_of< size_t > (Object& src) {
    std::vector< size_t > xs;

    for (size_t i = 0, n = src.arrayGetLength (); i < n; ++i) {
        xs.emplace_back (array_get< int > (src, i));
    }

    return xs;
}

template< typename T >
inline auto as_array_of (Dict& dict, const char* s) {
    Object obj;

    if (0 == dict.lookup (s, &obj)) {
        throw std::runtime_error ((fmt ("missing array \"%1%\"") % s).str ());
    }

    OBJECT_GUARD (&obj);
    return as_array_of< T > (obj);
}

template< typename T >
inline std::optional< std::vector< T > >
maybe_array_of (Object& obj) {
    try {
        return as_array_of< T > (obj);
    }
    catch (...) {
        return { };
    }
}

template< typename T >
inline std::optional< std::vector< T > >
maybe_array_of (Dict& dict, const char* s) {
    try {
        return as_array_of< T > (dict, s);
    }
    catch (...) {
        return { };
    }
}

} // namespace xpdf

#endif // XPDF_XPDF_XPDF_HH

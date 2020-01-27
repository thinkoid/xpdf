// -*- mode: c++; -*-
// Copyright 2001-2003 Glyph & Cog, LLC
// Copyright 2019-2020 Thinkoid, LLC.

#include <defs.hh>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <numeric>
#include <optional>
#include <tuple>
#include <vector>

#include <goo/memory.hh>
#include <goo/GList.hh>

#include <xpdf/bitpack.hh>
#include <xpdf/xpdf.hh>

#include <xpdf/array.hh>
#include <xpdf/obj.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Stream.hh>
#include <xpdf/Error.hh>
#include <xpdf/function.hh>
#include <xpdf/obj.hh>

#include <boost/noncopyable.hpp>

#include <range/v3/all.hpp>
using namespace ranges;

////////////////////////////////////////////////////////////////////////

namespace xpdf {

struct function_t::impl_t {
    virtual ~impl_t () { }

    virtual void
    operator() (const double*, const double* const, double*) const = 0;

    virtual size_t   arity () const = 0;
    virtual size_t coarity () const = 0;

    virtual int type () const = 0;

    virtual std::string to_ps () const = 0;
};

namespace {

inline auto domain_from (Dict& dict) {
    return as_array< std::tuple< double, double > > (dict, "Domain");
}

inline auto range_from (Dict& dict) {
    return as_array< std::tuple< double, double > > (dict, "Range");
}

inline auto optional_range_from (Dict& dict) {
    return maybe_array< std::tuple< double, double > > (dict, "Range");
}

////////////////////////////////////////////////////////////////////////

struct identity_function_t : function_t::impl_t {
    identity_function_t ();
    ~identity_function_t () { }

    void operator() (const double*, const double* const, double*) const;

    size_t   arity () const { return function_t::max_arity; }
    size_t coarity () const { return function_t::max_arity; }

    int type () const { return -1; }

    std::string to_ps () const;

    //
    // Domain, range (defaulted):
    //
    std::vector< std::tuple< double, double > > domain, range;
};

inline auto make_default_domain () {
    return std::vector< std::tuple< double, double > >{
        function_t::max_arity, { 0., 1. } };
}

inline auto make_default_range () {
    return std::vector< std::tuple< double, double > >{
        function_t::max_arity, { 0., 0. } };
}

identity_function_t::identity_function_t ()
    : domain (make_default_domain ()), range (make_default_range ())
{ }

void
identity_function_t::operator() (
    const double* src, const double* const end, double* dst) const {
    copy (src, end, dst);
}

std::string identity_function_t::to_ps () const {
    return "{}\n";
}

////////////////////////////////////////////////////////////////////////

struct sampled_function_t : function_t::impl_t {
    sampled_function_t (Object&, Dict&);
    ~sampled_function_t () { }

    void operator() (const double*, const double* const, double*) const;

    size_t   arity () const { return domain.size (); }
    size_t coarity () const { return range.size (); }

    int type () const { return 2; }

    std::string to_ps () const;

    //
    // Domain, range (required):
    //
    std::vector< std::tuple< double, double > > domain, range;

    //
    // Encode and decode arrays (optional):
    //
    std::vector< std::tuple< double, double > > encode, decode;

    //
    // Samples (required):
    //
    std::vector< double > samples;

    //
    // Number of samples (required):
    //
    std::vector< size_t > sizes;

    //
    // Helper data, offsets:
    //
    std::vector< off_t > off;

    //
    // Helper data, multipliers:
    //
    std::vector< double > multipliers;
};

inline std::vector< size_t > sample_sizes_from (Dict& dict) {
    return xpdf::as_array< size_t > (dict, "Size");
}

std::vector< off_t >
make_offsets (const std::vector< size_t >& zs, size_t n) {
    const size_t m = zs.size ();

    std::vector< off_t > xs;
    xs.reserve (1UL << zs.size ());

    for (size_t i = 0; i < (1UL << m); ++i) {
        off_t off = 0;

        size_t j, t, bit;
        for (j = m - 1, t = i; j >= 1; --j, t <<= 1) {
            if (zs [j] == 1) {
                bit = 0;
            }
            else {
                bit = (t >> (m - 1)) & 1;
            }

            off = (off + bit) * zs [j - 1];
        }

        if (zs [0] == 1) {
            bit = 0;
        }
        else {
            bit = (t >> (m - 1)) & 1;
        }

        xs.push_back ((off + bit) * n);
    }

    return xs;
}

inline size_t bps_from (Dict& dict) {
    auto n = xpdf::as< int > (dict, "BitsPerSample");
    ASSERT (xpdf::contains (n, 1, 2, 4, 8, 12, 16, 24, 32));
    return n;
}

std::vector< char >
block_from (Object& obj, size_t n) {
    auto str = obj.as_stream ();

    std::vector< char > xs (n);

    str->reset ();
    str->getBlock (xs.data (), n);

    return xs;
}

inline std::vector< double >
samples_from (const std::vector< char >& xs, size_t n, size_t bps) {
    return unpack (xs.data (), xs.data () + xs.size (), n, bps);
}

std::vector< std::tuple< double, double > >
encode_array_from (Dict& dict, const std::vector< size_t >& default_) {
    auto xs = maybe_array< std::tuple< double, double > > (dict, "Encode");

    if (xs.empty ()) {
        transform (default_, back_inserter (xs), [](auto x) {
            return std::make_tuple (size_t (0), size_t (x - 1));
        });
    }

    return xs;
}

std::vector< std::tuple< double, double > >
decode_array_from (
    Dict& dict, const std::vector< std::tuple< double, double > >& default_) {
    auto xs = maybe_array< std::tuple< double, double > > (dict, "Decode");
    return xs.empty () ? default_ : xs;
}

std::vector< double >
multipliers_from (
    const std::vector< std::tuple< double, double > >& d,
    const std::vector< std::tuple< double, double > >& e) {

    std::vector< double > xs;

    transform (
        views::zip (d, e), ranges::back_inserter (xs),
        [](auto arg) {
            const auto& [d, e] = arg;

            const auto& [d_0, d_1] = d;
            const auto& [e_0, e_1] = e;

            return double (e_1 - e_0) / (d_1 - d_0);
        });

    return xs;
}

sampled_function_t::sampled_function_t (Object& obj, Dict& dict)
    : domain (domain_from (dict)), range (range_from (dict)) {
    ASSERT (domain.size () <= function_t::max_arity);
    ASSERT ( range.size () <= function_t::max_arity);

    ASSERT (!range.empty ());

    sizes = sample_sizes_from (dict);
    ASSERT (domain.size () == sizes.size ());

    off = make_offsets (sizes, range.size ());

    const size_t nsamples = accumulate (
        sizes, range.size (), std::multiplies< size_t >{ });

    const size_t bps = bps_from (dict);
    const size_t nbytes = (nsamples * bps + 7) >> 3;

    samples = samples_from (block_from (obj, nbytes), nsamples, bps);
    ASSERT (nsamples == samples.size ());

    encode = encode_array_from (dict, sizes);
    ASSERT (encode.size () == domain.size ());

    decode = decode_array_from (dict, range);
    ASSERT (decode.size () == range.size ());

    multipliers = multipliers_from (domain, encode);
}

void
sampled_function_t::operator() (
    const double* src, const double* const end, double* dst) const {
    int e [function_t::max_arity] = { };

    double efrac0 [function_t::max_arity] = { };
    double efrac1 [function_t::max_arity] = { };

    int j, k, idx0, t;

    const size_t m = domain.size ();
    const size_t n = range.size ();

    ASSERT (m == std::distance (src, end));

    // map input values into sample array
    for (size_t i = 0; i < m; ++i) {
        const auto& [d_0, d_1] = domain [i];
        const auto& [e_0, e_1] = encode [i];

        double x = (src [i] - d_0) * multipliers [i] + e_0;
        x = std::isnan (x) ? 0 : std::clamp (x, 0., double (sizes [i] - 1));

        e [i] = int (x);

        if (e [i] == sizes [i] - 1 && sizes [i] > 1) {
            // this happens if in [i] = std::get< 1 > (d [i])
            e [i] = sizes [i] - 2;
        }

        efrac1 [i] = x - e [i];
        efrac0 [i] = 1 - efrac1 [i];
    }

    // compute index for the first sample to be used
    idx0 = 0;

    for (k = m - 1; k >= 1; --k) {
        idx0 = (idx0 + e [k]) * sizes [k - 1];
    }

    idx0 = (idx0 + e [0]) * n;

    std::vector< double > scratch (1UL << domain.size ());

    // for each output, do m-linear interpolation
    for (size_t i = 0; i < n; ++i) {
        // pull 2^m values out of the sample array
        for (j = 0; j < (1UL << m); ++j) {
            scratch [j] = samples [idx0 + off [j] + i];
        }

        // do m sets of interpolations
        for (j = 0, t = (1UL << m); j < m; ++j, t >>= 1) {
            for (k = 0; k < t; k += 2) {
                scratch [k >> 1] = efrac0 [j] * scratch [k] + efrac1 [j] * scratch [k + 1];
            }
        }

        const auto& [x_0, x_1] = decode [i];
        dst [i] = scratch [0] * (x_1 - x_0) + x_0;

        const auto& [r_0, r_1] = range [i];
        dst [i] = std::clamp (dst [i], r_0, r_1);
    }
}

std::string sampled_function_t::to_ps () const {
    return { };
}

////////////////////////////////////////////////////////////////////////

struct exponential_function_t : function_t::impl_t {
    exponential_function_t (Object&, Dict&);
    ~exponential_function_t () { }

    void operator() (const double*, const double* const, double*) const;

    size_t arity () const {
        ASSERT (1UL == domain.size ());
        return domain.size ();
    }

    size_t coarity () const {
        ASSERT (c0.size () == c1.size ());
        return c0.size ();
    }

    int type () const { return 3; }

    std::string to_ps () const;

    //
    // Domain (required), range (optional):
    //
    std::vector< std::tuple< double, double > > domain, range;

    //
    // Optional arrays, default to { 0 } and { 1 }, respectively:
    //
    std::vector< double > c0, c1;

    //
    // Interpolation exponent (required):
    //
    double e;
};

inline std::vector< double >
exponential_array_from (Dict& dict, const char* s, int default_) {
    auto xs = maybe_array< double > (dict, s);

    if (xs.empty ()) {
        xs.push_back (default_);
    }

    return xs;
}

exponential_function_t::exponential_function_t (Object& obj, Dict& dict)
    : domain (domain_from (dict)), range (optional_range_from (dict)),
      c0 (exponential_array_from (dict, "C0", 0)),
      c1 (exponential_array_from (dict, "C1", 1)),
      e (xpdf::as< double > (dict, "N")) {
    ASSERT (1UL == domain.size ());
    ASSERT (2 > range.size ());
}

void
exponential_function_t::operator() (
    const double* src, const double* const end, double* dst) const {
    const auto& [d_0, d_1] = domain [0];

    ASSERT (1UL == std::distance (src, end));
    double x = std::clamp (src [0], d_0, d_1);

    for (size_t i = 0, n = c0.size (); i < n; ++i) {
        dst [i] = c0 [i] + pow (x, e) * (c1 [i] - c0 [i]);

        if (!range.empty ()) {
            ASSERT (i < range.size ());
            auto& [r_0, r_1] = range [i];

            dst [i] = std::clamp (dst [i], r_0, r_1);
        }
    }
}

std::string exponential_function_t::to_ps () const {
    return { };
}

////////////////////////////////////////////////////////////////////////

std::shared_ptr< function_t::impl_t >
make_function (Object&, size_t = 0);

struct stitching_function_t : function_t::impl_t {
    stitching_function_t (Object&, Dict&, int /* max recursion */ = 0);
    ~stitching_function_t () { }

    void operator() (const double*, const double* const, double*) const;

    size_t   arity () const { return 1UL; }
    size_t coarity () const { return 1UL; }

    int type () const { return 3; }

    std::string to_ps () const;

    //
    // Domain (required), range (optional, if present all functions' coarities
    // agree:
    //
    std::vector< std::tuple< double, double > > domain, range;

    //
    // Stitched functions of arity 1 and coarity 1 (required):
    //
    std::vector< std::shared_ptr< function_t::impl_t > > fs;

    //
    // Array of values that partition the domain (required):
    //
    std::vector< double > bounds;

    //
    // Mapping of domain and bounds to the domain of each function:
    //
    std::vector< std::tuple< double, double > > encode;

    //
    // Helper data:
    //
    std::vector< double > scale;
};

std::vector< std::shared_ptr< function_t::impl_t > >
stitched_functions_from (Dict& dict, int recursion) {
    Object arr;
    dict.lookup ("Functions", &arr);

    std::vector< std::shared_ptr< function_t::impl_t > > fs;

    for (size_t i = 0, k = arr.as_array ().size (); i < k; ++i) {
        Object fun;
        fun = resolve (arr [i]);

        if (auto p = make_function (fun, recursion + 1)) {
            fs.push_back (p);
        }
    }

    return fs;
}

auto
bounds_array_from (Dict& dict, double lhs, double rhs) {
    std::vector< double > xs{ lhs };

    auto ys = as_array< double > (dict, "Bounds");
    xs.reserve (xs.size () + ys.size () + 1);

    xs.insert (xs.end (), ys.begin (), ys.end ());
    xs.push_back (rhs);

    return xs;
}

inline auto
encode_array_from (Dict& dict) {
    return as_array< std::tuple< double, double > > (dict, "Encode");
}

stitching_function_t::stitching_function_t (
    Object&, Dict& dict, int recursion)
    : domain (domain_from (dict)), range (optional_range_from (dict)) {

    ASSERT (1UL == domain.size ());

    fs = stitched_functions_from (dict, recursion);
    ASSERT (!fs.empty ());

    const size_t k = fs.size ();

    auto is_like = [](int n) { return [=](auto& f){ return f->coarity () == n; }; };
    ASSERT (::all_of (fs, is_like (fs [0]->coarity ())));
    ASSERT (range.empty () || range.size () == fs [0]->coarity ());

    bounds = bounds_array_from (
        dict, std::get< 0 > (domain [0]), std::get< 1 > (domain [0]));
    ASSERT (k + 1 == bounds.size ());

    encode = encode_array_from (dict);
    ASSERT (k == encode.size ());

    scale.resize (k);

    for (size_t i = 0; i < k; ++i) {
        auto [e_0, e_1] = encode [i];
        auto [d_0, d_1] = std::tie (bounds [i], bounds [i + 1]);
        scale [i] = d_0 == d_1 ? 0 : (e_1 - e_0) / (d_1 - d_0);
    }
}

void
stitching_function_t::operator() (
    const double* src, const double* const end, double* dst) const {
    ASSERT (1UL == domain.size ());
    const auto& [d_0, d_1] = domain [0];

    ASSERT (1 == std::distance (src, end));
    double x = std::clamp (src [0], d_0, d_1);

    size_t i = 0;
    for (; i < fs.size () - 1 && x >= bounds [i + 1]; ++i) ;

    x = std::get< 0 > (encode [i]) + (x - bounds [i]) * scale [i];
    (*fs [i]) (&x, &x + 1, dst);
}

std::string stitching_function_t::to_ps () const {
    return { };
}

////////////////////////////////////////////////////////////////////////

enum {
    psOpAbs,       //  0
    psOpAdd,       //  1
    psOpAnd,       //  2
    psOpAtan,      //  3
    psOpBitshift,  //  4
    psOpCeiling,   //  5
    psOpCopy,      //  6
    psOpCos,       //  7
    psOpCvi,       //  8
    psOpCvr,       //  9
    psOpDiv,       // 10
    psOpDup,       // 11
    psOpEq,        // 12
    psOpExch,      // 13
    psOpExp,       // 14
    psOpFalse,     // 15
    psOpFloor,     // 16
    psOpGe,        // 17
    psOpGt,        // 18
    psOpIdiv,      // 19
    psOpIndex,     // 20
    psOpLe,        // 21
    psOpLn,        // 22
    psOpLog,       // 23
    psOpLt,        // 24
    psOpMod,       // 25
    psOpMul,       // 26
    psOpNe,        // 27
    psOpNeg,       // 28
    psOpNot,       // 29
    psOpOr,        // 30
    psOpPop,       // 31
    psOpRoll,      // 32
    psOpRound,     // 33
    psOpSin,       // 34
    psOpSqrt,      // 35
    psOpSub,       // 36
    psOpTrue,      // 37
    psOpTruncate,  // 38
    psOpXor,       // 39
    psOpPush,      // 40
    psOpJ,         // 41
    psOpJz,        // 42
    nPSOps         // 43
};

struct postscript_function_t : function_t::impl_t {
    postscript_function_t (Object&, Dict&);
    ~postscript_function_t () { }

    void operator() (const double*, const double* const, double*) const;

    struct code_t {
        int op;
        union {
            double d;
            int i;
        } val;
    };

    size_t   arity () const { return domain.size (); }
    size_t coarity () const { return  range.size (); }

    int type () const { return 4; }

    std::string to_ps () const;

    //
    // Domain (required), range (optional, if present all functions' coarities
    // agree:
    //
    std::vector< std::tuple< double, double > > domain, range;

    std::vector< code_t > cs;
    std::string s;

    std::vector< double > exec (std::vector< double > stack) const;
};

static std::optional< std::string >
next_token (Stream& str) {
    int c = 0;

    for (bool comment = false;;) {
        c = str.getChar ();

        if (c == EOF) {
            return { };
        }

        if (comment) {
            if (c == '\x0a' || c == '\x0d') {
                comment = false;
            }
        }
        else if (c == '%') {
            comment = true;
        }
        else if (!isspace (c)) {
            break;
        }
    }

    std::string s;

    if (c == '{' || c == '}') {
        s.append (1UL, char (c));
    }
    else if (isdigit (c) || c == '.' || c == '-') {
        while (true) {
            s.append (1UL, char (c));
            c = str.lookChar ();

            if (c == EOF || !(isdigit (c) || c == '.' || c == '-')) {
                break;
            }

            str.getChar ();
        }
    }
    else {
        while (1) {
            s.append (1UL, char (c));
            c = str.lookChar ();

            if (c == EOF || !isalnum (c)) {
                break;
            }

            str.getChar ();
        }
    }

    return s;
}

static std::list< std::string >
tokenize (Stream& str) {
    std::list< std::string > ss;

    while (auto opt = next_token (str)) {
        ss.emplace_back (*opt);
    }

    return ss;
}

template< typename Iterator >
std::vector< postscript_function_t::code_t >
parse (Iterator& iter, Iterator last) {
    std::vector< postscript_function_t::code_t > xs;

    for (; iter != last;) {
        const auto& tok = *iter;

        if (isdigit (tok [0]) || tok [0] == '.' || tok [0] == '-') {
            //
            // Push value on the stack:
            //
            xs.push_back ({ psOpPush, { .d = std::stod (tok) } });
            ++iter;
        }
        else if (tok == "{") {
            //
            // Block start, signals the beginning of a block belonging to an
            // `if' or `ifelse' conditional:
            //
            auto then_block = parse (++iter, last);

            if (iter == last) {
                throw std::runtime_error ("incomplete PostScript conditional");
            }

            const auto& tok2 = *iter;

            if (tok2 == "if") {
                //
                // The conditional block is a plain `if':
                //
                xs.push_back ({ psOpJz, { .i = int (then_block.size ()) } });
                xs.insert (xs.end (), then_block.begin (), then_block.end ());

                ++iter;
            }
            else if (tok2 == "{") {
                xs.push_back ({ psOpJz, { .i = int (then_block.size ()) } });
                xs.insert (xs.end (), then_block.begin (), then_block.end ());

                auto else_block = parse (++iter, last);

                if (iter == last) {
                    throw std::runtime_error (
                        "incomplete PostScript conditional");
                }

                xs.push_back ({ psOpJ, { .i = int (else_block.size ()) } });
                xs.insert (xs.end (), else_block.begin (), else_block.end ());

                const auto& tok3 = *iter;

                if (tok3 != "ifelse") {
                    throw std::runtime_error ("incomplete PostScript conditional");
                }

                ++iter;
            }
            else {
                throw std::runtime_error (
                    format ("unexpected PostScript: {}", tok));
            }
        }
        else if (tok == "}") {
            ++iter;
            break;
        }
        else if (tok == "if" || tok == "ifelse") {
            throw std::runtime_error (
                format ("unexpected PostScript: {}", tok));
        }
        else {
            // Note: 'if' and 'ifelse' are parsed separately.
            // The rest are listed here in alphabetical order.
            // The index in this table is equivalent to the psOpXXX defines.
            static const std::vector< std::string > ns{
                "abs", "add", "and", "atan", "bitshift", "ceiling", "copy", "cos",
                "cvi", "cvr", "div", "dup", "eq", "exch", "exp", "false", "floor",
                "ge", "gt", "idiv", "index", "le", "ln", "log", "lt", "mod", "mul",
                "ne", "neg", "not", "or", "pop", "roll", "round", "sin", "sqrt",
                "sub", "true", "truncate", "xor"
            };

            auto iter2 = std::find (ns.begin (), ns.end (), tok);

            if (iter2 == ns.end ()) {
                throw std::runtime_error (
                    format ("invalid PostScript: {}", tok));
            }

            xs.push_back ({ int (std::distance (iter2, ns.end ())), { 0 } });

            ++iter;
        }
    }

    return xs;
}

postscript_function_t::postscript_function_t (Object& obj, Dict& dict)
    : domain (domain_from (dict)), range (optional_range_from (dict)) {
    ASSERT (domain.size () <= function_t::max_arity);

    ASSERT (range.size () <= function_t::max_arity);
    ASSERT (!range.empty ());

    auto str = obj.as_stream ();
    str->reset ();

    const auto ts = tokenize (*str);

    auto iter = ts.begin ();
    cs = parse (iter, ts.end ());

    ASSERT (!cs.empty ());
}

std::vector< double >
postscript_function_t::exec (std::vector< double > stack) const {
    auto _0 = [&]() -> double& { return stack.back (); };
    auto _1 = [&]() -> double& { return *++stack.rbegin (); };

    //
    // TODO: replace asserts
    //
    for (auto iter = cs.begin (), last = cs.end (); iter != last; ++iter) {
        switch (iter->op) {
        case psOpAbs:
            ASSERT (!stack.empty ());
            _0 () = fabs (_0 ());
            break;

        case psOpAdd:
            ASSERT (stack.size () > 1);
            _1 () = _0 () + _1 ();
            stack.pop_back ();
            break;

        case psOpAnd:
            ASSERT (stack.size () > 1);
            _1 () = int (_0 ()) & int (_1 ());
            stack.pop_back ();
            break;

        case psOpAtan:
            ASSERT (stack.size () > 1);
            _1 () = atan2 (_1 (), _0 ());
            stack.pop_back ();
            break;

        case psOpBitshift: {
            ASSERT (stack.size () > 1);

            auto k = int (_1 ());
            auto n = int (_0 ());

            _1 () = (n > 0) ? (k << n) : (n < 0) ? k >> -n : k;

            stack.pop_back ();
        }
            break;

        case psOpCeiling:
            ASSERT (!stack.empty ());
            _0 () = ceil (_0 ());
            break;

        case psOpCopy: {
            ASSERT (!stack.empty ());

            auto off = off_t (_0 ());

            if (off < 0 || off > stack.size ()) {
                throw std::runtime_error ("invalid PostScript copy operand");
            }

            if (off) {
                stack.reserve (stack.size () + off);
                stack.insert (stack.end (), stack.end () - 4, stack.end ());
            }
        }
            break;

        case psOpCos:
            ASSERT (!stack.empty ());
            _0 () = cos (_0 ());
            break;

        case psOpCvi:
            ASSERT (!stack.empty ());
            _0 () = int (_0 ());
            break;

        case psOpCvr:
            ASSERT (!stack.empty ());
            break;

        case psOpDiv:
            ASSERT (stack.size () > 1);
            _1 () = _1 () / _0 ();
            stack.pop_back ();
            break;

        case psOpDup:
            ASSERT (!stack.empty ());
            stack.push_back (stack.back ());
            break;

        case psOpEq:
            ASSERT (stack.size () > 1);
            _1 () = _0 () == _1 () ? 1 : 0;
            stack.pop_back ();
            break;

        case psOpExch:
            ASSERT (stack.size () > 1);
            std::swap (_0 (), _1 ());
            break;

        case psOpExp:
            ASSERT (stack.size () > 1);
            _1 () = pow (_1 (), _0 ());
            stack.pop_back ();
            break;

        case psOpFalse:
            stack.push_back (0);
            break;

        case psOpFloor:
            ASSERT (!stack.empty ());
            _0 () = floor (_0 ());
            break;

        case psOpGe:
            ASSERT (stack.size () > 1);
            _1 () = _1 () >= _0 () ? 1 : 0;
            stack.pop_back ();
            break;

        case psOpGt:
            ASSERT (stack.size () > 1);
            _1 () = _1 () > _0 () ? 1 : 0;
            stack.pop_back ();
            break;

        case psOpIdiv:
            ASSERT (stack.size () > 1);
            _1 () = int (_1 ()) / int (_0 ());
            stack.pop_back ();
            break;

        case psOpIndex: {
            ASSERT (!stack.empty ());

            const off_t k = off_t (_0 ());
            ASSERT (k >= 0);

            stack.pop_back ();
            ASSERT (k < stack.size ());

            stack.push_back (*(stack.end () - k));
        }
            break;

        case psOpLe:
            ASSERT (stack.size () > 1);
            _1 () = _1 () <= _0 () ? 1 : 0;
            stack.pop_back ();
            break;

        case psOpLn:
            ASSERT (!stack.empty ());
            _0 () = log (_0 ());
            break;

        case psOpLog:
            ASSERT (!stack.empty ());
            _0 () = log10 (_0 ());
            break;

        case psOpLt:
            ASSERT (stack.size () > 1);
            _1 () = _1 () < _0 () ? 1 : 0;
            stack.pop_back ();
            break;

        case psOpMod:
            ASSERT (stack.size () > 1);
            _1 () = int (_1 ()) % int (_0 ());
            stack.pop_back ();
            break;

        case psOpMul:
            ASSERT (stack.size () > 1);
            _1 () = _1 () * _0 ();
            stack.pop_back ();
            break;

        case psOpNe:
            ASSERT (stack.size () > 1);
            _1 () = _1 () != _0 () ? 1 : 0;
            stack.pop_back ();
            break;

        case psOpNeg:
            ASSERT (!stack.empty ());
            _0 () = -_0 ();
            break;

        case psOpNot:
            ASSERT (!stack.empty ());
            _0 () = _0 () == 0 ? 1 : 0;
            break;

        case psOpOr:
            ASSERT (stack.size () > 1);
            _1 () = (int)_1 () | (int)_0 ();
            stack.pop_back ();
            break;

        case psOpPop:
            ASSERT (!stack.empty ());
            stack.pop_back ();
            break;

        case psOpRoll: {
            ASSERT (stack.size () > 1);

            //
            // n is the width of the window, from the top of the stack:
            //
            const size_t n = _1 ();

            //
            // j indicates the circular motion offset:
            // -- positive pops top and inserts it at end of window
            // -- negative pops end of window and pushes it to top
            //
            const size_t j = size_t (_0 ()) % n;

            stack.resize (stack.size () - 2);
            ASSERT (n <= stack.size ());

            auto iter = stack.end () - n, last = stack.end (), iter2 = last - j;

            std::rotate (iter, iter2, last);
        }
            break;

        case psOpRound:
            ASSERT (!stack.empty ());
            _0 () = std::round (_0 ());
            break;

        case psOpSin:
            ASSERT (!stack.empty ());
            _0 () = sin (_0 ());
            break;

        case psOpSqrt:
            ASSERT (!stack.empty ());
            _0 () = sqrt (_0 ());
            break;

        case psOpSub:
            ASSERT (stack.size () > 1);
            _1 () = _1 () - _0 ();
            stack.pop_back ();
            break;

        case psOpTrue:
            stack.push_back (1);
            break;

        case psOpTruncate:
            ASSERT (!stack.empty ());
            _0 () = std::trunc (_0 ());
            break;

        case psOpXor:
            ASSERT (stack.size () > 1);
            _1 () = int (_1 ()) ^ int (_0 ());
            stack.pop_back ();
            break;

        case psOpPush:
            stack.push_back (iter->val.d);
            break;

        case psOpJ:
            if (const size_t off = iter->val.i) {
                ASSERT (std::distance (iter, last) < off);
                std::advance (iter, off - 1);
            }
            break;

        case psOpJz: {
            ASSERT (!stack.empty ());

            const auto b = bool (_0 ());

            stack.pop_back ();
            ASSERT (!stack.empty ());

            if (!b) {
                const size_t off = size_t (iter->val.i);

                if (off) {
                    ASSERT (std::distance (iter, last) < off);
                    std::advance (iter, off - 1);
                }
            }
        }
            break;

        default:
            throw std::runtime_error (
                format ("invalid PostScript code: {}", iter->op));
        }
    }

    return stack;
}

void
postscript_function_t::operator() (
    const double* src, const double* const end, double* dst) const {

    const auto stack = exec (std::vector< double > (src, end));
    ASSERT (stack.size () == range.size ());

    transform (views::zip (range, stack | views::reverse), dst, [](auto arg) {
        const auto& [r, x] = arg;
        const auto& [r_0, r_1] = r;
        return std::clamp (x, r_0, r_1);
    });
}

std::string postscript_function_t::to_ps () const {
    return { };
}

////////////////////////////////////////////////////////////////////////

static inline Dict*
dictionary_from (Object& obj) {
    return obj.is_stream ()
        ? obj.streamGetDict ()
        : obj.is_dict () ? obj.as_dict_ptr () : 0;
}

std::shared_ptr< function_t::impl_t >
make_function (Object& obj, size_t recursion /* = 0 */) {
    if (recursion > function_t::max_recursion) {
        throw std::runtime_error ("function definition recursion limit");
    }

    if (obj.is_name ("Identity")) {
        return std::make_shared< identity_function_t > ();
    }
    else {
        auto p = dictionary_from (obj);
        ASSERT (p);

        switch (auto type = as< int >(*p, "FunctionType")) {
        case 0: return std::make_shared<     sampled_function_t > (obj, *p);
        case 2: return std::make_shared< exponential_function_t > (obj, *p);
        case 3: return std::make_shared<   stitching_function_t > (obj, *p, recursion + 1);
        case 4: return std::make_shared<  postscript_function_t > (obj, *p);
        default:
            throw std::runtime_error (
                format ("invalid function type: {}", type));
        }
    }
}

} // anonymous

void
function_t::operator() (
    const double* src, const double* const end, double* dst) const {
    return p_->operator() (src, end, dst);
}

size_t
function_t::arity () const {
    return p_->arity ();
}

size_t
function_t::coarity () const {
    return p_->coarity ();
}

std::string
function_t::to_ps () const {
    return p_->to_ps ();
}

function_t make_function (Object& obj) {
    return function_t{ make_function (obj, 0) };
}

} // namespace xpdf

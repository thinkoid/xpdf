//========================================================================
//
// Function.cc
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#include <defs.hh>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <vector>

#include <goo/memory.hh>
#include <goo/GList.hh>

#include <xpdf/xpdf.hh>
#include <xpdf/Object.hh>
#include <xpdf/Dict.hh>
#include <xpdf/Stream.hh>
#include <xpdf/Error.hh>
#include <xpdf/Function.hh>

#include <boost/noncopyable.hpp>

#define STD_COPY_C_ARRAY(x, y)                                  \
    std::copy (&x[0], &x[0] + sizeof x / sizeof x[0], &y[0])

#define STD_COPY_C_ARRAY2(x, y)                                 \
    std::copy (&x[0][0], &x[0][0] + sizeof x / sizeof x[0][0], &y[0][0])

////////////////////////////////////////////////////////////////////////

// TODO: the copy constructors
Function::Function ()
    : m (), n (), domain (), range (), hasRange ()
{ }

Function::~Function () { }

Function* Function::parse (Object* pobj, int recursion_level) {
    static const size_t max_recursion = 8U;

    if (recursion_level > max_recursion) {
        error (errSyntaxError, -1, "Loop detected in function objects");
        return 0;
    }

    Dict* dict = 0;

    if (pobj->isStream ()) {
        dict = pobj->streamGetDict ();
    }
    else if (pobj->isDict ()) {
        dict = pobj->getDict ();
    }
    else if (pobj->isName ("Identity")) {
        return new IdentityFunction ();
    }
    else {
        error (errSyntaxError, -1, "Expected function dictionary or stream");
        return 0;
    }

    Object obj;

    dict->lookup ("FunctionType", &obj);
    OBJECT_GUARD (&obj);

    if (!obj.isInt ()) {
        error (errSyntaxError, -1, "Function type is missing or wrong type");
        return 0;
    }

    Function* func = 0;

    switch (const int type = obj.getInt ()) {
    case 0:
        func = new SampledFunction (pobj, dict);
        break;

    case 2:
        func = new ExponentialFunction (pobj, dict);
        break;

    case 3:
        func = new StitchingFunction (pobj, dict, recursion_level);
        break;

    case 4:
        func = new PostScriptFunction (pobj, dict);
        break;

    default:
        error (errSyntaxError, -1, "Unimplemented function type ({0:d})", type);
        break;
    }

    if (func && !func->isOk ()) {
        delete func;
        func = 0;
    }

    return func;
}

bool Function::init (Dict* dict) {
    // TODO: managed resource
    Object obj, obj2;

    {
        dict->lookup ("Domain", &obj);
        OBJECT_GUARD (&obj);

        // The domain
        if (!obj.isArray ()) {
            error (errSyntaxError, -1, "Function is missing domain");
            return false;
        }

        if (const auto opt = xpdf::maybe_array_of< double > (obj)) {
            const auto& xs = *opt;

            ASSERT (0 == (xs.size () & 1U));
            m = xs.size () / 2;

            for (size_t i = 0; i < xs.size (); ++i) {
                domain[i >> 1][i & 1] = xs[i];
            }
        }
        else {
            return false;
        }
    }

    hasRange = false;

    dict->lookup ("Range", &obj);
    OBJECT_GUARD (&obj);

    if (obj.isArray ()) {
        if (const auto opt = xpdf::maybe_array_of< double > (obj)) {
            const auto& xs = *opt;

            ASSERT (0 == (xs.size () & 1U));
            n = xs.size () / 2;

            for (size_t i = 0; i < xs.size (); ++i) {
                range[i >> 1][i & 1] = xs[i];
            }

            hasRange = true;
        }
        else {
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////

IdentityFunction::IdentityFunction () {
    //
    // Fill these in with arbitrary values just in case they get used
    // somewhere
    //
    m = funcMaxInputs;
    n = funcMaxOutputs;

    for (size_t i = 0; i < funcMaxInputs; ++i) {
        domain [i][0] = 0;
        domain [i][1] = 1;
    }

    hasRange = false;
}

IdentityFunction::~IdentityFunction () { }

void IdentityFunction::transform (double* in, double* out) {
    std::copy (in, in + funcMaxOutputs, out);
}

////////////////////////////////////////////////////////////////////////

SampledFunction::SampledFunction (Object* funcObj, Dict* dict)
    : sampleSize (),
      encode (),
      decode (),
      inputMul (),
      idxOffset (),
      samples (),
      nSamples (),
      sBuf (),
      cacheIn (),
      cacheOut (),
      ok ()
{
    Stream* str;
    int sampleBits;
    double sampleMul;
    unsigned buf, bitMask;
    int bits;
    unsigned s;
    double in[funcMaxInputs];
    int bit, idx;

    //----- initialize the generic stuff
    if (!init (dict)) {
        return;
    }

    if (!hasRange) {
        error (errSyntaxError, -1, "Type 0 function is missing range");
        return;
    }

    if (m > sampledFuncMaxInputs) {
        error (
            errSyntaxError, -1,
            "Sampled functions with more than {0:d} inputs are unsupported",
            sampledFuncMaxInputs);
        return;
    }

    //----- buffer
    sBuf = (double*)calloc (1 << m, sizeof (double));

    //----- get the stream
    if (!funcObj->isStream ()) {
        error (errSyntaxError, -1, "Type 0 function isn't a stream");
        return;
    }

    str = funcObj->getStream ();

    //----- Size
    {
        Object obj;

        dict->lookup ("Size", &obj);
        OBJECT_GUARD (&obj);

        if (!obj.isArray () || obj.arrayGetLength () != m) {
            error (errSyntaxError, -1, "Function has missing or invalid size array");
            return;
        }

        if (const auto opt = xpdf::maybe_array_of< int > (obj)) {
            const auto& xs = *opt;
            std::copy (xs.begin (), xs.end (), sampleSize);
        }
        else {
            return;
        }
    }

    idxOffset = (int*)calloc (1U << m, sizeof (int));

    for (size_t i = 0, n = 1U << m; i < n; ++i) {
        idx = 0;

        size_t j, t;

        for (j = m - 1, t = i; j >= 1; --j, t <<= 1) {
            if (sampleSize [j] == 1) {
                bit = 0;
            }
            else {
                bit = (t >> (m - 1)) & 1;
            }

            idx = (idx + bit) * sampleSize[j - 1];
        }

        if (sampleSize[0] == 1) {
            bit = 0;
        }
        else {
            bit = (t >> (m - 1)) & 1;
        }

        idxOffset[i] = (idx + bit) * Function::n;
    }

    //----- BitsPerSample
    {
        Object obj;

        dict->lookup ("BitsPerSample", &obj);
        OBJECT_GUARD (&obj);

        if (!obj.isInt ()) {
            error (
                errSyntaxError, -1,
                "Function has missing or invalid BitsPerSample");
            return;
        }

        sampleBits = obj.getInt ();
        sampleMul = 1.0 / (pow (2.0, (double)sampleBits) - 1);
    }

    //----- Encode
    {
        Object obj;

        dict->lookup ("Encode", &obj);
        OBJECT_GUARD (&obj);

        if (obj.isArray () && obj.arrayGetLength () == 2 * m) {
            if (const auto opt = xpdf::maybe_array_of< double > (obj)) {
                const auto& xs = *opt;
                ASSERT (0 == (xs.size () & 1U));

                for (size_t i = 0; i < xs.size (); ++i) {
                    encode[i >> 1][i & 1] = xs[i];
                }
            }
            else {
                return;
            }
        }
        else {
            for (size_t i = 0; i < m; ++i) {
                encode[i][0] = 0;
                encode[i][1] = sampleSize[i] - 1;
            }
        }
    }

    for (size_t i = 0; i < m; ++i) {
        inputMul[i] =
            (encode[i][1] - encode[i][0]) /
            (domain[i][1] - domain[i][0]);
    }

    //----- Decode
    {
        Object obj;

        dict->lookup ("Decode", &obj);
        OBJECT_GUARD (&obj);

        if (obj.isArray () && obj.arrayGetLength () == 2 * n) {
            if (const auto opt = xpdf::maybe_array_of< double > (obj)) {
                const auto& xs = *opt;
                ASSERT (0 == (xs.size () & 1U));

                for (size_t i = 0; i < xs.size (); ++i) {
                    decode[i >> 1][i & 1] = xs[i];
                }
            }
            else {
                return;
            }
        }
        else {
            for (size_t i = 0; i < n; ++i) {
                decode [i][0] = range [i][0];
                decode [i][1] = range [i][1];
            }
        }
    }

    //----- samples
    nSamples = std::accumulate (
        &sampleSize [0], &sampleSize [0] + m, n,
        std::multiplies< size_t >{ });

    samples = (double*)calloc (nSamples, sizeof (double));

    buf = 0;
    bits = 0;
    bitMask = (sampleBits < 32) ? ((1 << sampleBits) - 1) : 0xffffffffU;
    str->reset ();

    for (size_t i = 0; i < nSamples; ++i) {
        if (sampleBits == 8) {
            s = str->getChar ();
        }
        else if (sampleBits == 16) {
            s = str->getChar ();
            s = (s << 8) + str->getChar ();
        }
        else if (sampleBits == 32) {
            s = str->getChar ();
            s = (s << 8) + str->getChar ();
            s = (s << 8) + str->getChar ();
            s = (s << 8) + str->getChar ();
        }
        else {
            while (bits < sampleBits) {
                buf = (buf << 8) | (str->getChar () & 0xff);
                bits += 8;
            }
            s = (buf >> (bits - sampleBits)) & bitMask;
            bits -= sampleBits;
        }
        samples[i] = (double)s * sampleMul;
    }

    str->close ();

    // set up the cache
    for (size_t i = 0; i < m; ++i) {
        in[i] = domain[i][0];
        cacheIn[i] = in[i] - 1;
    }

    transform (in, cacheOut);
    ok = true;
}

SampledFunction::~SampledFunction () {
    if (idxOffset) {
        free (idxOffset);
    }

    if (samples) {
        free (samples);
    }

    if (sBuf) {
        free (sBuf);
    }
}

SampledFunction::SampledFunction (const SampledFunction& other)
    : Function (other),
      sampleSize (),
      encode (),
      decode (),
      inputMul (),
      idxOffset (),
      samples (),
      nSamples (),
      sBuf (),
      cacheIn (),
      cacheOut (),
      ok () {
    STD_COPY_C_ARRAY (other.sampleSize, sampleSize);
    STD_COPY_C_ARRAY (other.inputMul, inputMul);

    STD_COPY_C_ARRAY2 (other.encode, encode);
    STD_COPY_C_ARRAY2 (other.decode, decode);

    idxOffset = (int*)calloc (1 << m, sizeof (int));
    std::copy (&other.idxOffset[0], &other.idxOffset[0] + (1U << m), idxOffset);

    samples = (double*)calloc (nSamples, sizeof (double));
    std::copy (&other.samples[0], &other.samples[0] + nSamples, samples);

    sBuf = (double*)calloc (1 << m, sizeof (double));
}

void SampledFunction::transform (double* in, double* out) {
    double x;
    int e[funcMaxInputs] = { };

    double efrac0[funcMaxInputs] = { };
    double efrac1[funcMaxInputs] = { };

    int j, k, idx0, t;

    {
        // check the cache
        size_t i = 0;

        for (; i < m; ++i) {
            if (in[i] != cacheIn[i]) {
                break;
            }
        }

        if (i == m) {
            for (size_t i = 0; i < n; ++i) {
                out[i] = cacheOut[i];
            }
            return;
        }
    }

    // map input values into sample array
    for (size_t i = 0; i < m; ++i) {
        x = (in[i] - domain[i][0]) * inputMul[i] + encode[i][0];

        if (x < 0 || x != x) { // x!=x is a more portable version of isnan(x)
            x = 0;
        }
        else if (x > sampleSize[i] - 1) {
            x = sampleSize[i] - 1;
        }

        e[i] = (int)x;

        if (e[i] == sampleSize[i] - 1 && sampleSize[i] > 1) {
            // this happens if in[i] = domain[i][1]
            e[i] = sampleSize[i] - 2;
        }

        efrac1[i] = x - e[i];
        efrac0[i] = 1 - efrac1[i];
    }

    // compute index for the first sample to be used
    idx0 = 0;

    for (k = m - 1; k >= 1; --k) {
        idx0 = (idx0 + e[k]) * sampleSize[k - 1];
    }

    idx0 = (idx0 + e[0]) * n;

    // for each output, do m-linear interpolation
    for (size_t i = 0; i < n; ++i) {
        // pull 2^m values out of the sample array
        for (j = 0; j < (1 << m); ++j) {
            sBuf[j] = samples[idx0 + idxOffset[j] + i];
        }

        // do m sets of interpolations
        for (j = 0, t = (1 << m); j < m; ++j, t >>= 1) {
            for (k = 0; k < t; k += 2) {
                sBuf[k >> 1] = efrac0[j] * sBuf[k] + efrac1[j] * sBuf[k + 1];
            }
        }

        // map output value to range
        out[i] = sBuf[0] * (decode[i][1] - decode[i][0]) + decode[i][0];

        if (out[i] < range[i][0]) {
            out[i] = range[i][0];
        }
        else if (out[i] > range[i][1]) {
            out[i] = range[i][1];
        }
    }

    // save current result in the cache
    for (size_t i = 0; i < m; ++i) {
        cacheIn[i] = in[i];
    }

    for (size_t i = 0; i < n; ++i) {
        cacheOut[i] = out[i];
    }
}

////////////////////////////////////////////////////////////////////////

ExponentialFunction::ExponentialFunction (Object* funcObj, Dict* dict) {
    ok = false;

    //----- initialize the generic stuff
    if (!init (dict)) {
        return;
    }

    if (m != 1) {
        error (
            errSyntaxError, -1,
            "Exponential function with more than one input");
        return;
    }

    //----- C0
    {
        Object obj;

        dict->lookup ("C0", &obj);
        OBJECT_GUARD (&obj);

        if (obj.isArray ()) {
            if (hasRange && obj.arrayGetLength () != n) {
                error (errSyntaxError, -1, "Function's C0 array is wrong length");
                return;
            }

            if (const auto opt = xpdf::maybe_array_of< double > (obj)) {
                const auto& xs = *opt;

                n = xs.size ();
                std::copy (xs.begin (), xs.end (), c0);
            }
            else {
                return;
            }
        }
        else {
            if (hasRange && n != 1) {
                error (errSyntaxError, -1, "Function's C0 array is wrong length");
                return;
            }

            n = 1;
            c0[0] = 0;
        }
    }

    //----- C1
    {
        Object obj;

        dict->lookup ("C1", &obj);
        OBJECT_GUARD (&obj);

        if (obj.isArray ()) {
            if (obj.arrayGetLength () != n) {
                error (errSyntaxError, -1, "Function's C1 array is wrong length");
                return;
            }

            if (const auto opt = xpdf::maybe_array_of< double > (obj)) {
                const auto& xs = *opt;
                std::copy (xs.begin (), xs.end (), c1);
            }
            else {
                return;
            }
        }
        else {
            if (n != 1) {
                error (errSyntaxError, -1, "Function's C1 array is wrong length");
                return;
            }

            c1[0] = 1;
        }
    }

    //----- N (exponent)
    {
        Object obj;

        dict->lookup ("N", &obj);
        OBJECT_GUARD (&obj);

        if (!obj.isNum ()) {
            error (errSyntaxError, -1, "Function has missing or invalid N");
            return;
        }

        e = obj.getNum ();
    }

    ok = true;
    return;
}

ExponentialFunction::~ExponentialFunction () {}

ExponentialFunction::ExponentialFunction (const ExponentialFunction& other)
    : Function (other), e (other.e), ok (other.ok) {
    STD_COPY_C_ARRAY (other.c0, c0);
    STD_COPY_C_ARRAY (other.c1, c1);
}

void ExponentialFunction::transform (double* in, double* out) {
    double x;
    int i;

    if (in[0] < domain[0][0]) { x = domain[0][0]; }
    else if (in[0] > domain[0][1]) {
        x = domain[0][1];
    }
    else {
        x = in[0];
    }
    for (i = 0; i < n; ++i) {
        out[i] = c0[i] + pow (x, e) * (c1[i] - c0[i]);
        if (hasRange) {
            if (out[i] < range[i][0]) { out[i] = range[i][0]; }
            else if (out[i] > range[i][1]) {
                out[i] = range[i][1];
            }
        }
    }
    return;
}

//------------------------------------------------------------------------
// StitchingFunction
//------------------------------------------------------------------------

StitchingFunction::StitchingFunction (
    Object* funcObj, Dict* dict, int recursion) {
    int i;

    ok = false;
    funcs = NULL;
    bounds = NULL;
    encode = NULL;
    scale = NULL;

    //----- initialize the generic stuff
    if (!init (dict)) {
        return;
    }

    if (m != 1) {
        error (errSyntaxError, -1, "Stitching function with more than one input");
        return;
    }

    //----- Functions
    {
        Object obj;

        dict->lookup ("Functions", &obj);
        OBJECT_GUARD (&obj);

        if (!obj.isArray ()) {
            error (
                errSyntaxError, -1,
                "Missing 'Functions' entry in stitching function");
            return;
        }

        k = obj.arrayGetLength ();

        funcs = (Function**)calloc (k, sizeof (Function*));

        bounds = (double*)calloc (k + 1, sizeof (double));
        encode = (double*)calloc (2 * k, sizeof (double));

        scale = (double*)calloc (k, sizeof (double));

        for (i = 0; i < k; ++i) {
            funcs[i] = 0;
        }

        for (i = 0; i < k; ++i) {
            Object tmp;

            funcs [i] = Function::parse (obj.arrayGet (i, &tmp), recursion + 1);
            OBJECT_GUARD (&tmp);

            if (!funcs [i]) {
                return;
            }

            if (funcs[i]->getInputSize () != 1 ||
                (i > 0 && funcs[i]->getOutputSize () != funcs[0]->getOutputSize ())) {
                error (
                    errSyntaxError, -1,
                    "Incompatible subfunctions in stitching function");
                return;
            }
        }
    }

    //----- Bounds
    {
        Object obj;

        dict->lookup ("Bounds", &obj);
        OBJECT_GUARD (&obj);

        if (!obj.isArray () || obj.arrayGetLength () != k - 1) {
            error (
                errSyntaxError, -1,
                "Missing or invalid 'Bounds' entry in stitching function");
            return;
        }

        bounds[0] = domain[0][0];

        for (i = 1; i < k; ++i) {
            Object tmp;

            obj.arrayGet (i - 1, &tmp);
            OBJECT_GUARD (&tmp);

            if (!tmp.isNum ()) {
                error (
                    errSyntaxError, -1,
                    "Invalid type in 'Bounds' array in stitching function");
                return;
            }

            bounds[i] = tmp.getNum ();
        }

        bounds[k] = domain[0][1];
    }

    //----- Encode
    {
        Object obj;

        dict->lookup ("Encode", &obj);
        OBJECT_GUARD (&obj);

        if (!obj.isArray () || obj.arrayGetLength () != 2 * k) {
            error (
                errSyntaxError, -1,
                "Missing or invalid 'Encode' entry in stitching function");
            return;
        }

        for (i = 0; i < 2 * k; ++i) {
            Object tmp;

            obj.arrayGet (i, &tmp);
            OBJECT_GUARD (&tmp);

            if (!tmp.isNum ()) {
                error (
                    errSyntaxError, -1,
                    "Invalid type in 'Encode' array in stitching function");
                return;
            }

            encode[i] = tmp.getNum ();
        }
    }

    //----- pre-compute the scale factors
    for (i = 0; i < k; ++i) {
        if (bounds[i] == bounds[i + 1]) {
            // avoid a divide-by-zero -- in this situation, function i will
            // never be used anyway
            scale[i] = 0;
        }
        else {
            scale[i] = (encode[2 * i + 1] - encode[2 * i]) /
                       (bounds[    i + 1] - bounds[    i]);
        }
    }

    ok = true;
}

StitchingFunction::StitchingFunction (const StitchingFunction& other)
    : Function (other), k (other.k),
      funcs (), bounds (), encode (), scale (), ok (other.ok) {

    funcs = (Function**)calloc (k, sizeof (Function*));

    std::transform (
        &other.funcs[0], &other.funcs[0] + k, &funcs[0],
        [](auto f) { return f->copy (); });

    bounds = (double*)calloc (k + 1, sizeof (double));
    std::copy (&other.bounds[0], &other.bounds[0] + k + 1, bounds);

    encode = (double*)calloc (2 * k, sizeof (double));
    std::copy (&other.encode[0], &other.encode[0] + 2 * k, encode);

    scale = (double*)calloc (k, sizeof (double));
    std::copy (&other.scale[0], &other.scale[0] + k, scale);
}

StitchingFunction::~StitchingFunction () {
    int i;

    if (funcs) {
        for (i = 0; i < k; ++i) {
            if (funcs[i]) { delete funcs[i]; }
        }
    }
    free (funcs);
    free (bounds);
    free (encode);
    free (scale);
}

void StitchingFunction::transform (double* in, double* out) {
    double x;
    int i;

    if (in[0] < domain[0][0]) {
        x = domain[0][0];
    }
    else if (in[0] > domain[0][1]) {
        x = domain[0][1];
    }
    else {
        x = in[0];
    }

    for (i = 0; i < k - 1; ++i) {
        if (x < bounds[i + 1]) { break; }
    }

    x = encode[2 * i] + (x - bounds[i]) * scale[i];
    funcs[i]->transform (&x, out);
}

//------------------------------------------------------------------------
// PostScriptFunction
//------------------------------------------------------------------------

// This is not an enum, because we can't foreward-declare the enum
// type in Function.h
#define psOpAbs 0
#define psOpAdd 1
#define psOpAnd 2
#define psOpAtan 3
#define psOpBitshift 4
#define psOpCeiling 5
#define psOpCopy 6
#define psOpCos 7
#define psOpCvi 8
#define psOpCvr 9
#define psOpDiv 10
#define psOpDup 11
#define psOpEq 12
#define psOpExch 13
#define psOpExp 14
#define psOpFalse 15
#define psOpFloor 16
#define psOpGe 17
#define psOpGt 18
#define psOpIdiv 19
#define psOpIndex 20
#define psOpLe 21
#define psOpLn 22
#define psOpLog 23
#define psOpLt 24
#define psOpMod 25
#define psOpMul 26
#define psOpNe 27
#define psOpNeg 28
#define psOpNot 29
#define psOpOr 30
#define psOpPop 31
#define psOpRoll 32
#define psOpRound 33
#define psOpSin 34
#define psOpSqrt 35
#define psOpSub 36
#define psOpTrue 37
#define psOpTruncate 38
#define psOpXor 39
#define psOpPush 40
#define psOpJ 41
#define psOpJz 42

#define nPSOps 43

// Note: 'if' and 'ifelse' are parsed separately.
// The rest are listed here in alphabetical order.
// The index in this table is equivalent to the psOpXXX defines.
static const char* psOpNames[] = {
    "abs",   "add",   "and", "atan", "bitshift", "ceiling", "copy",     "cos",
    "cvi",   "cvr",   "div", "dup",  "eq",       "exch",    "exp",      "false",
    "floor", "ge",    "gt",  "idiv", "index",    "le",      "ln",       "log",
    "lt",    "mod",   "mul", "ne",   "neg",      "not",     "or",       "pop",
    "roll",  "round", "sin", "sqrt", "sub",      "true",    "truncate", "xor"
};

struct PSCode {
    int op;
    union {
        double d;
        int i;
    } val;
};

#define psStackSize 100

PostScriptFunction::PostScriptFunction (Object* funcObj, Dict* dict) {
    Stream* str;
    double in[funcMaxInputs];

    codeString = NULL;
    code = NULL;
    codeSize = 0;
    ok = false;

    //----- initialize the generic stuff
    if (!init (dict)) {
        return;
    }

    if (!hasRange) {
        error (errSyntaxError, -1, "Type 4 function is missing range");
        return;
    }

    //----- get the stream

    if (!funcObj->isStream ()) {
        error (errSyntaxError, -1, "Type 4 function isn't a stream");
        return;
    }

    str = funcObj->getStream ();
    str->reset ();

    codeString = new GString ();
    auto tokens = std::make_unique< GList > ();

    for (GString* tok = getToken (str); tok = getToken (str);) {
        tokens->append (tok);
    }

    str->close ();

    //----- parse the function
    if (tokens->getLength () < 1 || ((GString*)tokens->get (0))->cmp ("{")) {
        error (errSyntaxError, -1, "Expected '{' at start of PostScript function");
        return;
    }

    int tokPtr = 1, codePtr = 0;

    if (!parseCode (tokens.get (), &tokPtr, &codePtr)) {
        return;
    }

    codeLen = codePtr;

    //----- set up the cache
    for (size_t i = 0; i < m; ++i) {
        in[i] = domain[i][0];
        cacheIn[i] = in[i] - 1;
    }

    transform (in, cacheOut);
    ok = true;
}

PostScriptFunction::PostScriptFunction (const PostScriptFunction& other)
    : Function (other),
      codeString (other.codeString->copy ()),
      code (),
      codeLen (other.codeLen),
      codeSize (other.codeSize),
      cacheIn (),
      cacheOut (),
      ok (other.ok) {
    code = (PSCode*)calloc (codeSize, sizeof (PSCode));
    std::copy (&other.code[0], &other.code[0] + codeSize, &code [0]);
}

PostScriptFunction::~PostScriptFunction () {
    free (code);

    if (codeString) {
        delete codeString;
    }
}

void PostScriptFunction::transform (double* in, double* out) {
    double stack[psStackSize];
    double x;
    int sp, i;

    // check the cache
    for (i = 0; i < m; ++i) {
        if (in[i] != cacheIn[i]) { break; }
    }

    if (i == m) {
        for (i = 0; i < n; ++i) {
            out[i] = cacheOut[i];
        }

        return;
    }

    for (i = 0; i < m; ++i) {
        stack[psStackSize - 1 - i] = in[i];
    }

    sp = exec (stack, psStackSize - m);

    // if (sp < psStackSize - n) {
    //   error(errSyntaxWarning, -1,
    // 	  "Extra values on stack at end of PostScript function");
    // }

    if (sp > psStackSize - n) {
        error (errSyntaxError, -1, "Stack underflow in PostScript function");
        sp = psStackSize - n;
    }

    for (i = 0; i < n; ++i) {
        x = stack[sp + n - 1 - i];

        if (x < range[i][0]) {
            out[i] = range[i][0];
        }
        else if (x > range[i][1]) {
            out[i] = range[i][1];
        }
        else {
            out[i] = x;
        }
    }

    // save current result in the cache
    for (i = 0; i < m; ++i) { cacheIn[i] = in[i]; }
    for (i = 0; i < n; ++i) { cacheOut[i] = out[i]; }
}

bool PostScriptFunction::parseCode (GList* tokens, int* tokPtr, int* codePtr) {
    GString* tok;
    int a, b, mid, cmp;
    int codePtr0, codePtr1;

    while (1) {
        if (*tokPtr >= tokens->getLength ()) {
            error (
                errSyntaxError, -1,
                "Unexpected end of PostScript function stream");
            return false;
        }

        tok = (GString*)tokens->get ((*tokPtr)++);
        const char* p = tok->c_str ();

        if (isdigit (*p) || *p == '.' || *p == '-') {
            addCodeD (codePtr, psOpPush, atof (tok->c_str ()));
        }
        else if (!tok->cmp ("{")) {
            codePtr0 = *codePtr;
            addCodeI (codePtr, psOpJz, 0);
            if (!parseCode (tokens, tokPtr, codePtr)) { return false; }
            if (*tokPtr >= tokens->getLength ()) {
                error (
                    errSyntaxError, -1,
                    "Unexpected end of PostScript function stream");
                return false;
            }
            tok = (GString*)tokens->get ((*tokPtr)++);
            if (!tok->cmp ("if")) { code[codePtr0].val.i = *codePtr; }
            else if (!tok->cmp ("{")) {
                codePtr1 = *codePtr;
                addCodeI (codePtr, psOpJ, 0);
                code[codePtr0].val.i = *codePtr;
                if (!parseCode (tokens, tokPtr, codePtr)) { return false; }
                if (*tokPtr >= tokens->getLength ()) {
                    error (
                        errSyntaxError, -1,
                        "Unexpected end of PostScript function stream");
                    return false;
                }
                tok = (GString*)tokens->get ((*tokPtr)++);
                if (!tok->cmp ("ifelse")) { code[codePtr1].val.i = *codePtr; }
                else {
                    error (
                        errSyntaxError, -1,
                        "Expected 'ifelse' in PostScript function stream");
                    return false;
                }
            }
            else {
                error (
                    errSyntaxError, -1,
                    "Expected 'if' in PostScript function stream");
                return false;
            }
        }
        else if (!tok->cmp ("}")) {
            break;
        }
        else if (!tok->cmp ("if")) {
            error (
                errSyntaxError, -1,
                "Unexpected 'if' in PostScript function stream");
            return false;
        }
        else if (!tok->cmp ("ifelse")) {
            error (
                errSyntaxError, -1,
                "Unexpected 'ifelse' in PostScript function stream");
            return false;
        }
        else {
            a = -1;
            b = nPSOps;
            cmp = 0; // make gcc happy
            // invariant: psOpNames[a] < tok < psOpNames[b]
            while (b - a > 1) {
                mid = (a + b) / 2;
                cmp = tok->cmp (psOpNames[mid]);
                if (cmp > 0) { a = mid; }
                else if (cmp < 0) {
                    b = mid;
                }
                else {
                    a = b = mid;
                }
            }
            if (cmp != 0) {
                error (
                    errSyntaxError, -1,
                    "Unknown operator '{0:t}' in PostScript function", tok);
                return false;
            }
            addCode (codePtr, a);
        }
    }

    return true;
}

void PostScriptFunction::addCode (int* codePtr, int op) {
    if (*codePtr >= codeSize) {
        if (codeSize) { codeSize *= 2; }
        else {
            codeSize = 16;
        }
        code = (PSCode*)reallocarray (code, codeSize, sizeof (PSCode));
    }
    code[*codePtr].op = op;
    ++(*codePtr);
}

void PostScriptFunction::addCodeI (int* codePtr, int op, int x) {
    if (*codePtr >= codeSize) {
        if (codeSize) { codeSize *= 2; }
        else {
            codeSize = 16;
        }
        code = (PSCode*)reallocarray (code, codeSize, sizeof (PSCode));
    }
    code[*codePtr].op = op;
    code[*codePtr].val.i = x;
    ++(*codePtr);
}

void PostScriptFunction::addCodeD (int* codePtr, int op, double x) {
    if (*codePtr >= codeSize) {
        if (codeSize) { codeSize *= 2; }
        else {
            codeSize = 16;
        }
        code = (PSCode*)reallocarray (code, codeSize, sizeof (PSCode));
    }
    code[*codePtr].op = op;
    code[*codePtr].val.d = x;
    ++(*codePtr);
}

GString* PostScriptFunction::getToken (Stream* str) {
    GString* s;
    int c;
    bool comment;

    s = new GString ();
    comment = false;
    while (1) {
        if ((c = str->getChar ()) == EOF) {
            delete s;
            return NULL;
        }
        codeString->append (c);
        if (comment) {
            if (c == '\x0a' || c == '\x0d') { comment = false; }
        }
        else if (c == '%') {
            comment = true;
        }
        else if (!isspace (c)) {
            break;
        }
    }
    if (c == '{' || c == '}') { s->append ((char)c); }
    else if (isdigit (c) || c == '.' || c == '-') {
        while (1) {
            s->append ((char)c);
            c = str->lookChar ();
            if (c == EOF || !(isdigit (c) || c == '.' || c == '-')) { break; }
            str->getChar ();
            codeString->append (c);
        }
    }
    else {
        while (1) {
            s->append ((char)c);
            c = str->lookChar ();
            if (c == EOF || !isalnum (c)) { break; }
            str->getChar ();
            codeString->append (c);
        }
    }
    return s;
}

int PostScriptFunction::exec (double* stack, int sp0) {
    PSCode* c;
    double tmp[psStackSize];
    double t;
    int sp, ip, nn, k, i;

    sp = sp0;
    ip = 0;
    while (ip < codeLen) {
        c = &code[ip++];
        switch (c->op) {
        case psOpAbs:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = fabs (stack[sp]);
            break;
        case psOpAdd:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] + stack[sp];
            ++sp;
            break;
        case psOpAnd:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = (int)stack[sp + 1] & (int)stack[sp];
            ++sp;
            break;
        case psOpAtan:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = atan2 (stack[sp + 1], stack[sp]);
            ++sp;
            break;
        case psOpBitshift:
            if (sp + 1 >= psStackSize) { goto underflow; }
            k = (int)stack[sp + 1];
            nn = (int)stack[sp];
            if (nn > 0) { stack[sp + 1] = k << nn; }
            else if (nn < 0) {
                stack[sp + 1] = k >> -nn;
            }
            else {
                stack[sp + 1] = k;
            }
            ++sp;
            break;
        case psOpCeiling:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = ceil (stack[sp]);
            break;
        case psOpCopy:
            if (sp >= psStackSize) { goto underflow; }
            nn = (int)stack[sp++];
            if (nn < 0) { goto invalidArg; }
            if (sp + nn > psStackSize) { goto underflow; }
            if (sp - nn < 0) { goto overflow; }
            for (i = 0; i < nn; ++i) { stack[sp - nn + i] = stack[sp + i]; }
            sp -= nn;
            break;
        case psOpCos:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = cos (stack[sp]);
            break;
        case psOpCvi:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = (int)stack[sp];
            break;
        case psOpCvr:
            if (sp >= psStackSize) { goto underflow; }
            break;
        case psOpDiv:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] / stack[sp];
            ++sp;
            break;
        case psOpDup:
            if (sp >= psStackSize) { goto underflow; }
            if (sp < 1) { goto overflow; }
            stack[sp - 1] = stack[sp];
            --sp;
            break;
        case psOpEq:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] == stack[sp] ? 1 : 0;
            ++sp;
            break;
        case psOpExch:
            if (sp + 1 >= psStackSize) { goto underflow; }
            t = stack[sp];
            stack[sp] = stack[sp + 1];
            stack[sp + 1] = t;
            break;
        case psOpExp:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = pow (stack[sp + 1], stack[sp]);
            ++sp;
            break;
        case psOpFalse:
            if (sp < 1) { goto overflow; }
            stack[sp - 1] = 0;
            --sp;
            break;
        case psOpFloor:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = floor (stack[sp]);
            break;
        case psOpGe:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] >= stack[sp] ? 1 : 0;
            ++sp;
            break;
        case psOpGt:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] > stack[sp] ? 1 : 0;
            ++sp;
            break;
        case psOpIdiv:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = (int)stack[sp + 1] / (int)stack[sp];
            ++sp;
            break;
        case psOpIndex:
            if (sp >= psStackSize) { goto underflow; }
            k = (int)stack[sp];
            if (k < 0) { goto invalidArg; }
            if (sp + 1 + k >= psStackSize) { goto underflow; }
            stack[sp] = stack[sp + 1 + k];
            break;
        case psOpLe:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] <= stack[sp] ? 1 : 0;
            ++sp;
            break;
        case psOpLn:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = log (stack[sp]);
            break;
        case psOpLog:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = log10 (stack[sp]);
            break;
        case psOpLt:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] < stack[sp] ? 1 : 0;
            ++sp;
            break;
        case psOpMod:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = (int)stack[sp + 1] % (int)stack[sp];
            ++sp;
            break;
        case psOpMul:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] * stack[sp];
            ++sp;
            break;
        case psOpNe:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] != stack[sp] ? 1 : 0;
            ++sp;
            break;
        case psOpNeg:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = -stack[sp];
            break;
        case psOpNot:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = stack[sp] == 0 ? 1 : 0;
            break;
        case psOpOr:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = (int)stack[sp + 1] | (int)stack[sp];
            ++sp;
            break;
        case psOpPop:
            if (sp >= psStackSize) { goto underflow; }
            ++sp;
            break;
        case psOpRoll:
            if (sp + 1 >= psStackSize) { goto underflow; }
            k = (int)stack[sp++];
            nn = (int)stack[sp++];
            if (nn < 0) { goto invalidArg; }
            if (sp + nn > psStackSize) { goto underflow; }
            if (k >= 0) { k %= nn; }
            else {
                k = -k % nn;
                if (k) { k = nn - k; }
            }
            for (i = 0; i < nn; ++i) { tmp[i] = stack[sp + i]; }
            for (i = 0; i < nn; ++i) { stack[sp + i] = tmp[(i + k) % nn]; }
            break;
        case psOpRound:
            if (sp >= psStackSize) { goto underflow; }
            t = stack[sp];
            stack[sp] = (t >= 0) ? floor (t + 0.5) : ceil (t - 0.5);
            break;
        case psOpSin:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = sin (stack[sp]);
            break;
        case psOpSqrt:
            if (sp >= psStackSize) { goto underflow; }
            stack[sp] = sqrt (stack[sp]);
            break;
        case psOpSub:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = stack[sp + 1] - stack[sp];
            ++sp;
            break;
        case psOpTrue:
            if (sp < 1) { goto overflow; }
            stack[sp - 1] = 1;
            --sp;
            break;
        case psOpTruncate:
            if (sp >= psStackSize) { goto underflow; }
            t = stack[sp];
            stack[sp] = (t >= 0) ? floor (t) : ceil (t);
            break;
        case psOpXor:
            if (sp + 1 >= psStackSize) { goto underflow; }
            stack[sp + 1] = (int)stack[sp + 1] ^ (int)stack[sp];
            ++sp;
            break;
        case psOpPush:
            if (sp < 1) { goto overflow; }
            stack[--sp] = c->val.d;
            break;
        case psOpJ: ip = c->val.i; break;
        case psOpJz:
            if (sp >= psStackSize) { goto underflow; }
            k = (int)stack[sp++];
            if (k == 0) { ip = c->val.i; }
            break;
        }
    }
    return sp;

underflow:
    error (errSyntaxError, -1, "Stack underflow in PostScript function");
    return sp;
overflow:
    error (errSyntaxError, -1, "Stack overflow in PostScript function");
    return sp;
invalidArg:
    error (errSyntaxError, -1, "Invalid arg in PostScript function");
    return sp;
}

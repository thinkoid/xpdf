// -*- mode: c++; -*-

#include <memory.hh>

#include <errno.h>
#include <stdlib.h>

#if defined(__APPLE__)
#if defined(__GNUC__)

#define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))

void *reallocarray(void *ptr, size_t n, size_t size)
{
    size_t bytes;

    if (__builtin_mul_overflow(n, size, &bytes)) {
        errno = ENOMEM;
        return 0;
    }

    return realloc(ptr, bytes);
}

#endif // __GNUC__
#endif // __APPLE__

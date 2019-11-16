//========================================================================
//
// GMutex.h
//
// Portable mutex macros.
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef GMUTEX_H
#define GMUTEX_H

// Usage:
//
// GMutex m;
// gInitMutex(&m);
// ...
// gLockMutex(&m);
//   ... critical section ...
// gUnlockMutex(&m);
// ...
// gDestroyMutex(&m);

#include <pthread.h>

typedef pthread_mutex_t GMutex;

#define gInitMutex(m) pthread_mutex_init (m, NULL)
#define gDestroyMutex(m) pthread_mutex_destroy (m)
#define gLockMutex(m) pthread_mutex_lock (m)
#define gUnlockMutex(m) pthread_mutex_unlock (m)

#endif

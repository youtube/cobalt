// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <config.h>

// When defined, different heap allocators can be used via an environment
// variable set before running the program.  This may reduce the amount
// of inlining that we get with malloc/free/etc.  Disabling makes it
// so that only tcmalloc can be used.
#define ENABLE_DYNAMIC_ALLOCATOR_SWITCHING

// TODO(mbelshe): Ensure that all calls to tcmalloc have the proper call depth
// from the "user code" so that debugging tools (HeapChecker) can work.

// __THROW is defined in glibc systems.  It means, counter-intuitively,
// "This function will never throw an exception."  It's an optional
// optimization tool, but we may need to use it to match glibc prototypes.
#ifndef __THROW    // I guess we're not on a glibc system
# define __THROW   // __THROW is just an optimization, so ok to make it ""
#endif

// new_mode behaves similarly to MSVC's _set_new_mode.
// If flag is 0 (default), calls to malloc will behave normally.
// If flag is 1, calls to malloc will behave like calls to new,
// and the std_new_handler will be invoked on failure.
// Can be set by calling _set_new_mode().
static int new_mode = 0;

typedef enum {
  TCMALLOC,    // TCMalloc is the default allocator.
  JEMALLOC,    // JEMalloc
  WINDEFAULT,  // Windows Heap
  WINLFH,      // Windows LFH Heap
} Allocator;

// This is the default allocator.
static Allocator allocator = WINDEFAULT;

// We include tcmalloc and the win_allocator to get as much inlining as
// possible.
#include "tcmalloc.cc"
#include "win_allocator.cc"

// Forward declarations from jemalloc.
extern "C" {
void* je_malloc(size_t s);
void* je_realloc(void* p, size_t s);
void je_free(void* s);
size_t je_msize(void* p);
bool je_malloc_init_hard();
}

extern "C" {

// Call the new handler, if one has been set.
// Returns true on successfully calling the handler, false otherwise.
inline bool call_new_handler(bool nothrow) {
  // Get the current new handler.  NB: this function is not
  // thread-safe.  We make a feeble stab at making it so here, but
  // this lock only protects against tcmalloc interfering with
  // itself, not with other libraries calling set_new_handler.
  std::new_handler nh;
  {
    SpinLockHolder h(&set_new_handler_lock);
    nh = std::set_new_handler(0);
    (void) std::set_new_handler(nh);
  }
#if (defined(__GNUC__) && !defined(__EXCEPTIONS)) || (defined(_HAS_EXCEPTIONS) && !_HAS_EXCEPTIONS)
  if (!nh)
    return false;
  // Since exceptions are disabled, we don't really know if new_handler
  // failed.  Assume it will abort if it fails.
  (*nh)();
  return false;  // break out of the retry loop.
#else
  // If no new_handler is established, the allocation failed.
  if (!nh) {
    if (nothrow)
      return 0;
    throw std::bad_alloc();
  }
  // Otherwise, try the new_handler.  If it returns, retry the
  // allocation.  If it throws std::bad_alloc, fail the allocation.
  // if it throws something else, don't interfere.
  try {
    (*nh)();
  } catch (const std::bad_alloc&) {
    if (!nothrow)
      throw;
    return true;
  }
#endif  // (defined(__GNUC__) && !defined(__EXCEPTIONS)) || (defined(_HAS_EXCEPTIONS) && !_HAS_EXCEPTIONS)
}

void* malloc(size_t size) __THROW {
  void* ptr;
  for (;;) {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
    switch (allocator) {
      case JEMALLOC:
        ptr = je_malloc(size);
        break;
      case WINDEFAULT:
      case WINLFH:
        ptr = win_heap_malloc(size);
        break;
      case TCMALLOC:
      default:
        ptr = do_malloc(size);
        break;
    }
#else
    // TCMalloc case.
    ptr = do_malloc(size);
#endif
    if (ptr)
      return ptr;

    if (!new_mode || !call_new_handler(true))
      break;
  }
  return ptr;
}

void free(void* p) __THROW {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  switch (allocator) {
    case JEMALLOC:
      je_free(p);
      return;
    case WINDEFAULT:
    case WINLFH:
      win_heap_free(p);
      return;
  }
#endif
  // TCMalloc case.
  do_free(p);
}

void* realloc(void* ptr, size_t size) __THROW {
  // Webkit is brittle for allocators that return NULL for malloc(0).  The
  // realloc(0, 0) code path does not guarantee a non-NULL return, so be sure
  // to call malloc for this case.
  if (!ptr)
    return malloc(size);

  void* new_ptr;
  for (;;) {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
    switch (allocator) {
      case JEMALLOC:
        new_ptr = je_realloc(ptr, size);
        break;
      case WINDEFAULT:
      case WINLFH:
        new_ptr = win_heap_realloc(ptr, size);
        break;
      case TCMALLOC:
      default:
        new_ptr = do_realloc(ptr, size);
        break;
    }
#else
    // TCMalloc case.
    new_ptr = do_realloc(ptr, size);
#endif

    // Subtle warning:  NULL return does not alwas indicate out-of-memory.  If
    // the requested new size is zero, realloc should free the ptr and return
    // NULL.
    if (new_ptr || !size)
      return new_ptr;
    if (!new_mode || !call_new_handler(true))
      break;
  }
  return new_ptr;
}

// TODO(mbelshe): Implement this for other allocators.
void malloc_stats(void) __THROW {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  switch (allocator) {
    case JEMALLOC:
      // No stats.
      return;
    case WINDEFAULT:
    case WINLFH:
      // No stats.
      return;
  }
#endif
  tc_malloc_stats();
}

#ifdef WIN32

extern "C" size_t _msize(void* p) {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  switch (allocator) {
    case JEMALLOC:
      return je_msize(p);
    case WINDEFAULT:
    case WINLFH:
      return win_heap_msize(p);
  }
#endif
  return MallocExtension::instance()->GetAllocatedSize(p);
}

// This is included to resolve references from libcmt.
extern "C" intptr_t _get_heap_handle() {
  return 0;
}

// The CRT heap initialization stub.
extern "C" int _heap_init() {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  const char* override = GetenvBeforeMain("CHROME_ALLOCATOR");
  if (override) {
    if (!stricmp(override, "jemalloc"))
      allocator = JEMALLOC;
    else if (!stricmp(override, "winheap"))
      allocator = WINDEFAULT;
    else if (!stricmp(override, "winlfh"))
      allocator = WINLFH;
    else if (!stricmp(override, "tcmalloc"))
      allocator = TCMALLOC;
  }

  switch (allocator) {
    case JEMALLOC:
      return je_malloc_init_hard() ? 0 : 1;
    case WINDEFAULT:
      return win_heap_init(false) ? 1 : 0;
    case WINLFH:
      return win_heap_init(true) ? 1 : 0;
    case TCMALLOC:
    default:
      // fall through
      break;
  }
#endif
  // Initializing tcmalloc.
  // We intentionally leak this object.  It lasts for the process
  // lifetime.  Trying to teardown at _heap_term() is so late that
  // you can't do anything useful anyway.
  new TCMallocGuard();
  return 1;
}

// The CRT heap cleanup stub.
extern "C" void _heap_term() {}

// We set this to 1 because part of the CRT uses a check of _crtheap != 0
// to test whether the CRT has been initialized.  Once we've ripped out
// the allocators from libcmt, we need to provide this definition so that
// the rest of the CRT is still usable.
extern "C" void* _crtheap = reinterpret_cast<void*>(1);

#endif  // WIN32

#include "generic_allocators.cc"

}  // extern C

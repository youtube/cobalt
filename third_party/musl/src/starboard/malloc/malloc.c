#include <stdio.h>

#if SB_API_VERSION < 16
#include "starboard/memory.h"

void *calloc(size_t m, size_t n) {
  return SbMemoryCalloc(m, n);
}

void free(void *p) {
  SbMemoryDeallocate(p);
}

void *malloc(size_t n) {
  return SbMemoryAllocate(n);
}

void *realloc(void *p, size_t n) {
  return SbMemoryReallocate(p, n);
}

weak_alias(calloc, __libc_calloc);
weak_alias(free, __libc_free);
weak_alias(malloc, __libc_malloc);
weak_alias(realloc, __libc_realloc);

#endif  // SB_API_VERSION < 16

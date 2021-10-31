#include <stdio.h>
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
